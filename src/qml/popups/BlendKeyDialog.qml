import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.UplinkUi 1.0

import "../controls"

// Every use of the node's Blend signing key comes through here: what it signs, which
// keystore.yaml it's read from, and the user's go-ahead. The path is remembered; the key never is.
LogosDialog {
    id: root

    objectName: "uplink.BlendKeyDialog"

    property string action: "join"   // "join" | "cashOut" | "payoutCode"
    property int keystoreIssue: UplinkUi.KeystoreNotChosen
    property string keystorePath: ""
    property string keystoreNode: ""
    property string keystoreDetail: ""

    readonly property bool ready: root.keystoreIssue === UplinkUi.KeystoreReady

    signal chooseKeystoreRequested()
    signal confirmed()

    width: parent ? Math.min(560, parent.width - 2 * Theme.spacing.xxlarge) : 560
    dim: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    title: qsTr("Sign with your Blend key")
    headerItem.font.pixelSize: Theme.typography.subtitleText   // the prototype's dialog title, not the 14 px default

    QtObject {
        id: d

        readonly property string purpose: {
            switch (root.action) {
            case "cashOut":
                return qsTr("Uplink uses your node’s Blend signing key to make your cash-out receipt and sign your payout code.")
            case "payoutCode":
                return qsTr("Uplink signs your payout code with your node’s Blend signing key, so only you can redeem it.")
            }
            return qsTr("Uplink signs your join with your node’s Blend signing key: the key that identifies your node as a Blend node.")
        }

        readonly property string problem: {
            switch (root.keystoreIssue) {
            case UplinkUi.KeystoreUnreadable:
                return qsTr("Uplink couldn’t read that file. Choose your node’s keystore.yaml.")
                       + (root.keystoreDetail !== "" ? "\n\n" + root.keystoreDetail : "")
            case UplinkUi.KeystoreWrongNode:
                return qsTr("This file holds a different node’s Blend key than the one running. Choose your running node’s keystore.yaml.")
            }
            return ""
        }

        function shortPath(path) {
            return path.length > 48 ? "…" + path.slice(-46) : path
        }

        function shortId(id) {
            return id.length > 20 ? id.slice(0, 10) + "…" + id.slice(-8) : id
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacing.large

        LogosText {
            objectName: "uplink.blendKeyPurpose"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: d.purpose
            font.pixelSize: Theme.typography.primaryText
            color: Theme.palette.text
        }

        InfoCard {
            objectName: "uplink.blendKeyFileCard"
            title: qsTr("YOUR NODE’S KEYSTORE")
            status: root.ready ? qsTr("Ready") : ""
            detail: root.keystoreNode !== ""
                    ? qsTr("%1 · Blend key of node %2").arg(d.shortPath(root.keystorePath)).arg(d.shortId(root.keystoreNode))
                    : d.shortPath(root.keystorePath)
            showDetails: root.keystorePath !== ""

            LogosText {
                Layout.fillWidth: true
                visible: root.keystorePath === ""
                wrapMode: Text.WordWrap
                text: qsTr("Provide the path to your node’s keystore.yaml. It’s next to the node’s user_config.yaml.")
                font.pixelSize: Theme.typography.primaryText
                color: Theme.palette.text
            }

            LogosNotice {
                objectName: "uplink.blendKeyNotice"
                Layout.fillWidth: true
                shown: d.problem !== ""
                severity: LogosNotice.Error
                message: d.problem
            }
        }

        LogosText {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Uplink reads only the BlendSigning key from this file, for this signature only, and never stores it.")
            font.pixelSize: Theme.typography.secondaryText
            color: Theme.palette.textSecondary
        }
    }

    leftActions: [
        LogosButton {
            objectName: "uplink.blendKeyCancelButton"
            compact: true
            font.pixelSize: Theme.typography.primaryText
            font.weight: Theme.typography.weightBold
            background: Item {}
            text: qsTr("Cancel")
            onClicked: root.close()
        }
    ]
    // One main button at a time: choose the keystore until there's a usable one, then sign.
    // Choosing never signs; the user still presses Sign.
    rightActions: [
        LogosButton {
            objectName: "uplink.blendKeyChooseButton"
            visible: root.ready
            variant: LogosButton.Variant.Secondary
            text: qsTr("Change…")
            onClicked: root.chooseKeystoreRequested()
        },
        LogosButton {
            objectName: "uplink.blendKeyConfirmButton"
            Layout.minimumWidth: 160
            variant: LogosButton.Variant.Primary
            text: {
                if (root.keystorePath === "")
                    return qsTr("Choose keystore.yaml…")
                if (!root.ready)
                    return qsTr("Choose another…")
                switch (root.action) {
                case "cashOut": return qsTr("Sign and cash out")
                case "payoutCode": return qsTr("Sign code")
                }
                return qsTr("Sign and join")
            }
            onClicked: {
                if (!root.ready) {
                    root.chooseKeystoreRequested()
                    return
                }
                root.close()
                root.confirmed()
            }
        }
    ]
}
