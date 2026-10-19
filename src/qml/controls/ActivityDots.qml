import QtQuick

import Logos.Theme

// The last published epochs as dots, oldest first, then the epoch in progress.
// marks: 1 active, 0 not active, -1 unknown (not recorded), -2 before the node joined (a dash).
Row {
    id: root

    property var marks: []
    property bool live: true   // the epoch in progress pulses

    spacing: 3

    Repeater {
        model: root.marks

        Item {
            id: slot

            required property var modelData

            width: 8
            height: 8

            Rectangle {
                anchors.fill: parent
                visible: slot.modelData !== -2
                radius: 4
                color: slot.modelData === 1 ? Theme.palette.success
                     : slot.modelData === 0 ? Theme.palette.borderSecondary
                     : "transparent"
                border.width: slot.modelData === -1 ? 1 : 0
                border.color: Theme.palette.borderSecondary
            }
            Rectangle {
                anchors.centerIn: parent
                visible: slot.modelData === -2
                width: 6
                height: 1
                color: Theme.palette.borderSecondary
            }
        }
    }

    Rectangle {
        objectName: "uplink.currentEpochDot"
        width: 8
        height: 8
        radius: 4
        color: root.live ? Theme.palette.primary : Theme.palette.borderSecondary

        SequentialAnimation on opacity {
            running: root.live
            loops: Animation.Infinite
            NumberAnimation { to: 0.35; duration: 750; easing.type: Easing.InOutSine }
            NumberAnimation { to: 1; duration: 750; easing.type: Easing.InOutSine }
        }
    }
}
