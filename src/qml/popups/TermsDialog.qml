import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

import "../controls"

// The program terms again, to read. Nothing to accept: joining already did that.
LogosDialog {
    id: root

    objectName: "uplink.TermsDialog"

    width: parent ? Math.min(640, parent.width - 2 * Theme.spacing.xxlarge) : 640
    height: parent ? Math.min(680, parent.height - 2 * Theme.spacing.xxlarge) : 680
    dim: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    bottomPadding: 0
    title: qsTr("Program terms")

    onOpened: terms.reset()

    contentItem: TermsView {
        id: terms
    }

    rightActions: [
        LogosButton {
            objectName: "uplink.termsCloseButton"
            variant: LogosButton.Variant.Primary
            text: qsTr("Close")
            onClicked: root.close()
        }
    ]
}
