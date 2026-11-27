import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: notesView

    property string selectedPath: ""
    property string markdown: ""
    property string readError: ""

    property var collapsedDates: ({})
    readonly property var dateGroups: {
        const groups = []
        for (const note of quickNotes.notes) {
            let group = groups[groups.length - 1]
            if (!group || group.date !== note.date) {
                group = { date: note.date, heading: note.dateHeading, notes: [] }
                groups.push(group)
            }
            group.notes.push(note)
        }
        return groups
    }

    function toggleDate(date) {
        const dates = Object.assign({}, collapsedDates)
        dates[date] = !dates[date]
        collapsedDates = dates
    }

    function selectNote(path) {
        selectedPath = path
        markdown = path ? quickNotes.read(path) : ""
        readError = path ? quickNotes.error : ""
    }

    function updateSelection() {
        const selected = quickNotes.notes.find(note => note.path === selectedPath)
        selectNote(selected ? selected.path : (quickNotes.notes[0]?.path ?? ""))
    }

    onVisibleChanged: {
        if (visible) {
            quickNotes.refresh()
            updateSelection()
        }
    }

    Connections {
        target: quickNotes
        function onNotesChanged() {
            if (notesView.visible)
                notesView.updateSelection()
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 12
        visible: quickNotes.notes.length > 0

        Rectangle {
            Layout.preferredWidth: 282
            Layout.minimumWidth: 255
            Layout.fillHeight: true
            radius: 8
            color: "#222222"
            border.color: "#3a3a3a"

            ScrollView {
                anchors.fill: parent
                anchors.margins: 6
                contentWidth: availableWidth
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                Column {
                    width: parent.width
                    spacing: 5

                    Repeater {
                        model: notesView.dateGroups

                        delegate: Column {
                            id: dateGroup
                            required property var modelData
                            readonly property bool expanded: !notesView.collapsedDates[modelData.date]
                            width: parent.width
                            spacing: 4

                            Button {
                                id: dateButton
                                width: parent.width
                                text: dateGroup.modelData.heading
                                hoverEnabled: true
                                Accessible.checkable: true
                                Accessible.checked: dateGroup.expanded
                                onClicked: notesView.toggleDate(dateGroup.modelData.date)
                                CursorTooltip {
                                    text: dateGroup.expanded ? qsTr("Collapse notes for this date")
                                                             : qsTr("Expand notes for this date")
                                }

                                contentItem: RowLayout {
                                    spacing: 6
                                    Label {
                                        text: "›"
                                        rotation: dateGroup.expanded ? 90 : 0
                                        font.pixelSize: 18
                                        Behavior on rotation { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: dateButton.text
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                }

                                background: Rectangle {
                                    radius: 6
                                    color: dateButton.hovered ? "#383838" : "#2b2b2b"
                                    border.color: dateButton.visualFocus ? "#d6d6d6" : "transparent"
                                    Behavior on color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
                                }
                            }

                            Item {
                                width: parent.width
                                height: dateGroup.expanded ? noteRows.implicitHeight : 0
                                clip: true
                                enabled: dateGroup.expanded
                                Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.InOutCubic } }

                                Column {
                                    id: noteRows
                                    x: 12
                                    width: parent.width - x
                                    spacing: 4

                                    Repeater {
                                        model: dateGroup.modelData.notes

                                        delegate: Button {
                                            id: noteButton
                                            required property var modelData
                                            readonly property bool selected: modelData.path === notesView.selectedPath
                                            width: parent.width
                                            text: Qt.formatTime(modelData.savedAt, "HH:mm:ss")
                                            hoverEnabled: true
                                            onClicked: notesView.selectNote(modelData.path)
                                            CursorTooltip { text: qsTr("Read this saved note") }

                                            contentItem: Label {
                                                text: noteButton.text
                                                verticalAlignment: Text.AlignVCenter
                                                font.weight: Font.DemiBold
                                            }

                                            background: Rectangle {
                                                implicitHeight: 43
                                                radius: 6
                                                color: noteButton.selected ? "#383838" : noteButton.hovered ? "#303030" : "transparent"
                                                border.color: noteButton.visualFocus ? "#d6d6d6" : "transparent"
                                                Behavior on color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 8
            color: "#222222"
            border.color: "#3a3a3a"

            ScrollView {
                anchors.fill: parent
                anchors.margins: 1
                contentWidth: availableWidth
                clip: true

                TextArea {
                    text: notesView.readError || notesView.markdown
                    textFormat: notesView.readError ? TextEdit.PlainText : TextEdit.MarkdownText
                    color: notesView.readError ? "#ffb4ab" : "#f2f2f2"
                    wrapMode: TextEdit.Wrap
                    readOnly: true
                    selectByMouse: true
                    Accessible.name: qsTr("Saved note")
                    padding: 14
                    background: null
                }
            }
        }
    }

    Label {
        anchors.centerIn: parent
        visible: quickNotes.notes.length === 0
        text: qsTr("Saved notes will appear here.")
        color: "#b5b5b5"
    }
}
