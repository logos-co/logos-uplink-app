#pragma once

#include <QElapsedTimer>
#include <QMap>

#include "interfaces/ReferralService.h"

// In-memory referral programme, following the LEZ program on
// logos-execution-zone#896 as of 2026-10-07, plus two rules we assume it gains:
//  - accrual: a claim pays +1 per direct child per epoch since it was last paid
//    (#896 pays only the current epoch), plus every Credit note addressed to you;
//  - own-node activity: an epoch only pays if your own node was active in it too,
//    and a claim is rejected while your node is not active (#896 checks neither);
//  - a cash-out takes the whole reward balance and leaves a public receipt;
//  - a node registers once, under a registered referrer whose invitation was imported first.
// Operations settle on the next reconcile(). Registering seeds three children,
// and the oracle publishes a new epoch every 30 seconds; your own node is
// inactive every fourth epoch.
class MockReferralService : public referral::ReferralService {
public:
    explicit MockReferralService(bool walletOpen = true);

    Result<QString> createParticipant() override;
    Result<referral::Participant> participant(const QString& participant) override;
    Result<QString> invitation(const QString& participant, const QString& node) override;
    Result<QString> importInvitation(const QString& participant, const QString& blob) override;
    Result<QByteArray> prepareRegistration(const QString& participant, const QString& node,
                                           const QString& referrer) override;
    Result<bool> attachNodeSignature(const QString& participant, const QByteArray& signature) override;
    Result<referral::Registry> registry() override;
    Result<QList<referral::Note>> notes(const QString& participant) override;
    Result<QString> claimable(const QString& participant) override;
    Result<QList<referral::Receipt>> receipts(const QString& participant) override;
    Result<referral::Opening> opening(const QString& participant, quint64 index) override;
    Result<referral::Status> submitRegister(const QString& reference, const QString& participant) override;
    Result<referral::Status> submitClaim(const QString& reference, const QString& participant,
                                         const QStringList& noteAccounts) override;
    Result<referral::Status> cashOut(const QString& reference, const QString& participant) override;
    Result<referral::Status> reconcile(const QString& reference) override;
    Result<bool> walletOpen() override;
    Result<QString> resolveLabel(const QString& label) override;
    Result<bool> addLabel(const QString& label, const QString& account) override;
    Result<QString> accountAddress(const QString& account) override;
    Result<qint64> lastSyncedBlock() override;
    Result<qint64> currentBlockHeight() override;
    Result<bool> syncToBlock(qint64 block) override;

    // The oracle publishes the next epoch. Child i is active unless (epoch + i) % 3 == 0;
    // every even epoch each participant gets 1 point of Credit from deeper in its tree.
    void advanceEpoch();

    // A node that is already registered, so its invitations import cleanly.
    static QString inviterNode();
    static QString invitationFor(const QString& node);

    // A receipt's address from its opening, as #896's cash_out_receipt(): only someone
    // holding the opening can find the receipt. The payout side recomputes it.
    static QString receiptAddress(const QString& programAccount, const QString& node,
                                  const QString& blindingFactor);

private:
    struct Pending {
        QString node;
        QString referrer;
        QByteArray signature;
    };
    struct Account {
        bool registered = false;
        referral::Participant state;
        QString invitedBy;
        bool hasPending = false;
        Pending pending;
        QList<referral::Receipt> receipts;
    };
    struct MockNote {
        referral::Note note;
        QString recipient;
    };
    enum class Kind { Register, Claim, CashOut };
    struct Operation {
        Kind kind;
        QString participant;
        QStringList notes;
        referral::Status status = referral::Status::Pending;
    };

    QString nextId(const char* tag);
    static QString programAccount();
    static QString blindingFactor(const QString& participant, quint64 index);
    Result<referral::Status> record(const QString& reference, Operation operation);
    referral::Status settle(Operation& operation);
    void addNote(referral::Note note, const QString& recipient);
    QList<referral::Note> notesFor(const QString& node) const;

    bool m_walletOpen = true;
    qint64 m_syncedBlock = 0;
    QMap<QString, QString> m_labels;   // label -> account
    quint64 m_counter = 0;
    QElapsedTimer m_clock;
    qint64 m_epochsPublished = 0;
    referral::Registry m_registry;
    QStringList m_children;
    QMap<quint32, QStringList> m_activeHistory;   // epoch -> active set, as published
    QMap<QString, Account> m_accounts;
    QMap<QString, MockNote> m_notes;
    QMap<QString, Operation> m_operations;
};
