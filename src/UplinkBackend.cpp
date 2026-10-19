#include "UplinkBackend.h"

#include <algorithm>
#include <dlfcn.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QVariantMap>

#include "Points.h"
#include "blockchain/BlockchainNodeService.h"
#include "lez/LezReferralService.h"
#include "mock/MockNodeService.h"
#include "mock/MockReferralService.h"

using Rep = UplinkUiSimpleSource;

namespace {

constexpr int kPollMs = 5000;
constexpr qint64 kSyncChunk = 100;   // blocks per step, as the LEZ Wallet app does
constexpr int kActivityEpochs = 16;

// TODO: replace with the real payout form's URL once it exists. This is the prototype's form.
const QString kPayoutFormUrl = QStringLiteral("https://xalisher.github.io/lx-preview/web/");

// Names the points account in the user's LEZ wallet; also how Uplink finds it again.
const QString kIdentityLabel = QStringLiteral("Uplink points account");

QString newReference()
{
    QByteArray bytes(32, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(bytes.data()), bytes.size() / 4);
    return QString::fromLatin1(bytes.toHex());
}

int toRep(referral::Status status)
{
    switch (status) {
    case referral::Status::Pending: return Rep::OperationPending;
    case referral::Status::Settled: return Rep::OperationSettled;
    case referral::Status::Rejected: return Rep::OperationRejected;
    }
    return Rep::OperationRejected;
}

int toRep(node::Issue issue)
{
    switch (issue) {
    case node::Issue::None: return Rep::NoIssue;
    case node::Issue::ModuleUnavailable: return Rep::ModuleUnavailable;
    case node::Issue::NotRunning: return Rep::NodeNotRunning;
    case node::Issue::Bootstrapping: return Rep::NodeBootstrapping;
    case node::Issue::NotCore: return Rep::NotCoreNode;
    case node::Issue::NoBlendPeers: return Rep::NoBlendPeers;
    case node::Issue::NodeIdUnavailable: return Rep::NodeIdUnavailable;
    }
    return Rep::ModuleUnavailable;
}

// UPLINK_BACKEND=mock|real sets both sides and overrides the per-side variable.
QString backendFor(const char* side)
{
    const QString both = qEnvironmentVariable("UPLINK_BACKEND");
    return both.isEmpty() ? qEnvironmentVariable(side) : both;
}

// Where the key file picker opens: Basecamp's data folder, as Basecamp names it
// (LOGOS_USER_DIR, else <app data>/Logos/LogosBasecamp, "Dev" for dev builds). Uplink
// doesn't know where the node keeps its files under it; the user finds keystore.yaml there.
QString basecampDataFolder()
{
    const QString userDir = qEnvironmentVariable("LOGOS_USER_DIR");
    QStringList candidates;
    if (!userDir.isEmpty())
        candidates << userDir;
    const QString generic = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    candidates << generic + QStringLiteral("/Logos/LogosBasecampDev")
               << generic + QStringLiteral("/Logos/LogosBasecamp")
               << generic + QStringLiteral("/Logos");
    for (const QString& dir : candidates)
        if (QFileInfo(dir).isDir())
            return dir;
    return QDir::homePath();
}

QString localPath(const QString& pathOrUrl)
{
    const QUrl url(pathOrUrl.trimmed());
    return url.isLocalFile() ? url.toLocalFile() : pathOrUrl.trimmed();
}

// This plugin's own metadata.json, read at startup as Blockchain UI reads its own. The host
// installs it next to the plugin file, but the generated UI glue gets no module path (Blockchain
// UI's hand-written plugin does, through initModuleContext), so the folder is that of the file
// this code was loaded from.
QVariantMap ownMetadata()
{
    Dl_info info{};
    if (!dladdr(reinterpret_cast<void*>(&ownMetadata), &info) || !info.dli_fname) {
        qWarning() << "Uplink: can't tell where the plugin is installed, so metadata.json is unread";
        return {};
    }
    const QString path = QFileInfo(QString::fromLocal8Bit(info.dli_fname)).absoluteDir().filePath(QStringLiteral("metadata.json"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Uplink: could not open" << path << "- no release deployment";
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        qWarning() << "Uplink:" << path << "is not a JSON object - no release deployment";
        return {};
    }
    return doc.object().toVariantMap();
}

} // namespace

UplinkBackend::UplinkBackend(QObject* parent)
    : UplinkUiSimpleSource(parent)
{
    // real (default) | mock | mock:no_wallet | mock:no_program
    const QString lez = backendFor("UPLINK_LEZ");
    const bool mock = lez.section(':', 0, 0) == QLatin1String("mock");
    if (mock) {
        auto service = std::make_unique<MockReferralService>(lez.section(':', 1) != QLatin1String("no_wallet"));
        if (lez.section(':', 1) == QLatin1String("no_program"))
            service->configure({});
        m_referral = std::move(service);
    } else
        m_referral = std::make_unique<LezReferralService>(*this);

    // real (default) | mock | mock:<issue>
    const QString node = backendFor("UPLINK_NODE");
    if (node.section(':', 0, 0) == QLatin1String("mock"))
        m_node = std::make_unique<MockNodeService>(MockNodeService::issueFromName(node.section(':', 1)));
    else
        m_node = std::make_unique<BlockchainNodeService>(*this);

    init();

    // The deployment this release is for: metadata.json's referral_program. The mock is a
    // network of its own, so a release's deployment means nothing there.
    const QVariantMap release = ownMetadata().value(QStringLiteral("referral_program")).toMap();
    const QString chain = release.value(QStringLiteral("chain_id")).toString().trimmed();
    const QString program = release.value(QStringLiteral("program_account")).toString().trimmed();
    qInfo() << "Uplink: release deployment" << (program.isEmpty() ? QStringLiteral("none") : program)
            << "on chain" << (chain.isEmpty() ? QStringLiteral("any") : chain) << (mock ? "(ignored by the mock)" : "");
    if (!mock)
        setReleaseDeployment(chain, program);
}

UplinkBackend::UplinkBackend(std::unique_ptr<referral::ReferralService> referral,
                             std::unique_ptr<node::NodeService> node, const QString& dataDir,
                             QObject* parent)
    : UplinkUiSimpleSource(parent)
    , m_referral(std::move(referral))
    , m_node(std::move(node))
    , m_dataDir(dataDir)
{
    init();
}

void UplinkBackend::init()
{
    setProgramIssue(ProgramUnknown);
    setNodeIssue(ModuleUnavailable);
    setWalletIssue(LezCoreUnavailable);
    setEnrolState(NoIdentity);
    setCashOutState(CashOutIdle);
    setPayoutFormUrl(kPayoutFormUrl);
    setInvitationCheck(InvitationEmpty);
    setKeystoreIssue(KeystoreNotChosen);
    setKeystoreFolder(QUrl::fromLocalFile(basecampDataFolder()).toString());
    setClaimablePoints(QStringLiteral("0"));
    setRewardBalance(QStringLiteral("0"));
    setTotalPoints(QStringLiteral("0"));
    setCashedOutPoints(QStringLiteral("0"));
    loadSettings();

    m_poll.setInterval(kPollMs);
    connect(&m_poll, &QTimer::timeout, this, &UplinkBackend::refresh);
}

UplinkBackend::~UplinkBackend() = default;

void UplinkBackend::onContextReady()
{
    refresh();
    m_poll.start();
}

// Uplink's own settings: the key file's path and whether to claim automatically.
void UplinkBackend::loadSettings()
{
    QSettings ini(dataFile(), QSettings::IniFormat);
    setAutoClaim(ini.value(QStringLiteral("settings/autoClaim"), true).toBool());
    QString path = ini.value(QStringLiteral("settings/keystorePath")).toString();
    // The mock's node has a key file of its own, so the mock walkthrough needs no picker.
    if (path.isEmpty() && dynamic_cast<MockReferralService*>(m_referral.get()))
        path = MockReferralService::keystorePath();
    setKeystorePath(path);
}

void UplinkBackend::setReleaseDeployment(const QString& chainId, const QString& programAccount)
{
    m_releaseChain = chainId;
    m_releaseProgram = programAccount;
    m_deploymentApplied = false;
    setProgramIssue(ProgramUnknown);
}

void UplinkBackend::refresh()
{
    refreshNode();
    applyDeployment();
    checkKeystore();
    refreshTerms();   // the module's, whether or not a wallet is open
    refreshWallet();
    if (walletIssue() != WalletReady)
        return;
    checkProgram();
    if (programIssue() != ProgramReady)
        return;
    syncWallet();
    reconcileOperations();
    refreshReferral();
    autoClaimIfDue();
}

// Catches the wallet up in chunks so the event loop keeps turning. The LEZ Wallet
// app syncs the same wallet while it is open; lez_core serialises the calls.
void UplinkBackend::syncWallet()
{
    if (m_syncing)
        return;
    const auto height = m_referral->currentBlockHeight();
    const auto synced = m_referral->lastSyncedBlock();
    if (!height.ok() || !synced.ok())
        return;
    setChainHeight(static_cast<int>(height.value));
    setSyncedBlock(static_cast<int>(synced.value));
    if (synced.value >= height.value)
        return;
    m_syncing = true;
    m_syncTarget = height.value;
    QTimer::singleShot(0, this, &UplinkBackend::syncNextChunk);
}

void UplinkBackend::syncNextChunk()
{
    const auto synced = m_referral->lastSyncedBlock();
    if (!synced.ok() || synced.value >= m_syncTarget) {
        m_syncing = false;
        refreshReferral();
        return;
    }
    const auto stepped = m_referral->syncToBlock(qMin(synced.value + kSyncChunk, m_syncTarget));
    const auto after = m_referral->lastSyncedBlock();
    if (!stepped.ok() || !after.ok() || after.value <= synced.value) {
        m_syncing = false;
        if (!stepped.ok())
            fail(stepped.error);
        return;
    }
    setSyncedBlock(static_cast<int>(after.value));
    QTimer::singleShot(0, this, &UplinkBackend::syncNextChunk);
}

void UplinkBackend::refreshWallet()
{
    const auto open = m_referral->walletOpen();
    setWalletIssueDetail(open.ok() ? QString() : open.error);
    setWalletIssue(!open.ok() ? LezCoreUnavailable : open.value ? WalletReady : NoWalletOpen);
    if (walletIssue() == NoWalletOpen)
        forgetIdentity();
    if (walletIssue() != WalletReady)
        return;

    // The identity is the labelled account in whichever wallet is open, so switching
    // wallets in the LEZ Wallet app switches it too (or leaves none).
    const auto labelled = m_referral->resolveLabel(kIdentityLabel);
    if (!labelled.ok() || labelled.value.compare(participantId(), Qt::CaseInsensitive) == 0)
        return;
    forgetIdentity();
    if (labelled.value.isEmpty())
        return;
    setParticipantId(labelled.value);
    setEnrolState(IdentityCreated);
    showIdentity();
}

void UplinkBackend::refreshTerms()
{
    if (termsVersion() == 0) {
        const auto terms = m_referral->terms();
        if (terms.ok()) {
            setTermsVersion(terms.value.version);
            setTermsSha256(terms.value.sha256);
            setTermsLastUpdated(terms.value.lastUpdated);
            setTermsSections(terms.value.sections);
        }
    }
    const auto accepted = m_referral->termsAccepted();
    if (accepted.ok())
        setTermsAccepted(accepted.value);
}

// Reads only the file's public keys: which node it is for. Checked again when the running
// node changes, so a key file for another node is caught before anything is signed.
void UplinkBackend::checkKeystore()
{
    if (keystorePath().isEmpty()) {
        setKeystoreIssue(KeystoreNotChosen);
        return;
    }
    if (m_checkedKeystoreFor == nodeId() + QLatin1Char('|') + keystorePath() && keystoreIssue() != KeystoreNotChosen)
        return;
    m_checkedKeystoreFor = nodeId() + QLatin1Char('|') + keystorePath();
    const auto keyNode = m_referral->keystoreNode(keystorePath());
    if (!keyNode.ok()) {
        setKeystoreNode({});
        setKeystoreIssueDetail(keyNode.error);
        setKeystoreIssue(KeystoreUnreadable);
        m_checkedKeystoreFor.clear();   // try again next refresh (lez stack may not be up yet)
        return;
    }
    setKeystoreNode(keyNode.value);
    setKeystoreIssueDetail({});
    const bool wrongNode = !nodeId().isEmpty() && keyNode.value.compare(nodeId(), Qt::CaseInsensitive) != 0;
    setKeystoreIssue(wrongNode ? KeystoreWrongNode : KeystoreReady);
}

void UplinkBackend::chooseKeystore(QString pathOrUrl)
{
    setLastError({});
    const QString path = localPath(pathOrUrl);
    if (path.isEmpty())
        return;
    QSettings ini(dataFile(), QSettings::IniFormat);
    ini.setValue(QStringLiteral("settings/keystorePath"), path);
    setKeystorePath(path);
    m_checkedKeystoreFor.clear();
    checkKeystore();
}

void UplinkBackend::changeAutoClaim(bool on)
{
    QSettings ini(dataFile(), QSettings::IniFormat);
    ini.setValue(QStringLiteral("settings/autoClaim"), on);
    setAutoClaim(on);
}

void UplinkBackend::acceptTerms()
{
    setLastError({});
    if (termsVersion() == 0) {
        fail(QStringLiteral("The terms couldn't be loaded, so they can't be accepted yet."));
        return;
    }
    const auto accepted = m_referral->acceptTerms(termsVersion(), termsSha256());
    if (!accepted.ok()) {
        fail(accepted.error);
        return;
    }
    setTermsAccepted(accepted.value);
}

// Everything tied to the points account, as if none had been found.
void UplinkBackend::forgetIdentity()
{
    if (participantId().isEmpty())
        return;
    m_operations.clear();
    m_registerReference.clear();
    m_collectReference.clear();
    m_cashOutReference.clear();
    m_receiptsBefore.clear();
    m_labels.clear();
    m_labelsOwner.clear();
    setParticipantId({});
    setIdentityLabel({});
    setIdentityAddress({});
    setEnrolState(NoIdentity);
    setReferrerNode({});
    setInvitation({});
    setNodeActive(false);
    setMyActivity({});
    setReferrals({});
    setClaimablePoints(QStringLiteral("0"));
    setRewardBalance(QStringLiteral("0"));
    setTotalPoints(QStringLiteral("0"));
    setCashedOutPoints(QStringLiteral("0"));
    setReceipts({});
    setPayoutCode({});
    setPayoutPoints({});
    setCashOutState(CashOutIdle);
    publishOperations();
}

void UplinkBackend::showIdentity()
{
    setIdentityLabel(kIdentityLabel);
    const auto address = m_referral->accountAddress(participantId());
    setIdentityAddress(address.ok() ? address.value : participantId());
}

void UplinkBackend::refreshNode()
{
    const node::Status s = m_node->status();
    setNodeIssue(toRep(s.issue));
    setNodeIssueDetail(s.detail);
    setNodeMode(s.mode);
    setNodeCore(s.core);
    setHealthyBlendPeers(s.healthyBlendPeers);
    setNodeId(s.nodeId);
    setChainId(s.chainId);
}

// Selects the release's deployment in uplink_module, before anything is saved for it (terms
// acceptance is the first). A release naming none leaves the module's own choice.
void UplinkBackend::applyDeployment()
{
    if (!m_releaseChain.isEmpty() && !chainId().isEmpty() && chainId() != m_releaseChain) {
        setProgramIssueDetail(tr("This Uplink release is for the %1 network, and your node is on %2.")
                                  .arg(m_releaseChain, chainId()));
        setProgramIssue(ProgramOtherNetwork);
        return;
    }
    if (programIssue() == ProgramOtherNetwork) {
        setProgramIssue(ProgramUnknown);
        setProgramIssueDetail({});
    }
    if (m_releaseProgram.isEmpty() || m_deploymentApplied)
        return;
    const auto current = m_referral->programAccount();
    if (!current.ok())
        return;   // uplink_module isn't up yet
    if (current.value.compare(m_releaseProgram, Qt::CaseInsensitive) != 0) {
        const auto configured = m_referral->configure(m_releaseProgram);
        if (!configured.ok()) {
            qWarning() << "Uplink: uplink_module refused the release's program account:" << configured.error;
            return;
        }
    }
    m_deploymentApplied = true;
}

// Whether the selected deployment runs uplink_module's program, on the wallet's network.
// Checked until it does; uplink_module checks again before every transaction.
void UplinkBackend::checkProgram()
{
    if (programIssue() == ProgramReady || programIssue() == ProgramOtherNetwork)
        return;
    const auto check = m_referral->checkProgram();
    if (!check.ok())
        return;   // lez_core can't tell yet; try again next refresh
    setProgramIssueDetail(check.value.detail);
    switch (check.value.state) {
    case referral::ProgramCheck::Ready: setProgramIssue(ProgramReady); break;
    case referral::ProgramCheck::NotConfigured: setProgramIssue(ProgramNotConfigured); break;
    case referral::ProgramCheck::NotDeployed: setProgramIssue(ProgramNotDeployed); break;
    case referral::ProgramCheck::Mismatch: setProgramIssue(ProgramMismatch); break;
    }
}

void UplinkBackend::refreshReferral()
{
    const auto registry = m_referral->registry();
    if (!registry.ok()) {
        fail(registry.error);
        return;
    }
    setEpoch(static_cast<int>(registry.value.epoch));
    setActiveCount(registry.value.active.size());

    if (participantId().isEmpty())
        return;
    const auto participant = m_referral->participant(participantId());
    if (!participant.ok())
        return;   // not registered yet
    const referral::Participant& p = participant.value;
    setEnrolState(Enrolled);
    setNodeActive(registry.value.active.contains(p.node));
    setReferrerNode(p.referrer);
    loadLabels();

    const auto invitation = m_referral->invitation(participantId(), p.node);
    if (invitation.ok())
        setInvitation(invitation.value);

    QStringList referralNodes = p.children.keys();
    const auto notes = m_referral->notes(participantId());
    if (notes.ok())
        for (const referral::Note& note : notes.value)
            if (note.kind == referral::Note::Child && !referralNodes.contains(note.node))
                referralNodes << note.node;

    // The module knows each published epoch's active set (live, or from the indexer).
    const auto activity = m_referral->activity(QStringList{p.node} + referralNodes, kActivityEpochs);
    auto marks = [&](const QString& node) {
        QVariantList list;
        if (activity.ok())
            for (int mark : activity.value.nodes.value(node))
                list << mark;
        return list;
    };
    setMyActivity(marks(p.node));

    QVariantList referrals;
    auto addReferral = [&](const QString& node, quint32 lastPaid, bool pending) {
        referrals << QVariantMap{
            {QStringLiteral("node"), node},
            {QStringLiteral("label"), m_labels.value(node)},
            {QStringLiteral("activeThisEpoch"), registry.value.active.contains(node)},
            {QStringLiteral("lastPaidEpoch"), lastPaid},
            {QStringLiteral("pending"), pending},   // announced, not yet taken in by a claim
            {QStringLiteral("activity"), marks(node)},
        };
    };
    for (auto it = p.children.cbegin(); it != p.children.cend(); ++it)
        addReferral(it.key(), it.value(), false);
    if (notes.ok())
        for (const referral::Note& note : notes.value)
            if (note.kind == referral::Note::Child && !p.children.contains(note.node))
                addReferral(note.node, 0, true);
    setReferrals(referrals);

    const auto claimable = m_referral->claimable(participantId());
    if (claimable.ok())
        setClaimablePoints(points::normalized(claimable.value));
    setRewardBalance(points::normalized(p.rewardBalance));
    setTotalPoints(points::add(rewardBalance(), claimablePoints()));

    // The receipts the module already knows: listing them never reads the key.
    const auto cashOuts = m_referral->receipts(participantId(), {});
    if (cashOuts.ok()) {
        QString cashedOut = QStringLiteral("0");
        QVariantList receipts;
        for (const referral::Receipt& r : cashOuts.value) {
            cashedOut = points::add(cashedOut, r.points);
            receipts << QVariantMap{
                {QStringLiteral("index"), r.index},
                {QStringLiteral("account"), r.account},
                {QStringLiteral("points"), points::normalized(r.points)},
            };
        }
        setReceipts(receipts);
        setCashedOutPoints(cashedOut);
    }
}

// Referral labels are Uplink's own, one INI section per points account.
void UplinkBackend::loadLabels()
{
    if (m_labelsOwner == participantId())
        return;
    QSettings ini(dataFile(), QSettings::IniFormat);
    ini.beginGroup(participantId() + QStringLiteral("/labels"));
    m_labels.clear();
    for (const QString& node : ini.childKeys())
        m_labels.insert(node, ini.value(node).toString());
    ini.endGroup();
    m_labelsOwner = participantId();
}

// Claim needs no Blend key (the wallet signs it), so it can run in the background.
void UplinkBackend::autoClaimIfDue()
{
    if (!autoClaim() || enrolState() != Enrolled || cashOutState() != CashOutIdle
            || hasPendingOperation() || points::isZero(claimablePoints()))
        return;
    const auto reference = m_referral->autoClaim(participantId());
    if (!reference.ok()) {
        fail(reference.error);
        return;
    }
    if (reference.value.isEmpty())
        return;
    m_operations.insert(reference.value, {QStringLiteral("claim"), OperationPending});
    publishOperations();
}

// Uplink's own data — referral labels and settings — one INI file. None of it comes from the program.
QString UplinkBackend::dataFile() const
{
    const QString dir = !m_dataDir.isEmpty()
        ? m_dataDir
        : QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Logos/Uplink");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/uplink.ini");
}

