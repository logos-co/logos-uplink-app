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

        function join() {
            if (!d.backend)
                return
            if (d.backend.participantId !== "") {
                pages.currentIndex = 1
                return
            }
            if (d.backend.walletIssue !== UplinkUi.WalletReady) {
                walletSetup.open()
                return
            }
            termsDialog.open()
        }

        function createIdentity() {
            d.creatingIdentity = true
            d.backend.createIdentity()
        }

        function openLezWallet() {
            walletSetup.close()
            if (typeof logos === "undefined" || typeof logos.request !== "function") {
                d.walletLaunchFailed("no intents on this host")
                return
            }
            // A hand-off: the shell opens the app, or offers to install it, and leaves the user there.
            logos.request("basecamp.apps.launch", { app: "lez_wallet_ui" }, function (res) {
                if (!res.ok)
                    d.walletLaunchFailed(res.error)
            })
        }

        function walletLaunchFailed(reason) {
            console.warn("Uplink: could not open the LEZ Wallet app:", reason)
            toast.show(qsTr("Couldn't open the LEZ Wallet app"),
                       qsTr("Open it from the Basecamp sidebar, set up your wallet there, then come back and join."))
        }
    }

    StackLayout {
        id: pages
        anchors.fill: parent
        currentIndex: 0

        WelcomePage {
            network: d.backend ? d.backend.chainId : ""
            busy: d.creatingIdentity
            onJoinRequested: d.join()
        }

        IdentityPage {
            created: d.backend ? d.backend.participantId !== "" : false
            label: d.backend ? d.backend.identityLabel : ""
            address: d.backend ? d.backend.identityAddress : ""
            error: d.backend ? d.backend.lastError : ""
            onBackRequested: pages.currentIndex = 0
        }
    }

    WalletSetupPopup {
        id: walletSetup
        anchors.centerIn: parent
        issue: d.backend ? d.backend.walletIssue : UplinkUi.LezCoreUnavailable
        detail: d.backend ? d.backend.walletIssueDetail : ""
        onOpenWalletRequested: d.openLezWallet()
    }

    TermsDialog {
        id: termsDialog
        anchors.centerIn: parent
        onTermsAccepted: d.createIdentity()
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

    // createIdentity() answers through properties: an id on success, lastError on failure.
    Connections {
        target: d.backend
        enabled: d.creatingIdentity

        function onParticipantIdChanged() {
            d.creatingIdentity = false
            pages.currentIndex = 1
        }
        function onLastErrorChanged() {
            if (d.backend.lastError === "")
                return
            d.creatingIdentity = false
            pages.currentIndex = 1
        }
    }
}
