import QtQuick
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

LogosFrame {
    id: root

    property string title: ""
    property string description: ""
    property bool selected: false

    signal picked()

    Layout.fillWidth: true
    radius: Theme.spacing.radiusLarge
    backgroundColor: Theme.palette.surfaceRaised
    borderColor: Theme.palette.border
    padding: 0

    contentItem: Item {
        implicitWidth: row.implicitWidth + 2 * Theme.spacing.large
        implicitHeight: row.implicitHeight + 2 * Theme.spacing.large

        RowLayout {
            id: row

            anchors.fill: parent
            anchors.margins: Theme.spacing.large
            spacing: Theme.spacing.medium

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                LogosText {
                    Layout.fillWidth: true
                    text: root.title
                    color: Theme.palette.text
                    font.pixelSize: Theme.typography.primaryText
                    font.weight: Theme.typography.weightBold
                }

                LogosText {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: root.description
                    color: Theme.palette.textSecondary
                    font.pixelSize: Theme.typography.secondaryText
                    wrapMode: Text.WordWrap
                }
            }

            LogosRadioButton {
                Layout.alignment: Qt.AlignVCenter
                checked: root.selected
                onClicked: root.picked()
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.picked()
        }
    }
}
