import QtQuick

Item {
    id: mark
    property bool recording: false
    implicitWidth: 18
    implicitHeight: 18

    Rectangle {
        anchors.centerIn: parent
        width: 15
        height: 15
        radius: width / 2
        color: "transparent"
        border.color: "#ee857f"
    }

    Rectangle {
        id: dot
        anchors.centerIn: parent
        width: 9
        height: 9
        radius: mark.recording ? 2 : width / 2
        color: "#ee857f"

        SequentialAnimation on opacity {
            running: mark.recording && mark.visible
            loops: Animation.Infinite
            NumberAnimation { from: 1; to: 0.35; duration: 710; easing.type: Easing.InOutSine }
            NumberAnimation { from: 0.35; to: 1; duration: 710; easing.type: Easing.InOutSine }
            onStopped: dot.opacity = 1
        }
    }
}
