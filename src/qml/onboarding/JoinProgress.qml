import QtQuick
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.Icons
import Logos.UplinkUi 1.0

import "../controls"

// Where joining is: approved in the Blockchain app, the registration prepared (proved, which
// takes minutes without dev-mode proofs), then confirmed. Shown in the Join step while it runs.
InfoCard {
    id: root

    objectName: "uplink.joinProgress"

    property int enrolState: UplinkUi.NoIdentity

    // -1 while not joining.
    readonly property int stage: {
        switch (root.enrolState) {
        case UplinkUi.AwaitingSignature: return 0
        case UplinkUi.Enrolling:         return 1
        case UplinkUi.EnrolSent:         return 2
        }
        return -1
    }

    title: qsTr("JOINING")

    QtObject {
        id: d

        property real stageStarted: Date.now()
        property real now: Date.now()

        readonly property string elapsed: {
            const seconds = Math.max(0, Math.floor((d.now - d.stageStarted) / 1000))
            return Math.floor(seconds / 60) + ":" + String(seconds % 60).padStart(2, "0")
        }
    }

    onStageChanged: {
        d.stageStarted = Date.now()
        d.now = d.stageStarted
    }

    Timer {
        interval: 1000
        repeat: true
        running: root.visible && root.stage >= 0
        onTriggered: d.now = Date.now()
    }

    Repeater {
        model: [
            { label: qsTr("Approve in the Blockchain app"),
              hint: qsTr("It shows your node and what it signs.") },
            { label: qsTr("Prepare the registration"),
              hint: qsTr("Your computer does this, and it can take a few minutes. You can use other apps meanwhile.") },
            { label: qsTr("Confirm on the network"),
              hint: qsTr("Sent. Waiting for the network to include it.") }
        ]

        delegate: GridLayout {
            id: row

            required property var modelData
            required property int index
            readonly property bool done: index < root.stage
            readonly property bool current: index === root.stage

            objectName: "uplink.joinStage" + index
            Layout.fillWidth: true
            columnSpacing: Theme.spacing.medium
            rowSpacing: 2

            // Placed by row and column: a grid skips invisible items when it flows them.
            Item {
                Layout.row: 0
                Layout.column: 0
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18

                LogosIcon {
                    anchors.fill: parent
                    visible: row.done
                    source: LogosIcons.check
                    color: Theme.palette.success
                }
                LogosSpinner {
                    anchors.fill: parent
                    visible: row.current
                    running: row.current
                    thickness: 2
                    dotSize: 4
                }
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 2
                    visible: !row.done && !row.current
                    radius: width / 2
                    color: "transparent"
                    border.color: Theme.palette.borderSecondary
                    border.width: 2
                }
            }
            LogosText {
                Layout.row: 0
                Layout.column: 1
                Layout.fillWidth: true
                text: row.modelData.label
                font.pixelSize: Theme.typography.primaryText
                color: row.current ? Theme.palette.text : Theme.palette.textSecondary
            }
            LogosText {
                objectName: "uplink.joinElapsed"
                Layout.row: 0
                Layout.column: 2
                visible: row.current
                text: d.elapsed
                font.family: Theme.typography.mono
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.textTertiary
            }

            LogosText {
                Layout.row: 1
                Layout.column: 1
                Layout.columnSpan: 2
                Layout.fillWidth: true
                visible: row.current
                wrapMode: Text.WordWrap
                text: row.modelData.hint
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.textTertiary
            }
        }
    }
}
