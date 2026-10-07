import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.UplinkUi 1.0

import "../controls"

// Claim rewards: why there's nothing to cash out yet, or one Cash out (collect, cash
// out, prepare the code), then the payout code for the web form.
LogosDialog {
    id: root

    objectName: "uplink.ClaimDialog"

    property string points: "0"           // claimable + balance
    property int referralCount: 0
    property bool nodeActive: false
    property int cashOutState: UplinkUi.CashOutIdle
    property string payoutCode: ""
    property string payoutPoints: ""
    property string error: ""
    property string formUrl: ""

    signal cashOutRequested()
    signal signAgainRequested()
    signal inviteRequested()
    signal finished()

    width: parent ? Math.min(560, parent.width - 2 * Theme.spacing.xxlarge) : 560
    dim: true
    closePolicy: d.done ? Popup.CloseOnEscape : Popup.CloseOnEscape | Popup.CloseOnPressOutside
    title: qsTr("Claim rewards")
    headerItem.font.pixelSize: Theme.typography.subtitleText   // the prototype's dialog title, not the 14 px default

    QtObject {
        id: d

        readonly property bool done: root.cashOutState === UplinkUi.CashOutDone
        readonly property bool signing: root.cashOutState === UplinkUi.SigningCode
        // Signing failed: the points are cashed out, only the signature is asked for again.
        readonly property bool signFailed: signing && root.error !== ""
        readonly property bool busy: root.cashOutState === UplinkUi.Collecting
                                     || root.cashOutState === UplinkUi.CashingOut
                                     || root.cashOutState === UplinkUi.PreparingCode
                                     || signing
        readonly property bool empty: Number(root.points) <= 0 && !busy && !done
        readonly property bool blocked: !empty && !root.nodeActive && !busy && !done
        readonly property bool ready: !empty && !blocked && !done

        readonly property string buttonText: {
            switch (root.cashOutState) {
            case UplinkUi.Collecting: return qsTr("Collecting…")
            case UplinkUi.CashingOut: return qsTr("Cashing out…")
            case UplinkUi.PreparingCode: return qsTr("Preparing your code…")
            case UplinkUi.SigningCode: return d.signFailed ? qsTr("Sign again") : qsTr("Waiting for signature…")
            }
            return qsTr("Cash out")
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacing.medium

        // ---- Nothing to cash out yet ----------------------------------------------

        Section {
            visible: d.empty
            title: qsTr("No points to claim yet")
            body: root.referralCount === 0
                  ? qsTr("You earn points for each epoch the people you invite keep their nodes active. Invite someone to get started.")
                  : qsTr("Points arrive for each epoch your referrals’ nodes are active — and yours is too. Keep accruing.")
        }

        // ---- Points, but this node can't collect ------------------------------------

        Section {
            visible: d.blocked
            title: qsTr("%1 points waiting").arg(root.points)
            body: qsTr("Your node isn’t an active Blend core node this epoch, so you can’t collect yet. Check it in the Blockchain app; your points wait for you.")
        }

        // ---- Cash out ------------------------------------------------------------------

        Section {
            visible: d.signing
            title: qsTr("Sign your payout code")
            body: qsTr("Your points are cashed out. Approve the signature in the Blockchain app — it proves the code is yours, so nobody else can redeem it.")
        }

        Section {
            visible: d.ready && !d.signing
            title: qsTr("Cash out")
            body: qsTr("Cashing out takes your <b>entire balance</b> in one go — no partial or fractional claims. Two things happen: Uplink first <b>collects</b> your points on-chain (settling your active referrals’ epochs into your balance — private, no identity), then cashes them out and prepares a payout code.")
        }

        InfoCard {
            visible: d.ready && !d.signing
            title: qsTr("YOU WILL CASH OUT")

            LogosText {
                objectName: "uplink.cashOutPoints"
                text: qsTr("%1 points").arg(root.points)
                font.pixelSize: Theme.typography.panelTitleText
                font.weight: Theme.typography.weightBold
                color: Theme.palette.text
            }
        }

        Body {
            visible: d.ready && !d.signing
            text: qsTr("You copy the payout code into the web claim form, where you add payout and identity details. <b>No identity or money is handled here.</b>")
        }

        // ---- The payout code -----------------------------------------------------------

        LogosText {
            visible: d.done
            text: qsTr("Your payout code")
            font.pixelSize: Theme.typography.primaryText
            font.weight: Theme.typography.weightBold
            color: Theme.palette.text
        }

        Body {
            visible: d.done
            text: qsTr("Points collected and cashed out. This code is for your full balance of <b>%1 points</b>, signed by your node so only you can redeem it. Copy it and paste it into the web claim form.").arg(root.payoutPoints)
        }

        RowLayout {
            visible: d.done
            Layout.fillWidth: true
            spacing: Theme.spacing.small

            LogosTextArea {
                objectName: "uplink.payoutCode"
                Layout.fillWidth: true
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                font.family: Theme.typography.mono
                font.pixelSize: Theme.typography.secondaryText
                text: root.payoutCode
                onActiveFocusChanged: if (activeFocus) selectAll()
            }
            LogosCopyButton {
                objectName: "uplink.copyPayoutCodeButton"
                Layout.alignment: Qt.AlignTop
                value: root.payoutCode
            }
        }

        InfoCard {
            visible: d.done && root.formUrl !== ""
            title: qsTr("WEB CLAIM FORM")
            detail: root.formUrl
            copyText: root.formUrl
            note: qsTr("Open it in your browser. You add your identity and payout details there. The form only says thank you — it never tells you if the code is valid. The finance team verifies it and processes valid claims; your points stay reserved until then.")
        }

        LogosNotice {
            objectName: "uplink.claimError"
            Layout.fillWidth: true
            shown: root.error !== "" && (!d.busy || d.signFailed) && !d.done
            severity: LogosNotice.Error
            message: root.error
        }
    }

    leftActions: [
        LogosButton {
            visible: !d.done
            compact: true
            font.pixelSize: Theme.typography.primaryText
            font.weight: Theme.typography.weightBold
            background: Item {}
            text: qsTr("Close")
            onClicked: root.close()
        }
    ]
    rightActions: [
        LogosButton {
            objectName: "uplink.claimInviteButton"
            visible: d.empty && root.referralCount === 0
            variant: LogosButton.Variant.Primary
            text: qsTr("Invite peer")
            onClicked: {
                root.close()
                root.inviteRequested()
            }
        },
        LogosButton {
            objectName: "uplink.cashOutButton"
            visible: d.ready || d.busy
            Layout.minimumWidth: 160
            variant: LogosButton.Variant.Primary
            enabled: (d.ready && !d.busy) || d.signFailed
            text: d.buttonText
            onClicked: d.signFailed ? root.signAgainRequested() : root.cashOutRequested()
        },
        LogosButton {
            objectName: "uplink.claimDoneButton"
            visible: d.done
            variant: LogosButton.Variant.Primary
            text: qsTr("Done")
            onClicked: {
                root.finished()
                root.close()
            }
        }
    ]

    component Section: ColumnLayout {
        property string title
        property string body

        Layout.fillWidth: true
        spacing: Theme.spacing.small

        LogosText {
            Layout.fillWidth: true
            text: parent.title
            font.pixelSize: Theme.typography.primaryText
            font.weight: Theme.typography.weightBold
            color: Theme.palette.text
        }
        Body {
            text: parent.body
        }
    }

    component Body: LogosText {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        textFormat: Text.StyledText
        font.pixelSize: Theme.typography.primaryText
        color: Theme.palette.textSecondary
    }
}