bool UplinkBackend::hasPendingOperation() const
{
    return std::any_of(m_operations.cbegin(), m_operations.cend(),
                       [](const Operation& op) { return op.status == OperationPending; });
}

void UplinkBackend::reconcileOperations()
{
    bool changed = false;
    QMap<QString, int> settled;   // acted on after the loop: the next step adds an operation
    for (auto it = m_operations.begin(); it != m_operations.end(); ++it) {
        if (it->status != OperationPending)
            continue;
        const auto status = m_referral->status(it.key());
        if (!status.ok()) {
            it->status = OperationRejected;
            changed = true;
            settled.insert(it.key(), it->status);
            fail(status.error);
        } else if (toRep(status.value) != OperationPending) {
            it->status = toRep(status.value);
            changed = true;
            settled.insert(it.key(), it->status);
        } else {
            continue;
        }
        if (it.key() == m_registerReference && it->status == OperationRejected)
            setEnrolState(EnrolRejected);
    }
    if (changed)
        publishOperations();

    if (settled.contains(m_collectReference)) {
        const bool ok = settled.value(m_collectReference) == OperationSettled;
        m_collectReference.clear();
        if (ok)
            startCollecting();   // the next batch of epochs, or the cash-out once none is left
        else
            failCashOut(QStringLiteral("Collecting your points was rejected."));
    }
    if (settled.contains(m_cashOutReference)) {
        const bool ok = settled.value(m_cashOutReference) == OperationSettled;
        m_cashOutReference.clear();
        if (ok)
            prepareNewReceiptCode();
        else
            failCashOut(QStringLiteral("Cashing out was rejected."));
    }
}

