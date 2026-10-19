#include "mock/MockReferralService.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "mock/MockNodeService.h"
#include "mock/MockTerms.h"

using namespace referral;

namespace {

// UPLINK_MOCK_EPOCH_MS shortens it, so UI tests can reach a claim without waiting minutes.
qint64 epochMs()
{
    bool ok = false;
    const qint64 ms = qEnvironmentVariable("UPLINK_MOCK_EPOCH_MS").toLongLong(&ok);
    return ok && ms > 0 ? ms : 30000;
}

QByteArray hash(const QByteArray& seed)
{
    return QCryptographicHash::hash(seed, QCryptographicHash::Sha256);
}

QString hexId(const QByteArray& seed)
{
    return QString::fromLatin1(hash(seed).toHex());
}

const QString kInvitePrefix = QStringLiteral("uplink-invite-v1:");
const QString kPayoutPrefix = QStringLiteral("uplink-payout-v2:");
constexpr int kViewingKeyBytes = 1184;


QString base64url(const QByteArray& bytes)
{
    return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

// The inviter's node in an invitation, or "" if it isn't one: uplink_module's
// "uplink-invite-v1:" + base64url(node ‖ npk ‖ vpk).
QString invitationNode(const QString& blob)
{
    const QString trimmed = blob.trimmed();
    if (!trimmed.startsWith(kInvitePrefix))
        return {};
    const auto bytes = QByteArray::fromBase64Encoding(trimmed.mid(kInvitePrefix.size()).toLatin1(),
                                                      QByteArray::Base64UrlEncoding
                                                          | QByteArray::AbortOnBase64DecodingErrors);
    if (!bytes || (*bytes).size() != 64 + kViewingKeyBytes)
        return {};
    return QString::fromLatin1((*bytes).left(32).toHex());
}

// Participant::claim, with the program's rules: adopts announced children, pays each child
// for every epoch since it was last settled in which it AND your own node were active, and
// adds every credit.
quint64 applyClaim(Participant& p, quint32 epoch, const QMap<quint32, QStringList>& history,
                   const QList<Note>& notes)
{
    quint64 total = 0;
    for (const Note& note : notes) {
        if (note.kind == Note::Child && !p.children.contains(note.node))
            p.children.insert(note.node, 0);
        else if (note.kind == Note::Credit)
            total += note.amount.toULongLong();
    }
    for (auto it = p.children.begin(); it != p.children.end(); ++it) {
        for (quint32 e = it.value() + 1; e <= epoch; ++e)
            if (history.value(e).contains(it.key()) && history.value(e).contains(p.node))
                ++total;
        it.value() = epoch;
    }
    p.rewardBalance = QString::number(p.rewardBalance.toULongLong() + total);
    return total;
}

template <typename T>
Result<T> notRegistered()
{
    return Result<T>::failure(QStringLiteral("this participant is not registered"));
}

} // namespace

MockReferralService::MockReferralService(bool walletOpen)
    : m_walletOpen(walletOpen)
    , m_epochMs(epochMs())
    , m_program(deployedProgram())
{
    m_clock.start();
    m_registry.epoch = 1;
    m_registry.nodes << inviterNode();
}

QString MockReferralService::inviterNode() { return hexId("mock-inviter"); }

QString MockReferralService::invitationFor(const QString& node)
{
    QByteArray raw = QByteArray::fromHex(node.toLatin1());
    raw += hash("npk:" + node.toUtf8());
    QByteArray vpk;
    while (vpk.size() < kViewingKeyBytes)
        vpk += hash("vpk:" + node.toUtf8() + QByteArray::number(vpk.size()));
    raw += vpk.left(kViewingKeyBytes);
    return kInvitePrefix + base64url(raw);
}

QString MockReferralService::keystorePath() { return QStringLiteral("/mock/node/keystore.yaml"); }
QString MockReferralService::otherNodeKeystorePath() { return QStringLiteral("/mock/other-node/keystore.yaml"); }

QString MockReferralService::nextId(const char* tag)
{
    return hexId(QByteArray(tag) + QByteArray::number(++m_counter));
}

QString MockReferralService::deployedProgram() { return hexId("mock-referral-program"); }
QString MockReferralService::outdatedProgram() { return hexId("mock-outdated-program"); }

Result<QString> MockReferralService::programAccount()
{
    return Result<QString>::success(m_program);
}

Result<bool> MockReferralService::configure(const QString& programAccount)
{
    m_program = programAccount.trimmed().toLower();
    return Result<bool>::success(true);
}

Result<ProgramCheck> MockReferralService::checkProgram()
{
    if (!m_walletOpen)
        return Result<ProgramCheck>::failure(QStringLiteral("lez_core: no wallet is open"));
    if (m_program.isEmpty())
        return Result<ProgramCheck>::success({ProgramCheck::NotConfigured, QStringLiteral("set the referral program account")});
    if (m_program == outdatedProgram())
        return Result<ProgramCheck>::success({ProgramCheck::Mismatch, QStringLiteral("the program at %1 is another version").arg(m_program)});
    if (m_program != deployedProgram())
        return Result<ProgramCheck>::success({ProgramCheck::NotDeployed, QStringLiteral("no program is deployed at %1").arg(m_program)});
    return Result<ProgramCheck>::success({ProgramCheck::Ready, {}});
}

Result<QString> MockReferralService::createParticipant(const QString& label)
{
    if (!m_walletOpen)
        return Result<QString>::failure(QStringLiteral("no wallet is open"));
    if (m_labels.contains(label))
        return Result<QString>::success(m_labels.value(label));
    const QString id = nextId("participant");
    m_accounts.insert(id, Account{});
    m_labels.insert(label, id);
    return Result<QString>::success(id);
}

Result<Participant> MockReferralService::participant(const QString& participant)
{
    const Account account = m_accounts.value(participant);
    if (!account.registered)
        return notRegistered<Participant>();
    return Result<Participant>::success(account.state);
}

Result<QString> MockReferralService::invitation(const QString& participant, const QString& node)
{
    if (!m_accounts.contains(participant))
        return Result<QString>::failure(QStringLiteral("unknown participant"));
    return Result<QString>::success(invitationFor(node));
}

Result<InvitationCheck> MockReferralService::checkInvitation(const QString& blob)
{
    const QString node = invitationNode(blob);
    if (node.isEmpty())
        return Result<InvitationCheck>::failure(QStringLiteral("not an Uplink invitation"));
    registry();
    return Result<InvitationCheck>::success({node, m_registry.nodes.contains(node)});
}

Result<QString> MockReferralService::importInvitation(const QString& participant, const QString& blob)
{
    if (!m_accounts.contains(participant))
        return Result<QString>::failure(QStringLiteral("unknown participant"));
    const QString parent = invitationNode(blob);
    if (parent.isEmpty())
        return Result<QString>::failure(QStringLiteral("not an Uplink invitation"));
    m_accounts[participant].invitedBy = parent;
    return Result<QString>::success(parent);
}

Result<Registry> MockReferralService::registry()
{
    for (; m_epochsPublished < m_clock.elapsed() / m_epochMs; ++m_epochsPublished)
        advanceEpoch();
    return Result<Registry>::success(m_registry);
}

Result<Activity> MockReferralService::activity(const QStringList& nodes, int count)
{
    registry();
    Activity activity;
    for (quint32 e = quint32(qMax<qint64>(1, qint64(m_registry.epoch) - count + 1)); e <= m_registry.epoch; ++e)
        if (m_activeHistory.contains(e))
            activity.epochs << e;
    for (const QString& node : nodes) {
        QList<int> marks;
        for (quint32 e : activity.epochs)
            marks << (m_activeHistory.value(e).contains(node) ? 1 : 0);
        activity.nodes.insert(node, marks);
    }
    return Result<Activity>::success(activity);
}

QList<Note> MockReferralService::notesFor(const QString& node) const
{
    QList<Note> out;
    for (const MockNote& n : m_notes)
        if (n.recipient == node)
            out << n.note;
    return out;
}

Result<QList<Note>> MockReferralService::notes(const QString& participant)
{
    const Account account = m_accounts.value(participant);
    if (!account.registered)
        return notRegistered<QList<Note>>();
    return Result<QList<Note>>::success(notesFor(account.state.node));
}

Result<QString> MockReferralService::claimable(const QString& participant)
{
    const Account account = m_accounts.value(participant);
    if (!account.registered)
        return notRegistered<QString>();
    registry();
    Participant copy = account.state;
    return Result<QString>::success(QString::number(applyClaim(copy, m_registry.epoch, m_activeHistory, notesFor(copy.node))));
}

Result<Terms> MockReferralService::terms()
{
    const QByteArray json(mock_terms::kJson);
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    Terms terms;
    terms.version = o.value(QStringLiteral("version")).toInt();
    terms.lastUpdated = o.value(QStringLiteral("lastUpdated")).toString();
    terms.sections = o.value(QStringLiteral("sections")).toArray().toVariantList();
    terms.sha256 = QString::fromLatin1(hash(json).toHex());
    return Result<Terms>::success(terms);
}

Result<bool> MockReferralService::termsAccepted() { return Result<bool>::success(m_termsAccepted); }

Result<bool> MockReferralService::acceptTerms(int version, const QString& sha256)
{
    const Terms current = terms().value;
    if (version != current.version || sha256.compare(current.sha256, Qt::CaseInsensitive) != 0)
        return Result<bool>::failure(QStringLiteral("these are not the current terms"));
    m_termsAccepted = true;
    return Result<bool>::success(true);
}

Result<QString> MockReferralService::keystoreNode(const QString& keystorePath)
{
    if (keystorePath.trimmed().isEmpty())
        return Result<QString>::failure(QStringLiteral("no keystore given"));
    return Result<QString>::success(keystorePath.contains(QLatin1String("other-node")) ? hexId("mock-other-node")
                                                                                      : MockNodeService::nodeId());
}

bool MockReferralService::hasOpen(const QString& participant) const
{
    for (const Operation& op : m_operations)
        if (op.participant == participant && op.status == Status::Pending)
            return true;
    return false;
}

Result<Status> MockReferralService::record(const QString& reference, Operation operation)
{
    if (m_operations.contains(reference))
        return Result<Status>::success(m_operations.value(reference).status);
    if (hasOpen(operation.participant))
        return Result<Status>::failure(QStringLiteral("another operation for this participant is still pending"));
    m_operations.insert(reference, operation);
    return Result<Status>::success(Status::Pending);
}

Result<Status> MockReferralService::enrol(const QString& reference, const QString& participant,
                                          const QString& keystorePath, const QString& node)
{
    if (!m_accounts.contains(participant))
        return Result<Status>::failure(QStringLiteral("unknown participant"));
    if (!m_termsAccepted)
        return Result<Status>::failure(QStringLiteral("accept the programme's terms first"));
    const auto keyNode = keystoreNode(keystorePath);
    if (!keyNode.ok())
        return Result<Status>::failure(keyNode.error);
    if (!node.isEmpty() && keyNode.value != node)
        return Result<Status>::failure(QStringLiteral("this keystore has no key for node %1").arg(node));
    const Account& account = m_accounts[participant];
    if (account.registered)
        return Result<Status>::failure(QStringLiteral("this participant is already registered"));
    registry();
    if (m_registry.nodes.contains(keyNode.value))
        return Result<Status>::failure(QStringLiteral("this node is already registered"));
    if (!account.invitedBy.isEmpty() && !m_registry.nodes.contains(account.invitedBy))
        return Result<Status>::failure(QStringLiteral("the referrer is not registered"));
    return record(reference, {Kind::Register, participant, keyNode.value});
}

Result<Status> MockReferralService::claim(const QString& reference, const QString& participant)
{
    if (!m_accounts.value(participant).registered)
        return notRegistered<Status>();
    if (!m_operations.contains(reference) && claimable(participant).value == QLatin1String("0"))
        return Result<Status>::failure(QStringLiteral("nothing to claim"));
    return record(reference, {Kind::Claim, participant, {}});
}

Result<QString> MockReferralService::autoClaim(const QString& participant)
{
    if (!m_accounts.value(participant).registered)
        return notRegistered<QString>();
    if (hasOpen(participant) || claimable(participant).value == QLatin1String("0"))
        return Result<QString>::success({});
    const QString reference = QString::fromLatin1(hash("auto-claim" + nextId("claim").toLatin1()).toHex());
    const auto recorded = claim(reference, participant);
    return recorded.ok() ? Result<QString>::success(reference) : Result<QString>::failure(recorded.error);
}

Result<Status> MockReferralService::cashOut(const QString& reference, const QString& participant,
                                            const QString& keystorePath)
{
    const Account account = m_accounts.value(participant);
    if (!account.registered)
        return notRegistered<Status>();
    const auto keyNode = keystoreNode(keystorePath);
    if (!keyNode.ok())
        return Result<Status>::failure(keyNode.error);
    if (keyNode.value != account.state.node)
        return Result<Status>::failure(QStringLiteral("this keystore has no key for node %1").arg(account.state.node));
    if (!m_operations.contains(reference) && account.state.rewardBalance == QLatin1String("0"))
        return Result<Status>::failure(QStringLiteral("the reward balance is zero"));
    return record(reference, {Kind::CashOut, participant, {}});
}

Result<QList<Receipt>> MockReferralService::receipts(const QString& participant, const QString&)
{
    return Result<QList<Receipt>>::success(m_accounts.value(participant).receipts);
}

Result<QString> MockReferralService::payoutCode(const QString& participant, quint64 index, const QString& keystorePath)
{
    const Account account = m_accounts.value(participant);
    if (index >= quint64(account.receipts.size()))
        return Result<QString>::failure(QStringLiteral("no receipt with this index"));
    if (keystoreNode(keystorePath).value != account.state.node)
        return Result<QString>::failure(QStringLiteral("this keystore has no key for node %1").arg(account.state.node));
    const QJsonObject opening{
        {QStringLiteral("blinding_factor"), hexId("blinding:" + participant.toUtf8() + QByteArray::number(index))},
        {QStringLiteral("node"), account.state.node},
        {QStringLiteral("program_account"), m_program},
        {QStringLiteral("signature"), QString::fromLatin1(QCryptographicHash::hash(participant.toUtf8(), QCryptographicHash::Sha512).toHex())},
    };
    return Result<QString>::success(kPayoutPrefix + base64url(QJsonDocument(opening).toJson(QJsonDocument::Compact)));
}

Result<Status> MockReferralService::status(const QString& reference)
{
    if (!m_operations.contains(reference))
        return Result<Status>::failure(QStringLiteral("no operation with this reference"));
    Operation& operation = m_operations[reference];
    if (operation.status == Status::Pending)
        operation.status = settle(operation);
    return Result<Status>::success(operation.status);
}

Status MockReferralService::settle(Operation& operation)
{
    Account& account = m_accounts[operation.participant];
    switch (operation.kind) {
    case Kind::Register: {
        if (m_registry.nodes.contains(operation.node))
            return Status::Rejected;
        m_registry.nodes << operation.node;
        account.registered = true;
        account.state.node = operation.node;
        account.state.referrer = account.invitedBy;
        for (int i = 0; i < 3; ++i) {
            const QString child = nextId("child");
            m_children << child;
            m_registry.nodes << child;
            Note note;
            note.account = nextId("note");
            note.kind = Note::Child;
            note.node = child;
            addNote(note, account.state.node);
        }
        return Status::Settled;
    }
    case Kind::Claim: {
        const QList<Note> consumed = notesFor(account.state.node);
        for (auto it = m_notes.begin(); it != m_notes.end();)
            it = it->recipient == account.state.node ? m_notes.erase(it) : std::next(it);
        // The program forwards each epoch's total to the referrer as a Credit; the mock's referrers are outside it.
        applyClaim(account.state, m_registry.epoch, m_activeHistory, consumed);
        return Status::Settled;
    }
    case Kind::CashOut: {
        Receipt receipt;
        receipt.index = account.receipts.size();
        receipt.account = hexId("receipt:" + operation.participant.toUtf8() + QByteArray::number(receipt.index));
        receipt.points = account.state.rewardBalance;
        account.receipts << receipt;
        account.state.rewardBalance = QStringLiteral("0");
        return Status::Settled;
    }
    }
    return Status::Rejected;
}

void MockReferralService::addNote(Note note, const QString& recipient)
{
    m_notes.insert(note.account, {note, recipient});
}

void MockReferralService::advanceEpoch()
{
    ++m_registry.epoch;
    m_registry.active.clear();
    for (int i = 0; i < m_children.size(); ++i)
        if ((m_registry.epoch + i) % 3 != 0)
            m_registry.active << m_children[i];
    if (m_registry.epoch % 4 != 0)
        for (const Account& account : m_accounts)
            if (account.registered)
                m_registry.active << account.state.node;
    m_activeHistory.insert(m_registry.epoch, m_registry.active);
    if (m_registry.epoch % 2 != 0)
        return;
    for (const Account& account : m_accounts) {
        if (!account.registered)
            continue;
        Note credit;
        credit.account = nextId("note");
        credit.kind = Note::Credit;
        credit.amount = QStringLiteral("1");
        addNote(credit, account.state.node);
    }
}

Result<bool> MockReferralService::walletOpen() { return Result<bool>::success(m_walletOpen); }

Result<QString> MockReferralService::resolveLabel(const QString& label)
{
    return Result<QString>::success(m_labels.value(label));
}

Result<QString> MockReferralService::accountAddress(const QString& account)
{
    return Result<QString>::success(QStringLiteral("Mock") + account.left(40));
}

Result<qint64> MockReferralService::lastSyncedBlock() { return Result<qint64>::success(m_syncedBlock); }

// Starts 250 blocks ahead of the wallet and grows a block a second.
Result<qint64> MockReferralService::currentBlockHeight()
{
    return Result<qint64>::success(250 + m_clock.elapsed() / 1000);
}

Result<bool> MockReferralService::syncToBlock(qint64 block)
{
    m_syncedBlock = qMax(m_syncedBlock, qMin(block, currentBlockHeight().value));
    return Result<bool>::success(true);
}
