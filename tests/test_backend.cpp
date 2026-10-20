// UplinkBackend's flows, driven as QML drives them, against the mock services. Each test
// checks what the view would see: the backend's properties.

#include <logos_test.h>

#include <QCoreApplication>
#include <QTemporaryDir>
#include <memory>

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
    Result<QString> programAccount() override { return m->programAccount(); }
    Result<bool> configure(const QString& p) override { return m->configure(p); }
    Result<ProgramCheck> checkProgram() override { return m->checkProgram(); }
    Result<QString> createParticipant(const QString& l) override { return m->createParticipant(l); }
    Result<Participant> participant(const QString& p) override { return m->participant(p); }
    Result<QString> invitation(const QString& p, const QString& n) override { return m->invitation(p, n); }
    Result<InvitationCheck> checkInvitation(const QString& b) override { return m->checkInvitation(b); }
    Result<QString> importInvitation(const QString& p, const QString& b) override { return m->importInvitation(p, b); }
    Result<Registry> registry() override { return m->registry(); }
    Result<Activity> activity(const QStringList& n, int c) override { return m->activity(n, c); }
    Result<QList<Note>> notes(const QString& p) override { return m->notes(p); }
    Result<QString> claimable(const QString& p) override { return m->claimable(p); }
    Result<Terms> terms() override { return m->terms(); }
    Result<bool> termsAccepted() override { return m->termsAccepted(); }
    Result<bool> acceptTerms(int v, const QString& h) override { return m->acceptTerms(v, h); }
    Result<Status> enrolWithSignature(const QString& r, const QString& p, const QString& n, const QString& sig) override { return m->enrolWithSignature(r, p, n, sig); }
    Result<Status> claim(const QString& r, const QString& p) override { return m->claim(r, p); }
    Result<QString> autoClaim(const QString& p) override { return m->autoClaim(p); }
    Result<Status> cashOutWithSecret(const QString& r, const QString& p, const QString& sec) override { return m->cashOutWithSecret(r, p, sec); }
    Result<QList<Receipt>> receipts(const QString& p) override { return m->receipts(p); }
    Result<PayoutMessage> payoutMessage(const QString& p, quint64 i) override { return m->payoutMessage(p, i); }
    Result<QString> payoutCodeWithSignature(const QString& p, quint64 i, const QString& sig) override { return m->payoutCodeWithSignature(p, i, sig); }
    Result<Status> status(const QString& r) override { return m->status(r); }
    Result<bool> walletOpen() override { return m->walletOpen(); }
    bool walletReachable() override { return reachable; }
    Result<QString> resolveLabel(const QString& l) override { return m->resolveLabel(l); }
    Result<QStringList> privateAccounts() override { return m->privateAccounts(); }
    Result<bool> labelAccount(const QString& l, const QString& a) override { return m->labelAccount(l, a); }
    Result<QString> accountAddress(const QString& a) override { return m->accountAddress(a); }
    Result<qint64> lastSyncedBlock() override { return m->lastSyncedBlock(); }
    Result<qint64> currentBlockHeight() override { return m->currentBlockHeight(); }
    Result<bool> syncToBlock(qint64 b) override { return m->syncToBlock(b); }

    // With holdSends, a registration stays in flight (proving) until finishSend().
    void enrolWithSignatureAsync(const QString& r, const QString& p, const QString& n, const QString& sig,
                                 Done<Status> done) override
    {
        if (!holdSends)
            return ReferralService::enrolWithSignatureAsync(r, p, n, sig, std::move(done));
        held = [this, r, p, n, sig, done = std::move(done)](bool sent) {
            done(sent ? m->enrolWithSignature(r, p, n, sig) : Result<Status>::failure("lez_core is not connected"));
        };
    }
    void finishSend(bool sent = true)
    {
        auto send = std::move(held);
        held = nullptr;
        send(sent);
    }

    bool reachable = true;
    bool holdSends = false;
    std::function<void(bool sent)> held;

