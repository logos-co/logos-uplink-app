#pragma once

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include "interfaces/Result.h"

// The LEZ referral wallet facade (wallet::program_facades::referral::Referral,
// logos-execution-zone#896), one method per facade call. Accounts and nodes are
// hex; points are decimal strings.
namespace referral {

struct Registry {
    quint32 epoch = 0;
    QStringList nodes;
    QStringList active;   // published by the oracle for `epoch`
};

struct Participant {
    QString node;
    QString referrer;                 // "" = root of its own tree
    QMap<QString, quint32> children;  // child node -> last epoch it was paid for
    QString rewardBalance = QStringLiteral("0");
};

// A private note addressed to a participant: a new child, or credit forwarded by a child's claim.
struct Note {
    enum Kind { Child, Credit };
    QString account;
    Kind kind = Child;
    QString node;                     // Child: the child's node
    QString amount = QStringLiteral("0");  // Credit
};

struct Receipt {
    quint64 index = 0;
    QString account;                  // the public cash-out PDA
    QString points = QStringLiteral("0");
};

// What proves a receipt: its address is derived from these. #896's Opening.
struct Opening {
    QString programAccount;
    QString node;
    QString blindingFactor;           // 32 bytes, hex; derived from the wallet's keys
    QString account;                  // the receipt
};

enum class Status { Pending, Settled, Rejected };

class ReferralService {
public:
    virtual ~ReferralService() = default;

    virtual Result<QString> createParticipant() = 0;
    virtual Result<Participant> participant(const QString& participant) = 0;
    virtual Result<QString> invitation(const QString& participant, const QString& node) = 0;
    // Returns the inviting node. Must come before prepareRegistration.
    virtual Result<QString> importInvitation(const QString& participant, const QString& blob) = 0;
    // Returns the ParticipantAuthorizationV1 message the node key signs.
    virtual Result<QByteArray> prepareRegistration(const QString& participant, const QString& node,
                                                   const QString& referrer) = 0;
    virtual Result<bool> attachNodeSignature(const QString& participant, const QByteArray& signature) = 0;
    virtual Result<Registry> registry() = 0;
    virtual Result<QList<Note>> notes(const QString& participant) = 0;
    virtual Result<QString> claimable(const QString& participant) = 0;
    virtual Result<QList<Receipt>> receipts(const QString& participant) = 0;
    virtual Result<Opening> opening(const QString& participant, quint64 index) = 0;

    // `reference` is a caller-chosen 32-byte id (hex); resubmitting it is idempotent.
    virtual Result<Status> submitRegister(const QString& reference, const QString& participant) = 0;
    virtual Result<Status> submitClaim(const QString& reference, const QString& participant,
                                       const QStringList& noteAccounts) = 0;
    virtual Result<Status> cashOut(const QString& reference, const QString& participant) = 0;
    virtual Result<Status> reconcile(const QString& reference) = 0;

    // lez_core wallet calls, outside the facade. The wallet itself belongs to the
    // LEZ Wallet app; Uplink only checks it is open and names its own account in it.
    virtual Result<bool> walletOpen() = 0;
    virtual Result<QString> resolveLabel(const QString& label) = 0;   // "" if no account has it
    virtual Result<bool> addLabel(const QString& label, const QString& account) = 0;
    virtual Result<QString> accountAddress(const QString& account) = 0;  // base58, as the wallet shows it
    // New referrals and credits are private notes the wallet only finds by scanning blocks.
    virtual Result<qint64> lastSyncedBlock() = 0;
    virtual Result<qint64> currentBlockHeight() = 0;
    virtual Result<bool> syncToBlock(qint64 block) = 0;
};

} // namespace referral
