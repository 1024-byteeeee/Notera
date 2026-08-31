import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

Button {
    id: control

    property bool primary: false
    property bool danger: false
    property string symbol: ""

    implicitWidth: Math.max(96, contentRow.implicitWidth + 30)
    implicitHeight: 38
    leftPadding: 14
    rightPadding: 14
    hoverEnabled: true

    contentItem: Row {
        id: contentRow
        anchors.centerIn: parent
        spacing: 7

        Label {
            visible: control.symbol.length > 0
            text: control.symbol
            color: control.danger ? Theme.danger : (control.primary ? Theme.accentForeground : Theme.buttonText)
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }

        Label {
            text: control.text
            color: control.danger ? Theme.danger : (control.primary ? Theme.accentForeground : Theme.buttonText)
            font.pixelSize: Theme.fontMd
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
        }
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: {
            if (!control.enabled) return Theme.buttonDisabled
            if (control.down) return control.primary ? Theme.accentPressed : Theme.buttonPressed
            if (control.hovered) return control.primary ? Theme.accentHover : (control.danger ? Theme.dangerSoft : Theme.buttonHover)
            return control.primary ? Theme.accent : (control.danger ? Theme.dangerSoft : Theme.buttonBackground)
        }
        border.width: control.primary ? 0 : 1
        border.color: control.danger ? Theme.danger : Theme.buttonBorder

        Behavior on color { ColorAnimation { duration: 100 } }
    }
}