private:
    std::shared_ptr<MockReferralService> m;
};

// Whether the mock node app answers at once (as in mock mode) or the test plays the node app,
// answering backend->signRequest() itself (as QML does with the real one).
enum class NodeApp { Answers, Manual };

struct Harness {
    explicit Harness(node::Issue issue = node::Issue::None, NodeApp app = NodeApp::Answers)
        : lez(std::make_shared<MockReferralService>())
        , app(app)
    {
        start(issue);
    }

    // A new backend over the same chain and wallet: Uplink closed and opened again.
    // Uplink's own file (labels, settings) goes to a fresh place per test.
    void start(node::Issue issue = node::Issue::None)
    {
        backend.reset();
        UplinkBackend::NodeApp nodeApp;
        if (app == NodeApp::Answers)
            nodeApp = &MockNodeService::answerRequest;
        auto shared = std::make_unique<SharedReferral>(lez);
        referral = shared.get();
        backend = std::make_unique<UplinkBackend>(std::move(shared),
                                                  std::make_unique<MockNodeService>(issue), data.path(),
                                                  std::move(nodeApp));
        backend->refresh();
    }

    // The node app approves what the backend asks: the user clicked Sign (or Share).
    void approve()
    {
        const QVariantMap request = backend->signRequest();
        backend->completeSignRequest(request.value("ref").toString(), MockNodeService::answerRequest(request));
    }

    // Terms, account, join as the root of a new tree; true once Enrolled.
    bool enrol()
    {
        backend->acceptTerms();
        backend->createIdentity();
        backend->enrol();
        backend->refresh();   // the registration settles
        return backend->enrolState() == B::Enrolled;
    }

    // Lets the wallet sync catch up (it runs in chunks on the event loop), then polls.
    void sync()
    {
        backend->refresh();
        for (int i = 0; i < 10000 && backend->syncedBlock() < backend->chainHeight(); ++i)
            QCoreApplication::processEvents();
        QCoreApplication::processEvents();   // the last chunk ends the sync on the next pass
        backend->refresh();
    }

    // Collect then cash out, one refresh (poll) per transaction.
    void cashOut()
    {
        backend->cashOutAll();
        backend->refresh();   // the claim settles, CashOut is submitted
        backend->refresh();   // CashOut settles, the code is made
    }

    QTemporaryDir data;
    std::shared_ptr<MockReferralService> lez;
    SharedReferral* referral = nullptr;   // owned by backend
    NodeApp app;
    std::unique_ptr<UplinkBackend> backend;
};

} // namespace

// ---- Deployment -------------------------------------------------------------------

LOGOS_TEST(backend_selects_the_releases_deployment) {
    Harness h;
    h.lez->configure({});
    h.backend->setReleaseDeployment(QStringLiteral("mock-testnet"), MockReferralService::deployedProgram());
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.lez->programAccount().value, MockReferralService::deployedProgram());
    LOGOS_ASSERT_EQ(h.backend->programIssue(), int(B::ProgramReady));
    LOGOS_ASSERT_TRUE(h.enrol());
}

LOGOS_TEST(backend_leaves_the_modules_choice_when_the_release_names_none) {
    Harness h;   // the mock has its program selected, like REFERRAL_PROGRAM_ACCOUNT
    LOGOS_ASSERT_EQ(h.backend->programIssue(), int(B::ProgramReady));
    h.lez->configure({});
    h.backend->setReleaseDeployment({}, {});
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->programIssue(), int(B::ProgramNotConfigured));
}

LOGOS_TEST(backend_refuses_a_release_for_another_network) {
    Harness h;
    h.lez->configure({});
    h.backend->setReleaseDeployment(QStringLiteral("logos-devnet"), MockReferralService::deployedProgram());
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->programIssue(), int(B::ProgramOtherNetwork));
    LOGOS_ASSERT_TRUE(h.backend->programIssueDetail().contains("logos-devnet"));
    LOGOS_ASSERT_EQ(h.lez->programAccount().value, QString());   // not selected on the wrong chain
}

