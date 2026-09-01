import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

Button {
    id: control

    property bool primary: false
    property bool danger: false
    property string symbol: ""
    readonly property int hoverTransitionDuration: 0
    readonly property real visualContentCenterX: contentContainer.x + contentRow.x + contentRow.childrenRect.x
        + contentRow.childrenRect.width / 2

    implicitWidth: Math.max(96, contentRow.implicitWidth + 30)
    implicitHeight: 38
    leftPadding: 14
    rightPadding: 14
    hoverEnabled: true

    contentItem: Item {
        id: contentContainer
        implicitWidth: contentRow.implicitWidth
        implicitHeight: contentRow.implicitHeight
        scale: control.down && control.enabled ? 0.97 : 1
        Behavior on scale { NumberAnimation { duration: Motion.fast; easing.type: Easing.OutCubic } }

        Row {
            id: contentRow
            anchors.centerIn: parent
            spacing: 7

            AppIcon {
                visible: control.symbol.length > 0
                width: 16
                height: 16
                iconName: control.symbol
                iconColor: control.danger ? Theme.danger : (control.primary ? Theme.accentForeground : Theme.buttonText)
            }

            Label {
                text: control.text
                color: control.danger ? Theme.danger : (control.primary ? Theme.accentForeground : Theme.buttonText)
                font.pixelSize: Theme.fontMd
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: {
            if (!control.enabled) return Theme.buttonDisabled
            if (control.down) return control.primary ? Theme.accentHover : (control.danger ? Theme.dangerSoft : Theme.buttonHover)
            if (control.hovered) return control.primary ? Theme.accentHover : (control.danger ? Theme.dangerSoft : Theme.buttonHover)
            return control.primary ? Theme.accent : (control.danger ? Theme.dangerSoft : Theme.buttonBackground)
        }
        border.width: control.primary ? 0 : 1
        border.color: control.danger ? Theme.danger : Theme.buttonBorder

    }
}
