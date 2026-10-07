// The mock must follow the LEZ program's rules, or the UI is built against the wrong ones.

#include <logos_test.h>

#include "mock/MockNodeService.h"
#include <QSettings>
#include <QTemporaryDir>

#include "ActivityLog.h"
#include "InvitationCode.h"
#include "PayoutCode.h"
#include "mock/MockReferralService.h"

using namespace referral;

namespace {

int g_reference = 0;
QString nextReference() { return QString::number(++g_reference); }

QString settle(MockReferralService& s, const Result<Status>& submitted, const QString& reference)
{
    if (!submitted.ok())
        return submitted.error;
    return s.reconcile(reference).value == Status::Settled ? QString() : QStringLiteral("rejected");
}

// Registers a participant for `node` under `referrer` ("" = own tree).
QString enrol(MockReferralService& s, const QString& node, const QString& referrer = {})
{
    const QString p = s.createParticipant().value;
    if (!referrer.isEmpty())
        s.importInvitation(p, MockReferralService::invitationFor(referrer));
    s.prepareRegistration(p, node, referrer);
    s.attachNodeSignature(p, QByteArray(64, 'x'));
    const QString reference = nextReference();
    settle(s, s.submitRegister(reference, p), reference);
    return p;
}

QString claim(MockReferralService& s, const QString& p)
{
    QStringList accounts;
    for (const Note& n : s.notes(p).value)
        accounts << n.account;
    const QString reference = nextReference();
    return settle(s, s.submitClaim(reference, p, accounts), reference);
}

} // namespace

LOGOS_TEST(registration_settles_and_seeds_three_children) {
    MockReferralService s;
    const QString p = enrol(s, "node-a");
    LOGOS_ASSERT_TRUE(s.participant(p).ok());
    LOGOS_ASSERT_EQ(s.notes(p).value.size(), 3);
}

LOGOS_TEST(a_referrer_needs_its_invitation_first) {
    MockReferralService s;
    const QString p = s.createParticipant().value;
    LOGOS_ASSERT_FALSE(s.prepareRegistration(p, "node-a", MockReferralService::inviterNode()).ok());
    LOGOS_ASSERT_TRUE(s.importInvitation(p, MockReferralService::invitationFor(MockReferralService::inviterNode())).ok());
    LOGOS_ASSERT_TRUE(s.prepareRegistration(p, "node-a", MockReferralService::inviterNode()).ok());
}

LOGOS_TEST(a_node_registers_once) {
    MockReferralService s;
    enrol(s, "node-a");
    const QString second = s.createParticipant().value;
    s.prepareRegistration(second, "node-a", {});
    s.attachNodeSignature(second, QByteArray(64, 'x'));
    LOGOS_ASSERT_EQ(settle(s, s.submitRegister("dup", second), "dup"), QString("rejected"));
}

LOGOS_TEST(registration_needs_the_node_signature) {
    MockReferralService s;
    const QString p = s.createParticipant().value;
    s.prepareRegistration(p, "node-a", {});
    LOGOS_ASSERT_FALSE(s.submitRegister("r", p).ok());
    LOGOS_ASSERT_FALSE(s.attachNodeSignature(p, QByteArray(10, 'x')).ok());
}

LOGOS_TEST(claim_pays_each_active_child_once_per_epoch) {
    MockReferralService s;
    const QString p = enrol(s, "node-a");
    s.advanceEpoch();   // epoch 2: children 0 and 2 active, plus 1 point of credit
    LOGOS_ASSERT_EQ(s.claimable(p).value, QString("3"));
    LOGOS_ASSERT_EQ(claim(s, p), QString());
    LOGOS_ASSERT_EQ(s.participant(p).value.rewardBalance, QString("3"));
    LOGOS_ASSERT_EQ(s.claimable(p).value, QString("0"));
}

// Differs from #896, which pays only the current epoch (see MockReferralService.h).
LOGOS_TEST(epochs_accrue_until_claimed) {
    MockReferralService s;
    const QString p = enrol(s, "node-a");
    claim(s, p);           // takes the children in; epoch 1 has no active set
    s.advanceEpoch();      // epoch 2: children 0, 2 active
    s.advanceEpoch();      // epoch 3: children 1, 2 active
    // 2 + 2 active child-epochs, plus epoch 2's credit.
    LOGOS_ASSERT_EQ(s.claimable(p).value, QString("5"));
    claim(s, p);
    LOGOS_ASSERT_EQ(s.participant(p).value.rewardBalance, QString("5"));
    LOGOS_ASSERT_EQ(s.claimable(p).value, QString("0"));
}

LOGOS_TEST(cash_out_takes_the_whole_balance_and_leaves_a_receipt) {
    MockReferralService s;
    const QString p = enrol(s, "node-a");
    s.advanceEpoch();
    claim(s, p);
    LOGOS_ASSERT_EQ(settle(s, s.cashOut("c", p), "c"), QString());
    LOGOS_ASSERT_EQ(s.participant(p).value.rewardBalance, QString("0"));
    const QList<Receipt> receipts = s.receipts(p).value;
    LOGOS_ASSERT_EQ(receipts.size(), 1);
    LOGOS_ASSERT_EQ(receipts.first().points, QString("3"));
}

