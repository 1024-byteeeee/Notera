import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

MenuItem {
    id: control
    property bool danger: false
    readonly property int visibleArrowCount: control.arrow && control.arrow.visible
        && control.arrow.implicitWidth > 0 ? 1 : 0
    readonly property real arrowVisualWidth: control.arrow && control.arrow.visible ? control.arrow.implicitWidth : 0

    implicitHeight: 36
    leftPadding: 10
    rightPadding: 8
    indicator: Item {
        visible: false
        implicitWidth: 0
        implicitHeight: 0
        width: 0
        height: 0
    }
    arrow: Item {
        id: submenuArrow
        visible: control.subMenu !== null
        implicitWidth: 14
        implicitHeight: 14

        Canvas {
            id: arrowCanvas
            anchors.centerIn: parent
            width: 8
            height: 12
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = control.enabled ? Theme.mutedForeground : Theme.faintForeground
                ctx.lineWidth = 1.5
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.beginPath()
                ctx.moveTo(2.5, 2.5)
                ctx.lineTo(5.5, 6)
                ctx.lineTo(2.5, 9.5)
                ctx.stroke()
            }
            Connections {
                target: Theme
                function onModeChanged() { arrowCanvas.requestPaint() }
            }
            Connections {
                target: control
                function onEnabledChanged() { arrowCanvas.requestPaint() }
            }
        }
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
            color: !control.enabled ? Theme.faintForeground
                : control.danger ? Theme.danger : Theme.foreground
            font.pixelSize: Theme.fontMd
            verticalAlignment: Text.AlignVCenter
        }

    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.highlighted ? (control.danger ? Theme.dangerSoft : Theme.buttonHover) : "transparent"
    }
}
