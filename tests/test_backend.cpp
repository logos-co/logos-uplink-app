// UplinkBackend's flows, driven as QML drives them, against the mock services. Each test
// checks what the view would see: the backend's properties.

#include <logos_test.h>

#include <QTemporaryDir>
#include <memory>

#include "InvitationCode.h"
#include "PayoutCode.h"
#include "UplinkBackend.h"
#include "mock/MockNodeService.h"
#include "mock/MockReferralService.h"

using namespace referral;
using B = UplinkBackend;

namespace {

// Hands the backend a mock it doesn't own, so a test can restart the backend over the same
// chain and wallet, and reach the mock behind its back.
class SharedReferral : public ReferralService {
public:
    explicit SharedReferral(std::shared_ptr<MockReferralService> m) : m(std::move(m)) {}
    Result<QString> createParticipant() override { return m->createParticipant(); }
    Result<Participant> participant(const QString& p) override { return m->participant(p); }
    Result<QString> invitation(const QString& p, const QString& n) override { return m->invitation(p, n); }
    Result<QString> importInvitation(const QString& p, const QString& b) override { return m->importInvitation(p, b); }
    Result<QByteArray> prepareRegistration(const QString& p, const QString& n, const QString& r) override { return m->prepareRegistration(p, n, r); }
    Result<bool> attachNodeSignature(const QString& p, const QByteArray& s) override { return m->attachNodeSignature(p, s); }
    Result<Registry> registry() override { return m->registry(); }
    Result<QList<Note>> notes(const QString& p) override { return m->notes(p); }
    Result<QString> claimable(const QString& p) override { return m->claimable(p); }
    Result<QList<Receipt>> receipts(const QString& p) override { return m->receipts(p); }
    Result<Opening> opening(const QString& p, quint64 i) override { return m->opening(p, i); }
    Result<Status> submitRegister(const QString& r, const QString& p) override { return m->submitRegister(r, p); }
    Result<Status> submitClaim(const QString& r, const QString& p, const QStringList& n) override { return m->submitClaim(r, p, n); }
    Result<Status> cashOut(const QString& r, const QString& p) override { return m->cashOut(r, p); }
    Result<Status> reconcile(const QString& r) override { return m->reconcile(r); }
    Result<bool> walletOpen() override { return m->walletOpen(); }
    Result<QString> resolveLabel(const QString& l) override { return m->resolveLabel(l); }
    Result<bool> addLabel(const QString& l, const QString& a) override { return m->addLabel(l, a); }
    Result<QString> accountAddress(const QString& a) override { return m->accountAddress(a); }
    Result<qint64> lastSyncedBlock() override { return m->lastSyncedBlock(); }
    Result<qint64> currentBlockHeight() override { return m->currentBlockHeight(); }
    Result<bool> syncToBlock(qint64 b) override { return m->syncToBlock(b); }

private:
    std::shared_ptr<MockReferralService> m;
};

// The mock node, but signing only through the node app (as the real one does) when `prompts`:
// the backend then publishes a signRequest and waits for completeEnroll / completePayoutSignature.
class TestNode : public node::NodeService {
public:
    explicit TestNode(bool prompts, node::Issue issue = node::Issue::None) : m_prompts(prompts), m_mock(issue) {}
    node::Status status() override { return m_mock.status(); }
    Result<QByteArray> signWithoutPrompt(const QString& domain, const QByteArray& payload) override
    {
        if (m_prompts)
            return Result<QByteArray>::failure(QStringLiteral("the node app signs"));
        return m_mock.signWithoutPrompt(domain, payload);
    }
    // What the node app answers for a signRequest.
    QString signHex(const QVariantMap& request)
    {
        const QByteArray payload = QByteArray::fromHex(request.value("payload_hex").toString().toLatin1());
        return QString::fromLatin1(m_mock.signWithoutPrompt(request.value("domain").toString(), payload).value.toHex());
    }

private:
    bool m_prompts;
    MockNodeService m_mock;
};

struct Harness {
    explicit Harness(bool nodePrompts = false, node::Issue issue = node::Issue::None)
        : lez(std::make_shared<MockReferralService>())
    {
        start(nodePrompts, issue);
    }

    // A new backend over the same chain and wallet: Uplink closed and opened again.
    void start(bool nodePrompts = false, node::Issue issue = node::Issue::None)
    {
        backend.reset();
        auto n = std::make_unique<TestNode>(nodePrompts, issue);
        node = n.get();
        // Uplink's own file (labels, activity) goes to a fresh place per test.
        backend = std::make_unique<UplinkBackend>(std::make_unique<SharedReferral>(lez), std::move(n), data.path());
        backend->refresh();
    }

