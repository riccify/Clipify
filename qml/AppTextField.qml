import QtQuick
import QtQuick.Controls

TextField {
    id: field

    property color borderColor: activeFocus ? "#d6d6d6" : "#4b4b4b"

    color: "#f2f2f2"
    placeholderTextColor: "#ffffff"
    selectionColor: "#515151"
    selectedTextColor: "#ffffff"
    selectByMouse: true
    leftPadding: 11
    rightPadding: 11
    Accessible.name: placeholderText

    background: Rectangle {
        implicitHeight: 38
        radius: 6
        color: "#1c1c1c"
        border.color: field.borderColor
        Behavior on border.color { ColorAnimation { duration: 160; easing.type: Easing.OutCubic } }
    }
}
