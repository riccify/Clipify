import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: settings
    spacing: 14

    readonly property string selectedProvider: llmService.provider
    onSelectedProviderChanged: {
        apiKey?.clear()
        verifiedTimeout.stop()
    }
    readonly property bool cloudMode: llmService.cloud
    onCloudModeChanged: verifiedTimeout.stop()

    Timer {
        id: verifiedTimeout
        interval: 3500
    }

    Connections {
        target: llmService
        function onApiKeyVerified() {
            apiKey.clear()
            verifiedTimeout.restart()
        }
    }

    component SettingsButton: Button {
        id: control
        required property string iconSymbol
        Layout.preferredWidth: 38
        Layout.preferredHeight: 38
        hoverEnabled: true
        CursorTooltip { text: control.Accessible.name }
        contentItem: Item {
            LineIcon {
                anchors.centerIn: parent
                symbol: control.iconSymbol
                ink: control.enabled ? "#f2f2f2" : "#777777"
            }
        }
        background: Rectangle {
            implicitHeight: 34
            radius: 6
            color: control.down ? "#505050" : control.hovered ? "#454545" : "#353535"
            border.color: control.visualFocus ? "#d6d6d6" : "#444444"
            opacity: control.enabled ? 1 : 0.5
            Behavior on color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
        }
    }

    RowLayout {
        spacing: 8

        LineIcon { symbol: "sparkles" }

        Label {
            text: qsTr("LLM Settings")
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }
    }

    RowLayout {
        spacing: 10

        Dropdown {
            Layout.preferredWidth: 192
            Accessible.name: qsTr("LLM mode")
            selectedValue: llmService.cloud ? "cloud" : "local"
            options: [
                { label: qsTr("Local LLM (coming later)"), value: "local" },
                { label: qsTr("Cloud LLM"), value: "cloud" }
            ]
            onValueSelected: value => llmService.cloud = value === "cloud"
        }

        Dropdown {
            Layout.preferredWidth: 192
            enabled: llmService.cloud
            opacity: llmService.cloud ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
            Accessible.name: qsTr("Cloud provider")
            selectedValue: llmService.provider
            options: [
                { label: "OpenRouter", value: "openrouter" },
                { label: "OpenAI", value: "openai" },
                { label: "Anthropic", value: "anthropic" }
            ]
            onValueSelected: value => llmService.provider = value
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 14
        enabled: llmService.cloud
        opacity: llmService.cloud ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            AppTextField {
                id: apiKey
                Layout.fillWidth: true
                Accessible.name: qsTr("API key")
                placeholderText: llmService.maskedApiKey || qsTr("API key")
                echoMode: TextInput.Password
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                readOnly: llmService.checkingApiKey
                borderColor: llmService.settingsError ? "#ff7777"
                           : verifiedTimeout.running ? "#72d98f"
                           : activeFocus ? "#d6d6d6" : "#4b4b4b"
                onTextEdited: {
                    llmService.clearKeyFeedback()
                    verifiedTimeout.stop()
                }
                onAccepted: if (saveKey.enabled) saveKey.clicked()
            }
            SettingsButton {
                id: saveKey
                iconSymbol: "check"
                Accessible.name: qsTr("Verify and save API key")
                enabled: !llmService.checkingApiKey && apiKey.text.trim().length > 0
                onClicked: {
                    verifiedTimeout.stop()
                    llmService.saveApiKey(apiKey.text)
                }
            }
            SettingsButton {
                iconSymbol: "trash"
                Accessible.name: qsTr("Remove API key")
                enabled: !llmService.checkingApiKey && llmService.hasApiKey
                onClicked: {
                    verifiedTimeout.stop()
                    llmService.removeApiKey()
                    apiKey.clear()
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: llmService.checkingApiKey ? qsTr("Checking API key…") : llmService.settingsError || qsTr("Keys are stored in local app settings outside exported configs.")
            color: llmService.settingsError ? "#ffb4ab" : "#b5b5b5"
            wrapMode: Text.WordWrap
        }
    }
}
