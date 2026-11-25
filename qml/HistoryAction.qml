import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ToolButton {
    id: action
    property bool removesEntry: false
    Layout.preferredWidth: 26
    Layout.preferredHeight: 26
    hoverEnabled: true
    ToolTip.visible: hovered
    ToolTip.delay: 450
    ToolTip.text: Accessible.name

    background: Rectangle {
        radius: 5
        color: action.down ? "#525252" : action.hovered ? "#424242" : "transparent"
        border.color: "#d6d6d6"
        border.width: action.visualFocus ? 1 : 0
    }

    contentItem: Item {
        Canvas {
            anchors.centerIn: parent
            width: 16
            height: 16
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = "#f2f2f2"
                ctx.lineWidth = 1.2
                ctx.lineJoin = "round"
                ctx.lineCap = "round"
                ctx.beginPath()
                if (action.removesEntry) {
                    ctx.moveTo(2, 4)
                    ctx.lineTo(14, 4)
                    ctx.moveTo(6, 4)
                    ctx.lineTo(6, 2)
                    ctx.lineTo(10, 2)
                    ctx.lineTo(10, 4)
                    ctx.moveTo(4, 5)
                    ctx.lineTo(5, 14)
                    ctx.lineTo(11, 14)
                    ctx.lineTo(12, 5)
                } else {
                    ctx.rect(6, 6, 8, 8)
                    ctx.moveTo(10, 4)
                    ctx.lineTo(10, 2)
                    ctx.lineTo(2, 2)
                    ctx.lineTo(2, 10)
                    ctx.lineTo(4, 10)
                }
                ctx.stroke()
            }
        }
    }
}
