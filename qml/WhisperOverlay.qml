import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: overlay
    width: 340
    height: 64
    title: qsTr("Whisper dictation")
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
           | Qt.WindowDoesNotAcceptFocus | Qt.WindowTransparentForInput
    transientParent: null

    function present() {
        if (!visible) {
            whisperModule.placeOverlay(overlay)
            show()
        }
        if (fade.to !== 1 || panel.opacity === 0) {
            fade.stop()
            fade.to = 1
            fade.start()
        }
    }

    Connections {
        target: whisperModule

        function onChanged() {
            if (!whisperModule.status)
                return
            overlay.present()
            if (whisperModule.busy)
                dismiss.stop()
            else
                dismiss.restart()
        }
    }

    Timer {
        id: dismiss
        interval: 2400
        onTriggered: {
            fade.stop()
            fade.to = 0
            fade.start()
        }
    }

    Rectangle {
        id: panel
        anchors.fill: parent
        radius: 12
        color: "#252525"
        border.color: "#525252"
        opacity: 0

        NumberAnimation {
            id: fade
            target: panel
            property: "opacity"
            duration: to === 1 ? 180 : 250
            easing.type: Easing.OutCubic
            onFinished: {
                if (to === 0)
                    overlay.hide()
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            RecordingIcon {
                visible: whisperModule.recording
                recording: visible
            }

            LineIcon {
                visible: !whisperModule.recording
                symbol: whisperModule.failed ? "message-error" : whisperModule.busy ? "whisper" : "message-info"
                ink: whisperModule.failed ? "#ffb4ab" : "#f2f2f2"
            }

            Label {
                Layout.fillWidth: true
                text: whisperModule.status
                textFormat: Text.PlainText
                color: "#f2f2f2"
                elide: Text.ElideRight
                font.weight: Font.DemiBold
            }

            BusyIndicator {
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                visible: running
                running: whisperModule.busy && !whisperModule.recording
            }
        }
    }
}
