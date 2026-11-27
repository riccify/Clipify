import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: view
    spacing: 12
    signal llmSettingsRequested()

    MessageBox {
        id: promptMessage
        parent: Overlay.overlay
    }

    RowRemoval {
        id: removal
        onRemoved: id => whisperModule.deleteEntry(id)
    }

    component ActionButton: Button {
        id: control
        hoverEnabled: true
        background: Rectangle {
            implicitHeight: 34
            radius: 6
            color: control.down ? "#505050" : control.hovered ? "#454545" : "#353535"
            border.color: control.visualFocus ? "#d6d6d6" : "#444444"
            opacity: control.enabled ? 1 : 0.5
            Behavior on color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: modelControls.implicitHeight + 22
        radius: 8
        color: "#2a2a2a"
        border.color: "#3a3a3a"

        ColumnLayout {
            id: modelControls
            anchors.fill: parent
            anchors.margins: 11
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                LineIcon { symbol: "folder" }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Label {
                        text: qsTr("Local Whisper model")
                        font.weight: Font.DemiBold
                    }

                    Label {
                        Layout.fillWidth: true
                        text: whisperModule.modelPath
                        textFormat: Text.PlainText
                        color: "#b5b5b5"
                        elide: Text.ElideMiddle
                        CursorTooltip {
                            text: parent.text
                            available: parent.truncated
                        }
                    }
                }

                ActionButton {
                    text: qsTr("Use default")
                    CursorTooltip { text: qsTr("Switch back to the default Whisper model.") }
                    enabled: !whisperModule.busy
                    onClicked: whisperModule.useDefaultModel()
                }

                ActionButton {
                    text: qsTr("Choose model…")
                    CursorTooltip { text: qsTr("Select a local Whisper model file.") }
                    enabled: !whisperModule.busy
                    onClicked: whisperModule.chooseModel()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                LineIcon { symbol: "speech" }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Spoken language")
                }

                Dropdown {
                    Accessible.name: qsTr("Spoken language: %1").arg(text)
                    CursorTooltip { text: qsTr("Choose the language Whisper should transcribe.") }
                    Layout.preferredWidth: 164
                    enabled: !whisperModule.busy
                    selectedValue: whisperModule.language
                    options: [
                        { label: qsTr("Auto-detect"), value: "auto" },
                        { label: qsTr("German"), value: "de" },
                        { label: qsTr("English"), value: "en" }
                    ]
                    onValueSelected: value => whisperModule.setLanguage(value)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                LineIcon { symbol: "keys" }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Hotkey behavior")
                }

                Dropdown {
                    Accessible.name: qsTr("Hotkey behavior: %1").arg(text)
                    CursorTooltip { text: qsTr("Choose between a toggle shortcut and holding the keys while you speak.") }
                    Layout.preferredWidth: 164
                    enabled: !whisperModule.busy
                    selectedValue: whisperModule.holdToTalk ? "hold" : "toggle"
                    options: [
                        { label: qsTr("Toggle recording"), value: "toggle" },
                        { label: qsTr("Hold to talk"), value: "hold" }
                    ]
                    onValueSelected: value => whisperModule.setHoldToTalk(value === "hold")
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                LineIcon { symbol: "sparkles" }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Model for polishing text")
                }

                Dropdown {
                    Layout.preferredWidth: 200
                    menuWidth: 320
                    enabled: !whisperModule.busy
                    Accessible.name: qsTr("Model for polishing text")
                    searchable: true
                    searchPlaceholder: qsTr("Search models…")
                    emptyText: qsTr("No matching models.")
                    selectedValue: llmService.cloud ? llmService.polishModel : ""
                    options: {
                        const entries = [{ label: qsTr("None"), value: "" }]
                        const models = Array.from(llmService.models)
                        if (selectedValue && !models.includes(selectedValue))
                            models.unshift(selectedValue)
                        return entries.concat(models.map(id => ({ label: id, value: id })))
                    }
                    statusText: !llmService.cloud ? qsTr("Select Cloud LLM in LLM Settings.")
                              : !llmService.hasApiKey ? qsTr("Add an API key in LLM Settings.")
                              : llmService.loadingModels ? qsTr("Loading models…")
                              : llmService.modelError
                    onOpening: llmService.refreshModels()
                    onValueSelected: value => llmService.polishModel = value
                }

                ActionButton {
                    text: qsTr("Prompt")
                    CursorTooltip { text: qsTr("Open the dictation polishing prompt in your default editor.") }
                    onClicked: {
                        const error = whisperModule.openPolishingPrompt()
                        if (error.length > 0)
                            promptMessage.showMessage(qsTr("Open prompt"), MessageBox.Error,
                                                      [{ buttonLabel: qsTr("OK"), buttonIcon: "check" }], error)
                    }
                }

                ActionButton {
                    Layout.preferredWidth: 36
                    Accessible.name: qsTr("LLM Settings")
                    CursorTooltip { text: qsTr("LLM Settings") }
                    contentItem: Item {
                        LineIcon {
                            anchors.centerIn: parent
                            symbol: "settings"
                            ink: "#f2f2f2"
                        }
                    }
                    onClicked: view.llmSettingsRequested()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                LineIcon { symbol: "history" }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Dictation history size")
                    font.weight: Font.DemiBold
                }

                HistorySizeInput {
                    Accessible.name: qsTr("Dictation history size")
                    CursorTooltip { text: qsTr("Limit the number of saved dictations. ∞ keeps all entries.") }
                    from: 0
                    value: whisperModule.historyLimit
                    onValueModified: whisperModule.historyLimit = value
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Label {
            Layout.fillWidth: true
            text: whisperModule.status
            textFormat: Text.PlainText
            color: whisperModule.failed ? "#ffb4ab" : "#b5b5b5"
            elide: Text.ElideRight
            CursorTooltip {
                text: parent.text
                available: parent.truncated
            }
        }
    }

    HistoryToolbar {
        id: dictationToolbar
        controller: whisperModule
        title: qsTr("Dictation history")
        clearTitle: qsTr("Clear dictation history?")
        clearMessage: qsTr("All saved dictations will be deleted.")
        onSearchApplied: dictations.positionViewAtBeginning()
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        Label {
            anchors.centerIn: parent
            visible: dictations.count === 0
            text: dictationToolbar.searchText.trim() ? qsTr("No matching dictations.")
                                                     : qsTr("Completed dictations will appear here.")
            color: "#b5b5b5"
        }

        ListView {
            id: dictations
            anchors.fill: parent
            spacing: 6
            clip: true
            model: whisperModule.history
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: item
                required property var modelData
                readonly property real exitProgress: modelData.id === removal.rowId ? removal.progress : 0
                opacity: 1 - exitProgress
                transform: Translate { x: 46 * item.exitProgress }
                width: dictations.width
                height: entry.implicitHeight + 18
                radius: 8
                color: "#2a2a2a"
                border.color: "#3a3a3a"

                RowLayout {
                    id: entry
                    anchors.fill: parent
                    anchors.margins: 9
                    spacing: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            Layout.fillWidth: true
                            text: item.modelData.text
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            maximumLineCount: 3
                            elide: Text.ElideRight
                            CursorTooltip {
                                text: parent.text
                                available: parent.truncated
                            }
                        }

                        Label {
                            text: item.modelData.date
                            color: "#b5b5b5"
                            font.pixelSize: 11
                        }
                    }

                    HistoryAction {
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 30
                        Accessible.name: qsTr("Copy dictation")
                        onClicked: whisperModule.copyEntry(item.modelData.id)
                    }

                    HistoryAction {
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 30
                        removesEntry: true
                        Accessible.name: qsTr("Delete dictation")
                        enabled: !removal.running
                        onClicked: removal.remove(item.modelData.id)
                    }
                }
            }
        }
    }
}
