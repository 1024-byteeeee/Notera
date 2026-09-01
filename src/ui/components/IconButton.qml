import QtQuick
import QtQuick.Controls
import Notera

Button {
    id: control

    property string symbol: ""
    property bool selected: false
    property bool danger: false
    readonly property int hoverTransitionDuration: 0

    implicitWidth: 32
    implicitHeight: 32
    padding: 0
    hoverEnabled: true

    contentItem: Label {
        anchors.fill: parent
        text: control.symbol
        color: control.danger ? Theme.danger : (control.selected ? Theme.accent : Theme.mutedForeground)
        font.pixelSize: 16
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        scale: control.down && control.enabled ? 0.88 : control.hovered && control.enabled ? 1.06 : 1
        Behavior on scale { NumberAnimation { duration: Motion.fast; easing.type: Easing.OutCubic } }
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.down ? Theme.buttonPressed
            : control.hovered ? (control.danger ? Theme.dangerSoft : Theme.buttonHover)
            : control.selected ? Theme.accentSoft : "transparent"
        Behavior on color { ColorAnimation { duration: 60 } }
        border.width: control.activeFocus ? 1 : 0
        border.color: Theme.inputFocusBorder
    }
}
