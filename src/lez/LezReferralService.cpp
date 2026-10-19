#include "lez/LezReferralService.h"

#include "lez/ReferralJson.h"

#include "logos_api.h"
#include "logos_api_client.h"
#include "logos_sdk.h"
#include "logos_ui_plugin_context.h"

using namespace referral;

namespace {

template <typename T>
Result<T> notConnected(const char* module)
{
    return Result<T>::failure(QStringLiteral("%1 is not connected").arg(QLatin1String(module)));
}

// Proving takes as long as it takes on commodity hardware: wait indefinitely, as the LEZ Wallet
// app does for private transfers. uplink_module bounds its own wait on lez_core.
const Timeout kSendTimeout(-1);

// The module's reply read by `parse`, or the call's own error (a timeout, the module gone).
template <typename T>
std::function<void(logos::AsyncResult<QString>)> replyTo(ReferralService::Done<T> done,
                                                         Result<T> (*parse)(const QString&))
{
    return [done = std::move(done), parse](logos::AsyncResult<QString> r) {
        done(r.ok() ? parse(r.value) : Result<T>::failure(QString::fromStdString(r.error.message)));
    };
}

} // namespace

LezReferralService::LezReferralService(LogosUiPluginContext& context)
    : m_context(context)
{
}

bool LezReferralService::connected(const char* module) const
{
    if (!m_context.isContextReady())
        return false;
    LogosAPIClient* client = m_context.modules().api->getClient(QString::fromLatin1(module));
    return client && client->isConnected();
}

// ---- uplink_module -------------------------------------------------------------

#define REFERRAL(T, call) \
    if (!connected("uplink_module")) \
        return notConnected<T>("uplink_module"); \
    const QString json = m_context.modules().uplink_module.call

Result<QString> LezReferralService::programAccount()
{
    REFERRAL(QString, info());
    return referral_json::field(json, "programAccount");
}

Result<bool> LezReferralService::configure(const QString& programAccount)
{
    REFERRAL(bool, configure(programAccount));
    const auto r = referral_json::reply(json);
    return r.ok() ? Result<bool>::success(true) : Result<bool>::failure(r.error);
}

Result<ProgramCheck> LezReferralService::checkProgram()
{
    REFERRAL(ProgramCheck, check_program());
    return referral_json::programCheck(json);
}

Result<QString> LezReferralService::createParticipant(const QString& label)
{
    REFERRAL(QString, create_participant(label));
    return referral_json::field(json, "participant");
}

Result<Participant> LezReferralService::participant(const QString& participant)
{
    REFERRAL(Participant, participant(participant));
    return referral_json::participant(json);
}

Result<QString> LezReferralService::invitation(const QString& participant, const QString& node)
{
    REFERRAL(QString, invitation(participant, node));
    return referral_json::field(json, "invitation");
}

Result<InvitationCheck> LezReferralService::checkInvitation(const QString& blob)
{
    REFERRAL(InvitationCheck, check_invitation(blob));
    return referral_json::invitationCheck(json);
}

Result<QString> LezReferralService::importInvitation(const QString& participant, const QString& blob)
{
    REFERRAL(QString, import_invitation(participant, blob));
    return referral_json::field(json, "referrer");
}

Result<Registry> LezReferralService::registry()
{
    REFERRAL(Registry, registry());
    return referral_json::registry(json);
}

Result<Activity> LezReferralService::activity(const QStringList& nodes, int count)
{
    REFERRAL(Activity, activity(nodes.join(QLatin1Char(',')), count));
    return referral_json::activity(json);
}

Result<QList<Note>> LezReferralService::notes(const QString& participant)
{
    REFERRAL(QList<Note>, notes(participant));
    return referral_json::notes(json);
}

Result<QString> LezReferralService::claimable(const QString& participant)
{
    REFERRAL(QString, claimable(participant));
    return referral_json::field(json, "points");
}

Result<Terms> LezReferralService::terms()
{
    REFERRAL(Terms, terms());
    return referral_json::terms(json);
}

Result<bool> LezReferralService::termsAccepted()
{
    REFERRAL(bool, terms_status());
    return referral_json::flag(json, "accepted");
}

Result<bool> LezReferralService::acceptTerms(int version, const QString& sha256)
{
    REFERRAL(bool, accept_terms(version, sha256));
    return referral_json::flag(json, "accepted");
}

Result<QString> LezReferralService::keystoreNode(const QString& keystorePath)
{
    REFERRAL(QString, keystore_node(keystorePath));
    return referral_json::field(json, "node");
}

Result<Status> LezReferralService::enrol(const QString& reference, const QString& participant,
                                         const QString& keystorePath, const QString& node)
{
    REFERRAL(Status, enrol(reference, participant, keystorePath, node));
    return referral_json::operationStatus(json);
}

