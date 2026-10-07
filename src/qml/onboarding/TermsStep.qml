import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

import "Terms.js" as Terms

// Step 1. Acceptance is gated: scroll the terms to the end, then tick the box.
Item {
    id: root

    objectName: "uplink.TermsStep"

    readonly property bool accepted: agree.checked

    function reset() {
        d.readToEnd = false
        agree.checked = false
        terms.contentY = 0
    }

    QtObject {
        id: d

        property bool readToEnd: false

        function checkReadToEnd() {
            if (terms.contentHeight > terms.height
                    && terms.contentY + terms.height >= terms.contentHeight - 8)
                d.readToEnd = true
        }
    }

    ColumnLayout {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width, 660)
        spacing: Theme.spacing.small

        LogosText {
            Layout.fillWidth: true
            text: qsTr("Read to the end to enable acceptance.")
            font.pixelSize: Theme.typography.secondaryText
            color: Theme.palette.textTertiary
        }

        // The terms sit in a recessed box.
        LogosFrame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            padding: Theme.spacing.medium
            backgroundColor: Theme.palette.backgroundInset
            borderColor: Theme.palette.borderSecondary
            radius: Theme.spacing.radiusSmall

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

        LogosCheckbox {
            id: agree

            objectName: "uplink.termsCheckbox"
            Layout.topMargin: Theme.spacing.small
            enabled: d.readToEnd
            text: qsTr("I have read and accept the program terms")
        }
    }
}
