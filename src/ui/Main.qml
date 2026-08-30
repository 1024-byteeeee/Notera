import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: window
    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 640
    visible: true
    title: "Notera"
    color: Theme.background

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
        target: Theme
        property: "systemDark"
        value: (systemPalette.window.r + systemPalette.window.g + systemPalette.window.b) / 3 < 0.5
    }

    function runThemeSmokeTest() {
        const originalMode = appController.themeMode
        appController.themeMode = 1
        const lightBackground = Theme.background.toString()
        appController.themeMode = 2
        const darkBackground = Theme.background.toString()
        appController.themeMode = originalMode
        Qt.exit(lightBackground !== darkBackground ? 0 : 1)
    }

    AppShell {
        anchors.fill: parent
    }
}
