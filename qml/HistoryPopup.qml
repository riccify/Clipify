import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: popup

    property var entries: []
    readonly property var filteredEntries: {
        const filter = search.text.trim().toLowerCase()
        return filter ? entries.filter(entry => entry.text.toLowerCase().includes(filter)) : entries
    }

    width: 370
    property real expansion: 0
    height: Math.min(355, 108 + filteredEntries.length * 44)
    title: qsTr("Clipboard history")
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    transientParent: null
    property bool receivedFocus: false
    property Item removingRow: null
    property var removingEntry

    onVisibleChanged: {
        if (visible)
            unfold.restart()
        else {
            unfold.stop()
            expansion = 0
            receivedFocus = false
        }
    }

    onActiveChanged: {
        if (active)
            receivedFocus = true
        else if (visible && receivedFocus && !unfold.running)
            hide()
    }

    NumberAnimation on expansion {
        id: unfold
        running: false
        from: 0
        to: 1
        duration: 200
        easing.type: Easing.OutCubic
        onFinished: {
            if (popup.receivedFocus && !popup.active)
                popup.hide()
        }
    }

    function acceptEntry(entry) {
        if (entry === undefined || !clipboardModule.copyEntryById(entry.id))
            return

        hide()
        desktopShortcut.pasteClipboard()
    }

    function removeEntry(entry) {
        const selected = filteredEntries[history.currentIndex]
        const previousIndex = history.currentIndex
        if (!clipboardModule.deleteEntryById(entry.id))
            return false

        entries = clipboardModule.recentEntries()
        const selectedIndex = selected === undefined ? -1
                            : filteredEntries.findIndex(item => item.id === selected.id)
        history.currentIndex = selectedIndex >= 0 ? selectedIndex
                             : Math.min(Math.max(0, previousIndex), filteredEntries.length - 1)
        search.forceActiveFocus()
        return true
    }

    function animateRemoval(row, entry) {
        if (removal.running)
            return

        removingRow = row
        removingEntry = entry
        removal.start()
    }

    ParallelAnimation {
        id: removal

        NumberAnimation {
            target: popup.removingRow
            property: "deleteOffset"
            to: 42
            duration: 160
            easing.type: Easing.InQuad
        }
        NumberAnimation {
            target: popup.removingRow
            property: "opacity"
            to: 0
            duration: 160
            easing.type: Easing.InQuad
        }
        onFinished: {
            if (!popup.removeEntry(popup.removingEntry) && popup.removingRow) {
                popup.removingRow.deleteOffset = 0
                popup.removingRow.opacity = 1
            }
            popup.removingRow = null
            popup.removingEntry = undefined
        }
    }

    Connections {
        target: clipboardModule
        function onHistoryCleared() { popup.entries = [] }
    }

    Connections {
        target: desktopShortcut

        function onActivated() {
            if (popup.visible && popup.active) {
                popup.hide()
                return
            }

            popup.entries = clipboardModule.recentEntries()
            search.clear()
            history.currentIndex = popup.filteredEntries.length ? 0 : -1
            desktopShortcut.showAtPointer(popup)
            search.forceActiveFocus()
        }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: popup.hide()
    }

    Rectangle {
        width: popup.width
        height: popup.height * popup.expansion
        clip: true
        color: "#242424"
        border.color: "#494949"

        ColumnLayout {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 12
            height: popup.height - 24
            spacing: 8

            Label {
                text: qsTr("Clipboard history")
                color: "#f2f2f2"
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }

            AppTextField {
                id: search
                Layout.fillWidth: true
                placeholderText: qsTr("Search recent copies")
                CursorTooltip { text: qsTr("Filter recent entries. Use the arrow keys and Enter to paste.") }

                background: Rectangle {
                    radius: 6
                    color: "#191919"
                    border.color: "#565656"
                }

                onTextChanged: history.currentIndex = popup.filteredEntries.length ? 0 : -1
                Keys.onPressed: event => {
                    switch (event.key) {
                    case Qt.Key_Down:
                        history.currentIndex = Math.min(history.count - 1, history.currentIndex + 1)
                        break
                    case Qt.Key_Up:
                        history.currentIndex = Math.max(0, history.currentIndex - 1)
                        break
                    case Qt.Key_Return:
                    case Qt.Key_Enter:
                        popup.acceptEntry(popup.filteredEntries[history.currentIndex])
                        break
                    default:
                        return
                    }
                    event.accepted = true
                }
            }

            ListView {
                id: history
                Layout.fillWidth: true
                Layout.fillHeight: true
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                spacing: 4
                model: popup.filteredEntries
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    id: row
                    required property var modelData
                    required property int index
                    readonly property bool selected: history.currentIndex === index
                    property real deleteOffset: 0
                    transform: Translate { x: row.deleteOffset }

                    width: history.width - 8
                    height: 40
                    radius: 6
                    color: selected ? "#454545" : rowHover.hovered ? "#363636" : "transparent"
                    Behavior on color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }

                    HoverHandler { id: rowHover }

                    RowLayout {
                        anchors.fill: parent
                        anchors.rightMargin: 5
                        spacing: 4

                        ItemDelegate {
                            id: entryButton
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            focusPolicy: Qt.NoFocus
                            text: row.modelData.text

                            background: Item {}
                            contentItem: RowLayout {
                                spacing: 7

                                ClipboardTypeIcon { image: row.modelData.isImage }

                                Label {
                                    Layout.fillWidth: true
                                    text: entryButton.text.replace(/\s+/g, " ")
                                    textFormat: Text.PlainText
                                    color: "#f2f2f2"
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            onClicked: popup.acceptEntry(row.modelData)
                        }

                        HistoryAction {
                            visible: row.modelData.isImage
                            previewsEntry: true
                            focusPolicy: Qt.NoFocus
                            enabled: rowHover.hovered
                            opacity: rowHover.hovered ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                            Accessible.name: qsTr("Open image")
                            onClicked: {
                                const id = row.modelData.id
                                popup.hide()
                                clipboardModule.previewEntry(id)
                            }
                        }

                        HistoryAction {
                            removesEntry: true
                            focusPolicy: Qt.NoFocus
                            enabled: rowHover.hovered
                            opacity: rowHover.hovered ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                            Accessible.name: qsTr("Delete entry")
                            onClicked: popup.animateRemoval(row, row.modelData)
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: clipboardModule.storageError
                color: "#ffb4ab"
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                visible: history.count === 0
                text: search.text ? qsTr("No matching entries.") : qsTr("No clipboard entries yet.")
                color: "#b5b5b5"
            }
        }
    }
}