LOGOS_TEST(an_unknown_reference_reconciles_as_rejected) {
    MockReferralService s;
    LOGOS_ASSERT_TRUE(s.reconcile("nope").value == Status::Rejected);
}

LOGOS_TEST(mock_node_reports_the_issue_it_was_given) {
    LOGOS_ASSERT_FALSE(MockNodeService().status().nodeId.isEmpty());
    const node::Status notCore = MockNodeService(MockNodeService::issueFromName("not_core")).status();
    LOGOS_ASSERT_TRUE(notCore.issue == node::Issue::NotCore);
    LOGOS_ASSERT_FALSE(notCore.nodeId.isEmpty());   // the key is known whether or not the node is core
    LOGOS_ASSERT_TRUE(MockNodeService::issueFromName("typo") == node::Issue::None);
}

LOGOS_TEST(no_wallet_means_no_identity) {
    MockReferralService s(false);
    LOGOS_ASSERT_FALSE(s.walletOpen().value);
    LOGOS_ASSERT_FALSE(s.createParticipant().ok());
}

LOGOS_TEST(the_identity_is_found_again_by_its_label) {
    MockReferralService s;
    const QString p = s.createParticipant().value;
    LOGOS_ASSERT_EQ(s.resolveLabel("Uplink points account").value, QString());
    LOGOS_ASSERT_TRUE(s.addLabel("Uplink points account", p).ok());
    LOGOS_ASSERT_EQ(s.resolveLabel("Uplink points account").value, p);
    LOGOS_ASSERT_FALSE(s.addLabel("Uplink points account", "other").ok());
}

LOGOS_TEST(the_mock_wallet_starts_behind_and_catches_up) {
    MockReferralService s;
    const qint64 height = s.currentBlockHeight().value;
    LOGOS_ASSERT_LT(s.lastSyncedBlock().value, height);
    s.syncToBlock(100);
    LOGOS_ASSERT_EQ(s.lastSyncedBlock().value, qint64(100));
    s.syncToBlock(height + 1000);   // never past the chain
    LOGOS_ASSERT_LE(s.lastSyncedBlock().value, s.currentBlockHeight().value);
}

// Assumed rule: an epoch pays only if your own node was active in it, and you can't
// collect while your node is inactive. The mock's own node is off every fourth epoch.
LOGOS_TEST(own_node_inactivity_pauses_earning_and_blocks_collecting) {
    MockReferralService s;
    const QString p = enrol(s, "node-a");
    s.advanceEpoch();   // 2: children 0, 2 active, own node active, +1 credit
    s.advanceEpoch();   // 3: children 1, 2 active, own node active
    s.advanceEpoch();   // 4: children 0, 1 active, own node INACTIVE, +1 credit
    LOGOS_ASSERT_EQ(s.claimable(p).value, QString("6"));   // 2 + 2 + 0, plus 2 credits
    LOGOS_ASSERT_EQ(claim(s, p), QString("rejected"));
    s.advanceEpoch();   // 5: children 0, 2 active, own node active again
    LOGOS_ASSERT_EQ(claim(s, p), QString());
    LOGOS_ASSERT_EQ(s.participant(p).value.rewardBalance, QString("8"));
}

LOGOS_TEST(an_invitation_code_round_trips) {
    const QString blob = MockReferralService::invitationFor(MockReferralService::inviterNode());
    const QString code = invitation_code::encode(blob);
    LOGOS_ASSERT_TRUE(code.startsWith("uplink-invite:"));
    LOGOS_ASSERT_FALSE(code.contains('+') || code.contains('/') || code.contains('='));
    LOGOS_ASSERT_EQ(invitation_code::decode("  " + code + "\n"), blob);
    LOGOS_ASSERT_EQ(invitation_code::inviterNode(blob), MockReferralService::inviterNode());
}

LOGOS_TEST(a_bad_invitation_code_decodes_to_nothing) {
    LOGOS_ASSERT_EQ(invitation_code::decode("hello"), QString());
    LOGOS_ASSERT_EQ(invitation_code::decode("uplink-invite:%%%"), QString());
    LOGOS_ASSERT_EQ(invitation_code::inviterNode("{\"parent_node\":\"abc\",\"npk\":\"x\",\"vpk\":\"y\"}"), QString());
    LOGOS_ASSERT_EQ(invitation_code::inviterNode(invitation_code::decode(invitation_code::encode("{}"))), QString());
}

