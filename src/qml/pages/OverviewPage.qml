import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.UplinkUi 1.0

import "../controls"
import "../popups"

// Home for joined users: points and actions on top, then this node and each direct
// referral with their last 16 published epochs.
Item {
    id: root

    objectName: "uplink.OverviewPage"

    property string network: ""
    property string points: "0"
    property string cashedOut: "0"   // every receipt so far
    property var receipts: []
    property int nodeIssue: UplinkUi.ModuleUnavailable
    property string nodeId: ""
    property bool nodeActive: false
    property var myActivity: []
    property var referrals: []
    property string referrerNode: ""
    property string invitation: ""
    property int cashOutState: UplinkUi.CashOutIdle
    property string payoutCode: ""
    property string payoutPoints: ""
    property string error: ""
    property string payoutFormUrl: ""

    signal cashOutRequested()
    signal signAgainRequested()
    signal payoutCodeRequested(int index)
    signal cashOutFinished()
    signal labelEdited(string node, string label)

    onMyActivityChanged: d.rebuild()
    onReferralsChanged: d.rebuild()
    onNodeIdChanged: d.rebuild()
    onNodeActiveChanged: d.rebuild()
    Component.onCompleted: d.rebuild()

    QtObject {
        id: d

        readonly property bool nodeDown: root.nodeIssue === UplinkUi.ModuleUnavailable
                                         || root.nodeIssue === UplinkUi.NodeNotRunning

        function shortId(id) {
            return id.length > 20 ? id.slice(0, 10) + "…" + id.slice(-8) : id
        }

        // From the newest recorded epochs: how the node has been doing lately.
        function status(marks) {
            if (!marks || marks.length === 0 || marks[marks.length - 1] === -1)
                return { text: qsTr("No data yet"), color: String(Theme.palette.textTertiary) }
            if (marks[marks.length - 1] === 1)
                return { text: qsTr("Active"), color: String(Theme.palette.success) }
            let missed = 0
            for (let i = marks.length - 1; i >= 0 && marks[i] === 0; --i)
                ++missed
            if (missed === 1)
                return { text: qsTr("Missed last epoch"), color: String(Theme.palette.warning) }
            return { text: qsTr("Inactive for %1 epochs").arg(missed), color: String(Theme.palette.textTertiary) }
        }

        // ListModel roles can't hold arrays, so the marks travel as "1,0,-1,…".
        function rebuild() {
            const next = []
            const me = status(root.myActivity)
            next.push({
                node: root.nodeId, mine: true, name: qsTr("My node"), labelled: true,
                sub: shortId(root.nodeId), marks: (root.myActivity || []).join(","),
                statusText: me.text, statusColor: me.color,
                note: root.nodeActive ? qsTr("can collect") : qsTr("can’t collect yet"),
            })
            for (const r of (root.referrals || [])) {
                const st = status(r.activity)
                next.push({
                    node: r.node, mine: false, name: r.label || shortId(r.node), labelled: r.label !== "",
                    sub: r.label ? shortId(r.node) : "", marks: (r.activity || []).join(","),
                    statusText: st.text, statusColor: st.color, note: "",
                })
            }
            // In place, so the table's cells aren't torn down on every poll.
            for (let i = 0; i < next.length; ++i) {
                if (i < rows.count)
                    rows.set(i, next[i])
                else
                    rows.append(next[i])
            }
            if (rows.count > next.length)
                rows.remove(next.length, rows.count - next.length)
        }

        // What a cell reads while LogosTable tears its row down and rowItem is null.
        readonly property var emptyRow: ({
            node: "", mine: true, name: "", labelled: false, sub: "", marks: "",
            statusText: "", statusColor: "transparent", note: "",
        })

        function editLabel(node, current) {
            labelDialog.node = node
            labelField.text = current
            labelDialog.open()
        }
    }

    ListModel { id: rows }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- Header: title and tagline; points and actions ----------------------

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacing.xxlarge
            Layout.rightMargin: Theme.spacing.xxlarge
            Layout.topMargin: Theme.spacing.xlarge
            spacing: Theme.spacing.large

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing.tiny

                RowLayout {
                    spacing: Theme.spacing.small

                    LogosText {
                        text: qsTr("Logos Uplink")
                        font.pixelSize: Theme.typography.panelTitleText
                        font.weight: Theme.typography.weightBold
                        color: Theme.palette.text
                    }
                    NetworkTag {
                        Layout.alignment: Qt.AlignVCenter
                        network: root.network
                    }
                }
                LogosText {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Help stress-test the Logos Testnet: bring more people in to run nodes and keep yours active.")
                    font.pixelSize: Theme.typography.secondaryText
                    color: Theme.palette.textSecondary
                }
            }

            ColumnLayout {
                spacing: 0

                LogosText {
                    objectName: "uplink.pointsBalance"
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("%1 points").arg(root.points)
                    font.pixelSize: Theme.typography.subtitleText
                    font.weight: Theme.typography.weightBold
                    color: Theme.palette.text
                }
                LogosLink {
                    objectName: "uplink.cashedOut"
                    Layout.alignment: Qt.AlignRight
                    visible: root.receipts.length > 0
                    text: qsTr("%1 cashed out so far").arg(root.cashedOut)
                    labelItem.font.pixelSize: Theme.typography.secondaryText
                    underline: false   // until hovered
                    linkColor: Theme.palette.textTertiary
                    hoverColor: Theme.palette.textSecondary
                    onActivated: cashOutsDialog.open()
                }
            }
            LogosButton {
                objectName: "uplink.claimRewardsButton"
                font.pixelSize: Theme.typography.primaryText
                text: qsTr("Claim rewards")
                onClicked: claimDialog.open()
            }
            LogosButton {
                objectName: "uplink.invitePeerButton"
                variant: LogosButton.Variant.Primary
                font.pixelSize: Theme.typography.primaryText
                text: qsTr("Invite peer")
                onClicked: inviteDialog.open()
            }
        }

        LogosTabBar {
            id: tabs

            objectName: "uplink.overviewTabs"
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacing.xxlarge
            Layout.rightMargin: Theme.spacing.xxlarge
            Layout.topMargin: Theme.spacing.large

            LogosTabButton {
                width: implicitWidth
                text: qsTr("Overview")
                font.pixelSize: Theme.typography.secondaryText
            }
            LogosTabButton {
                width: implicitWidth
                text: qsTr("About")
                font.pixelSize: Theme.typography.secondaryText
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Theme.spacing.xxlarge
            Layout.rightMargin: Theme.spacing.xxlarge
            Layout.topMargin: Theme.spacing.large
            currentIndex: tabs.currentIndex

            // ---- Overview tab ------------------------------------------------------

            ColumnLayout {
                spacing: Theme.spacing.large

                LogosNotice {
                    objectName: "uplink.nodeDownBanner"
                    Layout.fillWidth: true
                    shown: d.nodeDown
                    severity: LogosNotice.Warning
                    message: qsTr("Your node is not running — referral points are paused until it is back.")
                }

                LogosTable {
                    id: nodeTable

                    objectName: "uplink.nodeTable"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: rows
                    rowHeight: 60

                    columns: [
                        LogosTableColumn {
                            title: qsTr("Node")
                            minWidth: 200
                            preferredWidth: 260
                            fillWidth: true
                            cellDelegate: nodeCell
                        },
                        LogosTableColumn {
                            title: qsTr("Activity · last 16 epochs")
                            minWidth: 220
                            preferredWidth: 240
                            maxWidth: 260
                            cellDelegate: activityCell
                        },
                        // Node and Status share the spare width, so a wide screen
                        // doesn't push Status to the far edge.
                        LogosTableColumn {
                            title: qsTr("Status")
                            minWidth: 200
                            preferredWidth: 240
                            fillWidth: true
                            cellDelegate: statusCell
                        },
                        LogosTableColumn {
                            title: ""
                            minWidth: 56
                            preferredWidth: 56
                            maxWidth: 56
                            cellDelegate: editCell
                        }
                    ]
                }

                LogosText {
                    objectName: "uplink.noReferrals"
                    Layout.fillWidth: true
                    visible: root.referrals.length === 0
                    wrapMode: Text.WordWrap
                    textFormat: Text.StyledText
                    text: qsTr("No referrals yet. Use <b>Invite peer</b> to share your invitation — invitees import it, join, and appear here. You only ever see this one direct layer.")
                    font.pixelSize: Theme.typography.secondaryText
                    color: Theme.palette.textSecondary
                }

                Flow {
                    Layout.fillWidth: true
                    Layout.bottomMargin: Theme.spacing.xlarge
                    spacing: Theme.spacing.large

                    LegendItem { mark: 1; text: qsTr("Active") }
                    LegendItem { mark: 0; text: qsTr("Not active") }
                    LegendItem { mark: -1; text: qsTr("Not recorded") }
                    LegendItem { current: true; text: qsTr("Current epoch") }
                }
            }

            // ---- About tab ---------------------------------------------------------

            AboutView {
                referrerNode: root.referrerNode
                onReadTermsRequested: termsDialog.open()
            }
        }
    }

    // ---- Table cells (rowItem is the ListModel row) ------------------------------

    Component {
        id: nodeCell

        RowLayout {
            id: nodeCellRoot

            readonly property var r: rowItem ? rowItem : d.emptyRow

            spacing: Theme.spacing.small

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 0

                LogosText {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: nodeCellRoot.r.name
                    font.pixelSize: Theme.typography.primaryText
                    font.weight: nodeCellRoot.r.mine ? Theme.typography.weightBold : Theme.typography.weightRegular
                    color: Theme.palette.text
                }
                LogosText {
                    Layout.fillWidth: true
                    visible: nodeCellRoot.r.sub !== ""
                    elide: Text.ElideRight
                    text: nodeCellRoot.r.sub
                    font.pixelSize: Theme.typography.secondaryText
                    color: Theme.palette.textTertiary
                }
            }
        }
    }

    Component {
        id: editCell

        Item {
            id: editCellRoot

            readonly property var r: rowItem ? rowItem : d.emptyRow

            LogosIconButton {
                objectName: "uplink.editLabelButton"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: !editCellRoot.r.mine
                flat: true
                size: 28
                iconSize: 16
                iconSource: Qt.resolvedUrl("../icons/edit.svg")
                iconColor: Theme.palette.textTertiary
                onClicked: d.editLabel(editCellRoot.r.node, editCellRoot.r.labelled ? editCellRoot.r.name : "")
            }
        }
    }

    Component {
        id: activityCell

        Item {
            id: activityCellRoot

            readonly property var r: rowItem ? rowItem : d.emptyRow

            ActivityDots {
                anchors.verticalCenter: parent.verticalCenter
                marks: activityCellRoot.r.marks ? activityCellRoot.r.marks.split(",").map(Number) : []
                live: !(activityCellRoot.r.mine && d.nodeDown)
            }
        }
    }

    Component {
        id: statusCell

        RowLayout {
            id: statusCellRoot

            readonly property var r: rowItem ? rowItem : d.emptyRow

            spacing: Theme.spacing.small

            LogosBadge {
                Layout.alignment: Qt.AlignVCenter
                text: statusCellRoot.r.statusText
                color: statusCellRoot.r.statusColor
            }
            LogosText {
                Layout.alignment: Qt.AlignVCenter
                visible: statusCellRoot.r.note !== ""
                text: "(" + statusCellRoot.r.note + ")"
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.textTertiary
            }
            // Keeps the note beside the badge; a row with nothing to fill spreads its items.
            Item { Layout.fillWidth: true }
        }
    }

    TermsDialog {
        id: termsDialog

        anchors.centerIn: parent
    }

    InviteDialog {
        id: inviteDialog

        anchors.centerIn: parent
        invitation: root.invitation
    }

    ClaimDialog {
        id: claimDialog

        anchors.centerIn: parent
        points: root.points
        referralCount: root.referrals.length
        nodeActive: root.nodeActive
        cashOutState: root.cashOutState
        payoutCode: root.payoutCode
        payoutPoints: root.payoutPoints
        error: root.error
        formUrl: root.payoutFormUrl
        onCashOutRequested: root.cashOutRequested()
        onSignAgainRequested: root.signAgainRequested()
        onInviteRequested: inviteDialog.open()
        onFinished: root.cashOutFinished()
    }

    CashOutsDialog {
        id: cashOutsDialog

        anchors.centerIn: parent
        receipts: root.receipts
        onCodeRequested: function (index) {
            root.payoutCodeRequested(index)
            claimDialog.open()
        }
    }

    // ---- Label dialog -------------------------------------------------------------

    LogosDialog {
        id: labelDialog

        property string node: ""

        objectName: "uplink.labelDialog"
        anchors.centerIn: parent
        width: Math.min(400, parent.width - 2 * Theme.spacing.xxlarge)
        dim: true
        title: qsTr("Label this referral")
        headerItem.font.pixelSize: Theme.typography.subtitleText

        // Ready to type: focus the field, cursor after any current label.
        onOpened: {
            labelField.textInput.forceActiveFocus()
            labelField.textInput.cursorPosition = labelField.text.length
        }
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        contentItem: ColumnLayout {
            spacing: Theme.spacing.small

            LogosTextField {
                id: labelField

                Layout.fillWidth: true
                placeholderText: d.shortId(labelDialog.node)
            }
            LogosText {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Only on this device. Leave it empty to show the node ID.")
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.textTertiary
            }
        }

        rightActions: [
            LogosButton {
                text: qsTr("Cancel")
                compact: true
                font.pixelSize: Theme.typography.primaryText
                font.weight: Theme.typography.weightBold
                background: Item {}
                onClicked: labelDialog.close()
            },
            LogosButton {
                objectName: "uplink.saveLabelButton"
                variant: LogosButton.Variant.Primary
                text: qsTr("Save")
                onClicked: {
                    root.labelEdited(labelDialog.node, labelField.text.trim())
                    labelDialog.close()
                }
            }
        ]
    }

    component LegendItem: Row {
        id: legend

        property int mark: 0
        property bool current: false
        property string text

        spacing: Theme.spacing.small

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 8
            height: 8
            radius: 4
            color: legend.current ? Theme.palette.primary
                 : legend.mark === 1 ? Theme.palette.success
                 : legend.mark === 0 ? Theme.palette.borderSecondary
                 : "transparent"
            border.width: !legend.current && legend.mark === -1 ? 1 : 0
            border.color: Theme.palette.borderSecondary
        }
        LogosText {
            text: legend.text
            font.pixelSize: Theme.typography.secondaryText
            color: Theme.palette.textSecondary
        }
    }
}
