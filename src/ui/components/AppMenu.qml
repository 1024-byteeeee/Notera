import QtQuick
import QtQuick.Controls
import Notera

Menu {
    id: menu
    property bool openedOnce: false
    implicitWidth: 196
    padding: 7
    topPadding: 7
    bottomPadding: 7
    popupType: Popup.Item
    transformOrigin: Item.TopLeft
    onAboutToShow: openedOnce = true

    delegate: AppMenuItem {}

    background: Rectangle {
        implicitWidth: 196
        radius: Theme.radiusMd
        color: Theme.surface
        border.width: 1
        border.color: Theme.strongBorder

        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: Theme.radiusMd + 4
            color: "transparent"
            border.width: 4
            border.color: Theme.dark ? "#26000000" : "#10000000"
            z: -1
        }
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.menuEnter; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Motion.menuEnter; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Motion.menuExit; easing.type: Easing.InCubic }
            NumberAnimation { property: "scale"; from: 1; to: 0.985; duration: Motion.menuExit; easing.type: Easing.InCubic }
        }
    }
}
