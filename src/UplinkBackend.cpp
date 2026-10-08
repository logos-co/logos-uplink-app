#include "UplinkBackend.h"

#include <algorithm>

#include <QRandomGenerator>
#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QVariantMap>

#include "InvitationCode.h"
#include "PayoutCode.h"
#include "Points.h"
#include "blockchain/BlockchainNodeService.h"
#include "lez/LezReferralService.h"
#include "mock/MockNodeService.h"
#include "mock/MockReferralService.h"

using Rep = UplinkUiSimpleSource;

namespace {

constexpr int kPollMs = 5000;
constexpr qint64 kSyncChunk = 100;   // blocks per step, as the LEZ Wallet app does

// The `domain` the node signs the registration under. Placeholder until LEZ#896
// and logos-blockchain-module#108 agree on one.
const QString kRegisterDomain = QStringLiteral("lez-referral/register");
// The domain for signing a payout code, so a join signature can never pass as one.
const QString kPayoutDomain = QStringLiteral("lez-referral/payout");

// TODO: replace with the real payout form's URL once it exists. This is the prototype's form.
const QString kPayoutFormUrl = QStringLiteral("https://xalisher.github.io/lx-preview/web/");

// The node key is Ed25519.
constexpr qsizetype kSignatureBytes = 64;

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

} // namespace

UplinkBackend::UplinkBackend(QObject* parent)
    : UplinkUiSimpleSource(parent)
{
    // real (default) | mock | mock:no_wallet
    const QString lez = backendFor("UPLINK_LEZ");
    if (lez.section(':', 0, 0) == QLatin1String("mock"))
        m_referral = std::make_unique<MockReferralService>(lez.section(':', 1) != QLatin1String("no_wallet"));
    else
        m_referral = std::make_unique<LezReferralService>(*this);

    // real (default) | mock | mock:<issue>
    const QString node = backendFor("UPLINK_NODE");
    if (node.section(':', 0, 0) == QLatin1String("mock"))
        m_node = std::make_unique<MockNodeService>(MockNodeService::issueFromName(node.section(':', 1)));
    else
        m_node = std::make_unique<BlockchainNodeService>(*this);

    init();
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
    setNodeIssue(ModuleUnavailable);
    setWalletIssue(LezCoreUnavailable);
    setEnrolState(NoIdentity);
    setCashOutState(CashOutIdle);
    setPayoutFormUrl(kPayoutFormUrl);
    setInvitationCheck(InvitationEmpty);
    setClaimablePoints(QStringLiteral("0"));
    setRewardBalance(QStringLiteral("0"));
    setTotalPoints(QStringLiteral("0"));
    setCashedOutPoints(QStringLiteral("0"));

    m_poll.setInterval(kPollMs);
    connect(&m_poll, &QTimer::timeout, this, &UplinkBackend::refresh);
}

UplinkBackend::~UplinkBackend() = default;

void UplinkBackend::onContextReady()
{
    refresh();
    m_poll.start();
}

