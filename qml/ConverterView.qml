import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: view
    spacing: 14
    clip: true
    property real batchExitProgress: 0
    property bool clearingBatch: false
    property real rowExitProgress: 0
    property int removingIndex: -1
    readonly property bool transitioning: batchExit.running || removingIndex >= 0

    Connections {
        target: converter
        function onBatchSaved() { batchExit.start() }
    }

    NumberAnimation on batchExitProgress {
        id: batchExit
        running: false
        from: 0
        to: 1
        duration: 250
        easing.type: Easing.InOutCubic
        onFinished: {
            if (view.clearingBatch)
                converter.clear()
            else
                converter.finishBatchSave()
            view.clearingBatch = false
            view.batchExitProgress = 0
        }
    }

    NumberAnimation on rowExitProgress {
        id: rowExit
        running: false
        from: 0
        to: 1
        duration: 190
        easing.type: Easing.InCubic
        onFinished: {
            converter.remove(view.removingIndex)
            view.removingIndex = -1
            view.rowExitProgress = 0
        }
    }

    component ActionButton: Button {
        id: action
        hoverEnabled: true

        background: Rectangle {
            implicitHeight: 34
            radius: 6
            color: action.down ? "#505050" : action.hovered ? "#454545" : "#353535"
            border.color: action.visualFocus ? "#d6d6d6" : "#3a3a3a"
            opacity: action.enabled ? 1 : 0.5
            Behavior on color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
        }
    }

    Shortcut {
        sequences: [StandardKey.Paste]
        enabled: view.visible && !formatButton.popupVisible && !view.transitioning
        onActivated: converter.paste()
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10

        Label {
            text: qsTr("Convert to")
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }

        Dropdown {
            id: formatButton
            Accessible.name: qsTr("Target format: %1").arg(text)
            Layout.preferredWidth: 148
            CursorTooltip { text: qsTr("Choose the output format for all images in the list.") }
            options: converter.formats
            selectedValue: converter.targetFormat
            searchable: true
            searchPlaceholder: qsTr("Search formats…")
            enabled: !converter.busy && !view.transitioning
            onValueSelected: value => converter.setTargetFormat(value)
        }

        Item { Layout.fillWidth: true }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: converter.entries.length === 0
        Layout.preferredHeight: 140
        radius: 9
        color: dropArea.containsDrag ? "#353535" : "#242424"
        border.color: dropArea.containsDrag ? "#b5b5b5" : "#444444"
        Behavior on color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }

        DropArea {
            id: dropArea
            anchors.fill: parent
            enabled: !view.transitioning

            onEntered: drag => {
                drag.accepted = drag.hasUrls || drag.keys.some(key => key.startsWith("image/"))
            }

            onDropped: drop => {
                if (drop.hasUrls && drop.urls[0]?.toString().startsWith("file:")) {
                    converter.openFiles(drop.urls)
                    drop.acceptProposedAction()
                    return
                }
                const type = drop.keys.find(key => key.startsWith("image/"))
                if (type) {
                    converter.openImageData(drop.getDataAsArrayBuffer(type))
                    drop.acceptProposedAction()
                }
            }
        }

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.max(0, parent.width - 38)
            spacing: 10

            LineIcon {
                Layout.alignment: Qt.AlignHCenter
                symbol: "converter"
                ink: "#d6d6d6"
            }

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Drop images here or press Ctrl+V")
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }

            ActionButton {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Choose images…")
                CursorTooltip { text: qsTr("Select one or more image files to convert.") }
                enabled: !view.transitioning
                onClicked: converter.chooseFile()
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: converter.entries.length > 0
        enabled: !view.transitioning
        opacity: 1 - view.batchExitProgress
        transform: Translate { x: 56 * view.batchExitProgress }

        Label {
            Layout.fillWidth: true
            font.weight: Font.DemiBold
            text: qsTr("%1 of %2 ready").arg(converter.readyCount).arg(converter.entries.length)
        }

        ActionButton {
            text: qsTr("Clear")
            CursorTooltip { text: qsTr("Remove all images from the conversion list.") }
            enabled: !converter.busy
            onClicked: {
                view.clearingBatch = true
                batchExit.start()
            }
        }

        ActionButton {
            text: qsTr("Save all…")
            CursorTooltip { text: qsTr("Export the converted images to a folder.") }
            enabled: !converter.busy && converter.readyCount > 0
            onClicked: converter.saveAll()
        }
    }

    ListView {
        id: results
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: converter.entries.length > 0
        enabled: !batchExit.running
        opacity: 1 - view.batchExitProgress
        transform: Translate { x: 56 * view.batchExitProgress }
        clip: true
        spacing: 6
        boundsBehavior: Flickable.StopAtBounds
        model: converter.entries
        ScrollBar.vertical: ScrollBar {}

        delegate: Rectangle {
            id: result
            required property int index
            required property var modelData
            readonly property real exitProgress: index === view.removingIndex ? view.rowExitProgress : 0
            width: results.width
            height: resultRow.implicitHeight + 20
            radius: 8
            color: "#2a2a2a"
            border.color: "#3a3a3a"
            opacity: 1 - exitProgress
            transform: Translate { x: 46 * result.exitProgress }

            RowLayout {
                id: resultRow
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Label {
                    Layout.preferredWidth: Math.min(implicitWidth, resultRow.width * 0.44)
                    text: result.modelData.name
                    textFormat: Text.PlainText
                    elide: Text.ElideMiddle
                    font.weight: Font.DemiBold
                    CursorTooltip {
                        text: parent.text
                        available: parent.truncated
                    }
                }

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: result.modelData.error || (result.modelData.ready
                          ? qsTr("%1 → %2").arg(result.modelData.sourceFormat).arg(formatButton.text)
                          : result.modelData.processing ? qsTr("Converting…") : qsTr("Waiting…"))
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: result.modelData.error ? "#ffb4ab" : "#b5b5b5"
                    CursorTooltip {
                        text: parent.text
                        available: parent.truncated
                    }
                }

                ActionButton {
                    text: qsTr("Copy")
                    CursorTooltip { text: qsTr("Copy the converted image to the clipboard.") }
                    enabled: result.modelData.ready && !view.transitioning
                    onClicked: converter.copy(result.index)
                }

                ActionButton {
                    text: qsTr("Save…")
                    CursorTooltip { text: qsTr("Save this converted image to a file.") }
                    enabled: result.modelData.ready && !converter.busy && !view.transitioning
                    onClicked: converter.save(result.index)
                }

                HistoryAction {
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    removesEntry: true
                    enabled: !converter.busy && !view.transitioning
                    Accessible.name: qsTr("Remove from Converter")
                    onClicked: {
                        view.removingIndex = result.index
                        rowExit.start()
                    }
                }
            }
        }
    }

    Item {
        id: converterStatus
        property bool shown: false
        Layout.fillWidth: true
        implicitHeight: statusContents.implicitHeight
        opacity: shown ? 1 : 0
        Accessible.ignored: opacity === 0

        RowLayout {
            id: statusContents
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(implicitWidth, parent.width)
            spacing: 6

            LineIcon {
                visible: converter.savedStatus
                symbol: "check"
                ink: "#a6e3b2"
            }

            Label {
                Layout.fillWidth: true
                text: converter.status
                color: converter.failed ? "#ffb4ab" : "#a6e3b2"
                wrapMode: Text.WordWrap
            }
        }

        Behavior on opacity {
            NumberAnimation { duration: converterStatus.shown ? 180 : 480; easing.type: Easing.OutCubic }
        }

        Timer {
            id: statusTimeout
            interval: 3000
            onTriggered: converterStatus.shown = false
        }

        Connections {
            target: converter
            function onStatusChanged() {
                converterStatus.shown = converter.status.length > 0
                statusTimeout.restart()
            }
        }
    }
}
