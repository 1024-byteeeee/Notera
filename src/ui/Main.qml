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

    AppShell {
        anchors.fill: parent
    }
}
