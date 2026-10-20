#pragma once

#include <QList>
#include <functional>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include "interfaces/Result.h"

// The referral programme as uplink_module offers it (logos-blockchain/uplink's program),
// one method per module call. Accounts and nodes are hex; points are decimal strings.
// `reference` is a caller-chosen 32-byte hex id: resubmitting one is idempotent.
namespace referral {

struct Registry {
    quint32 epoch = 0;
    QStringList nodes;
    QStringList active;   // published by the oracle for `epoch`
};

struct Participant {
    QString node;
    QString referrer;                 // "" = root of its own tree
    QMap<QString, quint32> children;  // child node -> last epoch settled for it (0 = none yet)
    QMap<QString, quint32> since;     // child node -> the epoch it joined in, where known
    QString rewardBalance = QStringLiteral("0");
};

// A private note addressed to a participant: a new child, or credit forwarded by a child's claim.
struct Note {
    enum Kind { Child, Credit };
    QString account;
    Kind kind = Child;
    QString node;                     // Child: the child's node
    quint32 since = 0;                // Child: the epoch it joined in
    QString amount = QStringLiteral("0");  // Credit
};

struct Receipt {
    quint64 index = 0;
    QString account;                  // the public cash-out receipt
    QString points = QStringLiteral("0");
};

// The text the node signs for a payout code, and the opening's values the node app builds it
// from (the node.sign_referral_payout intent's params).
struct PayoutMessage {
    QString message;
    QString programAccount;           // hex
    QString blindingFactor;           // hex
    QString receipt;
    QString points = QStringLiteral("0");
};

// The programme's terms, as the module ships them. Accepting names the version and hash.
struct Terms {
    int version = 0;
    QString sha256;
    QString lastUpdated;
    QVariantList sections;            // [{title, items: [{t, h?, i?}]}]
};

struct InvitationCheck {
    QString referrer;                 // the inviter's node
    bool registered = false;          // only a registered inviter can be joined under
};

// Whether each node was active in each recent published epoch: 1, 0, or -1 unknown.
struct Activity {
    QList<quint32> epochs;            // oldest first
    QMap<QString, QList<int>> nodes;
};

// Whether the selected deployment runs uplink_module's program.
struct ProgramCheck {
    enum State { Ready, NotConfigured, NotDeployed, Mismatch };
    State state = NotConfigured;
    QString detail;                   // the module's explanation, when not Ready
};

enum class Status { Pending, Settled, Rejected };

class ReferralService {
public:
    virtual ~ReferralService() = default;

    // ---- uplink_module ------------------------------------------------

    // The deployment: the program account the module uses ("" = none selected), selecting
    // one, and whether it runs the module's program (read on the wallet's network).
    virtual Result<QString> programAccount() = 0;
    virtual Result<bool> configure(const QString& programAccount) = 0;
    virtual Result<ProgramCheck> checkProgram() = 0;

    // A private account for the programme, labelled so it's found again (an account already
    // carrying the label is reused).
    virtual Result<QString> createParticipant(const QString& label) = 0;
    virtual Result<Participant> participant(const QString& participant) = 0;
    virtual Result<QString> invitation(const QString& participant, const QString& node) = 0;
    virtual Result<InvitationCheck> checkInvitation(const QString& blob) = 0;
    // Returns the inviting node. Must come before enrol.
    virtual Result<QString> importInvitation(const QString& participant, const QString& blob) = 0;
    virtual Result<Registry> registry() = 0;
    virtual Result<Activity> activity(const QStringList& nodes, int count) = 0;
    virtual Result<QList<Note>> notes(const QString& participant) = 0;
    virtual Result<QString> claimable(const QString& participant) = 0;

    virtual Result<Terms> terms() = 0;
    virtual Result<bool> termsAccepted() = 0;
    virtual Result<bool> acceptTerms(int version, const QString& sha256) = 0;

    // What the node's key provides comes from the node app, which holds the key and asks the
    // user (the node.sign_referral_* and node.referral_receipt_secret intents); the module
    // checks it before using it, and never sees the key.

    // Submits Register with `node`'s signature over the enrolment (hex).
    virtual Result<Status> enrolWithSignature(const QString& reference, const QString& participant,
                                              const QString& node, const QString& signature) = 0;
    // Submits the oldest claimable batch.
    virtual Result<Status> claim(const QString& reference, const QString& participant) = 0;
    // Claims a batch when something is claimable and nothing is pending; the reference it
    // submitted, or "" when it skipped.
    virtual Result<QString> autoClaim(const QString& participant) = 0;
    // Burns the whole balance into a receipt, found again with the node's receipt secret (hex).
    virtual Result<Status> cashOutWithSecret(const QString& reference, const QString& participant,
                                             const QString& receiptSecret) = 0;
    // The receipts already known (no secret needed).
    virtual Result<QList<Receipt>> receipts(const QString& participant) = 0;
    // What the node signs for receipt `index`'s payout code, then the code from its signature.
    virtual Result<PayoutMessage> payoutMessage(const QString& participant, quint64 index) = 0;
    virtual Result<QString> payoutCodeWithSignature(const QString& participant, quint64 index,
                                                    const QString& signature) = 0;
    virtual Result<Status> status(const QString& reference) = 0;

    // The same four submissions without blocking: each proves and sends a transaction, which
    // takes minutes without RISC0_DEV_MODE. By default they answer at once from the calls above;
    // the real service overrides them.
    template <typename T>
    using Done = std::function<void(Result<T>)>;
    virtual void enrolWithSignatureAsync(const QString& reference, const QString& participant,
                                         const QString& node, const QString& signature, Done<Status> done)
    {
        done(enrolWithSignature(reference, participant, node, signature));
    }
    virtual void claimAsync(const QString& reference, const QString& participant, Done<Status> done)
    {
        done(claim(reference, participant));
    }
    virtual void autoClaimAsync(const QString& participant, Done<QString> done)
    {
        done(autoClaim(participant));
    }
    virtual void cashOutWithSecretAsync(const QString& reference, const QString& participant,
                                        const QString& receiptSecret, Done<Status> done)
    {
        done(cashOutWithSecret(reference, participant, receiptSecret));
    }

    // ---- lez_core: the wallet --------------------------------------------
    // The wallet itself belongs to the LEZ Wallet app; Uplink only checks it is open,
    // finds its points account by label, and keeps it synced so notes are found.
    virtual Result<bool> walletOpen() = 0;
    // Whether lez_core is still there, without calling it: while it proves it answers nothing,
    // but its process is still listening.
    virtual bool walletReachable() { return true; }
    virtual Result<QString> resolveLabel(const QString& label) = 0;   // "" if no account has it
    virtual Result<QString> accountAddress(const QString& account) = 0;  // base58, as the wallet shows it
    virtual Result<qint64> lastSyncedBlock() = 0;
    virtual Result<qint64> currentBlockHeight() = 0;
    virtual Result<bool> syncToBlock(qint64 block) = 0;
};

} // namespace referral
