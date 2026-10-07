import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

import "Terms.js" as Terms

// The program terms in a recessed, scrolling box. Shared by the onboarding step
// (which gates acceptance on readToEnd) and the read-again dialog.
LogosFrame {
    id: root

    readonly property bool readToEnd: d.readToEnd

    function reset() {
        d.readToEnd = false
        terms.contentY = 0
    }

    padding: Theme.spacing.medium
    backgroundColor: Theme.palette.backgroundInset
    borderColor: Theme.palette.borderSecondary
    radius: Theme.spacing.radiusSmall

    QtObject {
        id: d

        property bool readToEnd: false

        // Latched by the user scrolling to the bottom (8 px slack), never by layout.
        function checkReadToEnd() {
            if (terms.contentHeight > terms.height
                    && terms.contentY + terms.height >= terms.contentHeight - 8)
                d.readToEnd = true
        }
    }

    Flickable {
        id: terms

        anchors.fill: parent
        contentHeight: termsColumn.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: LogosScrollBar {}

        onContentYChanged: d.checkReadToEnd()

        ColumnLayout {
            id: termsColumn

            width: terms.width - Theme.spacing.medium
            spacing: Theme.spacing.medium

            Repeater {
                model: Terms.sections

                ColumnLayout {
                    id: section

                    required property var modelData

                    Layout.fillWidth: true
                    spacing: Theme.spacing.small

                    LogosText {
                        Layout.fillWidth: true
                        text: section.modelData.title
                        wrapMode: Text.WordWrap
                        font.pixelSize: Theme.typography.primaryText
                        font.weight: Theme.typography.weightBold
                        color: Theme.palette.text
                    }

                    Repeater {
                        model: section.modelData.items

                        LogosText {
                            id: item

                            required property var modelData

                            Layout.fillWidth: true
                            leftPadding: (item.modelData.i || 0) * Theme.spacing.large
                            text: item.modelData.t
                            wrapMode: Text.WordWrap
                            font.pixelSize: Theme.typography.secondaryText
                            font.weight: item.modelData.h ? Theme.typography.weightBold : Theme.typography.weightRegular
                            color: item.modelData.h ? Theme.palette.text : Theme.palette.textSecondary
                        }
                    }
                }
            }
        }
    }
}
