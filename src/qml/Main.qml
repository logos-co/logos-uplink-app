import QtQuick
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.UplinkUi 1.0

import "pages"
import "popups"

Rectangle {
    id: root

    color: Theme.palette.background

    QtObject {
        id: d

        readonly property var backend: logos.module("uplink_ui")
        property bool creatingIdentity: false
        property bool signAfterImport: false
        property string pendingInvitation: ""   // carried across account creation

        readonly property int welcomePage: 0
        readonly property int joinPage: 1
        readonly property int joinedPage: 2

        // Joining is the acceptance (the node signs after the terms), so a joined user
        // never sees the welcome or the terms again.
        onBackendChanged: d.showJoinedIfEnrolled()
        Component.onCompleted: d.showJoinedIfEnrolled()

        function showJoinedIfEnrolled() {
            if (d.backend && d.backend.enrolState === UplinkUi.Enrolled)
                pages.currentIndex = d.joinedPage
        }

        function join() {
            if (!d.backend)
                return
            if (d.backend.participantId !== "") {
                pages.currentIndex = d.backend.enrolState === UplinkUi.Enrolled ? d.joinedPage : d.joinPage
                return
            }
            termsDialog.open()
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

        function continueJoin() {
            if (d.pendingInvitation !== "") {
                d.signAfterImport = true
                d.backend.joinUnder(d.pendingInvitation)
            } else {
                d.backend.prepareEnroll()
            }
        }

        // The node app signs on the user's approval; the shell brings them back here.
        function requestSignature() {
            if (!d.canRequest()) {
                d.backend.reportSignFailed("unavailable")
                return
            }
            logos.request("node.sign_message", d.backend.signRequest, function (res) {
                if (res.ok)
                    d.backend.completeEnroll(res.data.signature, res.data.public_key)
                else
                    d.backend.reportSignFailed(res.error)
            })
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
            toast.show(qsTr("Couldn't open the %1 app").arg(appName), hint)
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
            onJoinRequested: d.join()
        }

        JoinPage {
            nodeIssue: d.backend ? d.backend.nodeIssue : UplinkUi.ModuleUnavailable
            nodeId: d.backend ? d.backend.nodeId : ""
            creatingAccount: d.creatingIdentity
            walletIssue: d.backend ? d.backend.walletIssue : UplinkUi.LezCoreUnavailable
            walletDetail: d.backend && d.backend.walletIssue === UplinkUi.LezCoreUnavailable
                          ? d.backend.walletIssueDetail : ""
            syncedBlock: d.backend ? d.backend.syncedBlock : 0
            chainHeight: d.backend ? d.backend.chainHeight : 0
            invitationCheck: d.backend ? d.backend.invitationCheck : UplinkUi.InvitationEmpty
            invitationInviter: d.backend ? d.backend.invitationInviter : ""
            onInvitationEdited: function (text) { d.backend.checkInvitation(text) }
            enrolState: d.backend ? d.backend.enrolState : UplinkUi.IdentityCreated
            error: d.backend ? d.backend.lastError : ""
            onImportAndSignRequested: function (invitation) { d.startJoin(invitation) }
            onJoinWithoutReferralRequested: d.startJoin("")
            onOpenBlockchainAppRequested: d.launchApp("blockchain_ui", qsTr("Blockchain"),
                qsTr("Open it from the Basecamp sidebar and start your node, then come back."))
            onOpenLezWalletRequested: d.launchApp("lez_wallet_ui", qsTr("LEZ Wallet"),
                qsTr("Open it from the Basecamp sidebar, set up your wallet there, then come back."))
        }

        JoinedPage {
            referrerNode: d.backend ? d.backend.referrerNode : ""
            accountLabel: d.backend ? d.backend.identityLabel : ""
            accountAddress: d.backend ? d.backend.identityAddress : ""
        }
    }

    TermsDialog {
        id: termsDialog
        anchors.centerIn: parent
        onTermsAccepted: pages.currentIndex = d.joinPage
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
                if (d.signAfterImport) {
                    d.signAfterImport = false
                    d.backend.prepareEnroll()
                }
                break
            case UplinkUi.AwaitingSignature:
                d.requestSignature()
                break
            case UplinkUi.Enrolled:
                pages.currentIndex = d.joinedPage
                break
            }
        }

        function onLastErrorChanged() {
            if (d.backend.lastError === "")
                return
            d.signAfterImport = false
            d.creatingIdentity = false
        }
    }
}
