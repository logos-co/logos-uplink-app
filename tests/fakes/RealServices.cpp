// The real services need the generated SDK and the modules behind it, which a test
// run has none of. The backend under test only ever gets mocks; these satisfy the link.
// (The real service's reply parsing lives in lez/ReferralJson.h and is tested directly.)

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
bool LezReferralService::connected(const char*) const { return false; }
Result<QString> LezReferralService::programAccount() { return absent<QString>(); }
Result<bool> LezReferralService::configure(const QString&) { return absent<bool>(); }
Result<ProgramCheck> LezReferralService::checkProgram() { return absent<ProgramCheck>(); }
Result<QString> LezReferralService::createParticipant(const QString&) { return absent<QString>(); }
Result<Participant> LezReferralService::participant(const QString&) { return absent<Participant>(); }
Result<QString> LezReferralService::invitation(const QString&, const QString&) { return absent<QString>(); }
Result<InvitationCheck> LezReferralService::checkInvitation(const QString&) { return absent<InvitationCheck>(); }
Result<QString> LezReferralService::importInvitation(const QString&, const QString&) { return absent<QString>(); }
Result<Registry> LezReferralService::registry() { return absent<Registry>(); }
Result<Activity> LezReferralService::activity(const QStringList&, int) { return absent<Activity>(); }
Result<QList<Note>> LezReferralService::notes(const QString&) { return absent<QList<Note>>(); }
Result<QString> LezReferralService::claimable(const QString&) { return absent<QString>(); }
Result<Terms> LezReferralService::terms() { return absent<Terms>(); }
Result<bool> LezReferralService::termsAccepted() { return absent<bool>(); }
Result<bool> LezReferralService::acceptTerms(int, const QString&) { return absent<bool>(); }
Result<QString> LezReferralService::keystoreNode(const QString&) { return absent<QString>(); }
Result<Status> LezReferralService::enrol(const QString&, const QString&, const QString&, const QString&) { return absent<Status>(); }
Result<Status> LezReferralService::claim(const QString&, const QString&) { return absent<Status>(); }
Result<QString> LezReferralService::autoClaim(const QString&) { return absent<QString>(); }
Result<Status> LezReferralService::cashOut(const QString&, const QString&, const QString&) { return absent<Status>(); }
Result<QList<Receipt>> LezReferralService::receipts(const QString&, const QString&) { return absent<QList<Receipt>>(); }
Result<QString> LezReferralService::payoutCode(const QString&, quint64, const QString&) { return absent<QString>(); }
Result<Status> LezReferralService::status(const QString&) { return absent<Status>(); }
Result<bool> LezReferralService::walletOpen() { return absent<bool>(); }
Result<QString> LezReferralService::resolveLabel(const QString&) { return absent<QString>(); }
Result<QString> LezReferralService::accountAddress(const QString&) { return absent<QString>(); }
Result<qint64> LezReferralService::lastSyncedBlock() { return absent<qint64>(); }
Result<qint64> LezReferralService::currentBlockHeight() { return absent<qint64>(); }
Result<bool> LezReferralService::syncToBlock(qint64) { return absent<bool>(); }

BlockchainNodeService::BlockchainNodeService(LogosUiPluginContext& context) : m_context(context) {}
node::Status BlockchainNodeService::status() { return {}; }
