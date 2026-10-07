import QtQuick
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls

import "../controls"

Item {
    id: root

    objectName: "uplink.JoinedPage"

    property string referrerNode: ""   // "" = joined as a root
    property string accountLabel: ""
    property string accountAddress: ""

    signal finished()

    QtObject {
        id: d

        function shortId(id) {
            return id.length > 20 ? id.slice(0, 10) + "…" + id.slice(-8) : id
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Theme.spacing.xlarge, 660)
        spacing: Theme.spacing.large

        LogosText {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("You’re in")
            font.pixelSize: Theme.typography.titleText
            font.weight: Theme.typography.weightBold
            color: Theme.palette.text
        }

        LogosText {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.min(parent.width, 580)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            textFormat: Text.StyledText
            text: root.referrerNode !== ""
                  ? qsTr("<b>Your position is sealed.</b> You joined under your inviter. It can’t be changed, and nobody, including us, can read who invited you.")
                  : qsTr("<b>You’re enrolled as a root.</b> You continued without a referral — invite people to grow your own tree.")
            font.pixelSize: Theme.typography.primaryText
            color: Theme.palette.textSecondary
        }

        InfoCard {
            objectName: "uplink.pointsAccount"
            Layout.topMargin: Theme.spacing.small
            visible: root.accountAddress !== ""
            title: root.accountLabel.toUpperCase()
            detail: d.shortId(root.accountAddress)
            copyText: root.accountAddress
            note: qsTr("A private account in your LEZ wallet. Your points are kept here; only you can see them.")
        }

        LogosButton {
            objectName: "uplink.finishButton"
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Theme.spacing.small
            variant: LogosButton.Variant.Primary
            font.pixelSize: Theme.typography.primaryText
            text: qsTr("Finish")
            onClicked: root.finished()
        }
    }
}