void UplinkBackend::createIdentity()
{
    setLastError({});
    if (!participantId().isEmpty())
        return;
    if (walletIssue() != WalletReady) {
        fail(QStringLiteral("Set up your wallet in the LEZ Wallet app first."));
        return;
    }
    // The module labels the account, so it's found again next session.
    const auto created = m_referral->createParticipant(kIdentityLabel);
    if (!created.ok()) {
        fail(created.error);
        return;
    }
    setParticipantId(created.value);
    setEnrolState(IdentityCreated);
    showIdentity();
}

void UplinkBackend::checkInvitation(QString code)
{
    if (code.trimmed().isEmpty()) {
        setInvitationInviter({});
        setInvitationCheck(InvitationEmpty);
        return;
    }
    const auto check = m_referral->checkInvitation(code.trimmed());
    if (!check.ok()) {
        setInvitationInviter({});
        setInvitationCheck(InvitationInvalid);
        return;
    }
    setInvitationInviter(check.value.referrer);
    const QString me = !nodeId().isEmpty() ? nodeId() : keystoreNode();
    if (!me.isEmpty() && check.value.referrer.compare(me, Qt::CaseInsensitive) == 0) {
        setInvitationCheck(InvitationOwn);
        return;
    }
    setInvitationCheck(check.value.registered ? InvitationOk : InviterNotJoined);
}

