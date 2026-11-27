import QtQuick

Canvas {
    id: icon
    required property bool image
    width: 16
    height: 16
    implicitWidth: 16
    implicitHeight: 16
    onImageChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        ctx.strokeStyle = "#a9a9a9"
        ctx.lineWidth = 1.2
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        ctx.beginPath()
        if (icon.image) {
            ctx.rect(2.5, 3.5, 11, 9)
            ctx.moveTo(3, 11)
            ctx.lineTo(6, 8)
            ctx.lineTo(8.5, 10)
            ctx.lineTo(11, 7.5)
            ctx.lineTo(13.5, 10)
            ctx.moveTo(6.2, 6.2)
            ctx.lineTo(6.4, 6.2)
        } else {
            ctx.moveTo(3, 4)
            ctx.lineTo(13, 4)
            ctx.moveTo(8, 4)
            ctx.lineTo(8, 12)
            ctx.moveTo(6, 12)
            ctx.lineTo(10, 12)
        }
        ctx.stroke()
    }
}
