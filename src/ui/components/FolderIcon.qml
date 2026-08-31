import QtQuick

Item {
    id: root
    property color iconColor: "#c99425"

    Rectangle {
        x: root.width * 0.08
        y: root.height * 0.18
        width: root.width * 0.43
        height: root.height * 0.25
        radius: Math.max(1, root.height * 0.08)
        color: root.iconColor
    }

    Rectangle {
        x: root.width * 0.08
        y: root.height * 0.31
        width: root.width * 0.84
        height: root.height * 0.57
        radius: Math.max(2, root.height * 0.12)
        color: root.iconColor
    }
}