void UplinkBackend::joinUnder(QString code)
{
    setLastError({});
    if (participantId().isEmpty()) {
        fail(QStringLiteral("Create an identity first."));
        return;
    }
    const auto parent = m_referral->importInvitation(participantId(), code.trimmed());
    if (!parent.ok()) {
        fail(parent.error);
        return;
    }
    setReferrerNode(parent.value);
    setEnrolState(Invited);
}

// The Blend signing key signs inside uplink_module; Uplink hands it the keystore and the node
// that's running, so a key file for another node is refused rather than enrolled.
void UplinkBackend::enrol()
{
    setLastError({});
    if (participantId().isEmpty()) {
        fail(QStringLiteral("Create an identity first."));
        return;
    }
    if (enrolState() == Enrolling || enrolState() == Enrolled)
        return;
    refreshNode();
    checkKeystore();
    if (nodeIssue() == ModuleUnavailable || nodeIssue() == NodeNotRunning) {
        fail(QStringLiteral("Your node needs to be running to join."));
        return;
    }
    switch (keystoreIssue()) {
    case KeystoreNotChosen:
        fail(QStringLiteral("Choose your node's keystore.yaml first: joining is signed with its Blend key."));
        return;
    case KeystoreUnreadable:
        fail(QStringLiteral("Uplink couldn't read the keystore: %1").arg(keystoreIssueDetail()));
        return;
    case KeystoreWrongNode:
        fail(QStringLiteral("This keystore holds a different node's Blend key than the one running. Choose the keystore.yaml of the node shown in the Blockchain app."));
        return;
    }
    if (!termsAccepted()) {
        fail(QStringLiteral("Accept the program terms first."));
        return;
    }

    const QString reference = newReference();
    const auto submitted = m_referral->enrol(reference, participantId(), keystorePath(), nodeId());
    if (!track(QStringLiteral("register"), submitted, reference))
        return;
    m_registerReference = reference;
    setEnrolState(Enrolling);
}

