import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: box

    enum Kind { Warning, Info, Error }

    signal buttonChosen(int index, var button)

    property string title: ""
    property string message: ""
    property int kind: MessageBox.Info
    property var buttons: []
    property int chosenIndex: -1
    readonly property color symbolColor: kind === MessageBox.Warning ? "#e7c75c"
                                         : kind === MessageBox.Error ? "#ff9b91" : "#b8d2f2"
    readonly property string symbol: kind === MessageBox.Warning ? "message-warning"
                                      : kind === MessageBox.Error ? "message-error" : "message-info"

    function showMessage(title, kind, buttons, message = "") {
        if (visible)
            return

        box.title = title
        box.kind = kind
        box.buttons = buttons
        box.message = message
        box.chosenIndex = -1
        open()
    }

    anchors.centerIn: Overlay.overlay
    width: Math.min(452, parent ? parent.width - 32 : 452)
    padding: 20
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape

    Overlay.modal: Rectangle {
        color: "#99000000"
        Behavior on opacity { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
    }

    onOpened: {
        const firstButton = actions.itemAt(0)
        if (firstButton)
            firstButton.forceActiveFocus()
    }

    onClosed: {
        if (chosenIndex >= 0)
            buttonChosen(chosenIndex, buttons[chosenIndex])
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: 180; easing.type: Easing.OutCubic }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: 150; easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: 0.97; duration: 150; easing.type: Easing.InCubic }
    }

    background: Rectangle {
        radius: 12
        color: "#252525"
        border.color: "#454545"
    }

    contentItem: ColumnLayout {
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34
                radius: 8
                color: Qt.rgba(box.symbolColor.r, box.symbolColor.g, box.symbolColor.b, 0.12)

                LineIcon {
                    anchors.centerIn: parent
                    symbol: box.symbol
                    ink: box.symbolColor
                }
            }

            Label {
                Layout.fillWidth: true
                text: box.title
                textFormat: Text.PlainText
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: "#f2f2f2"
                wrapMode: Text.Wrap
            }
        }

        Label {
            Layout.fillWidth: true
            visible: box.message.length > 0
            text: box.message
            textFormat: Text.PlainText
            color: "#c4c4c4"
            wrapMode: Text.Wrap
        }

        Flow {
            Layout.fillWidth: true
            spacing: 8
            layoutDirection: Qt.RightToLeft

            Repeater {
                id: actions
                model: box.buttons

                Button {
                    id: action
                    required property int index
                    required property var modelData
                    text: modelData.buttonLabel
                    hoverEnabled: true
                    padding: 8
                    leftPadding: 12
                    rightPadding: 12

                    Keys.onReturnPressed: clicked()
                    Keys.onEnterPressed: clicked()

                    onClicked: {
                        if (box.chosenIndex < 0) {
                            box.chosenIndex = index
                            box.close()
                        }
                    }

                    contentItem: RowLayout {
                        spacing: 6

                        LineIcon {
                            visible: !!action.modelData.buttonIcon
                            symbol: action.modelData.buttonIcon || ""
                            ink: "#f2f2f2"
                        }

                        Label {
                            text: action.text
                            textFormat: Text.PlainText
                            color: "#f2f2f2"
                        }
                    }

                    background: Rectangle {
                        radius: 6
                        color: action.down ? "#505050" : action.hovered ? "#414141" : "#323232"
                        border.color: action.activeFocus ? "#ababab" : "#494949"
                        Behavior on color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
                        Behavior on border.color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
                    }
                }
            }
        }
    }
}
