import QtQuick
import QtQuick.Controls
import Notera

Menu {
    id: menu
    implicitWidth: 176
    padding: 6

    background: Rectangle {
        implicitWidth: 176
        radius: Theme.radiusMd
        color: Theme.surface
        border.width: 1
        border.color: Theme.strongBorder
    }
}
