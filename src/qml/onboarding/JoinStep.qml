import QtQuick
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.UplinkUi 1.0

import "../controls"

// Step 3. Join under an inviter or as a root. The footer's button does the joining.
StepPage {
    id: root

    objectName: "uplink.JoinStep"

    property int invitationCheck: UplinkUi.InvitationEmpty
    property bool busy: false
    property string error: ""

    property string mode: "invite"   // "invite" | "root"
    readonly property string invitationText: invitation.text.trim()

    signal invitationEdited(string text)

    QtObject {
        id: d

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
    }

    LogosText {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        textFormat: Text.StyledText
        text: qsTr("<b>Your node is your identity.</b> Uplink joins with the Logos node it's running beside — no keys to type. Either way you can invite others and grow your own tree. Nobody, including us, can read or change who invited you.")
        font.pixelSize: Theme.typography.primaryText
        color: Theme.palette.textSecondary
    }

    ChoiceCard {
        objectName: "uplink.inviteChoice"
        enabled: !root.busy
        title: qsTr("I have an invitation")
        description: qsTr("Paste the code your inviter shared. You join under them, and that can’t be changed later.")
        selected: root.mode === "invite"
        onPicked: root.mode = "invite"
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: root.mode === "invite"
        spacing: Theme.spacing.small

        LogosTextArea {
            id: invitation

            objectName: "uplink.invitationInput"
            Layout.fillWidth: true
            Layout.preferredHeight: 72
            enabled: !root.busy
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

    ChoiceCard {
        objectName: "uplink.rootChoice"
        enabled: !root.busy
        title: qsTr("Start my own tree")
        description: qsTr("Join without a referral, as the root of a new tree.")
        selected: root.mode === "root"
        onPicked: root.mode = "root"
    }

    LogosNotice {
        objectName: "uplink.joinError"
        Layout.fillWidth: true
        shown: root.error !== "" && !root.busy
        severity: LogosNotice.Error
        message: root.error
    }
}
