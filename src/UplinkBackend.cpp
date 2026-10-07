#include "UplinkBackend.h"

#include <QRandomGenerator>
#include <QVariantMap>

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

    setNodeIssue(ModuleUnavailable);
    setWalletIssue(LezCoreUnavailable);
    setEnrolState(NoIdentity);
    setClaimablePoints(QStringLiteral("0"));
    setRewardBalance(QStringLiteral("0"));
    setLifetimePoints(QStringLiteral("0"));

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
    if (walletIssue() != WalletReady || !participantId().isEmpty())
        return;

    // An identity from an earlier session: the labelled account in this wallet.
    const auto existing = m_referral->resolveLabel(kIdentityLabel);
    if (!existing.ok() || existing.value.isEmpty())
        return;
    setParticipantId(existing.value);
    setEnrolState(IdentityCreated);
    showIdentity();
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
        setInvitation(invitation.value);

    QVariantList referrals;
    auto addReferral = [&](const QString& node, quint32 lastPaid, bool pending) {
        referrals << QVariantMap{
            {QStringLiteral("node"), node},
            {QStringLiteral("label"), m_labels.value(node)},
            {QStringLiteral("activeThisEpoch"), registry.value.active.contains(node)},
            {QStringLiteral("lastPaidEpoch"), lastPaid},
            {QStringLiteral("pending"), pending},   // announced, not yet taken in by a claim
        };
    };
    for (auto it = p.children.cbegin(); it != p.children.cend(); ++it)
        addReferral(it.key(), it.value(), false);
    const auto notes = m_referral->notes(participantId());
    if (notes.ok())
        for (const referral::Note& note : notes.value)
            if (note.kind == referral::Note::Child && !p.children.contains(note.node))
                addReferral(note.node, 0, true);
    setReferrals(referrals);

    const auto claimable = m_referral->claimable(participantId());
    if (claimable.ok())
        setClaimablePoints(claimable.value);
    setRewardBalance(p.rewardBalance);

    // Points are u128 on chain but stay far below 2^64, so sums use quint64.
    quint64 lifetime = p.rewardBalance.toULongLong();
    QVariantList receipts;
    const auto cashOuts = m_referral->receipts(participantId());
    if (cashOuts.ok()) {
        for (const referral::Receipt& r : cashOuts.value) {
            lifetime += r.points.toULongLong();
            receipts << QVariantMap{
                {QStringLiteral("index"), r.index},
                {QStringLiteral("account"), r.account},
                {QStringLiteral("points"), r.points},
            };
        }
    }
    setReceipts(receipts);
    setLifetimePoints(QString::number(lifetime));
}

void UplinkBackend::reconcileOperations()
{
    bool changed = false;
    for (auto it = m_operations.begin(); it != m_operations.end(); ++it) {
        if (it->status != OperationPending)
            continue;
        const auto status = m_referral->reconcile(it.key());
        if (!status.ok() || toRep(status.value) == OperationPending)
            continue;
        it->status = toRep(status.value);
        changed = true;
        if (it.key() == m_registerReference && it->status == OperationRejected)
            setEnrolState(EnrolRejected);
    }
    if (changed)
        publishOperations();
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
    if (!labelled.ok())
        fail(labelled.error);
    showIdentity();
}

void UplinkBackend::joinUnder(QString invitationBlob)
{
    setLastError({});
    if (participantId().isEmpty()) {
        fail(QStringLiteral("Create an identity first."));
        return;
    }
    const auto parent = m_referral->importInvitation(participantId(), invitationBlob.trimmed());
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

void UplinkBackend::completeEnroll(QString signatureHex, QString publicKeyHex)
{
    setLastError({});
    if (enrolState() != AwaitingSignature)
        return;
    if (!nodeId().isEmpty() && publicKeyHex.compare(nodeId(), Qt::CaseInsensitive) != 0) {
        fail(QStringLiteral("The signature is from a different node key."));
        return;
    }
    submitRegistration(QByteArray::fromHex(signatureHex.toLatin1()));
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
void UplinkBackend::reportSignFailed(QString error)
{
    fail(error == QLatin1String("cancelled") ? QStringLiteral("Signing was cancelled.")
                                             : QStringLiteral("The node app could not sign (%1).").arg(error));
}

void UplinkBackend::claimPoints()
{
    setLastError({});
    if (enrolState() != Enrolled || claimablePoints().toULongLong() == 0) {
        fail(QStringLiteral("Nothing to collect."));
        return;
    }
    if (!nodeActive()) {
        fail(QStringLiteral("Your node isn't an active Blend core node this epoch, so you can't collect yet."));
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
    submit(QStringLiteral("claim"), m_referral->submitClaim(reference, participantId(), accounts), reference);
}

void UplinkBackend::cashOut()
{
    setLastError({});
    if (enrolState() != Enrolled || rewardBalance().toULongLong() == 0) {
        fail(QStringLiteral("Nothing to cash out."));
        return;
    }
    const QString reference = newReference();
    submit(QStringLiteral("cash_out"), m_referral->cashOut(reference, participantId()), reference);
}

void UplinkBackend::setReferralLabel(QString node, QString label)
{
    m_labels.insert(node, label.trimmed());
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
