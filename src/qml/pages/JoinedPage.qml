import QtQuick
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

// Placeholder for the step after joining.
Item {
    id: root

    objectName: "uplink.JoinedPage"

    property string referrerNode: ""   // "" = joined as a root
    property string accountLabel: ""
    property string accountAddress: ""

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Theme.spacing.xlarge, 620)
        spacing: Theme.spacing.medium

        LogosText {
            Layout.fillWidth: true
            text: qsTr("You’re in")
            font.pixelSize: Theme.typography.panelTitleText
            font.weight: Theme.typography.weightBold
            color: Theme.palette.text
        }

        LogosText {
            Layout.fillWidth: true
            wrapMode: Text.WrapAnywhere
            text: root.referrerNode !== ""
                  ? qsTr("Your position is sealed under %1.").arg(root.referrerNode)
                  : qsTr("You’re enrolled as a root. Invite people to grow your own tree.")
            font.pixelSize: Theme.typography.primaryText
            color: Theme.palette.textSecondary
        }

        LogosFrame {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacing.small
            visible: root.accountAddress !== ""
            padding: Theme.spacing.large
            backgroundColor: Theme.palette.backgroundTertiary
            borderColor: Theme.palette.borderSecondary
            radius: Theme.spacing.radiusLarge

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.spacing.small

                LogosText {
                    text: root.accountLabel.toUpperCase()
                    font.pixelSize: Theme.typography.secondaryText
                    font.letterSpacing: 0.6
                    color: Theme.palette.textSecondary
                }
                LogosCopyableText {
                    objectName: "uplink.pointsAccount"
                    Layout.fillWidth: true
                    text: root.accountAddress.length > 20
                          ? root.accountAddress.slice(0, 10) + "…" + root.accountAddress.slice(-8)
                          : root.accountAddress
                    copyText: root.accountAddress
                    textColor: Theme.palette.textSecondary
                }
                LogosText {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("A private account in your LEZ wallet. Your points are kept here; only you can see them.")
                    font.pixelSize: Theme.typography.secondaryText
                    color: Theme.palette.textTertiary
                }
            }
        }
    }
}
