import QtQuick
import QtQuick.Layouts

import Logos.UplinkUi 1.0

import Logos.Theme
import Logos.Controls

import "../controls"

// The Overview's About tab: what Uplink is, the terms, what anyone can see, and this
// enrolment. Colours follow the prototype: main text white, supporting text grey,
// fine print dim.
LogosScrollView {
    id: root

    objectName: "uplink.AboutView"

    property string referrerNode: ""   // "" = joined as a root
    property bool autoClaim: true
    property int keystoreIssue: UplinkUi.KeystoreNotChosen
    property string keystorePath: ""

    signal readTermsRequested()
    signal autoClaimToggled(bool on)
    signal chooseKeystoreRequested()

    Item {
        width: root.availableWidth
        implicitHeight: column.implicitHeight

        ColumnLayout {
            id: column

            width: parent.width
            spacing: Theme.spacing.large

            InfoCard {
                title: qsTr("WHAT UPLINK IS")
                backgroundColor: Theme.palette.backgroundSecondary

                Paragraph {
                    text: qsTr("Uplink helps stress-test the Logos Testnet by bringing more people in to run nodes. You take part by inviting real people who run a Logos node and stay active; your own node must be active in an epoch for that epoch to pay you, but it earns you no points itself — only the people you invite do. You are not paid separately for running your own node (its block rewards live in the node app). Nobody, including us, can reconstruct who invited whom.")
                }
                Paragraph {
                    text: qsTr("You see only the people who joined directly under you — one layer. Nobody sees deeper, including us. Participation is recognised a few levels deep, but only your direct layer is ever visible to you.")
                }
                Paragraph {
                    small: true
                    text: qsTr("Participation supports Logos Blockchain Testnet goals: testing of consensus and execution-layer functionality, node operation and validation, network behaviour, and identification of bugs and technical issues — not a scheme that pays people to recruit others.")
                }
                Paragraph {
                    small: true
                    text: qsTr("Testnet program · points are off-chain recognition of testnet participation — not a token, no token promise, and no planned airdrop · rewards unavailable in sanctioned jurisdictions · identification only at payout.")
                }
                Paragraph {
                    small: true
                    text: qsTr("Points measure participation and carry no monetary value in themselves. Any reward at payout is conditional and not guaranteed: it depends on eligibility (you must not be a sanctioned party) and the program requirements, and the program may adjust the reward formula or discontinue rewards if circumstances change.")
                }
            }

            InfoCard {
                title: qsTr("TERMS & CONDITIONS")

                Paragraph {
                    sub: true
                    text: qsTr("Supplementary to the Logos Blockchain Testnet Programme T&Cs, from IFT Studio Pte. Ltd. They cover privacy and data handling, eligibility and sanctions, Points (not a token, no monetary value), conditional Rewards, your obligations, programme changes and termination (unclaimed points may expire), disclaimers, and testnet risk. Draft — under legal review.")
                }
                Paragraph {
                    small: true
                    text: qsTr("You accepted these terms when you joined.")
                }
                TextButton {
                    objectName: "uplink.readTermsButton"
                    text: qsTr("Re-read the terms")
                    onClicked: root.readTermsRequested()
                }
            }

            InfoCard {
                title: qsTr("ABOUT THE DATA — WHAT ANYONE CAN SEE")

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Theme.spacing.large
                    rowSpacing: Theme.spacing.small

                    Repeater {
                        model: [
                            qsTr("Shown to you"), qsTr("Never shown — by design"),
                            qsTr("Your points, and whether your node can collect"), qsTr("Anyone else’s points"),
                            qsTr("Your direct referrals and their last 16 epochs"), qsTr("Anything deeper than one layer"),
                            qsTr("That you joined under an inviter"), qsTr("Your inviter’s inviter, or their points"),
                            qsTr("Your own invitation code, to share"), qsTr("The tree, or anyone’s position in it"),
                        ]

                        LogosText {
                            required property string modelData
                            required property int index

                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            wrapMode: Text.WordWrap
                            text: modelData
                            font.pixelSize: Theme.typography.secondaryText
                            font.weight: index < 2 ? Theme.typography.weightBold : Theme.typography.weightRegular
                            color: index < 2 || index % 2 === 1 ? Theme.palette.textSecondary : Theme.palette.text
                        }
                    }
                }
            }

            InfoCard {
                objectName: "uplink.settingsCard"
                title: qsTr("SETTINGS")

                LogosSwitch {
                    objectName: "uplink.settingsAutoClaimSwitch"
                    checked: root.autoClaim
                    text: qsTr("Collect my points automatically")
                    onToggled: root.autoClaimToggled(checked)
                }
                Paragraph {
                    small: true
                    text: qsTr("While Uplink is open it collects your points as soon as there are any, which also passes credit up to whoever invited you. Off: points wait until you cash out; nothing is lost by waiting.")
                }

                Paragraph {
                    sub: true
                    text: root.keystorePath !== ""
                          ? qsTr("Keystore for your Blend key: %1").arg(root.keystorePath)
                          : qsTr("Keystore for your Blend key: not chosen")
                }
                Paragraph {
                    small: true
                    visible: root.keystoreIssue === UplinkUi.KeystoreWrongNode
                             || root.keystoreIssue === UplinkUi.KeystoreUnreadable
                    color: Theme.palette.error
                    text: root.keystoreIssue === UplinkUi.KeystoreWrongNode
                          ? qsTr("This file holds a different node’s Blend key than the one running.")
                          : qsTr("Uplink couldn’t read this file.")
                }
                Paragraph {
                    small: true
                    text: qsTr("Used only when you join, cash out or get a payout code. Uplink reads only the BlendSigning key from it, for that action, and never stores it.")
                }
                TextButton {
                    objectName: "uplink.settingsChooseKeystoreButton"
                    text: root.keystorePath !== "" ? qsTr("Change keystore…") : qsTr("Choose keystore.yaml…")
                    onClicked: root.chooseKeystoreRequested()
                }
            }

            InfoCard {
                Layout.bottomMargin: Theme.spacing.xlarge
                title: qsTr("YOUR ENROLMENT")

                Paragraph {
                    text: root.referrerNode !== ""
                          ? qsTr("You joined under an inviter.")
                          : qsTr("Enrolled without an inviter (nobody invited you).")
                }
            }
        }
    }

    // White main text; `sub` for supporting text, `small` for fine print.
    component Paragraph: LogosText {
        property bool small: false
        property bool sub: false

        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.pixelSize: small ? Theme.typography.secondaryText : Theme.typography.primaryText
        color: small ? Theme.palette.textTertiary
             : sub ? Theme.palette.textSecondary
             : Theme.palette.text
    }

    // A small borderless button, as in the prototype's text links.
    component TextButton: LogosButton {
        compact: true
        leftPadding: 0
        font.pixelSize: Theme.typography.secondaryText
        font.weight: Theme.typography.weightMedium
        background: Item {}
    }
}
