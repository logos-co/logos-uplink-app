import QtQuick
import QtQuick.Controls

import Logos.Theme
import Logos.Controls
import Logos.UplinkUi 1.0

LogosWarningDialog {
    id: root

    objectName: "uplink.WalletSetupPopup"

    property int issue: UplinkUi.NoWalletOpen
    property string detail: ""

    signal openWalletRequested()

    width: parent ? Math.min(440, parent.width - 2 * Theme.spacing.xxlarge) : 440
    dim: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    title: qsTr("Set up your LEZ wallet first")
    message: qsTr("Uplink keeps your referral identity in your LEZ wallet. Open the LEZ Wallet app, create or open your wallet there, then come back and join. If the app isn't installed, Basecamp will offer to install it.")
             + (root.issue === UplinkUi.LezCoreUnavailable && root.detail !== "" ? "\n\n" + root.detail : "")

    leftActions: [
        LogosButton {
            objectName: "uplink.walletLaterButton"
            text: qsTr("Later")
            onClicked: root.close()
        }
    ]
    rightActions: [
        LogosButton {
            objectName: "uplink.openWalletButton"
            variant: LogosButton.Variant.Primary
            text: qsTr("Open LEZ Wallet")
            onClicked: root.openWalletRequested()
        }
    ]
}
