import QtQuick
import QtQuick.Controls
import Notera

ApplicationWindow {
    id: window
    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 640
    visible: true
    title: "Notera"
    color: Theme.background
    readonly property color themeBackground: Theme.background

    SystemPalette {
        id: systemPalette
        colorGroup: SystemPalette.Active
    }

    Binding {
        target: Theme
        property: "mode"
        value: appController.themeMode
    }

    Binding {
        target: Motion
        property: "enabled"
        value: appController.animationsEnabled
    }

    Binding {
        target: Theme
        property: "systemDark"
        value: (systemPalette.window.r + systemPalette.window.g + systemPalette.window.b) / 3 < 0.5
    }

    AppShell {
        anchors.fill: parent
    }
}
