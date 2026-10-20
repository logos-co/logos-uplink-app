import QtQuick
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

import "../controls"

// Step 4: shown once, right after joining, or when the account turns out to have joined
// already (its wallet restored during onboarding). Also where auto-claim is chosen (on by default).
StepPage {
    id: root

    objectName: "uplink.DoneStep"

    property string referrerNode: ""   // "" = joined as a root
    property string accountLabel: ""
    property string accountAddress: ""
    property bool autoClaim: true
    property bool recovered: false   // joined before, not just now

    signal autoClaimToggled(bool on)

    LogosText {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        textFormat: Text.StyledText
        text: root.recovered
              ? qsTr("<b>Your account is back.</b> You’d joined already, so there’s nothing more to do: your points, referrals and invitation are as you left them.")
              : root.referrerNode !== ""
              ? qsTr("<b>Your position is sealed.</b> You joined under your inviter. It can’t be changed, and nobody, including us, can read who invited you.")
              : qsTr("<b>You’re enrolled as a root.</b> You continued without a referral — invite people to grow your own tree.")
        font.pixelSize: Theme.typography.primaryText
        color: Theme.palette.textSecondary
    }

    InfoCard {
        objectName: "uplink.pointsAccount"
        visible: root.accountAddress !== ""
        title: root.accountLabel.toUpperCase()
        detail: root.accountAddress.length > 20
                ? root.accountAddress.slice(0, 10) + "…" + root.accountAddress.slice(-8)
                : root.accountAddress
        copyText: root.accountAddress
        note: qsTr("A private account in your LEZ wallet. Your points are kept here; only you can see them.")
    }

    InfoCard {
        objectName: "uplink.autoClaimCard"
        title: qsTr("COLLECT AUTOMATICALLY")
        note: qsTr("While Uplink is open it collects your points as soon as there are any, which also passes credit up to whoever invited you. Your Blend key isn’t needed for this. You can change it later in About.")

        LogosSwitch {
            objectName: "uplink.autoClaimSwitch"
            checked: root.autoClaim
            text: qsTr("Collect my points automatically")
            onToggled: root.autoClaimToggled(checked)
        }
    }
}
