import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: gallery

    required property var screenshots
    signal trashRequested(string url)
    property string selectedUrl: ""
    readonly property int selectedIndex: screenshots.findIndex(item => item.url === selectedUrl)
    readonly property var selected: selectedIndex >= 0 ? screenshots[selectedIndex] : null

    function showScreenshot(index, direction) {
        if (index < 0 || index >= screenshots.length)
            return

        imageTransition.stop()
        slide.x = direction * 24
        previewImage.opacity = 0
        selectedUrl = screenshots[index].url
        viewer.forceActiveFocus()
    }

    function closeViewer() {
        selectedUrl = ""
        grid.forceActiveFocus()
    }

    GridView {
        id: grid
        anchors.fill: parent
        visible: !gallery.selected
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        readonly property int columns: Math.max(2, Math.floor(width / 215))
        cellWidth: Math.floor(width / columns)
        cellHeight: 151
        model: gallery.screenshots
        ScrollBar.vertical: ScrollBar {}

        delegate: Item {
            id: card
            required property var modelData
            required property int index
            width: grid.cellWidth - 10
            height: grid.cellHeight - 10

            HoverHandler { id: cardHover }

            Button {
                id: tile
                anchors.fill: parent
                padding: 6
                hoverEnabled: true
                Accessible.name: card.modelData.name
                onClicked: gallery.showScreenshot(card.index, 1)

                background: Rectangle {
                    radius: 9
                    color: tile.hovered || tile.visualFocus ? "#353535" : "#2a2a2a"
                    border.color: tile.hovered || tile.visualFocus ? "#858585" : "#444444"
                    Behavior on border.color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
                }

                contentItem: ColumnLayout {
                    spacing: 5

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        Rectangle {
                            anchors.fill: parent
                            color: "#1b1b1b"
                            radius: 4
                        }

                        Image {
                            anchors.fill: parent
                            source: card.modelData.url
                            sourceSize: Qt.size(360, 220)
                            asynchronous: true
                            fillMode: Image.PreserveAspectCrop
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: 4
                        Layout.bottomMargin: 3
                        Layout.rightMargin: 30
                        text: card.modelData.name
                        color: "#ededed"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
            }

            HistoryAction {
                id: trashAction
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: 6
                anchors.bottomMargin: 4
                width: 26
                height: 26
                removesEntry: true
                activeFocusOnTab: true
                opacity: cardHover.hovered || activeFocus ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                Accessible.name: qsTr("Move screenshot to trash")
                onClicked: gallery.trashRequested(card.modelData.url)
            }
        }

        add: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180 }
        }

        displaced: Transition {
            NumberAnimation { properties: "x,y"; duration: 200; easing.type: Easing.OutCubic }
        }
    }

    Label {
        anchors.centerIn: parent
        visible: gallery.screenshots.length === 0
        text: qsTr("Your screenshots will appear here.")
        color: "#b5b5b5"
    }

    FocusScope {
        id: viewer
        anchors.fill: parent
        visible: !!gallery.selected

        Keys.onEscapePressed: gallery.closeViewer()
        Keys.onLeftPressed: gallery.showScreenshot(gallery.selectedIndex - 1, -1)
        Keys.onRightPressed: gallery.showScreenshot(gallery.selectedIndex + 1, 1)

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            RowLayout {
                Layout.fillWidth: true

                Button {
                    text: qsTr("Back to gallery")
                    onClicked: gallery.closeViewer()
                }

                Label {
                    Layout.fillWidth: true
                    text: gallery.selected ? gallery.selected.name : ""
                    color: "#f2f2f2"
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                }

                Button {
                    text: "‹"
                    Accessible.name: qsTr("Previous screenshot")
                    enabled: gallery.selectedIndex > 0
                    onClicked: gallery.showScreenshot(gallery.selectedIndex - 1, -1)
                }

                Button {
                    text: "›"
                    Accessible.name: qsTr("Next screenshot")
                    enabled: gallery.selectedIndex + 1 < gallery.screenshots.length
                    onClicked: gallery.showScreenshot(gallery.selectedIndex + 1, 1)
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#191919"
                radius: 8
                border.color: "#404040"
                clip: true

                Image {
                    id: previewImage
                    anchors.fill: parent
                    anchors.margins: 9
                    source: gallery.selected ? gallery.selected.url : ""
                    asynchronous: true
                    fillMode: Image.PreserveAspectFit
                    transform: Translate { id: slide }
                    onStatusChanged: {
                        if (status === Image.Ready)
                            imageTransition.restart()
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: previewImage.status === Image.Error
                    text: qsTr("Could not load the screenshot.")
                    color: "#b5b5b5"
                }
            }
        }
    }

    ParallelAnimation {
        id: imageTransition
        NumberAnimation { target: slide; property: "x"; to: 0; duration: 220; easing.type: Easing.OutCubic }
        NumberAnimation { target: previewImage; property: "opacity"; to: 1; duration: 190 }
    }
}