LOGOS_TEST(backend_reports_a_missing_or_outdated_program) {
    Harness h;
    h.backend->setReleaseDeployment({}, QString(64, QLatin1Char('e')));
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->programIssue(), int(B::ProgramNotDeployed));

    h.backend->setReleaseDeployment({}, MockReferralService::outdatedProgram());
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->programIssue(), int(B::ProgramMismatch));
}

LOGOS_TEST(backend_checks_the_program_once_the_wallet_is_open) {
    Harness h;
    h.lez->setWalletOpen(false);
    h.backend->setReleaseDeployment({}, MockReferralService::deployedProgram());
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->programIssue(), int(B::ProgramUnknown));
    h.lez->setWalletOpen(true);
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->programIssue(), int(B::ProgramReady));
}

// ---- Joining ----------------------------------------------------------------------

LOGOS_TEST(backend_joins_as_root_and_shows_its_referrals) {
    Harness h;
    LOGOS_ASSERT_EQ(h.backend->walletIssue(), int(B::WalletReady));
    LOGOS_ASSERT_EQ(h.backend->nodeIssue(), int(B::NoIssue));
    LOGOS_ASSERT_TRUE(h.enrol());
    LOGOS_ASSERT_TRUE(h.backend->invitation().startsWith("uplink-invite-v1:"));
    LOGOS_ASSERT_EQ(h.backend->referrals().size(), 3);   // the mock seeds three
    LOGOS_ASSERT_EQ(h.backend->referrerNode(), QString());
    LOGOS_ASSERT_EQ(h.backend->lastError(), QString());
}

LOGOS_TEST(backend_joins_under_an_invitation) {
    Harness h;
    const QString code = MockReferralService::invitationFor(MockReferralService::inviterNode());
    h.backend->checkInvitation(code);
    LOGOS_ASSERT_EQ(h.backend->invitationCheck(), int(B::InvitationOk));
    LOGOS_ASSERT_EQ(h.backend->invitationInviter(), MockReferralService::inviterNode());
    h.backend->acceptTerms();
    h.backend->createIdentity();
    h.backend->joinUnder(code);
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::Invited));
    h.backend->enrol();
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::EnrolSent));
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::Enrolled));
    LOGOS_ASSERT_EQ(h.backend->referrerNode(), MockReferralService::inviterNode());
}

LOGOS_TEST(backend_checks_an_invitation_before_joining) {
    Harness h;
    h.backend->checkInvitation("");
    LOGOS_ASSERT_EQ(h.backend->invitationCheck(), int(B::InvitationEmpty));
    h.backend->checkInvitation("not a code");
    LOGOS_ASSERT_EQ(h.backend->invitationCheck(), int(B::InvitationInvalid));
    h.backend->checkInvitation(MockReferralService::invitationFor(QString(64, 'e')));
    LOGOS_ASSERT_EQ(h.backend->invitationCheck(), int(B::InviterNotJoined));
    h.backend->checkInvitation(MockReferralService::invitationFor(h.backend->nodeId()));
    LOGOS_ASSERT_EQ(h.backend->invitationCheck(), int(B::InvitationOwn));
}

LOGOS_TEST(backend_needs_a_running_node_to_join) {
    Harness h(node::Issue::NotRunning);
    LOGOS_ASSERT_FALSE(h.enrol());
    LOGOS_ASSERT_TRUE(h.backend->lastError().contains("running"));
}

LOGOS_TEST(backend_reports_a_node_that_is_not_core) {
    Harness h(node::Issue::NotCore);
    LOGOS_ASSERT_EQ(h.backend->nodeIssue(), int(B::NotCoreNode));
    LOGOS_ASSERT_TRUE(h.enrol());   // joining has no Blend prerequisite
}