LOGOS_TEST(activity_window_marks_active_missed_unknown_and_not_yet) {
    ActivityLog log;
    log.record(10, {"me", "a"}, {"me"});
    log.record(12, {"me", "a"}, {"me", "a"});          // 11 published while not running
    log.record(13, {"me", "a", "b"}, {"b"});           // b appears
    const QVariantList me = log.window("me");
    LOGOS_ASSERT_EQ(me.size(), ActivityLog::kWindow);
    LOGOS_ASSERT_EQ(me.value(15).toInt(), int(ActivityLog::Inactive));   // 13
    LOGOS_ASSERT_EQ(me.value(14).toInt(), int(ActivityLog::Active));     // 12
    LOGOS_ASSERT_EQ(me.value(13).toInt(), int(ActivityLog::Unknown));    // 11
    LOGOS_ASSERT_EQ(me.value(12).toInt(), int(ActivityLog::Active));     // 10
    LOGOS_ASSERT_EQ(me.value(0).toInt(), int(ActivityLog::Unknown));     // before recording
    const QVariantList b = log.window("b");
    LOGOS_ASSERT_EQ(b.value(15).toInt(), int(ActivityLog::Active));
    LOGOS_ASSERT_EQ(b.value(14).toInt(), int(ActivityLog::NotYet));
}

LOGOS_TEST(activity_log_keeps_only_the_window_and_survives_a_restart) {
    ActivityLog log;
    for (quint32 e = 1; e <= 40; ++e)
        log.record(e, {"me"}, e % 2 ? QStringList{"me"} : QStringList{});
    LOGOS_ASSERT_FALSE(log.record(40, {"me"}, {"me"}));   // an epoch is recorded once

    QTemporaryDir dir;
    const QString path = dir.filePath("activity.ini");
    {
        QSettings ini(path, QSettings::IniFormat);
        ini.beginGroup("account");
        log.writeTo(ini);
    }
    QSettings ini(path, QSettings::IniFormat);
    ini.beginGroup("account");
    const ActivityLog again = ActivityLog::readFrom(ini);
    LOGOS_ASSERT_EQ(again.lastEpoch(), quint32(40));
    LOGOS_ASSERT_EQ(again.window("me"), log.window("me"));
    LOGOS_ASSERT_EQ(ini.value("marks/me").toString().split(',').size(), ActivityLog::kWindow);
}

LOGOS_TEST(a_receipt_opens_into_a_payout_code) {
    MockReferralService s;
    const QString p = enrol(s, "node-a");
    s.advanceEpoch();
    claim(s, p);
    settle(s, s.cashOut("c", p), "c");
    LOGOS_ASSERT_FALSE(s.opening(p, 1).ok());              // only receipt 0 exists
    const auto o = s.opening(p, 0);
    LOGOS_ASSERT_TRUE(o.ok());
    LOGOS_ASSERT_EQ(o.value.node, QString("node-a"));
    LOGOS_ASSERT_EQ(o.value.blindingFactor.size(), 64);
    const QString code = payout_code::encode(o.value, "ab12");
    LOGOS_ASSERT_TRUE(code.startsWith("uplink-payout-v2:"));
    LOGOS_ASSERT_EQ(payout_code::signature(code), QString("ab12"));
    const referral::Opening back = payout_code::decode(code);
    LOGOS_ASSERT_EQ(back.node, o.value.node);
    LOGOS_ASSERT_EQ(back.blindingFactor, o.value.blindingFactor);
    LOGOS_ASSERT_EQ(back.programAccount, o.value.programAccount);
    LOGOS_ASSERT_EQ(payout_code::decode("nonsense").node, QString());
}

// What the payout side does with a code: recompute the receipt's address from it and
// find the points there.
LOGOS_TEST(a_payout_code_recomputes_to_its_receipt) {
    MockReferralService s;
    const QString p = enrol(s, "node-a");
    for (int round = 0; round < 2; ++round) {
        s.advanceEpoch();
        claim(s, p);
        const QString ref = "cash-" + QString::number(round);
        settle(s, s.cashOut(ref, p), ref);
    }
    const QList<referral::Receipt> receipts = s.receipts(p).value;
    LOGOS_ASSERT_EQ(receipts.size(), 2);
    for (const referral::Receipt& r : receipts) {
        const referral::Opening fromCode = payout_code::decode(payout_code::encode(s.opening(p, r.index).value, "00"));
        LOGOS_ASSERT_EQ(MockReferralService::receiptAddress(fromCode.programAccount, fromCode.node,
                                                            fromCode.blindingFactor), r.account);
    }
    LOGOS_ASSERT_NE(receipts.at(0).account, receipts.at(1).account);   // a fresh factor per receipt
}

// The node signs the opening under the payout domain, so a join signature can't pass for it.
LOGOS_TEST(a_payout_signature_covers_the_opening_and_its_domain) {
    MockNodeService node;
    referral::Opening o;
    o.programAccount = "p"; o.node = "n"; o.blindingFactor = "b";
    const auto sign = [&](const QString& domain) {
        return QString::fromLatin1(node.signWithoutPrompt(domain, payout_code::signedPayload(o)).value.toHex());
    };
    const QString payout = sign("lez-referral/payout");
    LOGOS_ASSERT_EQ(payout.size(), 128);   // 64 bytes
    LOGOS_ASSERT_NE(payout, sign("lez-referral/register"));
    o.blindingFactor = "c";
    LOGOS_ASSERT_NE(payout, sign("lez-referral/payout"));
}