Result<Status> LezReferralService::claim(const QString& reference, const QString& participant)
{
    REFERRAL(Status, claim(reference, participant));
    return referral_json::operationStatus(json);
}

Result<QString> LezReferralService::autoClaim(const QString& participant)
{
    REFERRAL(QString, auto_claim(participant));
    return referral_json::autoClaimReference(json);
}

Result<Status> LezReferralService::cashOut(const QString& reference, const QString& participant,
                                           const QString& keystorePath)
{
    REFERRAL(Status, cash_out(reference, participant, keystorePath));
    return referral_json::operationStatus(json);
}

Result<QList<Receipt>> LezReferralService::receipts(const QString& participant, const QString& keystorePath)
{
    REFERRAL(QList<Receipt>, receipts(participant, keystorePath));
    return referral_json::receipts(json);
}

Result<QString> LezReferralService::payoutCode(const QString& participant, quint64 index, const QString& keystorePath)
{
    REFERRAL(QString, payout_code(participant, qint64(index), keystorePath));
    return referral_json::field(json, "code");
}

Result<Status> LezReferralService::status(const QString& reference)
{
    REFERRAL(Status, status(reference));
    return referral_json::operationStatus(json);
}

#undef REFERRAL

void LezReferralService::enrolAsync(const QString& reference, const QString& participant,
                                    const QString& keystorePath, const QString& node, Done<Status> done)
{
    if (!connected("uplink_module"))
        return done(notConnected<Status>("uplink_module"));
    m_context.modules().uplink_module.enrolAsyncResult(reference, participant, keystorePath, node,
                                                         replyTo(std::move(done), referral_json::operationStatus),
                                                         kSendTimeout);
}

void LezReferralService::claimAsync(const QString& reference, const QString& participant, Done<Status> done)
{
    if (!connected("uplink_module"))
        return done(notConnected<Status>("uplink_module"));
    m_context.modules().uplink_module.claimAsyncResult(reference, participant,
                                                         replyTo(std::move(done), referral_json::operationStatus),
                                                         kSendTimeout);
}

void LezReferralService::autoClaimAsync(const QString& participant, Done<QString> done)
{
    if (!connected("uplink_module"))
        return done(notConnected<QString>("uplink_module"));
    m_context.modules().uplink_module.auto_claimAsyncResult(participant,
                                                              replyTo(std::move(done), referral_json::autoClaimReference),
                                                              kSendTimeout);
}

void LezReferralService::cashOutAsync(const QString& reference, const QString& participant,
                                      const QString& keystorePath, Done<Status> done)
{
    if (!connected("uplink_module"))
        return done(notConnected<Status>("uplink_module"));
    m_context.modules().uplink_module.cash_outAsyncResult(reference, participant, keystorePath,
                                                            replyTo(std::move(done), referral_json::operationStatus),
                                                            kSendTimeout);
}

// ---- lez_core: the wallet ----------------------------------------------------------

// lez_core has no "is a wallet open" call. It answers get_sequencer_addr with ""
// exactly when it holds no wallet; an open wallet always has a sequencer URL.
Result<bool> LezReferralService::walletOpen()
{
    if (!connected("lez_core"))
        return notConnected<bool>("lez_core");
    return Result<bool>::success(!m_context.modules().lez_core.get_sequencer_addr().isEmpty());
}

// resolve_label answers "Private/<hex>" or "Public/<hex>", or "" when no account has the label.
Result<QString> LezReferralService::resolveLabel(const QString& label)
{
    if (!m_context.isContextReady())
        return notConnected<QString>("lez_core");
    const QString resolved = m_context.modules().lez_core.resolve_label(label);
    return Result<QString>::success(resolved.section('/', 1));
}

Result<QString> LezReferralService::accountAddress(const QString& account)
{
    if (!m_context.isContextReady())
        return notConnected<QString>("lez_core");
    const QString address = m_context.modules().lez_core.account_id_to_base58(account);
    if (address.isEmpty())
        return Result<QString>::failure(QStringLiteral("lez_core could not encode the account"));
    return Result<QString>::success(address);
}

Result<qint64> LezReferralService::lastSyncedBlock()
{
    if (!m_context.isContextReady())
        return notConnected<qint64>("lez_core");
    return Result<qint64>::success(m_context.modules().lez_core.get_last_synced_block());
}

Result<qint64> LezReferralService::currentBlockHeight()
{
    if (!m_context.isContextReady())
        return notConnected<qint64>("lez_core");
    return Result<qint64>::success(m_context.modules().lez_core.get_current_block_height());
}

Result<bool> LezReferralService::syncToBlock(qint64 block)
{
    if (!m_context.isContextReady())
        return notConnected<bool>("lez_core");
    const auto error = m_context.modules().lez_core.sync_to_block(block);
    if (error != 0)
        return Result<bool>::failure(QStringLiteral("Wallet sync failed (wallet FFI error %1).").arg(error));
    return Result<bool>::success(true);
}
