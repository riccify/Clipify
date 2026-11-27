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
    property real revealProgress: 0

    function present() {
        if (!visible) {
            whisperModule.placeOverlay(overlay)
            show()
        }
        if (reveal.to !== 1 || revealProgress === 0) {
            reveal.stop()
            reveal.to = 1
            reveal.start()
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
            reveal.stop()
            reveal.to = 0
            reveal.start()
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 4
        anchors.bottomMargin: 4
        radius: 12
        color: "#252525"
        border.color: "#525252"
        opacity: overlay.revealProgress
        transform: Translate { y: -overlay.height * (1 - overlay.revealProgress) }

        NumberAnimation {
            id: reveal
            target: overlay
            property: "revealProgress"
            duration: to === 1 ? 310 : 230
            easing.type: to === 1 ? Easing.OutCubic : Easing.InCubic
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

            Row {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: 28
                spacing: 3
                visible: whisperModule.recording

                Repeater {
                    model: whisperModule.waveform.length

                    Item {
                        required property int index
                        readonly property real level: whisperModule.waveform[index] ?? 0
                        width: 3
                        height: 28

                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width
                            height: 3 + 25 * parent.level
                            radius: width / 2
                            color: "#f2a6a0"
                            opacity: 0.55 + 0.45 * parent.level

                            Behavior on height {
                                NumberAnimation { duration: 105; easing.type: Easing.OutCubic }
                            }
                            Behavior on opacity { NumberAnimation { duration: 105 } }
                        }
                    }
                }
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
