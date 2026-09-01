import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

Dialog {
    id: dialog

    property string placeholderText: ""
    property alias value: editor.text
    signal submitted(string value)

    parent: Overlay.overlay
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    width: parent ? Math.min(380, parent.width - 48) : 380
    popupType: Popup.Item
    modal: true
    focus: true
    padding: 22
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    transformOrigin: Item.Center

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Motion.normal; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Motion.fast; easing.type: Easing.InCubic }
            NumberAnimation { property: "scale"; from: 1; to: 0.985; duration: Motion.fast; easing.type: Easing.InCubic }
        }
    }

    onOpened: {
        editor.forceActiveFocus()
        editor.selectAll()
    }

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

    contentItem: TextField {
        id: editor
        implicitHeight: 42
        color: Theme.foreground
        placeholderText: dialog.placeholderText
        placeholderTextColor: Theme.inputPlaceholder
        selectByMouse: true
        leftPadding: 12
        rightPadding: 12
        onAccepted: {
            if (text.trim().length > 0) {
                dialog.submitted(text.trim())
                dialog.close()
            }
        }
        background: Rectangle {
            radius: Theme.radiusMd
            color: Theme.inputBackground
            border.width: 1
            border.color: editor.activeFocus ? Theme.inputFocusBorder : Theme.inputBorder
        }
    }

    footer: Item {
        implicitHeight: 62

        RowLayout {
            anchors.right: parent.right
            anchors.rightMargin: 22
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            AppButton {
                text: "取消"
                onClicked: dialog.reject()
            }
            AppButton {
                text: "确定"
                primary: true
                enabled: editor.text.trim().length > 0
                onClicked: {
                    dialog.submitted(editor.text.trim())
                    dialog.accept()
                }
            }
        }
    }

    background: Rectangle {
        radius: Theme.radiusLg
        color: Theme.surface
        border.width: 1
        border.color: Theme.strongBorder
    }
}