LOGOS_TEST(backend_needs_the_terms_accepted_to_join) {
    Harness h;
    LOGOS_ASSERT_GT(h.backend->termsSections().size(), 0);
    LOGOS_ASSERT_EQ(h.backend->termsSha256().size(), 64);
    LOGOS_ASSERT_FALSE(h.backend->termsAccepted());
    h.backend->createIdentity();
    h.backend->enrol();
    LOGOS_ASSERT_TRUE(h.backend->lastError().contains("terms"));
    h.backend->acceptTerms();
    LOGOS_ASSERT_TRUE(h.backend->termsAccepted());
}

// ---- The node app (the node's key) ---------------------------------------------------

LOGOS_TEST(backend_asks_the_node_app_to_sign_the_enrolment) {
    Harness h(node::Issue::None, NodeApp::Manual);
    h.backend->acceptTerms();
    h.backend->createIdentity();
    h.backend->enrol();
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::AwaitingSignature));
    const QVariantMap request = h.backend->signRequest();
    LOGOS_ASSERT_EQ(request.value("intent").toString(), QString("node.sign_referral_enrolment"));
    const QVariantMap params = request.value("params").toMap();
    LOGOS_ASSERT_EQ(params.value("program").toString(), h.lez->programAccount().value);
    LOGOS_ASSERT_EQ(params.value("participant_account").toString(), h.backend->participantId());
    LOGOS_ASSERT_EQ(params.value("referrer").toString(), QString());
    h.approve();
    LOGOS_ASSERT_EQ(h.backend->signRequest().size(), 0);
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::EnrolSent));
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::Enrolled));
}

LOGOS_TEST(backend_reports_each_stage_of_joining) {
    Harness h(node::Issue::None, NodeApp::Manual);
    h.referral->holdSends = true;
    h.backend->acceptTerms();
    h.backend->createIdentity();
    h.backend->enrol();
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::AwaitingSignature));
    h.approve();
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::Enrolling));
    h.backend->refresh();   // still proving
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::Enrolling));
    h.referral->finishSend();
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::EnrolSent));
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::Enrolled));
    LOGOS_ASSERT_EQ(h.backend->lastError(), QString());
}

LOGOS_TEST(backend_gives_up_joining_when_the_wallet_stops_while_proving) {
    Harness h(node::Issue::None, NodeApp::Manual);
    h.referral->holdSends = true;
    h.backend->acceptTerms();
    h.backend->createIdentity();
    h.backend->enrol();
    h.approve();
    h.referral->reachable = false;
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::IdentityCreated));
    LOGOS_ASSERT_TRUE(h.backend->lastError().contains("LEZ wallet stopped"));

    // The module hasn't answered yet, so another attempt would queue behind it.
    h.referral->reachable = true;
    h.backend->enrol();
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::IdentityCreated));
    LOGOS_ASSERT_TRUE(h.backend->lastError().contains("still waiting"));

    // Its late answer changes nothing; then joining works again.
    h.referral->holdSends = false;
    h.referral->finishSend(false);
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::IdentityCreated));
    h.backend->enrol();
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::AwaitingSignature));
}

LOGOS_TEST(backend_goes_back_when_the_node_app_declines) {
    Harness h(node::Issue::None, NodeApp::Manual);
    h.backend->acceptTerms();
    h.backend->createIdentity();
    h.backend->enrol();
    h.backend->reportSignFailed(h.backend->signRequest().value("ref").toString(), "cancelled");
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::IdentityCreated));
    LOGOS_ASSERT_TRUE(h.backend->lastError().contains("declined"));
    LOGOS_ASSERT_EQ(h.backend->signRequest().size(), 0);
    h.backend->enrol();
    h.backend->reportSignFailed(h.backend->signRequest().value("ref").toString(), "unavailable");
    LOGOS_ASSERT_TRUE(h.backend->lastError().contains("Blockchain app"));
}

