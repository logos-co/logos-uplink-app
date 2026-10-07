import QtQuick

// Placeholder view: shows the backend's state until the real UI lands.
Item {
    readonly property var backend: logos.module("uplink_ui")

    Text {
        anchors.centerIn: parent
        horizontalAlignment: Text.AlignHCenter
        text: !backend ? "Uplink: connecting…"
            : "Uplink"
              + "\nnode issue " + backend.nodeIssue + ", enrol state " + backend.enrolState
              + "\nepoch " + backend.epoch + ", claimable " + backend.claimablePoints
              + ", lifetime " + backend.lifetimePoints
              + (backend.lastError ? "\n" + backend.lastError : "")
    }
}
