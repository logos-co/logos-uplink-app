pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import Logos.Theme
import Logos.Controls
import Logos.UplinkUi 1.0

ColumnLayout {
    id: root

    objectName: "uplink.Onboarding"

    property int nodeIssue: UplinkUi.ModuleUnavailable
    property string nodeId: ""
    property int walletIssue: UplinkUi.LezCoreUnavailable
    property string walletDetail: ""
    property int syncedBlock: 0
    property int chainHeight: 0
    property int invitationCheck: UplinkUi.InvitationEmpty
    property int enrolState: UplinkUi.NoIdentity
    property bool creatingAccount: false
    property string error: ""
    property string referrerNode: ""
    property string accountLabel: ""
    property string accountAddress: ""

    signal exitRequested()
    signal joinRequested(string invitation)   // "" = as a root
    signal invitationEdited(string text)
    signal openBlockchainAppRequested()
    signal openLezWalletRequested()
    signal finished()

    function begin() {
        termsStep.reset()
        d.stepIndex = 0
    }

    onEnrolStateChanged: {
        if (root.enrolState === UplinkUi.Enrolled && d.step === "join")
            d.stepIndex = d.steps.indexOf("done")
    }

    spacing: Theme.spacing.large

    QtObject {
        id: d

        property int stepIndex: 0
        readonly property var steps: ["terms", "ready", "join", "done"]
        readonly property string step: steps[stepIndex]
        readonly property var stepNames: [qsTr("Terms"), qsTr("Ready"), qsTr("Join"), qsTr("Done")]

        readonly property bool busy: root.creatingAccount
                                     || root.enrolState === UplinkUi.Invited
                                     || root.enrolState === UplinkUi.AwaitingSignature
                                     || root.enrolState === UplinkUi.Enrolling

        readonly property string phase: {
            if (root.creatingAccount)
                return qsTr("Creating account…")
            switch (root.enrolState) {
            case UplinkUi.Invited: return qsTr("Importing invitation…")
            case UplinkUi.AwaitingSignature: return qsTr("Waiting for signature…")
            case UplinkUi.Enrolling: return qsTr("Joining…")
            }
            return ""
        }

        readonly property string subtitle: {
            switch (step) {
            case "terms": return qsTr("Read and accept the program terms.")
            case "ready": return qsTr("Your node signs your join, and your LEZ wallet keeps your points.")
            case "join":  return qsTr("Join under the person who invited you, or start your own tree.")
            case "done":  return qsTr("You’re in.")
            }
            return ""
        }

        readonly property bool canGoBack: step !== "done" && !busy

        readonly property bool canAdvance: {
            switch (step) {
            case "terms": return termsStep.accepted
            case "ready": return readyStep.ready
            case "join":  return !busy && readyStep.ready
                                 && (joinStep.mode === "root"
                                     || root.invitationCheck === UplinkUi.InvitationOk)
            case "done":  return true
            }
            return false
        }

        readonly property string advanceText: {
            if (busy)
                return phase
            switch (step) {
            case "join":  return joinStep.mode === "root" ? qsTr("Join as root")
                                                          : qsTr("Import invitation & sign")
            case "done":  return qsTr("Finish")
            }
            return qsTr("Continue")
        }

        readonly property string advanceHint: {
            if (busy)
                return ""
            switch (step) {
            case "terms": return termsStep.accepted ? "" : qsTr("Read to the end and tick the box to continue.")
            case "ready": return readyStep.ready ? "" : qsTr("Your node and LEZ wallet need to be ready.")
            case "join":  return readyStep.ready ? "" : qsTr("Your node or wallet stopped. Go back to check.")
            }
            return ""
        }

        function back() {
            if (stepIndex === 0)
                root.exitRequested()
            else
                stepIndex -= 1
        }

        function advance() {
            switch (step) {
            case "join":
                root.joinRequested(joinStep.mode === "root" ? "" : joinStep.invitationText)
                return
            case "done":
                root.finished()
                return
            }
            stepIndex += 1
        }
    }

    // ---- Header: title, subtitle, step rail ---------------------------------

    ColumnLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Theme.spacing.xxlarge
        Layout.rightMargin: Theme.spacing.xxlarge
        Layout.topMargin: Theme.spacing.xxlarge
        spacing: Theme.spacing.large

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            LogosText {
                text: qsTr("Join the referral program")
                font.pixelSize: Theme.typography.titleText
                font.weight: Theme.typography.weightBold
                color: Theme.palette.text
            }
            LogosText {
                objectName: "uplink.onboardingSubtitle"
                text: d.subtitle
                font.pixelSize: Theme.typography.secondaryText
                color: Theme.palette.textSecondary
            }
        }

        RowLayout {
            objectName: "uplink.onboardingStepRail"
            Layout.fillWidth: true
            spacing: Theme.spacing.small

            Repeater {
                model: d.stepNames

                delegate: ColumnLayout {
                    id: railStep

                    required property int index
                    required property string modelData

                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    spacing: 4

                    Rectangle {
                        Layout.fillWidth: true
                        height: 4
                        radius: 2
                        color: railStep.index <= d.stepIndex ? Theme.palette.primary : Theme.palette.border
                    }
                    LogosText {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: (railStep.index + 1) + ". " + railStep.modelData
                        font.pixelSize: Theme.typography.secondaryText
                        font.weight: railStep.index === d.stepIndex ? Theme.typography.weightBold
                                                                     : Theme.typography.weightRegular
                        color: railStep.index === d.stepIndex ? Theme.palette.text : Theme.palette.textTertiary
                    }
                }
            }
        }
    }

    // ---- Steps ---------------------------------------------------------------

    StackLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: Theme.spacing.xxlarge
        Layout.rightMargin: Theme.spacing.xxlarge
        currentIndex: d.stepIndex

        TermsStep {
            id: termsStep
        }

        ReadyStep {
            id: readyStep

            nodeIssue: root.nodeIssue
            nodeId: root.nodeId
            walletIssue: root.walletIssue
            walletDetail: root.walletDetail
            syncedBlock: root.syncedBlock
            chainHeight: root.chainHeight
            onOpenBlockchainAppRequested: root.openBlockchainAppRequested()
            onOpenLezWalletRequested: root.openLezWalletRequested()
        }

        JoinStep {
            id: joinStep

            invitationCheck: root.invitationCheck
            busy: d.busy
            error: root.error
            onInvitationEdited: function (text) { root.invitationEdited(text) }
        }

        DoneStep {
            referrerNode: root.referrerNode
            accountLabel: root.accountLabel
            accountAddress: root.accountAddress
        }
    }

    // ---- Footer ----------------------------------------------------------------

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Theme.spacing.xxlarge
        Layout.rightMargin: Theme.spacing.xxlarge
        Layout.bottomMargin: Theme.spacing.xxlarge
        spacing: Theme.spacing.medium

        LogosButton {
            objectName: "uplink.onboardingBackButton"
            visible: d.step !== "done"
            enabled: d.canGoBack
            text: qsTr("Back")
            onClicked: d.back()
        }

        Item { Layout.fillWidth: true }

        LogosText {
            objectName: "uplink.onboardingAdvanceHint"
            visible: d.advanceHint !== ""
            text: d.advanceHint
            font.pixelSize: Theme.typography.secondaryText
            color: Theme.palette.textTertiary
        }

        LogosButton {
            objectName: "uplink.onboardingAdvanceButton"
            Layout.minimumWidth: 160
            variant: LogosButton.Variant.Primary
            font.pixelSize: Theme.typography.primaryText
            enabled: d.canAdvance
            text: d.advanceText
            onClicked: d.advance()
        }
    }
}
