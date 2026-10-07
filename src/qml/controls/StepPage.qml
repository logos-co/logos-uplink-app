import QtQuick
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

LogosScrollView {
    id: root

    default property alias content: column.data

    Item {
        width: root.availableWidth
        implicitHeight: column.implicitHeight

        ColumnLayout {
            id: column

            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(parent.width, 660)
            spacing: Theme.spacing.large
        }
    }
}
