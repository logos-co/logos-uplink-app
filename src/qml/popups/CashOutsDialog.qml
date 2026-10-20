import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.UplinkUi 1.0

// Every cash-out so far, each with a way to get its payout code again: after a restart,
// a cancelled signature, or a code that was never copied.
LogosDialog {
    id: root

    objectName: "uplink.CashOutsDialog"

    property var receipts: []   // [{index, account, points}], oldest first
    property int search: UplinkUi.SearchIdle
    property int found: 0
    property string error: ""

    signal codeRequested(int index)
    signal findRequested()

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
            text: qsTr("Each cash-out has its own payout code. Get one again if you didn’t copy it; it’s signed again with your Blend key.")
            font.pixelSize: Theme.typography.primaryText
            color: Theme.palette.textSecondary
        }

        LogosText {
            objectName: "uplink.noCashOuts"
            Layout.fillWidth: true
            visible: root.receipts.length === 0
            wrapMode: Text.WordWrap
            text: qsTr("No cash-outs here yet.")
            font.pixelSize: Theme.typography.primaryText
            color: Theme.palette.textTertiary
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

        // Basecamp remembers each cash-out it makes. Earlier ones it doesn't know of (its data
        // reset, or made elsewhere) are found on chain with the node's receipt secret.
        LogosText {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacing.small
            wrapMode: Text.WordWrap
            text: qsTr("Missing some? Cash-outs from before Basecamp's data was reset, or made on another computer, are listed once they're found. The Blockchain app asks you first.")
            font.pixelSize: Theme.typography.secondaryText
            color: Theme.palette.textTertiary
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.medium

            LogosButton {
                objectName: "uplink.findCashOutsButton"
                font.pixelSize: Theme.typography.primaryText
                enabled: root.search !== UplinkUi.SearchAwaitingKey
                text: root.search === UplinkUi.SearchAwaitingKey ? qsTr("Approve in the Blockchain app…")
                                                                 : qsTr("Find earlier cash-outs")
                onClicked: root.findRequested()
            }
            LogosText {
                objectName: "uplink.findCashOutsResult"
                Layout.fillWidth: true
                visible: root.search === UplinkUi.SearchDone
                wrapMode: Text.WordWrap
                text: root.found === 0 ? qsTr("No others found.")
                      : root.found === 1 ? qsTr("Found 1 earlier cash-out.")
                      : qsTr("Found %1 earlier cash-outs.").arg(root.found)
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.textSecondary
            }
        }
        LogosNotice {
            objectName: "uplink.findCashOutsError"
            Layout.fillWidth: true
            shown: root.error !== "" && root.search !== UplinkUi.SearchAwaitingKey
            severity: LogosNotice.Error
            message: root.error
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
