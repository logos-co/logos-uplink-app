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

            width: parent.width
            spacing: Theme.spacing.large
        }
    }
}
