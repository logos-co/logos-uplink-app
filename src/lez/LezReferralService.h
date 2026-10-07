#pragma once

#include "interfaces/ReferralService.h"

class LogosUiPluginContext;

// ReferralService over lez_core. The wallet, label, address and sync calls are
// real. lez_core has no referral calls yet (they need logos-execution-zone#896
// merged, referral bindings in its wallet-ffi, and matching lez_core methods);
// until then those fail with that reason.
class LezReferralService : public referral::ReferralService {
public:
    explicit LezReferralService(LogosUiPluginContext& context);

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

private:
    LogosUiPluginContext& m_context;
};