    // Joins as the root of a new tree; true once Enrolled.
    bool enrol()
    {
        backend->createIdentity();
        backend->prepareEnroll();
        if (backend->enrolState() == B::AwaitingSignature)
            backend->completeEnroll(payload(), node->signHex(backend->signRequest()), backend->nodeId());
        backend->refresh();   // the registration settles
        return backend->enrolState() == B::Enrolled;
    }

    // Collect then cash out, one refresh (poll) per transaction.
    void cashOut()
    {
        backend->cashOutAll();
        backend->refresh();   // Collect settles, CashOut is submitted
        backend->refresh();   // CashOut settles, the code is prepared
    }

    QString payload() const { return backend->signRequest().value("payload_hex").toString(); }

    QTemporaryDir data;
    std::shared_ptr<MockReferralService> lez;
    TestNode* node = nullptr;
    std::unique_ptr<UplinkBackend> backend;
};

QString strangerNode() { return QString(64, QLatin1Char('e')); }

} // namespace

// ---- Joining ----------------------------------------------------------------------

LOGOS_TEST(backend_joins_as_root_and_shows_its_referrals) {
    Harness h;
    LOGOS_ASSERT_EQ(h.backend->walletIssue(), int(B::WalletReady));
    LOGOS_ASSERT_EQ(h.backend->nodeIssue(), int(B::NoIssue));
    LOGOS_ASSERT_TRUE(h.enrol());
    LOGOS_ASSERT_TRUE(h.backend->invitation().startsWith("uplink-invite:"));
    LOGOS_ASSERT_EQ(h.backend->referrals().size(), 3);   // the mock seeds three
    LOGOS_ASSERT_EQ(h.backend->referrerNode(), QString());
    LOGOS_ASSERT_EQ(h.backend->lastError(), QString());
}

LOGOS_TEST(backend_joins_under_an_invitation) {
    Harness h;
    const QString code = invitation_code::encode(MockReferralService::invitationFor(MockReferralService::inviterNode()));
    h.backend->checkInvitation(code);
    LOGOS_ASSERT_EQ(h.backend->invitationCheck(), int(B::InvitationOk));
    h.backend->createIdentity();
    h.backend->joinUnder(code);
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::Invited));
    h.backend->prepareEnroll();
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::Enrolled));
    LOGOS_ASSERT_EQ(h.backend->referrerNode(), MockReferralService::inviterNode());
}

LOGOS_TEST(backend_checks_an_invitation_before_joining) {
    Harness h;
    h.backend->checkInvitation("  ");
    LOGOS_ASSERT_EQ(h.backend->invitationCheck(), int(B::InvitationEmpty));
    h.backend->checkInvitation("hello");
    LOGOS_ASSERT_EQ(h.backend->invitationCheck(), int(B::InvitationInvalid));
    h.backend->checkInvitation(invitation_code::encode(MockReferralService::invitationFor(h.backend->nodeId())));
    LOGOS_ASSERT_EQ(h.backend->invitationCheck(), int(B::InvitationOwn));
    h.backend->checkInvitation(invitation_code::encode(MockReferralService::invitationFor(strangerNode())));
    LOGOS_ASSERT_EQ(h.backend->invitationCheck(), int(B::InviterNotJoined));
}

LOGOS_TEST(backend_needs_a_running_node_to_join) {
    Harness h(false, node::Issue::NotRunning);
    LOGOS_ASSERT_EQ(h.backend->nodeIssue(), int(B::NodeNotRunning));
    h.backend->createIdentity();
    h.backend->prepareEnroll();
    LOGOS_ASSERT_TRUE(h.backend->lastError().contains("needs to be running"));
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::IdentityCreated));
}

LOGOS_TEST(backend_reports_a_node_that_is_not_core) {
    Harness h(false, node::Issue::NotCore);
    LOGOS_ASSERT_EQ(h.backend->nodeIssue(), int(B::NotCoreNode));
    LOGOS_ASSERT_FALSE(h.backend->nodeCore());
    LOGOS_ASSERT_TRUE(h.enrol());   // joining has no node prerequisite beyond signing
}

LOGOS_TEST(backend_finds_its_identity_again_after_a_restart) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    const QString participant = h.backend->participantId();
    h.start();
    LOGOS_ASSERT_EQ(h.backend->participantId(), participant);
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::Enrolled));
}

// ---- The node's signature -----------------------------------------------------------

