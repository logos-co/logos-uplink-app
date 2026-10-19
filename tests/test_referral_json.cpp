// uplink_module's replies, as its glue writes them, read by the real service.

#include <logos_test.h>

#include "lez/ReferralJson.h"

using namespace referral;

LOGOS_TEST(an_error_reply_carries_the_modules_message) {
    const auto r = referral_json::reply(R"({"ok":false,"code":"keystore_mismatch","error":"this keystore has no key for node ab"})");
    LOGOS_ASSERT_FALSE(r.ok());
    LOGOS_ASSERT_EQ(r.error, QString("this keystore has no key for node ab"));
    LOGOS_ASSERT_FALSE(referral_json::reply({}).ok());
}

LOGOS_TEST(the_program_check_names_what_is_wrong_with_the_deployment) {
    LOGOS_ASSERT_TRUE(referral_json::programCheck(R"({"ok":true,"programAccount":"aa","imageId":"bb"})").value.state
                      == ProgramCheck::Ready);
    const auto mismatch = referral_json::programCheck(R"({"ok":false,"code":"program_mismatch","error":"the program at X is Y"})");
    LOGOS_ASSERT_TRUE(mismatch.ok());
    LOGOS_ASSERT_TRUE(mismatch.value.state == ProgramCheck::Mismatch);
    LOGOS_ASSERT_EQ(mismatch.value.detail, QString("the program at X is Y"));
    LOGOS_ASSERT_TRUE(referral_json::programCheck(R"({"ok":false,"code":"program_not_deployed","error":"e"})").value.state
                      == ProgramCheck::NotDeployed);
    LOGOS_ASSERT_TRUE(referral_json::programCheck(R"({"ok":false,"code":"not_configured","error":"e"})").value.state
                      == ProgramCheck::NotConfigured);
    LOGOS_ASSERT_EQ(referral_json::programCheck(R"({"ok":false,"code":"lez_core","error":"no wallet"})").error, QString("no wallet"));
}

LOGOS_TEST(a_participant_shows_the_last_epoch_settled_per_child) {
    const auto p = referral_json::participant(
        R"({"ok":true,"registered":true,"node":"aa","referrer":"bb","rewardBalance":"340282366920938463463374607431768211455",)"
        R"("children":{"cc":[[0,3],[5,7]],"dd":[]}})");
    LOGOS_ASSERT_TRUE(p.ok());
    LOGOS_ASSERT_EQ(p.value.node, QString("aa"));
    LOGOS_ASSERT_EQ(p.value.referrer, QString("bb"));
    LOGOS_ASSERT_EQ(p.value.rewardBalance, QString("340282366920938463463374607431768211455"));
    LOGOS_ASSERT_EQ(p.value.children.value("cc"), quint32(7));
    LOGOS_ASSERT_EQ(p.value.children.value("dd"), quint32(0));
    LOGOS_ASSERT_TRUE(referral_json::participant(R"({"ok":true,"registered":false})").error.contains("not registered"));
    const auto joined = referral_json::participant(R"({"ok":true,"registered":true,"node":"aa","children":{"cc":[[0,5]]},"childrenSince":{"cc":4}})");
    LOGOS_ASSERT_EQ(joined.value.since.value("cc"), quint32(4));
}

LOGOS_TEST(operations_map_to_pending_settled_or_their_error) {
    LOGOS_ASSERT_TRUE(referral_json::operationStatus(R"({"ok":true,"status":"submitting"})").value == Status::Pending);
    LOGOS_ASSERT_TRUE(referral_json::operationStatus(R"({"ok":true,"status":"pending","txHash":"t"})").value == Status::Pending);
    LOGOS_ASSERT_TRUE(referral_json::operationStatus(R"({"ok":true,"status":"settled"})").value == Status::Settled);
    const auto failed = referral_json::operationStatus(R"({"ok":true,"status":"failed","error":"AccountNotFound"})");
    LOGOS_ASSERT_EQ(failed.error, QString("AccountNotFound"));
}

LOGOS_TEST(auto_claim_answers_a_reference_or_nothing) {
    LOGOS_ASSERT_EQ(referral_json::autoClaimReference(R"({"ok":true,"claimed":{"reference":"ab","status":"pending"}})").value,
                    QString("ab"));
    LOGOS_ASSERT_EQ(referral_json::autoClaimReference(R"({"ok":true,"skipped":"nothing_to_claim"})").value, QString());
}

LOGOS_TEST(registry_activity_notes_receipts_and_terms_read_through) {
    const auto registry = referral_json::registry(R"({"ok":true,"epoch":4,"nodes":["a","b"],"active":["a"]})");
    LOGOS_ASSERT_EQ(registry.value.epoch, quint32(4));
    LOGOS_ASSERT_TRUE(registry.value.nodes == QStringList({"a", "b"}));

    const auto activity = referral_json::activity(R"({"ok":true,"epochs":[2,3,4],"nodes":{"a":[-1,1,0]}})");
    LOGOS_ASSERT_TRUE(activity.value.epochs == QList<quint32>({2, 3, 4}));
    LOGOS_ASSERT_TRUE(activity.value.nodes.value("a") == QList<int>({-1, 1, 0}));

    const auto notes = referral_json::notes(
        R"({"ok":true,"notes":[{"account":"n1","kind":"child","node":"c","since":2},{"account":"n2","kind":"credit","amount":"4","epoch":1}]})");
    LOGOS_ASSERT_EQ(notes.value.size(), 2);
    LOGOS_ASSERT_TRUE(notes.value.at(0).kind == Note::Child);
    LOGOS_ASSERT_EQ(notes.value.at(0).node, QString("c"));
    LOGOS_ASSERT_EQ(notes.value.at(0).since, quint32(2));
    LOGOS_ASSERT_TRUE(notes.value.at(1).kind == Note::Credit);
    LOGOS_ASSERT_EQ(notes.value.at(1).amount, QString("4"));

    const auto receipts = referral_json::receipts(R"({"ok":true,"receipts":[{"index":0,"account":"r","points":"9"}]})");
    LOGOS_ASSERT_EQ(receipts.value.first().points, QString("9"));

    const auto terms = referral_json::terms(
        R"({"ok":true,"version":1,"sha256":"ab","lastUpdated":"2026-09-04","sections":[{"title":"1. About","items":[{"t":"x"}]}]})");
    LOGOS_ASSERT_EQ(terms.value.version, 1);
    LOGOS_ASSERT_EQ(terms.value.sections.size(), 1);

    const auto check = referral_json::invitationCheck(R"({"ok":true,"referrer":"aa","registered":true})");
    LOGOS_ASSERT_TRUE(check.value.registered);
    LOGOS_ASSERT_EQ(referral_json::field(R"({"ok":true,"participant":"p1","created":true})", "participant").value, QString("p1"));
}