LOGOS_TEST(backend_ignores_an_answer_for_an_older_request) {
    Harness h(node::Issue::None, NodeApp::Manual);
    h.backend->acceptTerms();
    h.backend->createIdentity();
    h.backend->enrol();
    const QVariantMap request = h.backend->signRequest();
    h.backend->completeSignRequest("an-older-ref", MockNodeService::answerRequest(request));
    h.backend->reportSignFailed("an-older-ref", "cancelled");
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::AwaitingSignature));
    LOGOS_ASSERT_EQ(h.backend->lastError(), QString());
    h.approve();
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::EnrolSent));
}

LOGOS_TEST(backend_refuses_an_answer_for_another_node) {
    Harness h(node::Issue::None, NodeApp::Manual);
    h.backend->acceptTerms();
    h.backend->createIdentity();
    h.backend->enrol();
    const QVariantMap request = h.backend->signRequest();
    QVariantMap answer = MockNodeService::answerRequest(request);
    answer.insert("public_key", QString(64, 'f'));
    h.backend->completeSignRequest(request.value("ref").toString(), answer);
    LOGOS_ASSERT_TRUE(h.backend->lastError().contains("another node"));
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::IdentityCreated));
}

LOGOS_TEST(backend_cashes_out_with_the_node_apps_receipt_key_and_signature) {
    Harness h(node::Issue::None, NodeApp::Manual);
    h.backend->changeAutoClaim(false);
    h.backend->acceptTerms();
    h.backend->createIdentity();
    h.backend->enrol();
    h.approve();
    h.backend->refresh();
    h.lez->advanceEpoch();
    h.backend->refresh();
    h.backend->cashOutAll();
    h.backend->refresh();   // the claim settles: the receipt key is asked for
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::AwaitingReceiptKey));
    LOGOS_ASSERT_EQ(h.backend->signRequest().value("intent").toString(), QString("node.referral_receipt_secret"));
    h.approve();
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashingOut));
    h.backend->refresh();   // the cash-out settles: the code's signature is asked for
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::SigningCode));
    const QVariantMap params = h.backend->signRequest().value("params").toMap();
    const PayoutMessage opening = h.lez->payoutMessage(h.backend->participantId(), 0).value;
    LOGOS_ASSERT_EQ(params.value("program_account").toString(), opening.programAccount);
    LOGOS_ASSERT_EQ(params.value("blinding_factor").toString(), opening.blindingFactor);
    h.approve();
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashOutDone));
    LOGOS_ASSERT_TRUE(h.backend->payoutCode().startsWith("uplink-payout-v2:"));
}

LOGOS_TEST(backend_cancels_a_cash_out_the_node_app_declines) {
    Harness h(node::Issue::None, NodeApp::Manual);
    h.backend->changeAutoClaim(false);
    h.backend->acceptTerms();
    h.backend->createIdentity();
    h.backend->enrol();
    h.approve();
    h.backend->refresh();
    h.lez->advanceEpoch();
    h.backend->refresh();
    h.backend->cashOutAll();
    h.backend->refresh();
    h.backend->reportSignFailed(h.backend->signRequest().value("ref").toString(), "cancelled");
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashOutIdle));
    LOGOS_ASSERT_TRUE(h.backend->lastError().contains("declined"));
    LOGOS_ASSERT_EQ(h.lez->receipts(h.backend->participantId()).value.size(), 0);
}

// ---- Identity across sessions ---------------------------------------------------------

LOGOS_TEST(backend_finds_its_identity_again_after_a_restart) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    const QString participant = h.backend->participantId();
    h.start();
    LOGOS_ASSERT_EQ(h.backend->participantId(), participant);
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::Enrolled));
    LOGOS_ASSERT_FALSE(h.backend->identityAddress().isEmpty());
}

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

