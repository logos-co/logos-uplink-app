import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

// Share your invitation code. The invitee imports it and signs; you sign nothing.
LogosDialog {
    id: root

    objectName: "uplink.InviteDialog"

    property string invitation: ""

    width: parent ? Math.min(560, parent.width - 2 * Theme.spacing.xxlarge) : 560
    dim: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    title: qsTr("Invite a peer")
    headerItem.font.pixelSize: Theme.typography.subtitleText   // the prototype's dialog title, not the 14 px default

    contentItem: ColumnLayout {
        spacing: Theme.spacing.medium

        LogosText {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            textFormat: Text.StyledText
            text: qsTr("They install Uplink, accept the terms, and <b>import this invitation</b>. You’ll see them in your node table once their join lands. There’s nothing for you to sign — the invitee signs.")
            font.pixelSize: Theme.typography.primaryText
            color: Theme.palette.textSecondary
        }

        LogosText {
            Layout.topMargin: Theme.spacing.small
            text: qsTr("Your invitation")
            font.pixelSize: Theme.typography.secondaryText
            color: Theme.palette.textSecondary
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.small

            LogosTextArea {
                objectName: "uplink.invitationCode"
                Layout.fillWidth: true
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                font.family: Theme.typography.mono
                font.pixelSize: Theme.typography.secondaryText
                text: root.invitation
                onActiveFocusChanged: if (activeFocus) selectAll()
            }
            LogosCopyButton {
                objectName: "uplink.copyInvitationButton"
                Layout.alignment: Qt.AlignTop
                enabled: root.invitation !== ""
                value: root.invitation
            }
        }

        LogosText {
            Layout.fillWidth: true
            visible: root.invitation === ""
            wrapMode: Text.WordWrap
            text: qsTr("Your invitation isn’t available yet. It appears once your join has landed.")
            font.pixelSize: Theme.typography.secondaryText
            color: Theme.palette.textTertiary
        }

        LogosText {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacing.small
            wrapMode: Text.WordWrap
            text: qsTr("Referrals accrue you points 2–3 levels deep, but you only ever see your direct ones.")
            font.pixelSize: Theme.typography.secondaryText
            color: Theme.palette.textTertiary
        }
    }

    rightActions: [
        LogosButton {
            objectName: "uplink.inviteCloseButton"
            variant: LogosButton.Variant.Primary
            text: qsTr("Done")
            onClicked: root.close()
        }
    ]
}
