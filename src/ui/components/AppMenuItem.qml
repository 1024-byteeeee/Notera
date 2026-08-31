import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

MenuItem {
    id: control
    property bool danger: false

    implicitHeight: 36
    leftPadding: 12
    rightPadding: 12
    indicator: Item {
        visible: false
        implicitWidth: 0
        implicitHeight: 0
        width: 0
        height: 0
    }

    contentItem: RowLayout {
        spacing: 8

        Label {
            visible: control.checkable
            Layout.preferredWidth: 16
            text: control.checked ? "✓" : ""
            color: Theme.accent
            font.pixelSize: Theme.fontMd
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignHCenter
        }

        Label {
            Layout.fillWidth: true
            text: control.text
            color: control.danger ? Theme.danger : Theme.foreground
            font.pixelSize: Theme.fontMd
            verticalAlignment: Text.AlignVCenter
        }
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.highlighted ? (control.danger ? Theme.dangerSoft : Theme.buttonHover) : "transparent"
    }
}
