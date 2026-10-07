import QtQuick

import Logos.Theme
import Logos.Controls

// The small "testnet <chain id>" pill next to the app title.
LogosBadge {
    property string network: ""

    visible: network !== ""
    text: qsTr("testnet %1").arg(network)
    verticalPadding: 2
    radius: Theme.spacing.radiusPill
    color: Theme.palette.textTertiary
    backgroundColor: Theme.palette.backgroundTertiary
    borderColor: Theme.palette.borderSecondary
}
