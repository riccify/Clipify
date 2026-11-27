import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: view
    spacing: 12

    RowRemoval {
        id: removal
        onRemoved: id => commandShortcuts.remove(id)
    }

    component CommandButton: Button {
        id: control
        hoverEnabled: true

        background: Rectangle {
            implicitHeight: 34
            radius: 6
            color: control.down ? "#505050" : control.hovered ? "#454545" : "#353535"
            border.color: control.visualFocus ? "#d6d6d6" : "#494949"
            opacity: control.enabled ? 1 : 0.45
            Behavior on color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
        }
    }

    RowLayout {
        Layout.fillWidth: true

        Item { Layout.fillWidth: true }

        CommandButton {
            text: qsTr("Add command")
            CursorTooltip { text: qsTr("Create a command with an optional global hotkey.") }
            onClicked: editor.edit(null)
        }
    }

    Label {
        Layout.fillWidth: true
        visible: !commandShortcuts.hotkeysAvailable
        text: qsTr("Global hotkeys require an X11 session. Run is available here.")
        color: "#b5b5b5"
        wrapMode: Text.WordWrap
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        Label {
            anchors.centerIn: parent
            visible: commandList.count === 0
            text: qsTr("Add a command to run it with a shortcut.")
            color: "#b5b5b5"
        }

        ListView {
            id: commandList
            anchors.fill: parent
            clip: true
            spacing: 8
            model: commandShortcuts.entries
            ScrollBar.vertical: ScrollBar {}
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: card
                required property var modelData
                readonly property real exitProgress: modelData.id === removal.rowId ? removal.progress : 0
                opacity: 1 - exitProgress
                transform: Translate { x: 46 * card.exitProgress }
                width: commandList.width
                height: row.implicitHeight + 22
                radius: 8
                color: "#2a2a2a"
                border.color: "#3a3a3a"

                RowLayout {
                    id: row
                    anchors.fill: parent
                    anchors.margins: 11
                    spacing: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            Layout.fillWidth: true
                            text: card.modelData.name
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            font.weight: Font.DemiBold
                        }

                        Label {
                            Layout.fillWidth: true
                            text: card.modelData.command
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: "#b5b5b5"
                            font.family: "monospace"
                            CursorTooltip {
                                text: parent.text
                                available: parent.truncated
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: text.length > 0
                            text: card.modelData.error || card.modelData.result
                            textFormat: Text.PlainText
                            color: card.modelData.error ? "#ffb4ab" : "#b5b5b5"
                            elide: Text.ElideRight
                            CursorTooltip {
                                text: parent.text
                                available: parent.truncated
                            }
                        }
                    }

                    ShortcutKeys {
                        visible: card.modelData.shortcut.length > 0
                        keys: visible ? card.modelData.shortcut.split("+") : []
                    }

                    CheckBox {
                        Accessible.name: qsTr("Enable %1").arg(card.modelData.name)
                        checked: card.modelData.enabled
                        hoverEnabled: true
                        CursorTooltip {
                            text: parent.checked ? qsTr("Disable command") : qsTr("Enable command")
                            delay: 500
                        }
                        onClicked: commandShortcuts.setEnabled(card.modelData.id, checked)
                    }

                    CommandButton {
                        text: card.modelData.running ? qsTr("Running…") : qsTr("Run")
                        CursorTooltip { text: qsTr("Run this shell command in the background.") }
                        enabled: card.modelData.enabled && !card.modelData.running
                        onClicked: commandShortcuts.run(card.modelData.id)
                    }

                    CommandButton {
                        text: qsTr("Edit")
                        CursorTooltip { text: qsTr("Change the command, name, or hotkey.") }
                        onClicked: editor.edit(card.modelData)
                    }

                    HistoryAction {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        removesEntry: true
                        Accessible.name: qsTr("Remove %1").arg(card.modelData.name)
                        enabled: !removal.running
                        onClicked: removal.remove(card.modelData.id)
                    }
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        visible: text.length > 0
        text: commandShortcuts.message
        textFormat: Text.PlainText
        color: "#ffb4ab"
        wrapMode: Text.WordWrap
    }

    Popup {
        id: editor
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(480, parent.width - 36)
        padding: 18
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape

        property string commandId: ""
        property string accelerator: ""
        property bool recording: false
        property string error: ""

        function edit(entry) {
            commandId = entry ? entry.id : ""
            nameField.text = entry ? entry.name : ""
            commandField.text = entry ? entry.command : ""
            accelerator = entry ? entry.accelerator : ""
            recording = false
            error = ""
            open()
        }

        onOpened: nameField.forceActiveFocus()

        Overlay.modal: Rectangle {
            color: "#99000000"
            Behavior on opacity { NumberAnimation { duration: 160 } }
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; to: 0; duration: 150; easing.type: Easing.InCubic }
        }

        background: Rectangle {
            radius: 11
            color: "#252525"
            border.color: "#484848"
        }

        contentItem: ColumnLayout {
            spacing: 10

            Label {
                text: editor.commandId ? qsTr("Edit command") : qsTr("Add command")
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }

            AppTextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: qsTr("Name")
                CursorTooltip { text: qsTr("Name shown in the command list.") }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 105
                contentWidth: availableWidth

                TextArea {
                    id: commandField
                    placeholderText: qsTr("Shell command")
                    CursorTooltip { text: qsTr("Command Clipify runs when you use the hotkey.") }
                    placeholderTextColor: "#b5b5b5"
                    Accessible.name: placeholderText
                    textFormat: TextEdit.PlainText
                    wrapMode: TextEdit.WrapAnywhere
                    font.family: "monospace"
                    background: Rectangle {
                        radius: 6
                        color: "#1c1c1c"
                        border.color: commandField.activeFocus ? "#737373" : "#4d4d4d"
                        Behavior on border.color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Shortcut")
                }

                CommandButton {
                    id: recorder
                    text: editor.recording ? qsTr("Press a key…")
                         : editor.accelerator ? commandShortcuts.shortcutLabel(editor.accelerator)
                                              : qsTr("Record shortcut")
                    enabled: commandShortcuts.hotkeysAvailable
                    CursorTooltip { text: qsTr("Click, then press the key combination to record.") }

                    contentItem: ShortcutKeys {
                        keys: editor.recording || !editor.accelerator ? [recorder.text] : recorder.text.split("+")
                        pressed: recorder.down
                    }

                    onClicked: {
                        editor.recording = !editor.recording
                        forceActiveFocus()
                    }
                    onActiveFocusChanged: {
                        if (!activeFocus)
                            editor.recording = false
                    }
                    Keys.onShortcutOverride: event => {
                        if (editor.recording)
                            event.accepted = true
                    }
                    Keys.onPressed: event => {
                        if (!editor.recording)
                            return
                        event.accepted = true
                        if (event.isAutoRepeat || [Qt.Key_Control, Qt.Key_Alt, Qt.Key_Shift, Qt.Key_Meta,
                                                   Qt.Key_Super_L, Qt.Key_Super_R].includes(event.key))
                            return
                        editor.recording = false
                        if (event.key === Qt.Key_Escape)
                            return
                        const captured = commandShortcuts.keyAccelerator(event.key, event.modifiers)
                        if (captured && captured !== "<Control>v") {
                            editor.accelerator = captured
                            editor.error = ""
                        } else {
                            editor.error = qsTr("Choose a different shortcut.")
                        }
                    }
                }

                CommandButton {
                    visible: editor.accelerator.length > 0
                    text: qsTr("Clear")
                    CursorTooltip { text: qsTr("Remove the recorded hotkey from this command.") }
                    onClicked: editor.accelerator = ""
                }
            }

            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: editor.error
                color: "#ffb4ab"
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }

                CommandButton {
                    text: qsTr("Cancel")
                    CursorTooltip { text: qsTr("Discard changes and close the editor.") }
                    onClicked: editor.close()
                }

                CommandButton {
                    text: qsTr("Save")
                    CursorTooltip { text: qsTr("Save the command and its hotkey.") }
                    onClicked: {
                        editor.error = commandShortcuts.save(editor.commandId, nameField.text,
                                                              commandField.text, editor.accelerator)
                        if (!editor.error)
                            editor.close()
                    }
                }
            }
        }
    }
}