// One action for the user: collect what's claimable, cash the whole balance out, then
// make the payout code. Each step waits for the previous transaction to settle
// (reconcileOperations moves it on).
void UplinkBackend::cashOutAll()
{
    setLastError({});
    if (enrolState() != Enrolled || cashOutState() != CashOutIdle)
        return;
    if (keystoreIssue() != KeystoreReady) {
        fail(QStringLiteral("Choose your node's keystore.yaml first: cashing out uses its Blend key."));
        return;
    }
    if (points::isZero(claimablePoints()) && points::isZero(rewardBalance())) {
        fail(QStringLiteral("No points to cash out yet."));
        return;
    }
    // An auto-claim still settling becomes this cash-out's collect step: it moves on when it lands.
    for (auto it = m_operations.cbegin(); it != m_operations.cend(); ++it) {
        if (it->status != OperationPending)
            continue;
        if (it->kind != QLatin1String("claim")) {
            fail(QStringLiteral("Another operation is still settling. Try again in a moment."));
            return;
        }
        m_collectReference = it.key();
        setCashOutState(Collecting);
        return;
    }
    setCashOutState(Collecting);
    startCollecting();
}

// A claim covers at most 16 epochs, so a long catch-up takes a few.
void UplinkBackend::startCollecting()
{
    const auto claimable = m_referral->claimable(participantId());
    if (!claimable.ok()) {
        failCashOut(claimable.error);
        return;
    }
    if (points::isZero(claimable.value)) {
        startCashOut();
        return;
    }
    const QString reference = newReference();
    const auto submitted = m_referral->claim(reference, participantId());
    if (!track(QStringLiteral("claim"), submitted, reference)) {
        failCashOut(lastError());
        return;
    }
    m_collectReference = reference;
    setCashOutState(Collecting);
}

