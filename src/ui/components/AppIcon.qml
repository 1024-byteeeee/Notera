import QtQuick

Canvas {
    id: root
    objectName: "appIcon"
    property string iconName: ""
    property color iconColor: "#ffffff"
    property real strokeWidth: 1.7
    property bool filled: iconName === "star-filled"

    implicitWidth: 18
    implicitHeight: 18
    antialiasing: true

    onIconNameChanged: requestPaint()
    onIconColorChanged: requestPaint()
    onStrokeWidthChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    function canonicalName(value) {
        const aliases = {
            "+": "plus", "−": "minus", "-": "minus", "✓": "check",
            "★": "star-filled", "☆": "star", "✎": "edit", "▣": "folder",
            "↖": "folder-up", "⌫": "trash", "↗": "open", "↓": "import",
            "♪": "music", "♫": "music", "◷": "recent", "⚙": "settings",
            "▾": "chevron-down", "▸": "chevron-right", "⌕": "search",
            "←": "back", "‹": "previous", "›": "next", "▶": "play",
            "⏸": "pause", "↶": "rotate-left", "↷": "rotate-right"
        }
        return aliases[value] || value
    }

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const sx = width / 24
        const sy = height / 24
        ctx.scale(sx, sy)
        ctx.strokeStyle = root.iconColor
        ctx.fillStyle = root.iconColor
        ctx.lineWidth = root.strokeWidth * 24 / Math.max(width, height)
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        const name = canonicalName(root.iconName)

        function line(points, close) {
            ctx.beginPath()
            ctx.moveTo(points[0][0], points[0][1])
            for (let i = 1; i < points.length; ++i) ctx.lineTo(points[i][0], points[i][1])
            if (close) ctx.closePath()
            ctx.stroke()
        }
        function circle(x, y, radius, fill) {
            ctx.beginPath(); ctx.arc(x, y, radius, 0, Math.PI * 2)
            fill ? ctx.fill() : ctx.stroke()
        }
        function rect(x, y, w, h, radius) {
            ctx.beginPath(); ctx.rect(x, y, w, h); ctx.stroke()
        }

        if (name === "plus" || name === "minus") {
            line([[5, 12], [19, 12]])
            if (name === "plus") line([[12, 5], [12, 19]])
        } else if (name === "check") {
            line([[4.5, 12.5], [9.5, 17.5], [19.5, 6.5]])
        } else if (name === "star" || name === "star-filled") {
            const points = []
            for (let i = 0; i < 10; ++i) {
                const angle = -Math.PI / 2 + i * Math.PI / 5
                const radius = i % 2 === 0 ? 9 : 4.2
                points.push([12 + Math.cos(angle) * radius, 12 + Math.sin(angle) * radius])
            }
            ctx.beginPath(); ctx.moveTo(points[0][0], points[0][1])
            for (let i = 1; i < points.length; ++i) ctx.lineTo(points[i][0], points[i][1])
            ctx.closePath(); name === "star-filled" ? ctx.fill() : ctx.stroke()
        } else if (name === "edit") {
            line([[5, 19], [7, 14], [16.5, 4.5], [19.5, 7.5], [10, 17], [5, 19]], true)
            line([[14.5, 6.5], [17.5, 9.5]])
        } else if (name === "folder" || name === "folder-up") {
            ctx.beginPath(); ctx.moveTo(3, 7); ctx.lineTo(9, 7); ctx.lineTo(11, 9)
            ctx.lineTo(21, 9); ctx.lineTo(20, 19); ctx.lineTo(4, 19); ctx.closePath(); ctx.stroke()
            if (name === "folder-up") { line([[12, 17], [12, 11]]); line([[9.5, 13.5], [12, 11], [14.5, 13.5]]) }
        } else if (name === "trash") {
            line([[5, 7], [19, 7]]); line([[9, 4], [15, 4], [16, 7]])
            line([[7, 7], [8, 20], [16, 20], [17, 7]]); line([[10, 10], [10.5, 17]]); line([[14, 10], [13.5, 17]])
        } else if (name === "open" || name === "import") {
            rect(4, 6, 14, 14, 2)
            if (name === "open") { line([[11, 13], [20, 4]]); line([[14, 4], [20, 4], [20, 10]]) }
            else { line([[12, 3], [12, 15]]); line([[8.5, 11.5], [12, 15], [15.5, 11.5]]) }
        } else if (name === "music") {
            line([[10, 18], [10, 6], [19, 4], [19, 16]])
            line([[10, 8], [19, 6]])
            circle(7.5, 18, 2.5, true); circle(16.5, 16, 2.5, true)
        } else if (name === "recent") {
            circle(12, 12, 8, false); line([[12, 7], [12, 12], [16, 14]])
        } else if (name === "settings") {
            circle(12, 12, 3, false); circle(12, 12, 8, false)
            for (let i = 0; i < 8; ++i) {
                const a = i * Math.PI / 4; line([[12 + Math.cos(a) * 8, 12 + Math.sin(a) * 8], [12 + Math.cos(a) * 10, 12 + Math.sin(a) * 10]])
            }
        } else if (name.indexOf("chevron-") === 0 || name === "back" || name === "previous" || name === "next") {
            if (name === "chevron-down") line([[6, 9], [12, 15], [18, 9]])
            else if (name === "chevron-right" || name === "next") line([[9, 5], [16, 12], [9, 19]])
            else line([[15, 5], [8, 12], [15, 19]])
        } else if (name === "search") {
            circle(10.5, 10.5, 6, false); line([[15, 15], [20, 20]])
        } else if (name === "play") {
            ctx.beginPath(); ctx.moveTo(8, 5); ctx.lineTo(19, 12); ctx.lineTo(8, 19); ctx.closePath(); ctx.fill()
        } else if (name === "pause") {
            ctx.fillRect(7, 5, 3.5, 14); ctx.fillRect(13.5, 5, 3.5, 14)
        } else if (name === "rotate-left" || name === "rotate-right" || name === "reset") {
            ctx.beginPath(); ctx.arc(12, 12, 7, name === "rotate-right" ? -2.5 : -0.6, name === "rotate-right" ? 0.6 : 2.5, name !== "rotate-right"); ctx.stroke()
            if (name === "rotate-right") line([[18, 5], [19, 10], [14, 9]])
            else line([[6, 5], [5, 10], [10, 9]])
        }
    }
}
