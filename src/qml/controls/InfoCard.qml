import QtQuick
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

LogosFrame {
    id: root

    property string title
    property string status
    property string detail
    property string copyText
    property string note
    property bool showDetails: true
    default property alias content: extras.data

    Layout.fillWidth: true
    padding: Theme.spacing.large
    backgroundColor: Theme.palette.backgroundTertiary
    borderColor: Theme.palette.borderSecondary
    radius: Theme.spacing.radiusLarge

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing.small

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.small

            LogosText {
                Layout.fillWidth: true
                text: root.title
                font.pixelSize: Theme.typography.secondaryText
                font.letterSpacing: 0.6
                color: Theme.palette.textSecondary
            }
            LogosBadge {
                visible: root.status !== ""
                text: root.status
                color: Theme.palette.success
            }
        }

        LogosText {
            Layout.fillWidth: true
            visible: root.showDetails && root.detail !== "" && root.copyText === ""
            text: root.detail
            elide: Text.ElideRight
            font.pixelSize: Theme.typography.primaryText
            color: Theme.palette.text
        }

        LogosCopyableText {
            Layout.fillWidth: true
            visible: root.showDetails && root.detail !== "" && root.copyText !== ""
            text: root.detail
            copyText: root.copyText
            textColor: Theme.palette.text
        }

        LogosText {
            Layout.fillWidth: true
            visible: root.showDetails && root.note !== ""
            wrapMode: Text.WordWrap
            text: root.note
            font.pixelSize: Theme.typography.secondaryText
            color: Theme.palette.textTertiary
        }

        ColumnLayout {
            id: extras

            Layout.fillWidth: true
            spacing: Theme.spacing.small
        }
    }
}