LOGOS_TEST(backend_finds_its_points_account_in_a_restored_wallet) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    const QString account = h.backend->participantId();
    h.lez->restoreWallet();
    h.start();   // Uplink opened again, on the restored wallet
    LOGOS_ASSERT_EQ(h.backend->participantId(), QString());
    LOGOS_ASSERT_TRUE(h.backend->nodeRegistered());
    LOGOS_ASSERT_TRUE(h.backend->findingAccount());   // until the wallet has synced
    h.sync();
    LOGOS_ASSERT_EQ(h.backend->participantId(), account);
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::Enrolled));
    LOGOS_ASSERT_FALSE(h.backend->findingAccount());
    LOGOS_ASSERT_EQ(h.lez->resolveLabel("Uplink points account").value, account);   // labelled again
}

LOGOS_TEST(backend_keeps_a_found_account_whose_label_is_taken) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    const QString account = h.backend->participantId();
    h.lez->restoreWallet();
    h.lez->createParticipant("Uplink points account");   // joined again before Uplink could tell
    h.start();
    h.sync();
    LOGOS_ASSERT_EQ(h.backend->participantId(), account);
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::Enrolled));
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->participantId(), account);
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::Enrolled));
}

LOGOS_TEST(backend_says_when_the_node_joined_from_another_wallet) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    h.lez->switchWallet();
    h.sync();
    LOGOS_ASSERT_TRUE(h.backend->nodeRegistered());
    LOGOS_ASSERT_FALSE(h.backend->findingAccount());
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::NoIdentity));
    h.backend->createIdentity();
    h.backend->enrol();
    LOGOS_ASSERT_TRUE(h.backend->lastError().contains("already joined"));
    LOGOS_ASSERT_EQ(h.backend->enrolState(), int(B::IdentityCreated));
}

// ---- Activity -------------------------------------------------------------------------

LOGOS_TEST(backend_shows_activity_from_the_module) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    h.lez->advanceEpoch();
    h.lez->advanceEpoch();
    h.backend->refresh();
    LOGOS_ASSERT_TRUE(h.backend->myActivity() == QVariantList({1, 1}));
    for (const QVariant& r : h.backend->referrals())
        LOGOS_ASSERT_EQ(r.toMap().value("activity").toList().size(), 2);
}

LOGOS_TEST(backend_shows_when_each_referral_joined) {
    Harness h;
    h.lez->advanceEpoch();
    h.lez->advanceEpoch();   // epochs 2 and 3 published before joining
    LOGOS_ASSERT_TRUE(h.enrol());   // the referrals the mock seeds join in epoch 3
    h.lez->advanceEpoch();
    h.lez->advanceEpoch();
    h.backend->refresh();
    LOGOS_ASSERT_GT(h.backend->referrals().size(), 0);
    for (const QVariant& r : h.backend->referrals()) {
        const QVariantMap referral = r.toMap();
        LOGOS_ASSERT_EQ(referral.value("joinedEpoch").toInt(), 3);
        LOGOS_ASSERT_EQ(referral.value("joinedAgo").toInt(), 2);
        const QVariantList activity = referral.value("activity").toList();   // epochs 2..5
        LOGOS_ASSERT_EQ(activity.size(), 4);
        LOGOS_ASSERT_EQ(activity.first().toInt(), -2);   // before it joined
        LOGOS_ASSERT_NE(activity.at(1).toInt(), -2);
    }
    // Your own node isn't a referral: no epoch of it is "before joining".
    LOGOS_ASSERT_FALSE(h.backend->myActivity().contains(-2));
}

// ---- Auto-claim ---------------------------------------------------------------------------

LOGOS_TEST(backend_claims_automatically_by_default) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.backend->autoClaim());
    LOGOS_ASSERT_TRUE(h.enrol());
    h.lez->advanceEpoch();
    h.backend->refresh();   // claimable 3: the claim is submitted
    LOGOS_ASSERT_EQ(h.backend->operations().size(), 2);   // register + claim
    h.backend->refresh();   // it settles
    LOGOS_ASSERT_EQ(h.backend->rewardBalance(), QString("3"));
    LOGOS_ASSERT_EQ(h.backend->claimablePoints(), QString("0"));
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->operations().size(), 2);   // nothing more to claim
}

