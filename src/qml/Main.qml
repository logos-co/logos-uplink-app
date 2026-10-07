import QtQuick

// Placeholder view until the real UI lands.
Item {
    readonly property var backend: logos.module("uplink_ui")

    Text {
        anchors.centerIn: parent
        text: backend && backend.ready ? "Uplink" : "Uplink: connecting…"
    }
}