void UplinkBackend::refresh()
{
    refreshNode();
    refreshWallet();
    if (walletIssue() != WalletReady)
        return;
    syncWallet();
    reconcileOperations();
    refreshReferral();
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
    if (labelled.value.isEmpty() && m_identityUnlabelled)
        return;
    forgetIdentity();
    if (labelled.value.isEmpty())
        return;
    setParticipantId(labelled.value);
    setEnrolState(IdentityCreated);
    showIdentity();
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
    m_payoutOpening = {};
    m_labels.clear();
    m_activity = {};
    m_activityOwner.clear();
    m_identityUnlabelled = false;
    setParticipantId({});
    setIdentityLabel({});
    setIdentityAddress({});
    setEnrolState(NoIdentity);
    setReferrerNode({});
    setInvitation({});
    setSignRequest({});
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

    const auto invitation = m_referral->invitation(participantId(), p.node);
    if (invitation.ok())
        setInvitation(invitation_code::encode(invitation.value));

    QStringList referralNodes = p.children.keys();
    const auto notes = m_referral->notes(participantId());
    if (notes.ok())
        for (const referral::Note& note : notes.value)
            if (note.kind == referral::Note::Child && !referralNodes.contains(note.node))
                referralNodes << note.node;
    recordActivity(registry.value, p.node, referralNodes);
    setMyActivity(m_activity.window(p.node));

    QVariantList referrals;
    auto addReferral = [&](const QString& node, quint32 lastPaid, bool pending) {
        referrals << QVariantMap{
            {QStringLiteral("node"), node},
            {QStringLiteral("label"), m_labels.value(node)},
            {QStringLiteral("activeThisEpoch"), registry.value.active.contains(node)},
            {QStringLiteral("lastPaidEpoch"), lastPaid},
            {QStringLiteral("pending"), pending},   // announced, not yet taken in by a claim
            {QStringLiteral("activity"), m_activity.window(node)},
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

    const auto cashOuts = m_referral->receipts(participantId());
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

// The program only holds the latest published epoch, so each one is noted as it appears,
// for this node and every direct referral, in an INI file (one section per points account).
void UplinkBackend::recordActivity(const referral::Registry& registry, const QString& myNode,
                                   const QStringList& referralNodes)
{
    QSettings ini(dataFile(), QSettings::IniFormat);
    if (m_activityOwner != participantId()) {
        ini.beginGroup(participantId() + QStringLiteral("/activity"));
        m_activity = ActivityLog::readFrom(ini);
        ini.endGroup();
        ini.beginGroup(participantId() + QStringLiteral("/labels"));
        m_labels.clear();
        for (const QString& node : ini.childKeys())
            m_labels.insert(node, ini.value(node).toString());
        ini.endGroup();
        m_activityOwner = participantId();
    }
    if (registry.epoch != 0   // nothing published yet
            && m_activity.record(registry.epoch, QStringList{myNode} + referralNodes, registry.active)) {
        ini.beginGroup(participantId() + QStringLiteral("/activity"));
        m_activity.writeTo(ini);
        ini.endGroup();
    }
}

// Uplink's own data — activity seen and referral labels — one INI file, a section per
// points account. None of it comes from the program.
QString UplinkBackend::dataFile() const
{
    const QString dir = !m_dataDir.isEmpty()
        ? m_dataDir
        : QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Logos/Uplink");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/uplink.ini");
}

void UplinkBackend::reconcileOperations()
{
    bool changed = false;
    QMap<QString, int> settled;   // acted on after the loop: the next step adds an operation
    for (auto it = m_operations.begin(); it != m_operations.end(); ++it) {
        if (it->status != OperationPending)
            continue;
        const auto status = m_referral->reconcile(it.key());
        if (!status.ok() || toRep(status.value) == OperationPending)
            continue;
        it->status = toRep(status.value);
        changed = true;
        settled.insert(it.key(), it->status);
        if (it.key() == m_registerReference && it->status == OperationRejected)
            setEnrolState(EnrolRejected);
    }
    if (changed)
        publishOperations();

    if (settled.contains(m_collectReference)) {
        const bool ok = settled.value(m_collectReference) == OperationSettled;
        m_collectReference.clear();
        if (ok)
            startCashOut();
        else
            failCashOut(QStringLiteral("Collecting your points was rejected. Your node may not have been active this epoch."));
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
    const auto created = m_referral->createParticipant();
    if (!created.ok()) {
        fail(created.error);
        return;
    }
    setParticipantId(created.value);
    setEnrolState(IdentityCreated);
    // Without the label the identity can't be found again next session.
    const auto labelled = m_referral->addLabel(kIdentityLabel, created.value);
    m_identityUnlabelled = !labelled.ok();
    if (!labelled.ok())
        fail(labelled.error);
    showIdentity();
}

void UplinkBackend::checkInvitation(QString code)
{
    if (code.trimmed().isEmpty()) {
        setInvitationInviter({});
        setInvitationCheck(InvitationEmpty);
        return;
    }
    const QString inviter = invitation_code::inviterNode(invitation_code::decode(code));
    setInvitationInviter(inviter);
    if (inviter.isEmpty()) {
        setInvitationCheck(InvitationInvalid);
        return;
    }
    if (!nodeId().isEmpty() && inviter.compare(nodeId(), Qt::CaseInsensitive) == 0) {
        setInvitationCheck(InvitationOwn);
        return;
    }
    // Only a registered node can be a referrer. If the registry can't be read, don't block on it.
    const auto registry = m_referral->registry();
    if (registry.ok() && !registry.value.nodes.contains(inviter, Qt::CaseInsensitive)) {
        setInvitationCheck(InviterNotJoined);
        return;
    }
    setInvitationCheck(InvitationOk);
}

void UplinkBackend::joinUnder(QString code)
{
    setLastError({});
    if (participantId().isEmpty()) {
        fail(QStringLiteral("Create an identity first."));
        return;
    }
    const QString blob = invitation_code::decode(code);
    if (blob.isEmpty()) {
        fail(QStringLiteral("That doesn't look like an invitation code."));
        return;
    }
    const auto parent = m_referral->importInvitation(participantId(), blob);
    if (!parent.ok()) {
        fail(parent.error);
        return;
    }
    setReferrerNode(parent.value);
    setEnrolState(Invited);
}

void UplinkBackend::prepareEnroll()
{
    setLastError({});
    if (participantId().isEmpty()) {
        fail(QStringLiteral("Create an identity first."));
        return;
    }
    if (enrolState() == Enrolling || enrolState() == Enrolled)
        return;
    // Joining has no prerequisites beyond the node key signing the registration.
    refreshNode();
    if (nodeIssue() == ModuleUnavailable || nodeIssue() == NodeNotRunning || nodeId().isEmpty()) {
        fail(QStringLiteral("Your node needs to be running to sign your enrolment."));
        return;
    }

    const auto prepared = m_referral->prepareRegistration(participantId(), nodeId(), referrerNode());
    if (!prepared.ok()) {
        fail(prepared.error);
        return;
    }
    // A mock node signs on the spot; a real one goes through the node app.
    const auto signedNow = m_node->signWithoutPrompt(kRegisterDomain, prepared.value);
    if (signedNow.ok()) {
        submitRegistration(signedNow.value);
        return;
    }
    setSignRequest({
        {QStringLiteral("domain"), kRegisterDomain},
        {QStringLiteral("payload_hex"), QString::fromLatin1(prepared.value.toHex())},
    });
    setEnrolState(AwaitingSignature);
}

void UplinkBackend::completeEnroll(QString payloadHex, QString signatureHex, QString publicKeyHex)
{
    if (enrolState() != AwaitingSignature || !isCurrentSignRequest(payloadHex))
        return;
    setLastError({});
    const QString problem = checkSignature(signatureHex, publicKeyHex);
    if (!problem.isEmpty()) {
        fail(problem);
        return;
    }
    submitRegistration(QByteArray::fromHex(signatureHex.toLatin1()));
}

// The node app answers asynchronously, so an answer can arrive after its request was replaced.
bool UplinkBackend::isCurrentSignRequest(const QString& payloadHex) const
{
    const QString current = signRequest().value(QStringLiteral("payload_hex")).toString();
    return !current.isEmpty() && payloadHex.compare(current, Qt::CaseInsensitive) == 0;
}

// "" if the node app's answer is an Ed25519 signature from this node's key.
QString UplinkBackend::checkSignature(const QString& signatureHex, const QString& publicKeyHex) const
{
    static const QRegularExpression hex(QStringLiteral("^[0-9a-fA-F]*$"));
    if (signatureHex.size() != 2 * kSignatureBytes || !hex.match(signatureHex).hasMatch())
        return QStringLiteral("The node app returned a malformed signature.");
    if (!nodeId().isEmpty() && publicKeyHex.compare(nodeId(), Qt::CaseInsensitive) != 0)
        return QStringLiteral("The signature is from a different node key.");
    return {};
}

void UplinkBackend::submitRegistration(const QByteArray& signature)
{
    const auto attached = m_referral->attachNodeSignature(participantId(), signature);
    if (!attached.ok()) {
        fail(attached.error);
        return;
    }
    const QString reference = newReference();
    const auto submitted = m_referral->submitRegister(reference, participantId());
    if (!submitted.ok()) {
        fail(submitted.error);
        return;
    }
    m_registerReference = reference;
    submit(QStringLiteral("register"), submitted, reference);
    setSignRequest({});
    setEnrolState(Enrolling);
}

// Stays AwaitingSignature so the user can retry.
void UplinkBackend::reportSignFailed(QString payloadHex, QString error)
{
    if (enrolState() != AwaitingSignature || !isCurrentSignRequest(payloadHex))
        return;
    fail(error == QLatin1String("cancelled") ? QStringLiteral("Signing was cancelled.")
                                             : QStringLiteral("The node app could not sign (%1).").arg(error));
}

// One action for the user: collect what's claimable, cash the whole balance out,
// then turn the receipt's opening into the payout code. Each step waits for the
// previous transaction to settle (reconcileOperations moves it on).
void UplinkBackend::cashOutAll()
{
    setLastError({});
    if (enrolState() != Enrolled || cashOutState() != CashOutIdle)
        return;
    if (!nodeActive()) {
        fail(QStringLiteral("Your node isn't an active Blend core node this epoch, so you can't collect yet."));
        return;
    }
    const bool claimable = !points::isZero(claimablePoints());
    if (!claimable && points::isZero(rewardBalance())) {
        fail(QStringLiteral("No points to cash out yet."));
        return;
    }
    if (!claimable) {
        startCashOut();
        return;
    }
    const auto notes = m_referral->notes(participantId());
    if (!notes.ok()) {
        fail(notes.error);
        return;
    }
    QStringList accounts;
    for (const referral::Note& note : notes.value)
        accounts << note.account;
    const QString reference = newReference();
    const auto submitted = m_referral->submitClaim(reference, participantId(), accounts);
    if (!submitted.ok()) {
        failCashOut(submitted.error);
        return;
    }
    m_collectReference = reference;
    submit(QStringLiteral("claim"), submitted, reference);
    setCashOutState(Collecting);
}

void UplinkBackend::startCashOut()
{
    // So the receipt this cash-out makes can be told from the earlier ones.
    const auto before = m_referral->receipts(participantId());
    if (!before.ok()) {
        failCashOut(before.error);
        return;
    }
    m_receiptsBefore.clear();
    for (const referral::Receipt& r : before.value)
        m_receiptsBefore.insert(r.index);

    const QString reference = newReference();
    const auto submitted = m_referral->cashOut(reference, participantId());
    if (!submitted.ok()) {
        failCashOut(submitted.error);
        return;
    }
    m_cashOutReference = reference;
    submit(QStringLiteral("cash_out"), submitted, reference);
    setCashOutState(CashingOut);
}

// The receipt a cash-out made is the one that wasn't there before it. Anything but
// exactly one, and the user picks it from their cash-outs rather than Uplink guessing.
void UplinkBackend::prepareNewReceiptCode()
{
    const auto receipts = m_referral->receipts(participantId());
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

// Also leaves a code being signed for another receipt.
void UplinkBackend::payoutCodeFor(int receiptIndex)
{
    if (enrolState() != Enrolled || receiptIndex < 0)
        return;
    if (cashOutState() != CashOutIdle && cashOutState() != SigningCode && cashOutState() != CashOutDone)
        return;
    setLastError({});
    setPayoutCode({});
    preparePayoutCode(quint64(receiptIndex));
}

void UplinkBackend::preparePayoutCode(quint64 receiptIndex)
{
    setCashOutState(PreparingCode);
    const auto receipts = m_referral->receipts(participantId());
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
    const auto opening = m_referral->opening(participantId(), receiptIndex);
    if (!opening.ok()) {
        failCashOut(opening.error);
        return;
    }
    setPayoutPoints(points::normalized(receipt->points));
    m_payoutOpening = opening.value;

    // A mock node signs on the spot; a real one goes through the node app.
    const QByteArray payload = payout_code::signedPayload(m_payoutOpening);
    const auto signedNow = m_node->signWithoutPrompt(kPayoutDomain, payload);
    if (signedNow.ok()) {
        setPayoutCode(payout_code::encode(m_payoutOpening, QString::fromLatin1(signedNow.value.toHex())));
        setCashOutState(CashOutDone);
        return;
    }
    setSignRequest({
        {QStringLiteral("domain"), kPayoutDomain},
        {QStringLiteral("payload_hex"), QString::fromLatin1(payload.toHex())},
    });
    setCashOutState(SigningCode);
}

void UplinkBackend::completePayoutSignature(QString payloadHex, QString signatureHex, QString publicKeyHex)
{
    if (cashOutState() != SigningCode || !isCurrentSignRequest(payloadHex))
        return;
    setLastError({});
    const QString problem = checkSignature(signatureHex, publicKeyHex);
    if (!problem.isEmpty()) {
        fail(problem);
        return;
    }
    setPayoutCode(payout_code::encode(m_payoutOpening, signatureHex));
    setSignRequest({});
    setCashOutState(CashOutDone);
}

// The points are already cashed out; only the signature is missing, so asking again retries.
void UplinkBackend::reportPayoutSignFailed(QString payloadHex, QString error)
{
    if (cashOutState() != SigningCode || !isCurrentSignRequest(payloadHex))
        return;
    fail(error == QLatin1String("cancelled") ? QStringLiteral("Signing was cancelled. Your points are cashed out; sign again to get your code.")
                                             : QStringLiteral("The node app could not sign (%1). Your points are cashed out; sign again to get your code.").arg(error));
}

void UplinkBackend::failCashOut(const QString& error)
{
    m_collectReference.clear();
    m_cashOutReference.clear();
    m_receiptsBefore.clear();
    setSignRequest({});
    setCashOutState(CashOutIdle);
    fail(error);
}

void UplinkBackend::finishCashOut()
{
    m_payoutOpening = {};
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

void UplinkBackend::submit(const QString& kind, const Result<referral::Status>& submitted, const QString& reference)
{
    if (!submitted.ok()) {
        fail(submitted.error);
        return;
    }
    m_operations.insert(reference, {kind, toRep(submitted.value)});
    publishOperations();
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
