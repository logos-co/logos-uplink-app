import QtQuick
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

import "../controls"

// Step 1. Acceptance is gated: scroll the terms to the end, then tick the box.
Item {
    id: root

    objectName: "uplink.TermsStep"

    readonly property bool accepted: agree.checked

    function reset() {
        terms.reset()
        agree.checked = false
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing.small

        LogosText {
            Layout.fillWidth: true
            text: qsTr("Read to the end to enable acceptance.")
            font.pixelSize: Theme.typography.secondaryText
            color: Theme.palette.textTertiary
        }

        TermsView {
            id: terms

            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        LogosCheckbox {
            id: agree

            objectName: "uplink.termsCheckbox"
            Layout.topMargin: Theme.spacing.small
            enabled: terms.readToEnd
            text: qsTr("I have read and accept the program terms")
        }
    }
}
