// The real services need the generated SDK and the modules behind it, which a test
// run has none of. The backend under test only ever gets mocks; these satisfy the link.

#include "blockchain/BlockchainNodeService.h"
#include "lez/LezReferralService.h"

using namespace referral;

namespace {

template <typename T>
Result<T> absent()
{
    return Result<T>::failure(QStringLiteral("not in tests"));
}

} // namespace

LezReferralService::LezReferralService(LogosUiPluginContext& context) : m_context(context) {}
Result<QString> LezReferralService::createParticipant() { return absent<QString>(); }
Result<Participant> LezReferralService::participant(const QString&) { return absent<Participant>(); }
Result<QString> LezReferralService::invitation(const QString&, const QString&) { return absent<QString>(); }
Result<QString> LezReferralService::importInvitation(const QString&, const QString&) { return absent<QString>(); }
Result<QByteArray> LezReferralService::prepareRegistration(const QString&, const QString&, const QString&) { return absent<QByteArray>(); }
Result<bool> LezReferralService::attachNodeSignature(const QString&, const QByteArray&) { return absent<bool>(); }
Result<Registry> LezReferralService::registry() { return absent<Registry>(); }
Result<QList<Note>> LezReferralService::notes(const QString&) { return absent<QList<Note>>(); }
Result<QString> LezReferralService::claimable(const QString&) { return absent<QString>(); }
Result<QList<Receipt>> LezReferralService::receipts(const QString&) { return absent<QList<Receipt>>(); }
Result<Opening> LezReferralService::opening(const QString&, quint64) { return absent<Opening>(); }
Result<Status> LezReferralService::submitRegister(const QString&, const QString&) { return absent<Status>(); }
Result<Status> LezReferralService::submitClaim(const QString&, const QString&, const QStringList&) { return absent<Status>(); }
Result<Status> LezReferralService::cashOut(const QString&, const QString&) { return absent<Status>(); }
Result<Status> LezReferralService::reconcile(const QString&) { return absent<Status>(); }
Result<bool> LezReferralService::walletOpen() { return absent<bool>(); }
Result<QString> LezReferralService::resolveLabel(const QString&) { return absent<QString>(); }
Result<bool> LezReferralService::addLabel(const QString&, const QString&) { return absent<bool>(); }
Result<QString> LezReferralService::accountAddress(const QString&) { return absent<QString>(); }
Result<qint64> LezReferralService::lastSyncedBlock() { return absent<qint64>(); }
Result<qint64> LezReferralService::currentBlockHeight() { return absent<qint64>(); }
Result<bool> LezReferralService::syncToBlock(qint64) { return absent<bool>(); }

BlockchainNodeService::BlockchainNodeService(LogosUiPluginContext& context) : m_context(context) {}
node::Status BlockchainNodeService::status() { return {}; }
Result<QByteArray> BlockchainNodeService::signWithoutPrompt(const QString&, const QByteArray&) { return absent<QByteArray>(); }
