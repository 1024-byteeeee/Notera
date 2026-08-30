import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

Dialog {
    id: dialog

    property string message: ""
    property string confirmText: "删除"

    parent: Overlay.overlay
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    width: parent ? Math.min(400, parent.width - 48) : 400
    modal: true
    focus: true
    padding: 22
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    header: Label {
        leftPadding: 22
        rightPadding: 22
        topPadding: 20
        bottomPadding: 4
        text: dialog.title
        color: Theme.foreground
        font.pixelSize: Theme.fontLg
        font.weight: Font.DemiBold
    }

    contentItem: Label {
        text: dialog.message
        color: Theme.secondaryForeground
        font.pixelSize: Theme.fontMd
        wrapMode: Text.WordWrap
    }

    footer: Item {
        implicitHeight: 62
        RowLayout {
            anchors.right: parent.right
            anchors.rightMargin: 22
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10
            AppButton { text: "取消"; onClicked: dialog.reject() }
            AppButton { text: dialog.confirmText; danger: true; onClicked: dialog.accept() }
        }
    }

    background: Rectangle {
        radius: Theme.radiusLg
        color: Theme.surface
        border.width: 1
        border.color: Theme.strongBorder
    }
}
