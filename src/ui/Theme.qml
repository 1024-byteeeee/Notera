pragma Singleton

import QtQuick

QtObject {
    // 0 浅色，1 深色。
    property int mode: 0
    property bool systemDark: false
    readonly property bool dark: mode === 1

    // ── 基础色板 ──────────────────────────────────────────
    readonly property color background: dark ? "#0d0d0e" : "#f7f6f3"
    readonly property color surface: dark ? "#161618" : "#ffffff"
    readonly property color elevatedSurface: dark ? "#1e1e21" : "#faf9f7"
    readonly property color sunkenSurface: dark ? "#121214" : "#f0efec"

    readonly property color foreground: dark ? "#ececea" : "#1a1917"
    readonly property color secondaryForeground: dark ? "#b4b3ae" : "#57534e"
    readonly property color mutedForeground: dark ? "#787774" : "#a8a29e"
    readonly property color faintForeground: dark ? "#52514f" : "#d6d3d1"

    readonly property color border: dark ? "#2a2a2d" : "#e4e2de"
    readonly property color strongBorder: dark ? "#3a3a3e" : "#d4d1cc"

    // ── Accent（暖金） ───────────────────────────────────
    readonly property color accent: dark ? "#e0b45c" : "#c9962e"
    readonly property color accentHover: dark ? "#ecc46f" : "#d9a43a"
    readonly property color accentPressed: dark ? "#c99a3f" : "#b08124"
    readonly property color accentSoft: dark ? "#2a2315" : "#faf3e0"
    readonly property color accentForeground: dark ? "#1a1408" : "#ffffff"

    // 框选区域使用跨主题固定色，避免浅色与深色模式切换时改变视觉语义。
    readonly property color marqueeFill: Qt.rgba(224 / 255, 180 / 255, 92 / 255, 0.09)
    readonly property color marqueeBorder: "#e0b45c"

    // ── 按钮状态色 ───────────────────────────────────────
    readonly property color buttonBackground: dark ? "#232326" : "#f5f4f1"
    readonly property color buttonHover: dark ? "#2e2e32" : "#ebeae6"
    readonly property color buttonPressed: dark ? "#1c1c1f" : "#e0dfda"
    readonly property color buttonDisabled: dark ? "#1a1a1c" : "#f0efec"

    readonly property color buttonBorder: dark ? "#333337" : "#e0ded9"
    readonly property color buttonText: dark ? "#e4e3df" : "#292724"
    readonly property color buttonDisabledText: dark ? "#555452" : "#b8b5b0"

    // 选中/激活态按钮
    readonly property color selectedBackground: dark ? "#2e2a20" : "#f5edd6"
    readonly property color selectedText: dark ? "#ecc46f" : "#9a7620"
    readonly property color selectedBorder: dark ? "#4a3f25" : "#e8d5a0"

    // ── 输入框 ───────────────────────────────────────────
    readonly property color inputBackground: dark ? "#1a1a1d" : "#ffffff"
    readonly property color inputBorder: dark ? "#333337" : "#dcdad5"
    readonly property color inputFocusBorder: dark ? "#5a4d30" : "#d4b876"
    readonly property color inputPlaceholder: dark ? "#5c5b58" : "#b0ada8"

    // ── 卡片 ─────────────────────────────────────────────
    readonly property color cardBackground: dark ? "#1a1a1d" : "#ffffff"
    readonly property color cardHover: dark ? "#222226" : "#fdfcf9"
    readonly property color cardBorder: dark ? "#2a2a2d" : "#e8e6e1"
    readonly property color cardShadow: dark ? "#000000" : "#000000"

    // ── 危险色 ───────────────────────────────────────────
    readonly property color danger: dark ? "#e06c5c" : "#d4503f"
    readonly property color dangerSoft: dark ? "#2e1a17" : "#fdecea"

    // ── 尺寸系统 ─────────────────────────────────────────
    readonly property int radiusSm: 6
    readonly property int radiusMd: 10
    readonly property int radiusLg: 14
    readonly property int radiusXl: 20

    readonly property int spacingXs: 6
    readonly property int spacingSm: 10
    readonly property int spacingMd: 16
    readonly property int spacingLg: 24
    readonly property int spacingXl: 36

    readonly property int controlHeight: 34
    readonly property int sidebarWidth: 232

    // ── 字号 ─────────────────────────────────────────────
    readonly property int fontXs: 11
    readonly property int fontSm: 12
    readonly property int fontMd: 14
    readonly property int fontLg: 16
    readonly property int fontXl: 20
    readonly property int font2xl: 26

    // ── 兼容旧属性 ───────────────────────────────────────
    readonly property color page: "#fafafa"
}
