import QtQuick
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

Item {
    id: root

    objectName: "uplink.WelcomePage"

    property string network: ""   // the node's chain ID; hidden until the node reports one
    property bool busy: false

    signal joinRequested()

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Theme.spacing.xlarge, 660)
        spacing: Theme.spacing.large

        LogosText {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Logos Uplink")
            font.pixelSize: Theme.typography.titleText
            font.weight: Theme.typography.weightBold
            color: Theme.palette.text
        }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            visible: root.network !== ""
            implicitWidth: tag.implicitWidth + 2 * Theme.spacing.medium
            implicitHeight: tag.implicitHeight + Theme.spacing.tiny * 2
            color: Theme.palette.backgroundTertiary
            border.color: Theme.palette.borderSecondary
            radius: Theme.spacing.radiusPill

            LogosText {
                id: tag
                anchors.centerIn: parent
                text: qsTr("testnet %1").arg(root.network)
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.textTertiary
            }
        }

        LogosText {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.min(parent.width, 580)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Help stress-test the Logos Testnet: bring more people in to run Logos Blockchain nodes and keep them active. Keep your own node running too — that is what keeps you eligible — while nobody, including us, can reconstruct who invited whom.")
            font.pixelSize: Theme.typography.primaryText
            color: Theme.palette.textSecondary
        }

        LogosButton {
            objectName: "uplink.joinButton"
            Layout.alignment: Qt.AlignHCenter
            variant: LogosButton.Variant.Primary
            font.pixelSize: Theme.typography.primaryText
            text: root.busy ? qsTr("Joining…") : qsTr("Join Uplink")
            enabled: !root.busy
            onClicked: root.joinRequested()
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacing.xlarge
            spacing: Theme.spacing.large

            Block {
                icon: Qt.resolvedUrl("../icons/server.svg")
                title: qsTr("Run")
                body: qsTr("Your node’s Blend activity produces an accepted proof each epoch. That keeps you eligible to collect — your own node earns you no points, only the people you invite do.")
            }
            Block {
                icon: Qt.resolvedUrl("../icons/user-plus.svg")
                title: qsTr("Invite")
                body: qsTr("No referral codes. Someone you invite signs one transaction to your key — that bind is the referral, sealed and private.")
            }
            Block {
                icon: Qt.resolvedUrl("../icons/award.svg")
                title: qsTr("Collect")
                body: qsTr("Points recognise your invitees’ active epochs — not your own node, and not a token. Claim manually, whenever you choose.")
            }
        }
    }

    component Block: LogosFrame {
        id: block

        property url icon
        property string title
        property string body

        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.fillHeight: true
        padding: Theme.spacing.large
        backgroundColor: Theme.palette.backgroundTertiary
        borderColor: Theme.palette.borderSecondary
        radius: Theme.spacing.radiusLarge

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacing.small

            LogosIcon {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                source: block.icon
                color: Theme.palette.primary
            }
            LogosText {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: block.title
                font.pixelSize: Theme.typography.primaryText
                font.weight: Theme.typography.weightBold
                color: Theme.palette.text
            }
            LogosText {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: block.body
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.textSecondary
            }
            Item { Layout.fillHeight: true }
        }
    }
}
