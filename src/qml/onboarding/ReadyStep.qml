import QtQuick
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.UplinkUi 1.0

import "../controls"

// Step 2. The node signs the join and the LEZ wallet keeps the points, so both must be up.
StepPage {
    id: root

    objectName: "uplink.ReadyStep"

    property int nodeIssue: UplinkUi.ModuleUnavailable
    property string nodeId: ""
    property int walletIssue: UplinkUi.LezCoreUnavailable
    property string walletDetail: ""
    property int syncedBlock: 0
    property int chainHeight: 0

    readonly property bool ready: d.nodeDetected && d.walletReady

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
        readonly property bool notCore: root.nodeIssue === UplinkUi.NotCoreNode
                                        || root.nodeIssue === UplinkUi.NoBlendPeers

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

        function shortId(id) {
            return id.length > 20 ? id.slice(0, 10) + "…" + id.slice(-8) : id
        }
    }

    InfoCard {
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
}
