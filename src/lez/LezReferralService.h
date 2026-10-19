#pragma once

#include "interfaces/ReferralService.h"

class LogosUiPluginContext;

// ReferralService over uplink_module (the programme) and lez_core (the wallet).
// uplink_module answers JSON: {"ok": true, …} or {"ok": false, "code", "error"}.
class LezReferralService : public referral::ReferralService {
public:
    explicit LezReferralService(LogosUiPluginContext& context);

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
    void enrolAsync(const QString& reference, const QString& participant, const QString& keystorePath,
                    const QString& node, Done<referral::Status> done) override;
    void claimAsync(const QString& reference, const QString& participant, Done<referral::Status> done) override;
    void autoClaimAsync(const QString& participant, Done<QString> done) override;
    void cashOutAsync(const QString& reference, const QString& participant, const QString& keystorePath,
                      Done<referral::Status> done) override;

    Result<bool> walletOpen() override;
    Result<QString> resolveLabel(const QString& label) override;
    Result<QString> accountAddress(const QString& account) override;
    Result<qint64> lastSyncedBlock() override;
    Result<qint64> currentBlockHeight() override;
    Result<bool> syncToBlock(qint64 block) override;

private:
    bool connected(const char* module) const;
    LogosUiPluginContext& m_context;
};
