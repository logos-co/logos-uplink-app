import QtQuick

import Logos.Controls
import Logos.UplinkUi 1.0

// Why Uplink can't run here. Hidden while the deployment is fine or not checked yet.
LogosNotice {
    id: root

    objectName: "uplink.programNotice"

    property int issue: UplinkUi.ProgramUnknown
    property string detail: ""

    readonly property bool blocking: issue !== UplinkUi.ProgramUnknown && issue !== UplinkUi.ProgramReady

    shown: blocking
    severity: LogosNotice.Error
    message: {
        switch (root.issue) {
        case UplinkUi.ProgramNotConfigured:
            return qsTr("Uplink isn’t available on this network yet.")
        case UplinkUi.ProgramOtherNetwork:
            return qsTr("Uplink isn’t available on your node’s network.") + "\n\n" + root.detail
        case UplinkUi.ProgramNotDeployed:
            return qsTr("Uplink isn’t available on the network your LEZ wallet is connected to. Check the wallet is on the same network as your node.")
        case UplinkUi.ProgramMismatch:
            return qsTr("This version of Uplink doesn’t match the referral program on this network. Update Uplink to continue.")
        }
        return ""
    }
}
