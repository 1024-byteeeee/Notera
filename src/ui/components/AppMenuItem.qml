import QtQuick
import QtQuick.Controls
import Notera

MenuItem {
    id: control
    property bool danger: false

    implicitHeight: 36
    leftPadding: 12
    rightPadding: 12

    contentItem: Label {
        text: control.text
        color: control.danger ? Theme.danger : Theme.foreground
        font.pixelSize: Theme.fontMd
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.highlighted ? (control.danger ? Theme.dangerSoft : Theme.buttonHover) : "transparent"
    }
}
