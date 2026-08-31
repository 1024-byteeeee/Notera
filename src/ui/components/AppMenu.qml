import QtQuick
import QtQuick.Controls
import Notera

Menu {
    id: menu
    property bool openedOnce: false
    implicitWidth: 176
    padding: 6
    onOpened: openedOnce = true

    delegate: AppMenuItem {}

    background: Rectangle {
        implicitWidth: 176
        radius: Theme.radiusMd
        color: Theme.surface
        border.width: 1
        border.color: Theme.strongBorder
    }
}