void UplinkBackend::startCashOut()
{
    // So the receipt this cash-out makes can be told from the earlier ones.
    const auto before = m_referral->receipts(participantId(), {});
    if (!before.ok()) {
        failCashOut(before.error);
        return;
    }
    m_receiptsBefore.clear();
    for (const referral::Receipt& r : before.value)
        m_receiptsBefore.insert(r.index);

    const QString reference = newReference();
    const auto submitted = m_referral->cashOut(reference, participantId(), keystorePath());
    if (!track(QStringLiteral("cash_out"), submitted, reference)) {
        failCashOut(lastError());
        return;
    }
    m_cashOutReference = reference;
    setCashOutState(CashingOut);
}

// The receipt a cash-out made is the one that wasn't there before it. Anything but
// exactly one, and the user picks it from their cash-outs rather than Uplink guessing.
void UplinkBackend::prepareNewReceiptCode()
{
    const auto receipts = m_referral->receipts(participantId(), {});
    QList<quint64> fresh;
    if (receipts.ok())
        for (const referral::Receipt& r : receipts.value)
            if (!m_receiptsBefore.contains(r.index))
                fresh << r.index;
    m_receiptsBefore.clear();
    if (fresh.size() != 1) {
        failCashOut(QStringLiteral("Your points are cashed out, but Uplink couldn't tell which receipt is this one. Get its code from your cash-outs."));
        return;
    }
    preparePayoutCode(fresh.first());
}

