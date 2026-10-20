// The mock must follow the program's rules (logos-blockchain/uplink, through uplink_module),
// or the UI is built against the wrong ones.

#include <logos_test.h>

#include "Points.h"
#include "mock/MockNodeService.h"
#include "mock/MockReferralService.h"
#include "mock/MockSigning.h"

using namespace referral;

namespace {

int g_reference = 0;
QString nextReference() { return QString::number(++g_reference); }

QString settle(MockReferralService& s, const Result<Status>& submitted, const QString& reference)
{
    if (!submitted.ok())
        return submitted.error;
    return s.status(reference).value == Status::Settled ? QString() : QStringLiteral("rejected");
}

void acceptTerms(MockReferralService& s)
{
    const Terms terms = s.terms().value;
    s.acceptTerms(terms.version, terms.sha256);
}

// What the node app signs for `p`'s enrolment, as Uplink asks for it.
QString enrolmentSignature(MockReferralService& s, const QString& p, const QString& referrer)
{
    return MockNodeService::answerRequest({{"intent", mock_signing::kEnrolIntent},
                                           {"params", QVariantMap{{"program", s.programAccount().value},
                                                                  {"participant_account", p},
                                                                  {"referrer", referrer}}}})
        .value("signature").toString();
}

// Registers a participant for the mock node under `referrer` ("" = own tree).
QString enrol(MockReferralService& s, const QString& referrer = {}, QString* error = nullptr)
{
    acceptTerms(s);
    const QString p = s.createParticipant(nextReference()).value;
    if (!referrer.isEmpty())
        s.importInvitation(p, MockReferralService::invitationFor(referrer));
    const QString reference = nextReference();
    const QString signature = enrolmentSignature(s, p, referrer);
    const QString result = settle(s, s.enrolWithSignature(reference, p, MockNodeService::nodeId(), signature), reference);
    if (error)
        *error = result;
    return p;
}

QString claim(MockReferralService& s, const QString& p)
{
    const QString reference = nextReference();
    return settle(s, s.claim(reference, p), reference);
}

} // namespace

LOGOS_TEST(registration_settles_and_seeds_three_children) {
    MockReferralService s;
    const QString p = enrol(s);
    LOGOS_ASSERT_EQ(s.participant(p).value.node, MockNodeService::nodeId());
    LOGOS_ASSERT_EQ(s.notes(p).value.size(), 3);
}

LOGOS_TEST(joining_needs_the_current_terms_accepted) {
    MockReferralService s;
    const Terms terms = s.terms().value;
    LOGOS_ASSERT_GT(terms.sections.size(), 0);
    LOGOS_ASSERT_EQ(terms.sha256.size(), 64);
    LOGOS_ASSERT_FALSE(s.acceptTerms(terms.version, QString(64, '0')).ok());
    const QString p = s.createParticipant("me").value;
    LOGOS_ASSERT_TRUE(s.enrolWithSignature("r", p, MockNodeService::nodeId(), enrolmentSignature(s, p, {}))
                          .error.contains("terms"));
    LOGOS_ASSERT_TRUE(s.acceptTerms(terms.version, terms.sha256).ok());
    LOGOS_ASSERT_TRUE(s.termsAccepted().value);
}

LOGOS_TEST(a_signature_by_another_node_or_over_other_values_is_refused) {
    MockReferralService s;
    acceptTerms(s);
    const QString p = s.createParticipant("me").value;
    const QString other = s.createParticipant("other").value;
    const QVariantMap values{{"program", s.programAccount().value}, {"participant_account", p}, {"referrer", ""}};
    const QString byAnotherNode = mock_signing::signature(QString(64, 'f'), mock_signing::kEnrolIntent, values);
    LOGOS_ASSERT_TRUE(s.enrolWithSignature("r", p, MockNodeService::nodeId(), byAnotherNode).error.contains("not the node's"));
    LOGOS_ASSERT_TRUE(s.enrolWithSignature("r", p, MockNodeService::nodeId(), enrolmentSignature(s, other, {}))
                          .error.contains("not the node's"));
    LOGOS_ASSERT_TRUE(s.enrolWithSignature("r", p, MockNodeService::nodeId(), enrolmentSignature(s, p, {})).ok());
}

LOGOS_TEST(the_referrer_must_be_registered) {
    MockReferralService s;
    QString error;
    enrol(s, QString(64, 'e'), &error);
    LOGOS_ASSERT_TRUE(error.contains("referrer is not registered"));
    const QString p = enrol(s, MockReferralService::inviterNode(), &error);
    LOGOS_ASSERT_EQ(error, QString());
    LOGOS_ASSERT_EQ(s.participant(p).value.referrer, MockReferralService::inviterNode());
}