LOGOS_TEST(backend_ignores_a_signature_for_another_request) {
    Harness h(true);
    h.backend->createIdentity();
    h.backend->prepareEnroll();
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::AwaitingSignature));
    const QString signature = h.node->signHex(h.backend->signRequest());
    h.backend->completeEnroll("00ff", signature, h.backend->nodeId());
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::AwaitingSignature));
    LOGOS_ASSERT_EQ(h.backend->lastError(), QString());
    h.backend->completeEnroll(h.payload(), signature, h.backend->nodeId());
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::Enrolling));
}

LOGOS_TEST(backend_ignores_a_failure_for_another_request) {
    Harness h(true);
    h.backend->createIdentity();
    h.backend->prepareEnroll();
    h.backend->reportSignFailed("00ff", "cancelled");
    LOGOS_ASSERT_EQ(h.backend->lastError(), QString());
    h.backend->reportSignFailed(h.payload(), "cancelled");
    LOGOS_ASSERT_EQ(h.backend->lastError(), QString("Signing was cancelled."));
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::AwaitingSignature));   // can sign again
}

LOGOS_TEST(backend_refuses_a_malformed_signature) {
    Harness h(true);
    h.backend->createIdentity();
    h.backend->prepareEnroll();
    for (const QString& bad : {QString("abcd"), QString(128, QLatin1Char('z')), QString(130, QLatin1Char('a'))}) {
        h.backend->completeEnroll(h.payload(), bad, h.backend->nodeId());
        LOGOS_ASSERT_TRUE(h.backend->lastError().contains("malformed"));
        LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::AwaitingSignature));
    }
}

LOGOS_TEST(backend_refuses_a_signature_from_another_key) {
    Harness h(true);
    h.backend->createIdentity();
    h.backend->prepareEnroll();
    h.backend->completeEnroll(h.payload(), h.node->signHex(h.backend->signRequest()), strangerNode());
    LOGOS_ASSERT_TRUE(h.backend->lastError().contains("different node key"));
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::AwaitingSignature));
}

// ---- The wallet ------------------------------------------------------------------------

LOGOS_TEST(backend_forgets_the_identity_when_the_wallet_closes) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    h.lez->setWalletOpen(false);
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->walletIssue(), int(B::NoWalletOpen));
    LOGOS_ASSERT_EQ(h.backend->participantId(), QString());
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::NoIdentity));
    LOGOS_ASSERT_EQ(h.backend->referrals().size(), 0);
    LOGOS_ASSERT_EQ(h.backend->invitation(), QString());
    h.lez->setWalletOpen(true);
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::Enrolled));   // found again by its label
}

LOGOS_TEST(backend_drops_the_old_account_when_the_wallet_is_switched) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    const QString first = h.backend->participantId();
    h.lez->switchWallet();
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->participantId(), QString());
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::NoIdentity));
    h.backend->createIdentity();
    LOGOS_ASSERT_NE(h.backend->participantId(), first);
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::IdentityCreated));   // the new one sticks
}

// ---- Cashing out ---------------------------------------------------------------------------

LOGOS_TEST(backend_cashes_out_into_a_payout_code) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    h.lez->advanceEpoch();   // epoch 2: own node and two children active, plus a credit
    h.backend->refresh();
    LOGOS_ASSERT_TRUE(h.backend->nodeActive());
    LOGOS_ASSERT_EQ(h.backend->claimablePoints(), QString("3"));
    LOGOS_ASSERT_EQ(h.backend->totalPoints(), QString("3"));

    h.backend->cashOutAll();
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::Collecting));
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashingOut));
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashOutDone));
    LOGOS_ASSERT_EQ(h.backend->payoutPoints(), QString("3"));
    const QString code = h.backend->payoutCode();
    LOGOS_ASSERT_TRUE(code.startsWith(payout_code::kPrefix));
    LOGOS_ASSERT_EQ(payout_code::decode(code).node, h.backend->nodeId());
    LOGOS_ASSERT_EQ(payout_code::signature(code).size(), 128);

    h.backend->finishCashOut();
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashOutIdle));
    LOGOS_ASSERT_EQ(h.backend->totalPoints(), QString("0"));
    LOGOS_ASSERT_EQ(h.backend->cashedOutPoints(), QString("3"));
    LOGOS_ASSERT_EQ(h.backend->receipts().size(), 1);

    h.backend->cashOutAll();   // the same epoch: nothing new
    LOGOS_ASSERT_EQ(h.backend->lastError(), QString("No points to cash out yet."));
}

