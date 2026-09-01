import QtQuick

Item {
    id: root
    property color iconColor: "#c99425"

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = root.iconColor
            ctx.lineWidth = Math.max(1.3, width * 0.09)
            ctx.lineJoin = "round"
            ctx.beginPath()
            ctx.moveTo(width * 0.12, height * 0.22)
            ctx.lineTo(width * 0.56, height * 0.22)
            ctx.lineTo(width * 0.9, height * 0.5)
            ctx.lineTo(width * 0.56, height * 0.78)
            ctx.lineTo(width * 0.12, height * 0.78)
            ctx.closePath()
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(width * 0.31, height * 0.5, width * 0.07, 0, Math.PI * 2)
            ctx.stroke()
        }
        Connections {
            target: root
            function onIconColorChanged() { canvas.requestPaint() }
        }
    }
}