void UplinkBackend::payoutCodeFor(int receiptIndex)
{
    if (enrolState() != Enrolled || receiptIndex < 0)
        return;
    if (cashOutState() != CashOutIdle && cashOutState() != CashOutDone)
        return;
    setLastError({});
    setPayoutCode({});
    if (keystoreIssue() != KeystoreReady) {
        fail(QStringLiteral("Choose your node's keystore.yaml first: the payout code is signed with its Blend key."));
        return;
    }
    preparePayoutCode(quint64(receiptIndex));
}

// The Blend signing key signs the code inside uplink_module, so only the node that earned the
// points can redeem it.
void UplinkBackend::preparePayoutCode(quint64 receiptIndex)
{
    setCashOutState(PreparingCode);
    const auto receipts = m_referral->receipts(participantId(), {});
    if (!receipts.ok()) {
        failCashOut(receipts.error);
        return;
    }
    const auto receipt = std::find_if(receipts.value.cbegin(), receipts.value.cend(),
                                      [&](const referral::Receipt& r) { return r.index == receiptIndex; });
    if (receipt == receipts.value.cend()) {
        failCashOut(QStringLiteral("That cash-out receipt wasn't found."));
        return;
    }
    const auto code = m_referral->payoutCode(participantId(), receiptIndex, keystorePath());
    if (!code.ok()) {
        failCashOut(QStringLiteral("Your points are cashed out, but the payout code couldn't be made (%1). Get it again from your cash-outs.").arg(code.error));
        return;
    }
    setPayoutPoints(points::normalized(receipt->points));
    setPayoutCode(code.value);
    setCashOutState(CashOutDone);
}