LOGOS_TEST(backend_wont_collect_while_its_node_is_inactive) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    for (int i = 0; i < 3; ++i)
        h.lez->advanceEpoch();   // epoch 4: the mock's own node is off
    h.backend->refresh();
    LOGOS_ASSERT_FALSE(h.backend->nodeActive());
    LOGOS_ASSERT_NE(h.backend->totalPoints(), QString("0"));
    h.backend->cashOutAll();
    LOGOS_ASSERT_TRUE(h.backend->lastError().contains("isn't an active"));
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashOutIdle));
}

// Another cash-out for the same account lands while ours is pending, so two receipts are new:
// the backend must not guess which is ours.
LOGOS_TEST(backend_does_not_guess_between_new_receipts) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    h.lez->advanceEpoch();
    h.backend->refresh();
    h.backend->cashOutAll();
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashingOut));
    const QString participant = h.backend->participantId();
    h.lez->cashOut("elsewhere", participant);
    h.lez->reconcile("elsewhere");
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashOutIdle));
    LOGOS_ASSERT_TRUE(h.backend->lastError().contains("couldn't tell which receipt"));
    LOGOS_ASSERT_EQ(h.backend->payoutCode(), QString());
    LOGOS_ASSERT_EQ(h.backend->receipts().size(), 2);
}

LOGOS_TEST(backend_codes_the_receipt_its_cash_out_made) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    h.lez->advanceEpoch();
    h.backend->refresh();
    h.cashOut();
    h.backend->finishCashOut();
    h.lez->advanceEpoch();
    h.lez->advanceEpoch();   // epoch 4: own node off; 5: on again
    h.lez->advanceEpoch();
    h.backend->refresh();
    h.cashOut();
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashOutDone));
    const auto opening = h.lez->opening(h.backend->participantId(), 1);
    LOGOS_ASSERT_EQ(payout_code::decode(h.backend->payoutCode()).blindingFactor, opening.value.blindingFactor);
}

// A restart (or a cancelled signature) between cashing out and copying the code.
LOGOS_TEST(backend_gets_a_past_receipts_code_again_after_a_restart) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    h.lez->advanceEpoch();
    h.backend->refresh();
    h.cashOut();
    const QString code = h.backend->payoutCode();
    h.start();
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashOutIdle));
    LOGOS_ASSERT_EQ(h.backend->receipts().size(), 1);
    h.backend->payoutCodeFor(0);
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashOutDone));
    LOGOS_ASSERT_EQ(h.backend->payoutCode(), code);
    LOGOS_ASSERT_EQ(h.backend->payoutPoints(), QString("3"));
}

LOGOS_TEST(backend_reports_a_receipt_that_does_not_exist) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    h.backend->payoutCodeFor(4);
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashOutIdle));
    LOGOS_ASSERT_TRUE(h.backend->lastError().contains("wasn't found"));
}

LOGOS_TEST(backend_payout_signing_waits_for_the_current_request) {
    Harness h(true);
    LOGOS_ASSERT_TRUE(h.enrol());
    h.lez->advanceEpoch();
    h.backend->refresh();
    h.cashOut();
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::SigningCode));
    const QString signature = h.node->signHex(h.backend->signRequest());
    h.backend->reportPayoutSignFailed(h.payload(), "cancelled");
    LOGOS_ASSERT_TRUE(h.backend->lastError().contains("sign again"));
    h.backend->completePayoutSignature("00ff", signature, h.backend->nodeId());
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::SigningCode));
    h.backend->completePayoutSignature(h.payload(), signature, h.backend->nodeId());
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashOutDone));
    LOGOS_ASSERT_EQ(payout_code::signature(h.backend->payoutCode()), signature);
}

// ---- Local labels ----------------------------------------------------------------------

namespace {

QString labelOf(const UplinkBackend& backend, const QString& node)
{
    for (const QVariant& r : backend.referrals())
        if (r.toMap().value("node").toString() == node)
            return r.toMap().value("label").toString();
    return {};
}

} // namespace

LOGOS_TEST(backend_keeps_referral_labels_across_a_restart) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    const QString node = h.backend->referrals().first().toMap().value("node").toString();
    h.backend->setReferralLabel(node, "  Alice ");
    LOGOS_ASSERT_EQ(labelOf(*h.backend, node), QString("Alice"));
    h.start();
    LOGOS_ASSERT_EQ(labelOf(*h.backend, node), QString("Alice"));
    h.backend->setReferralLabel(node, "");
    LOGOS_ASSERT_EQ(labelOf(*h.backend, node), QString());
}
