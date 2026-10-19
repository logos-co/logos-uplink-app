import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.UplinkUi 1.0

import "onboarding"
import "pages"
import "popups"

Rectangle {
    id: root

    color: Theme.palette.background

    QtObject {
        id: d

        readonly property var backend: logos.module("uplink_ui")
        property bool creatingIdentity: false
        property bool enrolAfterImport: false
        property string pendingInvitation: ""   // carried across account creation
        property var afterBlendKey: null        // what runs once the Blend key dialog is confirmed

        readonly property int welcomePage: 0
        readonly property int onboardingPage: 1
        readonly property int overviewPage: 2

        // A joined user accepted the terms on the way in, so never sees the welcome or the terms again.
        onBackendChanged: d.showJoinedIfEnrolled()
        Component.onCompleted: d.showJoinedIfEnrolled()

        function showJoinedIfEnrolled() {
            if (d.backend && d.backend.enrolState === UplinkUi.Enrolled
                    && pages.currentIndex === d.welcomePage)
                pages.currentIndex = d.overviewPage
        }

        function join() {
            if (!d.backend)
                return
            if (d.backend.enrolState === UplinkUi.Enrolled) {
                pages.currentIndex = d.overviewPage
                return
            }
            onboarding.begin()
            pages.currentIndex = d.onboardingPage
        }

        // Joining creates the points account first (or reuses one found in the wallet).
        function startJoin(invitation) {
            d.pendingInvitation = invitation
            if (d.backend.participantId === "") {
                d.creatingIdentity = true
                d.backend.createIdentity()
                return
            }
            d.continueJoin()
        }

        // The Blend signing key signs inside uplink_module, read from the keystore the user chose.
        function continueJoin() {
            if (d.pendingInvitation !== "") {
                d.enrolAfterImport = true
                d.backend.joinUnder(d.pendingInvitation)
            } else {
                d.backend.enrol()
            }
        }

        // Every use of the Blend signing key is confirmed first, with the keystore it's read from.
        function withBlendKey(action, then) {
            d.afterBlendKey = then
            blendKeyDialog.action = action
            blendKeyDialog.open()
        }

        // A hand-off: the shell opens the app, or offers to install it, and leaves the user there.
        function launchApp(app, appName, hint) {
            if (!d.canRequest()) {
                d.launchFailed(appName, hint, "no intents on this host")
                return
            }
            logos.request("basecamp.apps.launch", { app: app }, function (res) {
                if (!res.ok)
                    d.launchFailed(appName, hint, res.error)
            })
        }

        function launchFailed(appName, hint, reason) {
            console.warn("Uplink: could not open", appName, ":", reason)
            d.launchFailedSeverity()
            toast.show(qsTr("Couldn't open the %1 app").arg(appName), hint)
        }

        function launchFailedSeverity() {
            toast.severity = LogosNotice.Error
        }

        function canRequest() {
            return typeof logos !== "undefined" && typeof logos.request === "function"
        }
    }

    StackLayout {
        id: pages
        anchors.fill: parent
        currentIndex: d.welcomePage

        WelcomePage {
            network: d.backend ? d.backend.chainId : ""
            programIssue: d.backend ? d.backend.programIssue : UplinkUi.ProgramUnknown
            programDetail: d.backend ? d.backend.programIssueDetail : ""
            onJoinRequested: d.join()
        }

        OnboardingView {
            id: onboarding

            nodeIssue: d.backend ? d.backend.nodeIssue : UplinkUi.ModuleUnavailable
            nodeId: d.backend ? d.backend.nodeId : ""
            walletIssue: d.backend ? d.backend.walletIssue : UplinkUi.LezCoreUnavailable
            walletDetail: d.backend && d.backend.walletIssue === UplinkUi.LezCoreUnavailable
                          ? d.backend.walletIssueDetail : ""
            syncedBlock: d.backend ? d.backend.syncedBlock : 0
            chainHeight: d.backend ? d.backend.chainHeight : 0
            invitationCheck: d.backend ? d.backend.invitationCheck : UplinkUi.InvitationEmpty
            enrolState: d.backend ? d.backend.enrolState : UplinkUi.NoIdentity
            creatingAccount: d.creatingIdentity
            error: d.backend ? d.backend.lastError : ""
            referrerNode: d.backend ? d.backend.referrerNode : ""
            accountLabel: d.backend ? d.backend.identityLabel : ""
            accountAddress: d.backend ? d.backend.identityAddress : ""
            termsSections: d.backend ? d.backend.termsSections : []
            termsAccepted: d.backend ? d.backend.termsAccepted : false
            autoClaim: d.backend ? d.backend.autoClaim : true
            programIssue: d.backend ? d.backend.programIssue : UplinkUi.ProgramUnknown
            programDetail: d.backend ? d.backend.programIssueDetail : ""

            onExitRequested: pages.currentIndex = d.welcomePage
            onAcceptTermsRequested: d.backend.acceptTerms()
            onAutoClaimToggled: function (on) { d.backend.changeAutoClaim(on) }
            onJoinRequested: function (invitation) {
                d.withBlendKey("join", function () { d.startJoin(invitation) })
            }
            onInvitationEdited: function (text) { d.backend.checkInvitation(text) }
            onOpenBlockchainAppRequested: d.launchApp("blockchain_ui", qsTr("Blockchain"),
                qsTr("Open it from the Basecamp sidebar and start your node, then come back."))
            onOpenLezWalletRequested: d.launchApp("lez_wallet_ui", qsTr("LEZ Wallet"),
                qsTr("Open it from the Basecamp sidebar, set up your wallet there, then come back."))
            onFinished: pages.currentIndex = d.overviewPage
        }

        OverviewPage {
            id: overview

            network: d.backend ? d.backend.chainId : ""
            points: d.backend ? d.backend.totalPoints : "0"
            cashedOut: d.backend ? d.backend.cashedOutPoints : "0"
            receipts: d.backend ? d.backend.receipts : []
            nodeIssue: d.backend ? d.backend.nodeIssue : UplinkUi.ModuleUnavailable
            nodeId: d.backend ? d.backend.nodeId : ""
            nodeActive: d.backend ? d.backend.nodeActive : false
            myActivity: d.backend ? d.backend.myActivity : []
            referrals: d.backend ? d.backend.referrals : []
            referrerNode: d.backend ? d.backend.referrerNode : ""
            invitation: d.backend ? d.backend.invitation : ""
            cashOutState: d.backend ? d.backend.cashOutState : UplinkUi.CashOutIdle
            payoutCode: d.backend ? d.backend.payoutCode : ""
            payoutPoints: d.backend ? d.backend.payoutPoints : ""
            error: d.backend ? d.backend.lastError : ""
            payoutFormUrl: d.backend ? d.backend.payoutFormUrl : ""
            termsSections: d.backend ? d.backend.termsSections : []
            autoClaim: d.backend ? d.backend.autoClaim : true
            keystoreIssue: d.backend ? d.backend.keystoreIssue : UplinkUi.KeystoreNotChosen
            keystorePath: d.backend ? d.backend.keystorePath : ""
            programIssue: d.backend ? d.backend.programIssue : UplinkUi.ProgramUnknown
            programDetail: d.backend ? d.backend.programIssueDetail : ""
            onCashOutRequested: d.withBlendKey("cashOut", function () { d.backend.cashOutAll() })
            onAutoClaimToggled: function (on) { d.backend.changeAutoClaim(on) }
            onChooseKeystoreRequested: keystoreDialog.open()
            onPayoutCodeRequested: function (index) {
                d.withBlendKey("payoutCode", function () {
                    d.backend.payoutCodeFor(index)
                    overview.showClaim()
                })
            }
            onCashOutFinished: d.backend.finishCashOut()
            onLabelEdited: function (node, label) { d.backend.setReferralLabel(node, label) }
        }
    }

    // Opens at Basecamp's data folder; Uplink doesn't know where under it the node keeps its
    // files, so the user finds the node's keystore.yaml there.
    FileDialog {
        id: keystoreDialog

        objectName: "uplink.keystoreDialog"
        title: qsTr("Choose your node’s keystore.yaml")
        currentFolder: d.backend ? d.backend.keystoreFolder : ""
        nameFilters: [qsTr("Keystore (*.yaml *.yml)"), qsTr("All files (*)")]
        onAccepted: d.backend.chooseKeystore(selectedFile.toString())
    }

    BlendKeyDialog {
        id: blendKeyDialog

        anchors.centerIn: parent
        keystoreIssue: d.backend ? d.backend.keystoreIssue : UplinkUi.KeystoreNotChosen
        keystorePath: d.backend ? d.backend.keystorePath : ""
        keystoreNode: d.backend ? d.backend.keystoreNode : ""
        keystoreDetail: d.backend ? d.backend.keystoreIssueDetail : ""
        onChooseKeystoreRequested: keystoreDialog.open()
        onConfirmed: {
            const then = d.afterBlendKey
            d.afterBlendKey = null
            if (then)
                then()
        }
    }

    LogosToast {
        id: toast
        objectName: "uplink.toast"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spacing.large
        width: Math.min(480, parent.width - 2 * Theme.spacing.large)
        severity: LogosNotice.Error
        duration: 8000
    }

    // The backend answers through properties, so the flow advances on their changes.
    Connections {
        target: d.backend

        function onParticipantIdChanged() {
            if (!d.creatingIdentity)
                return
            d.creatingIdentity = false
            d.continueJoin()
        }

        function onEnrolStateChanged() {
            switch (d.backend.enrolState) {
            case UplinkUi.Invited:
                if (d.enrolAfterImport) {
                    d.enrolAfterImport = false
                    d.backend.enrol()
                }
                break
            case UplinkUi.Enrolled:
                if (pages.currentIndex !== d.onboardingPage)
                    d.showJoinedIfEnrolled()
                break
            case UplinkUi.NoIdentity:
                // The wallet was closed or switched to one that hasn't joined.
                if (pages.currentIndex === d.overviewPage)
                    pages.currentIndex = d.welcomePage
                break
            }
        }

        function onLastErrorChanged() {
            if (d.backend.lastError === "")
                return
            d.enrolAfterImport = false
            d.creatingIdentity = false
        }
    }
}
