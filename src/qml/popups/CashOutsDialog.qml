import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

// Every cash-out so far, each with a way to get its payout code again: after a restart,
// a cancelled signature, or a code that was never copied.
LogosDialog {
    id: root

    objectName: "uplink.CashOutsDialog"

    property var receipts: []   // [{index, account, points}], oldest first

    signal codeRequested(int index)

    width: parent ? Math.min(480, parent.width - 2 * Theme.spacing.xxlarge) : 480
    dim: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    title: qsTr("Your cash-outs")
    headerItem.font.pixelSize: Theme.typography.subtitleText   // the prototype's dialog title, not the 14 px default

    contentItem: ColumnLayout {
        spacing: Theme.spacing.medium

        LogosText {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Each cash-out has its own payout code. Get one again if you didn’t copy it; your node signs it again.")
            font.pixelSize: Theme.typography.primaryText
            color: Theme.palette.textSecondary
        }

        Repeater {
            model: root.receipts.slice().reverse()   // newest first

            RowLayout {
                id: row

                required property var modelData

                Layout.fillWidth: true
                spacing: Theme.spacing.small

                LogosText {
                    Layout.fillWidth: true
                    text: qsTr("Cash-out %1").arg(row.modelData.index + 1)
                    font.pixelSize: Theme.typography.primaryText
                    color: Theme.palette.text
                }
                LogosText {
                    text: qsTr("%1 points").arg(row.modelData.points)
                    font.pixelSize: Theme.typography.primaryText
                    font.weight: Theme.typography.weightBold
                    color: Theme.palette.text
                }
                LogosButton {
                    objectName: "uplink.cashOutCodeButton"
                    font.pixelSize: Theme.typography.primaryText
                    text: qsTr("Get code")
                    onClicked: {
                        root.close()
                        root.codeRequested(row.modelData.index)
                    }
                }
            }
        }
    }

    rightActions: [
        LogosButton {
            variant: LogosButton.Variant.Primary
            text: qsTr("Done")
            onClicked: root.close()
        }
    ]
}