void UplinkBackend::failCashOut(const QString& error)
{
    m_collectReference.clear();
    m_cashOutReference.clear();
    m_receiptsBefore.clear();
    setCashOutState(CashOutIdle);
    fail(error);
}

void UplinkBackend::finishCashOut()
{
    setPayoutCode({});
    setPayoutPoints({});
    setCashOutState(CashOutIdle);
}

// Local only; an empty label removes it, so the node ID shows again.
void UplinkBackend::setReferralLabel(QString node, QString label)
{
    label = label.trimmed();
    QSettings ini(dataFile(), QSettings::IniFormat);
    ini.beginGroup(participantId() + QStringLiteral("/labels"));
    if (label.isEmpty()) {
        m_labels.remove(node);
        ini.remove(node);
    } else {
        m_labels.insert(node, label);
        ini.setValue(node, label);
    }
    ini.endGroup();
    refreshReferral();
}

// Records a submitted operation; false (with lastError set) if it wasn't submitted.
bool UplinkBackend::track(const QString& kind, const Result<referral::Status>& submitted, const QString& reference)
{
    if (!submitted.ok()) {
        fail(submitted.error);
        return false;
    }
    m_operations.insert(reference, {kind, toRep(submitted.value)});
    publishOperations();
    return true;
}

void UplinkBackend::publishOperations()
{
    QVariantList list;
    for (auto it = m_operations.cbegin(); it != m_operations.cend(); ++it)
        list << QVariantMap{
            {QStringLiteral("ref"), it.key()},
            {QStringLiteral("kind"), it->kind},
            {QStringLiteral("status"), it->status},
        };
    setOperations(list);
}

void UplinkBackend::fail(const QString& error)
{
    setLastError(error);
}
