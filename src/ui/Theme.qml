pragma Singleton

import QtQuick

QtObject {
    // 0 跟随系统，1 浅色，2 深色。
    property int mode: 0
    property bool systemDark: false
    readonly property bool dark: mode === 2 || (mode === 0 && systemDark)

    readonly property color background: dark ? "#171717" : "#f5f5f4"
    readonly property color surface: dark ? "#222222" : "#ffffff"
    readonly property color elevatedSurface: dark ? "#2b2b2b" : "#fafaf9"
    readonly property color foreground: dark ? "#f4f4f3" : "#1c1917"
    readonly property color mutedForeground: dark ? "#a1a1aa" : "#6b7280"
    readonly property color border: dark ? "#3f3f46" : "#d6d3d1"
    readonly property color accent: "#d4a64a"
    readonly property color page: "#fafafa"
}
