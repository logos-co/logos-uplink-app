import QtQuick

import Logos.Theme

// The last published epochs as dots, oldest first, then the epoch in progress.
// marks: 1 active, 0 not active, -1 unknown (not recorded).
Row {
    id: root

    property var marks: []
    property bool live: true   // the epoch in progress pulses

    spacing: 3

    Repeater {
        model: root.marks

        Rectangle {
            required property var modelData

            width: 8
            height: 8
            radius: 4
            color: modelData === 1 ? Theme.palette.success
                 : modelData === 0 ? Theme.palette.borderSecondary
                 : "transparent"
            border.width: modelData === -1 ? 1 : 0
            border.color: Theme.palette.borderSecondary
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
