import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: popup

    width: 480
    height: 403
    title: qsTr("Quick Note")
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    transientParent: null

    property bool saved: false
    property bool choosingLocation: false
    property string saveError: ""
    property real expansion: 0

    function closeAnimated() {
        if (fold.running)
            return

        closeAfterSave.stop()
        unfold.stop()
        fold.start()
    }

    function saveNote(chooseLocation) {
        if (saved || choosingLocation || !editor.getText(0, editor.length).trim())
            return

        choosingLocation = chooseLocation
        const markdown = quickNotes.markdownText(editor.textDocument)
        const ok = chooseLocation ? quickNotes.saveAs(markdown, popup) : quickNotes.save(markdown)
        choosingLocation = false
        saveError = quickNotes.error
        if (ok) {
            saved = true
            closeAfterSave.start()
        } else {
            editor.forceActiveFocus()
        }
    }

    Connections {
        target: notesShortcut
        function onActivated() {
            if (popup.choosingLocation || fold.running)
                return

            if (!popup.visible) {
                popup.saved = false
                popup.saveError = ""
                editor.clear()
            }
            notesShortcut.showAtPointer(popup)
            if (!popup.saved)
                editor.forceActiveFocus()
        }
    }

    onVisibleChanged: {
        if (visible)
            unfold.restart()
        else {
            closeAfterSave.stop()
            unfold.stop()
            fold.stop()
            expansion = 0
        }
    }

    onClosing: event => {
        event.accepted = false
        if (!choosingLocation)
            closeAnimated()
    }

    NumberAnimation on expansion {
        id: unfold
        running: false
        from: 0
        to: 1
        duration: 200
        easing.type: Easing.OutCubic
    }

    NumberAnimation on expansion {
        id: fold
        running: false
        to: 0
        duration: 200
        easing.type: Easing.InCubic
        onFinished: popup.hide()
    }

    Timer {
        id: closeAfterSave
        interval: 1000
        onTriggered: popup.closeAnimated()
    }

    Shortcut {
        sequence: "Escape"
        enabled: popup.visible && !popup.choosingLocation
        onActivated: popup.closeAnimated()
    }

    Shortcut {
        sequence: "Ctrl+S"
        enabled: popup.visible && !popup.saved && !popup.choosingLocation && !fold.running
        onActivated: popup.saveNote(false)
    }

    Shortcut {
        sequence: "Ctrl+Shift+S"
        enabled: popup.visible && !popup.saved && !popup.choosingLocation && !fold.running
        onActivated: popup.saveNote(true)
    }

    Rectangle {
        id: surface
        width: popup.width
        height: popup.height * popup.expansion
        clip: true
        radius: 12
        color: "#1e1e1e"
        border.color: "#4c4c4c"

        ColumnLayout {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 17
            height: popup.height - 2 * anchors.margins
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: qsTr("Quick Note")
                horizontalAlignment: Text.AlignHCenter
                color: "#f2f2f2"
                font.pixelSize: 19
                font.weight: Font.DemiBold

                LineIcon {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    symbol: "notes"
                    ink: "#f2f2f2"
                }

                MouseArea {
                    anchors.fill: parent
                    anchors.topMargin: -17
                    anchors.leftMargin: -17
                    anchors.rightMargin: -17
                    onPressed: windowControls.move(popup)
                }
            }

            Rectangle {
                Layout.fillHeight: true
                Layout.fillWidth: true
                radius: 8
                color: "#252525"

                ScrollView {
                    anchors.fill: parent
                    contentWidth: availableWidth
                    clip: true

                    TextArea {
                        id: editor
                        color: "#f2f2f2"
                        textFormat: TextEdit.RichText
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        readOnly: popup.saved
                        placeholderText: qsTr("Write a note in Markdown…")
                        placeholderTextColor: "#dedede"
                        Accessible.name: qsTr("Note")
                        padding: 12
                        background: null

                        Keys.onPressed: event => {
                            const enter = event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                            const modifiers = event.modifiers & ~Qt.KeypadModifier
                            if (readOnly || selectedText.length > 0 || modifiers !== Qt.NoModifier)
                                return
                            let position = -1
                            if (event.key === Qt.Key_Backspace)
                                position = quickNotes.restoreInline(textDocument, cursorPosition)
                            else if (enter || event.key === Qt.Key_Space)
                                position = quickNotes.formatInline(textDocument, cursorPosition, enter)

                            event.accepted = position >= 0
                            if (event.accepted)
                                cursorPosition = position
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: popup.saveError.length > 0
                text: popup.saveError
                wrapMode: Text.WordWrap
                color: "#ffb4ab"
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 10
                rowSpacing: 5

                ShortcutKeys { Layout.row: 0; Layout.column: 0; keys: ["Ctrl", "S"] }
                Label {
                    Layout.row: 0
                    Layout.column: 2
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("Save to default folder")
                    color: "#b5b5b5"
                    font.pixelSize: 12
                }

                ShortcutKeys { Layout.row: 1; Layout.column: 0; keys: ["Ctrl", "Shift", "S"] }
                Label {
                    Layout.row: 1
                    Layout.column: 2
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("Choose where to save")
                    color: "#b5b5b5"
                    font.pixelSize: 12
                }

                RowLayout {
                    Layout.row: 0
                    Layout.column: 1
                    Layout.rowSpan: 2
                    opacity: popup.saved ? 1 : 0
                    Accessible.ignored: !popup.saved
                    Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

                    Canvas {
                        id: check
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        property real progress: 0

                        Connections {
                            target: popup
                            function onSavedChanged() {
                                if (popup.saved)
                                    drawCheck.restart()
                                else {
                                    drawCheck.stop()
                                    check.progress = 0
                                }
                            }
                        }

                        NumberAnimation on progress {
                            id: drawCheck
                            running: false
                            from: 0
                            to: 1
                            duration: 320
                            easing.type: Easing.OutCubic
                        }

                        onProgressChanged: requestPaint()
                        onPaint: {
                            const context = getContext("2d")
                            context.clearRect(0, 0, width, height)
                            if (progress <= 0)
                                return

                            context.strokeStyle = "#a6e3b2"
                            context.lineWidth = 2.4
                            context.lineCap = "round"
                            context.lineJoin = "round"
                            context.beginPath()
                            context.moveTo(4, 11)
                            if (progress <= 0.32) {
                                const first = progress / 0.32
                                context.lineTo(4 + 5 * first, 11 + 5 * first)
                            } else {
                                const second = (progress - 0.32) / 0.68
                                context.lineTo(9, 16)
                                context.lineTo(9 + 9 * second, 16 - 10 * second)
                            }
                            context.stroke()
                        }
                    }

                    Label {
                        text: qsTr("Saved")
                        color: "#a6e3b2"
                    }
                }
            }
        }
    }
}
