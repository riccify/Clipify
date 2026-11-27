import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: view
    spacing: 12

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
                        HoverHandler { id: modelHover }
                        ToolTip.visible: modelHover.hovered && truncated
                        ToolTip.text: text
                    }
                }

                ActionButton {
                    text: qsTr("Use default")
                    enabled: !whisperModule.busy
                    onClicked: whisperModule.useDefaultModel()
                }

                ActionButton {
                    text: qsTr("Choose model…")
                    enabled: !whisperModule.busy
                    onClicked: whisperModule.chooseModel()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Spoken language")
                }

                Dropdown {
                    Accessible.name: qsTr("Spoken language: %1").arg(text)
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

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Hotkey behavior")
                }

                Dropdown {
                    Accessible.name: qsTr("Hotkey behavior: %1").arg(text)
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
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        ActionButton {
            id: recordButton
            enabled: !whisperModule.busy || whisperModule.recording
            text: whisperModule.recording ? qsTr("Stop and transcribe") : qsTr("Start dictation")
            onClicked: whisperModule.toggleRecording()

            contentItem: RowLayout {
                spacing: 7

                RecordingIcon { recording: whisperModule.recording }

                Label {
                    text: recordButton.text
                    color: "#f2f2f2"
                    font.weight: Font.DemiBold
                }
            }
        }

        ActionButton {
            enabled: whisperModule.busy
            text: qsTr("Cancel")
            onClicked: whisperModule.cancel()
        }

        Label {
            Layout.fillWidth: true
            text: whisperModule.status
            textFormat: Text.PlainText
            color: whisperModule.failed ? "#ffb4ab" : "#b5b5b5"
            elide: Text.ElideRight
            HoverHandler { id: statusHover }
            ToolTip.visible: statusHover.hovered && truncated
            ToolTip.text: text
        }
    }

    Label {
        text: qsTr("Dictation history")
        font.pixelSize: 19
        font.weight: Font.DemiBold
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        Label {
            anchors.centerIn: parent
            visible: dictations.count === 0
            text: qsTr("Completed dictations will appear here.")
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
                            HoverHandler { id: dictationHover }
                            ToolTip.visible: dictationHover.hovered && truncated
                            ToolTip.text: text
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
