pragma Singleton

import QtQuick

QtObject {
    property bool enabled: true

    readonly property int instant: 0
    readonly property int fast: enabled ? 90 : 0
    readonly property int normal: enabled ? 160 : 0
    readonly property int slow: enabled ? 220 : 0
    readonly property int menuEnter: enabled ? 110 : 0
    readonly property int menuExit: enabled ? 80 : 0
}
