import QtQuick
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

// Placeholder: shows whether the identity was created. The next step (node check,
// invitation or own tree) replaces it.
Item {
    id: root

    objectName: "uplink.IdentityPage"

    property bool created: false
    property string label: ""
    property string address: ""
    property string error: ""

    signal backRequested()

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Theme.spacing.xlarge, 480)
        spacing: Theme.spacing.medium

        LogosText {
            Layout.fillWidth: true
            text: root.created ? qsTr("Identity created") : qsTr("Identity not created")
            font.pixelSize: Theme.typography.titleText
            font.weight: Theme.typography.weightBold
            color: Theme.palette.text
        }

        LogosText {
            Layout.fillWidth: true
            visible: root.created
            text: qsTr("“%1” in your LEZ wallet\n%2").arg(root.label).arg(root.address)
            wrapMode: Text.WrapAnywhere
            font.pixelSize: Theme.typography.secondaryText
            color: Theme.palette.textSecondary
        }

        LogosText {
            Layout.fillWidth: true
            visible: !root.created
            text: root.error
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.typography.secondaryText
            color: Theme.palette.error
        }

        LogosButton {
            visible: !root.created
            text: qsTr("Back")
            onClicked: root.backRequested()
        }
    }
}
