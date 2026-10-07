import QtQuick
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

// Placeholder: the home page for joined users comes next.
Item {
    id: root

    objectName: "uplink.OverviewPage"

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Theme.spacing.xlarge, 660)
        spacing: Theme.spacing.large

        LogosText {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Overview")
            font.pixelSize: Theme.typography.titleText
            font.weight: Theme.typography.weightBold
            color: Theme.palette.text
        }

        LogosText {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Your node, your referrals and your points will show here.")
            font.pixelSize: Theme.typography.primaryText
            color: Theme.palette.textSecondary
        }
    }
}
