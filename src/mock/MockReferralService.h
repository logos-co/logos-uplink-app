#pragma once

#include <QElapsedTimer>
#include <QMap>
#include <QSet>

#include "interfaces/ReferralService.h"

// In-memory referral programme, following logos-blockchain/uplink's program (as of
// 8021f33) the way uplink_module drives it:
//  - accrual: a claim pays +1 per direct child for every epoch since it was last settled
//    in which it AND your own node were active, plus every credit addressed to you;
//  - a cash-out takes the whole reward balance and leaves a public receipt;
//  - a node registers once, under a registered referrer whose invitation was imported
//    first, and only after the current terms were accepted;
//  - the Blend signing key comes from a keystore path: one naming "other-node" holds
//    another node's key, so it's refused (keystore_mismatch).
// Operations settle on the next status(). Registering seeds three children, and the oracle
// publishes a new epoch every 30 seconds (UPLINK_MOCK_EPOCH_MS); your own node is inactive
// every fourth epoch.
class MockReferralService : public referral::ReferralService {
public:
    explicit MockReferralService(bool walletOpen = true);

    Result<QString> programAccount() override;
    Result<bool> configure(const QString& programAccount) override;
    Result<referral::ProgramCheck> checkProgram() override;
    Result<QString> createParticipant(const QString& label) override;
    Result<referral::Participant> participant(const QString& participant) override;
    Result<QString> invitation(const QString& participant, const QString& node) override;
    Result<referral::InvitationCheck> checkInvitation(const QString& blob) override;
    Result<QString> importInvitation(const QString& participant, const QString& blob) override;
    Result<referral::Registry> registry() override;
    Result<referral::Activity> activity(const QStringList& nodes, int count) override;
    Result<QList<referral::Note>> notes(const QString& participant) override;
    Result<QString> claimable(const QString& participant) override;
    Result<referral::Terms> terms() override;
    Result<bool> termsAccepted() override;
    Result<bool> acceptTerms(int version, const QString& sha256) override;
    Result<QString> keystoreNode(const QString& keystorePath) override;
    Result<referral::Status> enrol(const QString& reference, const QString& participant,
                                   const QString& keystorePath, const QString& node) override;
    Result<referral::Status> claim(const QString& reference, const QString& participant) override;
    Result<QString> autoClaim(const QString& participant) override;
    Result<referral::Status> cashOut(const QString& reference, const QString& participant,
                                     const QString& keystorePath) override;
    Result<QList<referral::Receipt>> receipts(const QString& participant, const QString& keystorePath) override;
    Result<QString> payoutCode(const QString& participant, quint64 index, const QString& keystorePath) override;
    Result<referral::Status> status(const QString& reference) override;
    Result<bool> walletOpen() override;
    Result<QString> resolveLabel(const QString& label) override;
    Result<QString> accountAddress(const QString& account) override;
    Result<qint64> lastSyncedBlock() override;
    Result<qint64> currentBlockHeight() override;
    Result<bool> syncToBlock(qint64 block) override;

    // The oracle publishes the next epoch. Child i is active unless (epoch + i) % 3 == 0;
    // every even epoch each participant gets 1 point of Credit from deeper in its tree.
    void advanceEpoch();

    // What the LEZ Wallet app does to the wallet Uplink sees.
    void setWalletOpen(bool open) { m_walletOpen = open; }
    // Another wallet: none of the labels the last one had. The program's state stays.
    void switchWallet() { m_labels.clear(); }

    // A node that is already registered, so its invitations import cleanly.
    static QString inviterNode();
    // An invitation from `node`, in uplink_module's format.
    static QString invitationFor(const QString& node);
    // The keystore the mock offers by default, and one for another node.
    static QString keystorePath();
    static QString otherNodeKeystorePath();
    // The mock network's referral program, selected from the start; and an account holding
    // another version of it. Any other account has nothing deployed.
    static QString deployedProgram();
    static QString outdatedProgram();

private:
    struct Account {
        bool registered = false;
        referral::Participant state;
        QString invitedBy;
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
        QString node;                                // Register: the node joining
        referral::Status status = referral::Status::Pending;
    };

    QString nextId(const char* tag);
    Result<referral::Status> record(const QString& reference, Operation operation);
    referral::Status settle(Operation& operation);
    bool hasOpen(const QString& participant) const;
    void addNote(referral::Note note, const QString& recipient);
    QList<referral::Note> notesFor(const QString& node) const;

    bool m_walletOpen = true;
    bool m_termsAccepted = false;
    qint64 m_epochMs = 30000;
    QString m_program;
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
