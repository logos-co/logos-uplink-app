import QtQuick
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.UplinkUi 1.0

import "../controls"

// Step 2: join under an inviter or as a root. Joining creates the points account, then binds the node to it.
Item {
    id: root

    objectName: "uplink.JoinPage"

    property int nodeIssue: UplinkUi.ModuleUnavailable
    property string nodeId: ""
    property bool creatingAccount: false
    property int walletIssue: UplinkUi.LezCoreUnavailable
    property string walletDetail: ""
    property int syncedBlock: 0
    property int chainHeight: 0
    property int invitationCheck: UplinkUi.InvitationEmpty
    property int enrolState: UplinkUi.IdentityCreated
    property string error: ""

    signal importAndSignRequested(string invitation)
    signal invitationEdited(string text)
    signal joinWithoutReferralRequested()
    signal openBlockchainAppRequested()
    signal openLezWalletRequested()

    QtObject {
        id: d

        readonly property bool nodeDetected: root.nodeId !== ""
                                             && root.nodeIssue !== UplinkUi.ModuleUnavailable
                                             && root.nodeIssue !== UplinkUi.NodeNotRunning
        readonly property bool walletReady: root.walletIssue === UplinkUi.WalletReady
        readonly property bool syncing: d.walletReady && root.chainHeight > 0
                                        && root.syncedBlock < root.chainHeight
        readonly property bool ready: d.nodeDetected && d.walletReady
        readonly property bool notCore: root.nodeIssue === UplinkUi.NotCoreNode
                                        || root.nodeIssue === UplinkUi.NoBlendPeers
        readonly property bool busy: root.creatingAccount
                                     || root.enrolState === UplinkUi.Invited
                                     || root.enrolState === UplinkUi.AwaitingSignature
                                     || root.enrolState === UplinkUi.Enrolling

        readonly property string phase: {
            if (root.creatingAccount)
                return qsTr("Creating your points account…")
            switch (root.enrolState) {
            case UplinkUi.Invited: return qsTr("Importing invitation…")
            case UplinkUi.AwaitingSignature: return qsTr("Waiting for your signature in the Blockchain app…")
            case UplinkUi.Enrolling: return qsTr("Joining…")
            }
            return ""
        }

        function nodeProblem() {
            switch (root.nodeIssue) {
            case UplinkUi.ModuleUnavailable:
            case UplinkUi.NodeNotRunning:
                return qsTr("Your node isn't running. Start it in the Blockchain node app to join.")
            case UplinkUi.NodeBootstrapping:
                return qsTr("Your node is still starting up. You can join once it's online.")
            }
            return qsTr("Your node is running, but Uplink can't read its key yet.")
        }

        function invitationNote() {
            switch (root.invitationCheck) {
            case UplinkUi.InvitationInvalid:
                return qsTr("That doesn’t look like an invitation code.")
            case UplinkUi.InvitationOwn:
                return qsTr("That’s your own invitation — you can’t join under yourself.")
            case UplinkUi.InviterNotJoined:
                return qsTr("This inviter hasn’t joined the program yet.")
            }
            return ""
        }

        function shortId(id) {
            return id.length > 20 ? id.slice(0, 10) + "…" + id.slice(-8) : id
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Theme.spacing.xlarge, 660)
        spacing: Theme.spacing.large

        LogosText {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Join the referral program")
            font.pixelSize: Theme.typography.titleText
            font.weight: Theme.typography.weightBold
            color: Theme.palette.text
        }

        LogosText {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.min(parent.width, 580)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            textFormat: Text.StyledText
            text: qsTr("<b>Your node is your identity.</b> Uplink joins with the Logos node it's running beside — no keys to type. If someone invited you, paste their invitation and sign one transaction to them; otherwise continue without a referral. Either way you can invite others and grow your own tree. Nobody, including us, can read or change who invited you.")
            font.pixelSize: Theme.typography.primaryText
            color: Theme.palette.textSecondary
        }

        InfoCard {
            Layout.topMargin: Theme.spacing.small
            title: d.nodeDetected ? qsTr("YOUR NODE (DETECTED)") : qsTr("YOUR NODE (NOT DETECTED)")
            status: d.nodeDetected ? qsTr("Online") : ""
            detail: d.shortId(root.nodeId)
            note: qsTr("Signs your join; this key is your identity in the program.")
            showDetails: d.nodeDetected

            RowLayout {
                Layout.fillWidth: true
                visible: !d.nodeDetected
                spacing: Theme.spacing.medium

                LogosNotice {
                    Layout.fillWidth: true
                    shown: true
                    severity: LogosNotice.Error
                    message: d.nodeProblem()
                }
                LogosButton {
                    objectName: "uplink.openBlockchainButton"
                    Layout.alignment: Qt.AlignVCenter
                    variant: LogosButton.Variant.Primary
                    text: qsTr("Open Blockchain app")
                    onClicked: root.openBlockchainAppRequested()
                }
            }

            LogosNotice {
                Layout.fillWidth: true
                shown: d.nodeDetected && d.notCore
                severity: LogosNotice.Warning
                message: qsTr("You can join, but you won't collect points until you activate Blend core on this node.")
            }
        }

        InfoCard {
            title: d.walletReady ? qsTr("YOUR LEZ WALLET (OPEN)") : qsTr("YOUR LEZ WALLET (NOT OPEN)")
            status: d.walletReady ? qsTr("Open") : ""
            detail: d.syncing ? qsTr("Syncing… block %1 of %2").arg(root.syncedBlock).arg(root.chainHeight)
                              : qsTr("Synced to block %1").arg(root.syncedBlock)
            note: qsTr("Joining adds a private points account to this wallet.")
            showDetails: d.walletReady

            RowLayout {
                Layout.fillWidth: true
                visible: !d.walletReady
                spacing: Theme.spacing.medium

                LogosNotice {
                    Layout.fillWidth: true
                    shown: true
                    severity: LogosNotice.Error
                    message: qsTr("Your LEZ wallet isn't open. Open the LEZ Wallet app: it opens your wallet, or helps you create one if you don't have one yet. Then come back here.")
                         + (root.walletDetail !== "" ? "\n\n" + root.walletDetail : "")
                }
                LogosButton {
                    objectName: "uplink.openWalletButton"
                    Layout.alignment: Qt.AlignVCenter
                    variant: LogosButton.Variant.Primary
                    text: qsTr("Open LEZ Wallet")
                    onClicked: root.openLezWalletRequested()
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacing.small
            spacing: Theme.spacing.small

            LogosText {
                text: qsTr("Inviter’s invitation (optional)")
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.textSecondary
            }

            LogosTextArea {
                id: invitation

                objectName: "uplink.invitationInput"
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                enabled: !d.busy
                wrapMode: TextEdit.WrapAnywhere
                font.family: Theme.typography.mono
                font.pixelSize: Theme.typography.secondaryText
                placeholderText: qsTr("Paste your inviter’s invitation (the code they shared)")
                onTextChanged: root.invitationEdited(text)
            }

            LogosText {
                objectName: "uplink.invitationNote"
                Layout.fillWidth: true
                visible: text !== ""
                wrapMode: Text.WordWrap
                text: d.invitationNote()
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.error
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.spacing.small

            LogosButton {
                objectName: "uplink.importAndSignButton"
                variant: LogosButton.Variant.Primary
                font.pixelSize: Theme.typography.primaryText
                text: qsTr("Import invitation & sign")
                enabled: d.ready && !d.busy && root.invitationCheck === UplinkUi.InvitationOk
                onClicked: root.importAndSignRequested(invitation.text.trim())
            }
            LogosButton {
                objectName: "uplink.noReferralButton"
                compact: true
                font.pixelSize: Theme.typography.primaryText
                font.weight: Theme.typography.weightBold
                background: Item {}
                text: qsTr("Continue without referral")
                enabled: d.ready && !d.busy
                onClicked: root.joinWithoutReferralRequested()
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            visible: d.phase !== ""
            spacing: Theme.spacing.small

            LogosSpinner {
                running: parent.visible
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
            }
            LogosText {
                text: d.phase
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.textSecondary
            }
        }

        LogosNotice {
            objectName: "uplink.joinError"
            Layout.fillWidth: true
            shown: root.error !== "" && !d.busy
            severity: LogosNotice.Error
            message: root.error
        }
    }
}
