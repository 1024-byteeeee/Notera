import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

MenuItem {
    id: control
    property bool danger: false
    property string symbol: ""
    property bool tagIcon: false
    readonly property string effectiveSymbol: control.symbol.length > 0 ? control.symbol
        : (control.subMenu && control.subMenu.symbol ? control.subMenu.symbol : "")
    readonly property bool effectiveTagIcon: control.tagIcon
        || (control.subMenu && control.subMenu.tagIcon ? control.subMenu.tagIcon : false)
    readonly property int visibleArrowCount: control.arrow && control.arrow.visible
        && control.arrow.implicitWidth > 0 ? 1 : 0
    readonly property real arrowVisualWidth: control.arrow && control.arrow.visible ? control.arrow.implicitWidth : 0
    readonly property real arrowRightInset: control.arrow && control.arrow.visible
        ? control.width - control.arrow.x - control.arrow.width : 0

    implicitHeight: 40
    leftPadding: 12
    rightPadding: control.subMenu !== null ? 36 : 12
    topPadding: 0
    bottomPadding: 0
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
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter

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
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            TagIcon {
                visible: control.effectiveTagIcon
                anchors.centerIn: parent
                width: 16
                height: 16
                iconColor: control.danger ? Theme.danger : Theme.secondaryForeground
            }
            Label {
                visible: control.checkable && !control.effectiveTagIcon
                anchors.centerIn: parent
                text: control.checked ? "✓" : ""
                color: Theme.accent
                font.pixelSize: Theme.fontMd
                font.weight: Font.Bold
            }
            Label {
                visible: !control.checkable && !control.effectiveTagIcon && control.effectiveSymbol.length > 0
                anchors.centerIn: parent
                text: control.effectiveSymbol
                color: control.danger ? Theme.danger : Theme.secondaryForeground
                font.pixelSize: 15
                font.weight: Font.Medium
            }
            Label {
                visible: control.checkable && control.effectiveTagIcon && control.checked
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                text: "✓"
                color: Theme.accent
                font.pixelSize: 10
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