LOGOS_TEST(a_node_registers_once) {
    MockReferralService s;
    enrol(s);
    QString error;
    enrol(s, {}, &error);
    LOGOS_ASSERT_TRUE(error.contains("already registered"));
}

LOGOS_TEST(claim_pays_each_active_child_once_per_epoch) {
    MockReferralService s;
    const QString p = enrol(s);
    s.advanceEpoch();   // epoch 2: children 0 and 2 active, plus 1 point of credit
    LOGOS_ASSERT_EQ(s.claimable(p).value.points, QString("3"));
    LOGOS_ASSERT_EQ(claim(s, p), QString());
    LOGOS_ASSERT_EQ(s.participant(p).value.rewardBalance, QString("3"));
    LOGOS_ASSERT_EQ(s.claimable(p).value.points, QString("0"));
    LOGOS_ASSERT_TRUE(s.claim("again", p).error.contains("nothing to claim"));
}

LOGOS_TEST(epochs_accrue_until_claimed) {
    MockReferralService s;
    const QString p = enrol(s);
    s.advanceEpoch();      // epoch 2: children 0, 2 active, +1 credit
    s.advanceEpoch();      // epoch 3: children 1, 2 active
    LOGOS_ASSERT_EQ(s.claimable(p).value.points, QString("5"));
    claim(s, p);
    LOGOS_ASSERT_EQ(s.participant(p).value.rewardBalance, QString("5"));
    LOGOS_ASSERT_EQ(s.claimable(p).value.points, QString("0"));
}

// An epoch pays only if your own node was active in it. The mock's own node is off every fourth epoch.
LOGOS_TEST(an_epoch_your_node_missed_pays_nothing) {
    MockReferralService s;
    const QString p = enrol(s);
    s.advanceEpoch();   // 2: children 0, 2 active, own node active, +1 credit
    s.advanceEpoch();   // 3: children 1, 2 active, own node active
    s.advanceEpoch();   // 4: children 0, 1 active, own node INACTIVE, +1 credit
    LOGOS_ASSERT_EQ(s.claimable(p).value.points, QString("6"));   // 2 + 2 + 0, plus 2 credits
    LOGOS_ASSERT_EQ(claim(s, p), QString());
    LOGOS_ASSERT_EQ(s.participant(p).value.rewardBalance, QString("6"));
}

LOGOS_TEST(auto_claim_claims_when_something_is_claimable_and_waits_while_pending) {
    MockReferralService s;
    const QString p = enrol(s);
    LOGOS_ASSERT_EQ(s.autoClaim(p).value, QString());   // nothing yet
    s.advanceEpoch();
    const QString reference = s.autoClaim(p).value;
    LOGOS_ASSERT_FALSE(reference.isEmpty());
    LOGOS_ASSERT_EQ(s.autoClaim(p).value, QString());   // pending
    LOGOS_ASSERT_TRUE(s.status(reference).value == Status::Settled);
    LOGOS_ASSERT_EQ(s.participant(p).value.rewardBalance, QString("3"));
}

LOGOS_TEST(cash_out_takes_the_whole_balance_with_the_nodes_receipt_secret) {
    MockReferralService s;
    const QString p = enrol(s);
    s.advanceEpoch();
    claim(s, p);
    const QString secret = MockNodeService::answerRequest({{"intent", mock_signing::kReceiptSecretIntent}})
                               .value("receipt_secret").toString();
    LOGOS_ASSERT_TRUE(s.cashOutWithSecret("c0", p, QString(64, '0')).error.contains("not the node's"));
    LOGOS_ASSERT_EQ(settle(s, s.cashOutWithSecret("c", p, secret), "c"), QString());
    LOGOS_ASSERT_EQ(s.participant(p).value.rewardBalance, QString("0"));
    const QList<Receipt> receipts = s.receipts(p).value;
    LOGOS_ASSERT_EQ(receipts.size(), 1);
    LOGOS_ASSERT_EQ(receipts.first().points, QString("3"));
    LOGOS_ASSERT_TRUE(s.cashOutWithSecret("c2", p, secret).error.contains("zero"));
}

