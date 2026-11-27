import QtQuick
import QtQuick.Controls

Row {
    id: keycaps
    required property var keys
    property bool pressed: false
    spacing: 6

    Repeater {
        model: keycaps.keys

        Row {
            required property int index
            required property string modelData
            spacing: 6

            Label {
                visible: index > 0
                anchors.verticalCenter: parent.verticalCenter
                text: "+"
                color: "#b5b5b5"
                font.pixelSize: 12
            }

            Rectangle {
                width: Math.max(28, keyLabel.implicitWidth + 16)
                height: 28
                radius: 5
                color: keycaps.pressed ? "#444444" : "#303030"
                border.color: "#5d5d5d"

                Label {
                    id: keyLabel
                    anchors.centerIn: parent
                    text: modelData
                    color: "#f2f2f2"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
            }
        }
    }
}