LOGOS_TEST(backend_leaves_points_unclaimed_when_auto_claim_is_off) {
    Harness h;
    h.backend->changeAutoClaim(false);
    LOGOS_ASSERT_TRUE(h.enrol());
    h.lez->advanceEpoch();
    h.backend->refresh();
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->claimablePoints(), QString("3"));
    LOGOS_ASSERT_EQ(h.backend->rewardBalance(), QString("0"));
    h.start();
    LOGOS_ASSERT_FALSE(h.backend->autoClaim());   // remembered
}

// ---- Cashing out ---------------------------------------------------------------------------

LOGOS_TEST(backend_cashes_out_into_a_payout_code) {
    Harness h;
    h.backend->changeAutoClaim(false);
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
    LOGOS_ASSERT_TRUE(h.backend->payoutCode().startsWith("uplink-payout-v2:"));

    h.backend->finishCashOut();
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashOutIdle));
    LOGOS_ASSERT_EQ(h.backend->totalPoints(), QString("0"));
    LOGOS_ASSERT_EQ(h.backend->cashedOutPoints(), QString("3"));
    LOGOS_ASSERT_EQ(h.backend->receipts().size(), 1);

    h.backend->cashOutAll();   // the same epoch: nothing new
    LOGOS_ASSERT_EQ(h.backend->lastError(), QString("No points to cash out yet."));
}

LOGOS_TEST(backend_cashes_out_what_auto_claim_already_collected) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    h.lez->advanceEpoch();
    h.backend->refresh();
    h.backend->refresh();   // auto-claim settled: balance 3, nothing claimable
    h.backend->cashOutAll();
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashingOut));
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashOutDone));
    LOGOS_ASSERT_EQ(h.backend->payoutPoints(), QString("3"));
}

// Cash out pressed while an auto-claim is still settling: it waits for that claim, then cashes out.
LOGOS_TEST(backend_cash_out_waits_for_a_settling_auto_claim) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    h.lez->advanceEpoch();
    h.backend->refresh();   // the auto-claim is submitted, still pending
    h.backend->cashOutAll();
    LOGOS_ASSERT_EQ(h.backend->lastError(), QString());
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::Collecting));
    h.backend->refresh();   // the claim settles, CashOut is submitted
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashingOut));
    h.backend->refresh();
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashOutDone));
    LOGOS_ASSERT_EQ(h.backend->payoutPoints(), QString("3"));
}

LOGOS_TEST(backend_codes_the_receipt_its_cash_out_made) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    h.lez->advanceEpoch();
    h.backend->refresh();
    h.backend->refresh();
    h.cashOut();
    h.backend->finishCashOut();
    h.lez->advanceEpoch();
    h.lez->advanceEpoch();
    h.backend->refresh();
    h.backend->refresh();
    h.cashOut();
    LOGOS_ASSERT_EQ(h.backend->cashOutState(), int(B::CashOutDone));
    const PayoutMessage opening = h.lez->payoutMessage(h.backend->participantId(), 1).value;
    const QString signature = MockNodeService::answerRequest(
        {{"intent", "node.sign_referral_payout"},
         {"params", QVariantMap{{"program_account", opening.programAccount}, {"blinding_factor", opening.blindingFactor}}}})
        .value("signature").toString();
    LOGOS_ASSERT_EQ(h.backend->payoutCode(), h.lez->payoutCodeWithSignature(h.backend->participantId(), 1, signature).value);
}

// A restart between cashing out and copying the code.
LOGOS_TEST(backend_gets_a_past_receipts_code_again_after_a_restart) {
    Harness h;
    LOGOS_ASSERT_TRUE(h.enrol());
    h.lez->advanceEpoch();
    h.backend->refresh();
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
