import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

MenuItem {
    id: control
    property bool danger: false
    readonly property int visibleArrowCount: control.arrow && control.arrow.visible
        && control.arrow.implicitWidth > 0 ? 1 : 0

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

        Item {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            Label {
                visible: control.checkable
                anchors.centerIn: parent
                text: control.checked ? "✓" : ""
                color: Theme.accent
                font.pixelSize: Theme.fontMd
                font.weight: Font.Bold
            }
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
