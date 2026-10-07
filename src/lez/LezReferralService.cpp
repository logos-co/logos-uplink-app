#include "lez/LezReferralService.h"

#include "logos_api.h"
#include "logos_api_client.h"
#include "logos_sdk.h"
#include "logos_ui_plugin_context.h"

using namespace referral;

namespace {

template <typename T>
Result<T> unavailable()
{
    return Result<T>::failure(QStringLiteral("lez_core has no referral calls yet"));
}

template <typename T>
Result<T> notConnected()
{
    return Result<T>::failure(QStringLiteral("lez_core is not connected"));
}

} // namespace

LezReferralService::LezReferralService(LogosUiPluginContext& context)
    : m_context(context)
{
}

Result<QString> LezReferralService::createParticipant() { return unavailable<QString>(); }
Result<Participant> LezReferralService::participant(const QString&) { return unavailable<Participant>(); }
Result<QString> LezReferralService::invitation(const QString&, const QString&) { return unavailable<QString>(); }
Result<QString> LezReferralService::importInvitation(const QString&, const QString&) { return unavailable<QString>(); }
Result<QByteArray> LezReferralService::prepareRegistration(const QString&, const QString&, const QString&) { return unavailable<QByteArray>(); }
Result<bool> LezReferralService::attachNodeSignature(const QString&, const QByteArray&) { return unavailable<bool>(); }
Result<Registry> LezReferralService::registry() { return unavailable<Registry>(); }
Result<QList<Note>> LezReferralService::notes(const QString&) { return unavailable<QList<Note>>(); }
Result<QString> LezReferralService::claimable(const QString&) { return unavailable<QString>(); }
Result<QList<Receipt>> LezReferralService::receipts(const QString&) { return unavailable<QList<Receipt>>(); }
Result<Status> LezReferralService::submitRegister(const QString&, const QString&) { return unavailable<Status>(); }
Result<Status> LezReferralService::submitClaim(const QString&, const QString&, const QStringList&) { return unavailable<Status>(); }
Result<Status> LezReferralService::cashOut(const QString&, const QString&) { return unavailable<Status>(); }
Result<Status> LezReferralService::reconcile(const QString&) { return unavailable<Status>(); }

Result<bool> LezReferralService::walletOpen()
{
    return Result<bool>::failure(QStringLiteral("lez_core can't report whether a wallet is open yet"));
}

// resolve_label answers "Private/<hex>" or "Public/<hex>", or "" when no account has the label.
Result<QString> LezReferralService::resolveLabel(const QString& label)
{
    if (!m_context.isContextReady())
        return notConnected<QString>();
    const QString resolved = m_context.modules().lez_core.resolve_label(label);
    return Result<QString>::success(resolved.section('/', 1));
}

Result<bool> LezReferralService::addLabel(const QString& label, const QString& account)
{
    if (!m_context.isContextReady())
        return notConnected<bool>();
    // add_label reports success even when the wallet refused, so read it back.
    m_context.modules().lez_core.add_label(label, account, true);
    const auto resolved = resolveLabel(label);
    if (!resolved.ok())
        return Result<bool>::failure(resolved.error);
    if (resolved.value.compare(account, Qt::CaseInsensitive) != 0)
        return Result<bool>::failure(QStringLiteral("The wallet did not keep the label \"%1\".").arg(label));
    return Result<bool>::success(true);
}

Result<QString> LezReferralService::accountAddress(const QString& account)
{
    if (!m_context.isContextReady())
        return notConnected<QString>();
    const QString address = m_context.modules().lez_core.account_id_to_base58(account);
    if (address.isEmpty())
        return Result<QString>::failure(QStringLiteral("lez_core could not encode the account"));
    return Result<QString>::success(address);
}

Result<qint64> LezReferralService::lastSyncedBlock()
{
    if (!m_context.isContextReady())
        return notConnected<qint64>();
    return Result<qint64>::success(m_context.modules().lez_core.get_last_synced_block());
}

Result<qint64> LezReferralService::currentBlockHeight()
{
    if (!m_context.isContextReady())
        return notConnected<qint64>();
    return Result<qint64>::success(m_context.modules().lez_core.get_current_block_height());
}

Result<bool> LezReferralService::syncToBlock(qint64 block)
{
    if (!m_context.isContextReady())
        return notConnected<bool>();
    const auto error = m_context.modules().lez_core.sync_to_block(block);
    if (error != 0)
        return Result<bool>::failure(QStringLiteral("Wallet sync failed (wallet FFI error %1).").arg(error));
    return Result<bool>::success(true);
}
