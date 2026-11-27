import QtQuick
import QtQuick.Controls

Item {
    id: tip
    anchors.fill: parent

    required property string text
    property bool available: true
    property int delay: 550
    readonly property bool wanted: available && pointer.hovered && text.length > 0

    onWantedChanged: {
        if (wanted) {
            dwell.restart()
        } else {
            dwell.stop()
            bubble.close()
        }
    }

    HoverHandler { id: pointer }

    TextMetrics {
        id: metrics
        text: tip.text
        font.pixelSize: 12
    }

    Timer {
        id: dwell
        interval: tip.delay
        onTriggered: {
            if (tip.wanted && bubble.parent)
                bubble.open()
        }
    }

    Popup {
        id: bubble
        parent: Overlay.overlay
        z: 100
        padding: 10
        focus: false
        modal: false
        closePolicy: Popup.NoAutoClose
        width: Math.min(304, Math.ceil(metrics.advanceWidth) + leftPadding + rightPadding,
                        parent ? Math.max(0, parent.width - 12) : 304)

        readonly property point cursor: parent ? tip.mapToItem(parent, pointer.point.position.x,
                                                               pointer.point.position.y) : Qt.point(0, 0)
        x: parent ? Math.max(6, Math.min(cursor.x + 14, parent.width - width - 6)) : 0
        y: parent && cursor.y + height + 24 > parent.height
           ? Math.max(6, cursor.y - height - 10) : cursor.y + 18

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 320; easing.type: Easing.OutCubic }
        }

        exit: Transition {
            NumberAnimation { property: "opacity"; to: 0; duration: 180; easing.type: Easing.OutCubic }
        }

        background: Rectangle {
            radius: 7
            color: "#282828"
            border.color: "#505050"
        }

        contentItem: Label {
            text: tip.text
            textFormat: Text.PlainText
            color: "#f2f2f2"
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
    }
}
