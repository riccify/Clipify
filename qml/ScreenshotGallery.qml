import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: gallery

    required property var screenshots
    signal trashRequested(string url)

    function updateModel() {
        const urls = new Set(screenshots.map(screenshot => screenshot.url))
        for (let index = screenshotModel.count - 1; index >= 0; --index) {
            if (!urls.has(screenshotModel.get(index).imageUrl))
                screenshotModel.remove(index)
        }

        for (let index = 0; index < screenshots.length; ++index) {
            const screenshot = screenshots[index]
            let previous = index
            while (previous < screenshotModel.count && screenshotModel.get(previous).imageUrl !== screenshot.url)
                ++previous

            if (previous === screenshotModel.count)
                screenshotModel.insert(index, {imageUrl: screenshot.url, imageName: screenshot.name})
            else if (previous !== index)
                screenshotModel.move(previous, index, 1)
        }
    }

    onScreenshotsChanged: updateModel()
    Component.onCompleted: updateModel()

    ListModel { id: screenshotModel }

    GridView {
        id: grid
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        readonly property int columns: Math.max(2, Math.floor(width / 215))
        cellWidth: Math.floor(width / columns)
        cellHeight: 151
        model: screenshotModel
        ScrollBar.vertical: ScrollBar {}

        delegate: Item {
            id: card
            required property string imageUrl
            required property string imageName
            property real removalOffset: 0
            transform: Translate { x: card.removalOffset }
            width: grid.cellWidth - 10
            height: grid.cellHeight - 10

            HoverHandler { id: cardHover }

            Button {
                id: tile
                anchors.fill: parent
                padding: 6
                hoverEnabled: true
                Accessible.name: card.imageName
                onClicked: Qt.openUrlExternally(card.imageUrl)
                CursorTooltip {
                    available: !trashAction.hovered
                    text: qsTr("Open in the system image viewer")
                }

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
                            source: card.imageUrl
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
                        text: card.imageName
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
                onClicked: gallery.trashRequested(card.imageUrl)
            }
        }

        add: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180 }
        }

        remove: Transition {
            NumberAnimation { property: "opacity"; to: 0; duration: 160 }
            NumberAnimation { property: "scale"; to: 0.96; duration: 160; easing.type: Easing.InCubic }
            NumberAnimation { property: "removalOffset"; to: 24; duration: 160; easing.type: Easing.InCubic }
        }

        displaced: Transition {
            NumberAnimation { properties: "x,y"; duration: 220; easing.type: Easing.OutCubic }
        }
    }

    Label {
        anchors.centerIn: parent
        visible: gallery.screenshots.length === 0
        text: qsTr("Your screenshots will appear here.")
        color: "#b5b5b5"
    }
}
