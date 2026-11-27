import QtQuick
import QtQuick.Layouts

Canvas {
    required property string symbol
    property color ink: "#b5b5b5"
    implicitWidth: 18
    implicitHeight: 18
    Layout.alignment: Qt.AlignVCenter

    onInkChanged: requestPaint()
    onSymbolChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        ctx.strokeStyle = ink
        ctx.lineWidth = 1.35
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        ctx.beginPath()

        switch (symbol) {
        case "clipboard":
            ctx.moveTo(6, 4)
            ctx.lineTo(3.5, 4)
            ctx.lineTo(3.5, 16)
            ctx.lineTo(14.5, 16)
            ctx.lineTo(14.5, 4)
            ctx.lineTo(12, 4)
            ctx.rect(6, 2, 6, 4)
            ctx.moveTo(6.5, 9)
            ctx.lineTo(11.5, 9)
            ctx.moveTo(6.5, 12)
            ctx.lineTo(11.5, 12)
            break
        case "screen":
            ctx.rect(2, 3, 14, 10)
            ctx.moveTo(9, 13)
            ctx.lineTo(9, 16)
            ctx.moveTo(6, 16)
            ctx.lineTo(12, 16)
            break
        case "history":
            ctx.arc(9, 9, 6.5, Math.PI, Math.PI * 2.85)
            ctx.moveTo(2.5, 6)
            ctx.lineTo(2.5, 9)
            ctx.lineTo(5.5, 9)
            ctx.moveTo(9, 5.5)
            ctx.lineTo(9, 9)
            ctx.lineTo(11.5, 10.5)
            break
        case "keys":
            ctx.rect(2, 4.5, 14, 9)
            for (let x = 5; x <= 13; x += 4) {
                ctx.moveTo(x, 7.5)
                ctx.lineTo(x + 0.5, 7.5)
            }
            ctx.moveTo(6, 10.5)
            ctx.lineTo(12, 10.5)
            break
        case "folder":
            ctx.moveTo(2, 5)
            ctx.lineTo(7, 5)
            ctx.lineTo(8.5, 7)
            ctx.lineTo(16, 7)
            ctx.lineTo(16, 15)
            ctx.lineTo(2, 15)
            ctx.closePath()
            break
        case "notes":
            ctx.strokeRect(4, 2, 10, 14)
            ctx.moveTo(7, 6)
            ctx.lineTo(11, 6)
            ctx.moveTo(7, 9)
            ctx.lineTo(11, 9)
            ctx.moveTo(7, 12)
            ctx.lineTo(10, 12)
            break
        case "search":
            ctx.arc(7.5, 7.5, 4.5, 0, Math.PI * 2)
            ctx.moveTo(11, 11)
            ctx.lineTo(15, 15)
            break
        case "whisper":
            ctx.moveTo(6, 5)
            ctx.arc(9, 5, 3, Math.PI, Math.PI * 2)
            ctx.lineTo(12, 9)
            ctx.arc(9, 9, 3, 0, Math.PI)
            ctx.lineTo(6, 5)
            ctx.moveTo(3.5, 9)
            ctx.arc(9, 9, 5.5, Math.PI, 0, true)
            ctx.moveTo(9, 14.5)
            ctx.lineTo(9, 17)
            break
        case "commands":
            ctx.rect(2, 3, 14, 12)
            ctx.moveTo(5, 6)
            ctx.lineTo(8, 9)
            ctx.lineTo(5, 12)
            ctx.moveTo(9, 12)
            ctx.lineTo(13, 12)
            break
        case "converter":
            ctx.moveTo(2.5, 6)
            ctx.lineTo(15.5, 6)
            ctx.moveTo(12.5, 3)
            ctx.lineTo(15.5, 6)
            ctx.lineTo(12.5, 9)
            ctx.moveTo(15.5, 12)
            ctx.lineTo(2.5, 12)
            ctx.moveTo(5.5, 9)
            ctx.lineTo(2.5, 12)
            ctx.lineTo(5.5, 15)
            break
        case "settings":
            for (const y of [4, 9, 14]) {
                const x = y === 9 ? 12 : 6
                ctx.moveTo(2, y)
                ctx.lineTo(x - 2, y)
                ctx.moveTo(x + 2, y)
                ctx.lineTo(16, y)
                ctx.moveTo(x + 2, y)
                ctx.arc(x, y, 2, 0, Math.PI * 2)
            }
            break
        case "about":
            ctx.arc(9, 9, 6.5, 0, Math.PI * 2)
            ctx.moveTo(9, 7.7)
            ctx.lineTo(9, 12)
            ctx.moveTo(9, 5.4)
            ctx.lineTo(9, 5.6)
            break
        case "exit":
            ctx.moveTo(9, 3)
            ctx.lineTo(3, 3)
            ctx.lineTo(3, 15)
            ctx.lineTo(9, 15)
            ctx.moveTo(7, 9)
            ctx.lineTo(15, 9)
            ctx.moveTo(12, 6)
            ctx.lineTo(15, 9)
            ctx.lineTo(12, 12)
            break
        case "message-warning":
            ctx.arc(9, 9, 7, 0, Math.PI * 2)
            ctx.moveTo(6.8, 6.4)
            ctx.bezierCurveTo(6.8, 3.6, 12.7, 4.5, 11.4, 7.2)
            ctx.bezierCurveTo(10.8, 8.1, 9, 8.4, 9, 10.3)
            ctx.moveTo(9, 12.7)
            ctx.lineTo(9, 13)
            break
        case "message-info":
            ctx.arc(9, 9, 7, 0, Math.PI * 2)
            ctx.moveTo(9, 5.2)
            ctx.lineTo(9, 10)
            ctx.moveTo(9, 12.6)
            ctx.lineTo(9, 12.9)
            break
        case "message-error":
            ctx.arc(9, 9, 7, 0, Math.PI * 2)
            ctx.moveTo(6.5, 6.5)
            ctx.lineTo(11.5, 11.5)
            ctx.moveTo(11.5, 6.5)
            ctx.lineTo(6.5, 11.5)
            break
        case "cancel":
            ctx.moveTo(5, 5)
            ctx.lineTo(13, 13)
            ctx.moveTo(13, 5)
            ctx.lineTo(5, 13)
            break
        case "check":
            ctx.moveTo(3.5, 9)
            ctx.lineTo(7.5, 13)
            ctx.lineTo(14.5, 5)
            break
        case "reset":
            ctx.arc(9, 9, 5.5, -2.45, Math.PI * 1.1)
            ctx.moveTo(3.4, 3.6)
            ctx.lineTo(3.4, 6.7)
            ctx.lineTo(6.5, 6.7)
            break
        case "fast":
            ctx.moveTo(10, 1.5)
            ctx.lineTo(3.5, 10)
            ctx.lineTo(8, 10)
            ctx.lineTo(7.5, 16.5)
            ctx.lineTo(14.5, 7.5)
            ctx.lineTo(10, 7.5)
            ctx.closePath()
            break
        case "accurate":
            ctx.arc(9, 9, 6, 0, Math.PI * 2)
            ctx.moveTo(12, 9)
            ctx.arc(9, 9, 3, 0, Math.PI * 2)
            break
        }
        ctx.stroke()
    }
}