LOGOS_TEST(a_payout_code_needs_the_nodes_signature_over_its_opening) {
    MockReferralService s;
    const QString p = enrol(s);
    s.advanceEpoch();
    claim(s, p);
    const QString secret = mock_signing::receiptSecret(MockNodeService::nodeId());
    settle(s, s.cashOutWithSecret("c", p, secret), "c");
    const PayoutMessage m = s.payoutMessage(p, 0).value;
    LOGOS_ASSERT_TRUE(m.message.contains(m.blindingFactor) && m.message.contains(MockNodeService::nodeId()));
    const QString signature = MockNodeService::answerRequest(
        {{"intent", mock_signing::kPayoutIntent},
         {"params", QVariantMap{{"program_account", m.programAccount}, {"blinding_factor", m.blindingFactor}}}})
        .value("signature").toString();
    LOGOS_ASSERT_TRUE(s.payoutCodeWithSignature(p, 0, signature).value.startsWith("uplink-payout-v2:"));
    LOGOS_ASSERT_FALSE(s.payoutCodeWithSignature(p, 0, QString(128, '0')).ok());
    LOGOS_ASSERT_FALSE(s.payoutMessage(p, 1).ok());
}

LOGOS_TEST(an_unknown_reference_has_no_status) {
    MockReferralService s;
    LOGOS_ASSERT_FALSE(s.status("nope").ok());
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
    LOGOS_ASSERT_FALSE(s.createParticipant("Uplink points account").ok());
}

LOGOS_TEST(the_identity_is_found_again_by_its_label) {
    MockReferralService s;
    LOGOS_ASSERT_EQ(s.resolveLabel("Uplink points account").value, QString());
    const QString p = s.createParticipant("Uplink points account").value;
    LOGOS_ASSERT_EQ(s.resolveLabel("Uplink points account").value, p);
    LOGOS_ASSERT_EQ(s.createParticipant("Uplink points account").value, p);   // reused, not a second one
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

LOGOS_TEST(an_invitation_names_its_inviter_and_whether_it_joined) {
    const auto check = MockReferralService().checkInvitation(MockReferralService::invitationFor(MockReferralService::inviterNode()));
    LOGOS_ASSERT_EQ(check.value.referrer, MockReferralService::inviterNode());
    LOGOS_ASSERT_TRUE(check.value.registered);
    LOGOS_ASSERT_TRUE(MockReferralService::invitationFor(MockReferralService::inviterNode()).startsWith("uplink-invite-v1:"));
    LOGOS_ASSERT_FALSE(MockReferralService().checkInvitation(MockReferralService::invitationFor(QString(64, 'e'))).value.registered);
    LOGOS_ASSERT_FALSE(MockReferralService().checkInvitation("uplink-invite-v1:AAAA").ok());
    LOGOS_ASSERT_FALSE(MockReferralService().checkInvitation("hello").ok());
}

LOGOS_TEST(activity_marks_each_node_per_published_epoch) {
    MockReferralService s;
    const QString p = enrol(s);
    s.advanceEpoch();   // 2
    s.advanceEpoch();   // 3
    const QString child = s.notes(p).value.first().node;
    const Activity a = s.activity({MockNodeService::nodeId(), child}, 16).value;
    LOGOS_ASSERT_TRUE(a.epochs == QList<quint32>({2, 3}));
    LOGOS_ASSERT_TRUE(a.nodes.value(MockNodeService::nodeId()) == QList<int>({1, 1}));
    // Child i is active unless (epoch + i) % 3 == 0: [1, 0], [0, 1] or [1, 1] in epochs 2 and 3.
    const QList<int> marks = a.nodes.value(child);
    LOGOS_ASSERT_TRUE(marks == QList<int>({1, 0}) || marks == QList<int>({0, 1}) || marks == QList<int>({1, 1}));
}

// u128 points as decimal strings: past 2^53 a JS Number rounds, past 2^64 toULongLong() reads 0.
LOGOS_TEST(points_add_past_2_to_the_64) {
    LOGOS_ASSERT_EQ(points::add("18446744073709551615", "1"), QString("18446744073709551616"));
    LOGOS_ASSERT_EQ(points::add("9007199254740993", "0"), QString("9007199254740993"));
    LOGOS_ASSERT_EQ(points::add("340282366920938463463374607431768211455", "0"),
                    QString("340282366920938463463374607431768211455"));   // u128 max
    LOGOS_ASSERT_EQ(points::add("999", "1"), QString("1000"));
    LOGOS_ASSERT_EQ(points::add("0", "0"), QString("0"));
}

LOGOS_TEST(points_normalise_and_treat_garbage_as_zero) {
    LOGOS_ASSERT_EQ(points::normalized("007"), QString("7"));
    LOGOS_ASSERT_EQ(points::normalized(" 000 "), QString("0"));
    LOGOS_ASSERT_EQ(points::normalized(""), QString("0"));
    LOGOS_ASSERT_EQ(points::normalized("-5"), QString("0"));
    LOGOS_ASSERT_EQ(points::normalized("1e3"), QString("0"));
    LOGOS_ASSERT_TRUE(points::isZero("000"));
    LOGOS_ASSERT_FALSE(points::isZero("18446744073709551616"));   // 2^64, which toULongLong() reads as 0
}
