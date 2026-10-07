import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

import "Terms.js" as Terms

LogosDialog {
    id: root

    objectName: "uplink.TermsDialog"

    signal termsAccepted()

    width: parent ? Math.min(560, parent.width - 2 * Theme.spacing.xxlarge) : 560
    height: parent ? Math.min(640, parent.height - 2 * Theme.spacing.xxlarge) : 640
    dim: true
    closePolicy: Popup.CloseOnEscape
    bottomPadding: 0

    onOpened: {
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

    contentItem: ColumnLayout {
        spacing: Theme.spacing.small

        LogosText {
            Layout.fillWidth: true
            text: qsTr("Program terms")
            font.pixelSize: Theme.typography.subtitleText
            font.weight: Theme.typography.weightBold
            color: Theme.palette.text
        }

        LogosText {
            Layout.fillWidth: true
            text: qsTr("Read to the end to enable acceptance.")
            font.pixelSize: Theme.typography.secondaryText
            color: Theme.palette.textTertiary
        }

        LogosFrame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: Theme.spacing.small
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
            enabled: d.readToEnd
            text: qsTr("I have read and accept the program terms")
        }
    }

    rightActions: [
        LogosButton {
            objectName: "uplink.termsDeclineButton"
            anchors.verticalCenter: parent ? parent.verticalCenter : undefined
            text: qsTr("Decline")
            compact: true
            font.pixelSize: Theme.typography.primaryText
            font.weight: Theme.typography.weightBold
            background: Item {}
            onClicked: root.close()
        },
        LogosButton {
            objectName: "uplink.termsAcceptButton"
            variant: LogosButton.Variant.Primary
            text: qsTr("Accept and continue")
            enabled: agree.checked
            onClicked: {
                root.close()
                root.termsAccepted()
            }
        }
    ]
}
