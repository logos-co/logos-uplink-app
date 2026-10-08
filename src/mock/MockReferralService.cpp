#include "mock/MockReferralService.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>

#include "InvitationCode.h"

using namespace referral;

namespace {

// UPLINK_MOCK_EPOCH_MS shortens it, so UI tests can reach a claim without waiting minutes.
qint64 epochMs()
{
    bool ok = false;
    const qint64 ms = qEnvironmentVariable("UPLINK_MOCK_EPOCH_MS").toLongLong(&ok);
    return ok && ms > 0 ? ms : 30000;
}

QString hexId(const QByteArray& seed)
{
    return QString::fromLatin1(QCryptographicHash::hash(seed, QCryptographicHash::Sha256).toHex());
}

// Participant::claim, with the assumed rules: registers new children, pays each
// child for every epoch since it was last paid in which it AND your own node were
// active, and adds every credit.
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
{
    m_clock.start();
    m_registry.epoch = 1;
    m_registry.nodes << inviterNode();
}

QString MockReferralService::inviterNode() { return hexId("mock-inviter"); }

// An Invitation blob as lez_core is expected to hand it out: #896's field names, hex keys.
QString MockReferralService::invitationFor(const QString& node)
{
    const QJsonObject blob{
        {QStringLiteral("parent_node"), node},
        {QStringLiteral("npk"), hexId("npk:" + node.toUtf8())},
        {QStringLiteral("vpk"), hexId("vpk:" + node.toUtf8())},
    };
    return QString::fromUtf8(QJsonDocument(blob).toJson(QJsonDocument::Compact));
}

QString MockReferralService::nextId(const char* tag)
{
    return hexId(QByteArray(tag) + QByteArray::number(++m_counter));
}

Result<QString> MockReferralService::createParticipant()
{
    if (!m_walletOpen)
        return Result<QString>::failure(QStringLiteral("no wallet is open"));
    const QString id = nextId("participant");
    m_accounts.insert(id, Account{});
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

Result<QString> MockReferralService::importInvitation(const QString& participant, const QString& blob)
{
    if (!m_accounts.contains(participant))
        return Result<QString>::failure(QStringLiteral("unknown participant"));
    const QString parent = invitation_code::inviterNode(blob);
    if (parent.isEmpty())
        return Result<QString>::failure(QStringLiteral("not an invitation"));
    m_accounts[participant].invitedBy = parent;
    return Result<QString>::success(parent);
}

Result<QByteArray> MockReferralService::prepareRegistration(const QString& participant, const QString& node,
                                                            const QString& referrer)
{
    if (!m_accounts.contains(participant))
        return Result<QByteArray>::failure(QStringLiteral("unknown participant"));
    Account& account = m_accounts[participant];
    if (!referrer.isEmpty() && account.invitedBy != referrer)
        return Result<QByteArray>::failure(
            QStringLiteral("the referrer's invitation is needed before its registration"));
    if (account.registered)
        return Result<QByteArray>::failure(QStringLiteral("this participant is already registered"));
    if (account.hasPending && (account.pending.node != node || account.pending.referrer != referrer))
        return Result<QByteArray>::failure(
            QStringLiteral("a pending registration for another node or referrer is unresolved"));
    if (!account.hasPending) {
        account.hasPending = true;
        account.pending = {node, referrer, {}};
    }
    return Result<QByteArray>::success(
        QStringLiteral("mock-authorization|%1|%2|%3").arg(participant, node, referrer).toUtf8());
}

Result<bool> MockReferralService::attachNodeSignature(const QString& participant, const QByteArray& signature)
{
    if (!m_accounts.contains(participant))
        return Result<bool>::failure(QStringLiteral("unknown participant"));
    Account& account = m_accounts[participant];
    if (!account.hasPending)
        return Result<bool>::failure(QStringLiteral("no registration is being prepared"));
    if (signature.size() != 64)
        return Result<bool>::failure(QStringLiteral("a node signature is 64 bytes"));
    account.pending.signature = signature;
    return Result<bool>::success(true);
}

Result<Registry> MockReferralService::registry()
{
    for (; m_epochsPublished < m_clock.elapsed() / m_epochMs; ++m_epochsPublished)
        advanceEpoch();
    return Result<Registry>::success(m_registry);
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
    Participant copy = account.state;
    return Result<QString>::success(QString::number(applyClaim(copy, m_registry.epoch, m_activeHistory, notesFor(copy.node))));
}

Result<QList<Receipt>> MockReferralService::receipts(const QString& participant)
{
    return Result<QList<Receipt>>::success(m_accounts.value(participant).receipts);
}

QString MockReferralService::programAccount() { return hexId("mock-referral-program"); }

// Stands in for #896's HKDF over the wallet's nullifier key: stable per receipt.
QString MockReferralService::blindingFactor(const QString& participant, quint64 index)
{
    return hexId("blinding:" + participant.toUtf8() + ":" + QByteArray::number(index));
}

QString MockReferralService::receiptAddress(const QString& programAccount, const QString& node,
                                            const QString& blindingFactor)
{
    return hexId("receipt:" + programAccount.toUtf8() + ":" + node.toUtf8() + ":" + blindingFactor.toUtf8());
}

Result<Opening> MockReferralService::opening(const QString& participant, quint64 index)
{
    const Account account = m_accounts.value(participant);
    if (!account.registered || index >= quint64(account.receipts.size()))
        return Result<Opening>::failure(QStringLiteral("no such receipt"));
    Opening o;
    o.programAccount = programAccount();
    o.node = account.state.node;
    o.blindingFactor = blindingFactor(participant, index);
    o.account = account.receipts.at(int(index)).account;
    return Result<Opening>::success(o);
}

Result<Status> MockReferralService::record(const QString& reference, Operation operation)
{
    if (m_operations.contains(reference))
        return Result<Status>::success(m_operations.value(reference).status);
    m_operations.insert(reference, operation);
    return Result<Status>::success(Status::Pending);
}

Result<Status> MockReferralService::submitRegister(const QString& reference, const QString& participant)
{
    const Account account = m_accounts.value(participant);
    if (!account.hasPending || account.pending.signature.isEmpty())
        return Result<Status>::failure(QStringLiteral("the registration has no node signature"));
    return record(reference, {Kind::Register, participant, {}});
}

Result<Status> MockReferralService::submitClaim(const QString& reference, const QString& participant,
                                                const QStringList& noteAccounts)
{
    if (!m_accounts.value(participant).registered)
        return notRegistered<Status>();
    return record(reference, {Kind::Claim, participant, noteAccounts});
}

Result<Status> MockReferralService::cashOut(const QString& reference, const QString& participant)
{
    if (!m_accounts.value(participant).registered)
        return notRegistered<Status>();
    return record(reference, {Kind::CashOut, participant, {}});
}

Result<Status> MockReferralService::reconcile(const QString& reference)
{
    if (!m_operations.contains(reference))
        return Result<Status>::success(Status::Rejected);
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
        if (m_registry.nodes.contains(account.pending.node))
            return Status::Rejected;
        if (!account.pending.referrer.isEmpty() && !m_registry.nodes.contains(account.pending.referrer))
            return Status::Rejected;
        m_registry.nodes << account.pending.node;
        account.registered = true;
        account.state.node = account.pending.node;
        account.state.referrer = account.pending.referrer;
        account.hasPending = false;
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
        if (!m_registry.active.contains(account.state.node))
            return Status::Rejected;
        QList<Note> consumed;
        for (const QString& id : operation.notes) {
            if (!m_notes.contains(id))
                return Status::Rejected;
            consumed << m_notes.value(id).note;
        }
        for (const QString& id : operation.notes)
            m_notes.remove(id);
        // The program forwards the total to the referrer as a Credit; the mock's referrers are outside it.
        applyClaim(account.state, m_registry.epoch, m_activeHistory, consumed);
        return Status::Settled;
    }
    case Kind::CashOut: {
        Receipt receipt;
        receipt.index = account.receipts.size();
        receipt.account = receiptAddress(programAccount(), account.state.node,
                                         blindingFactor(operation.participant, receipt.index));
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

Result<bool> MockReferralService::addLabel(const QString& label, const QString& account)
{
    if (m_labels.contains(label))
        return Result<bool>::failure(QStringLiteral("label already in use"));
    m_labels.insert(label, account);
    return Result<bool>::success(true);
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
