import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

// 通用加载对话框：半透明遮罩 + 居中卡片 + BusyIndicator 旋转动画 + 提示文字。
// 用于文件打开、数据库导入/导出等耗时操作，给用户明确的"正在处理"反馈。
// 即使任务秒开也保证至少显示 minDisplayDuration，避免"一闪而过"的闪烁感缺失。
Popup {
    id: root

    property string message: "正在加载"
    // 最小显示时长（毫秒）：保证加载动画至少可见一段时间，避免秒开时完全无感。
    property int minDisplayDuration: 350

    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose
    padding: 0
    leftInset: 0
    rightInset: 0
    topInset: 0
    bottomInset: 0

    // 记录打开时间，用于最小显示时长控制
    property real _openTimestamp: 0
    property bool _hidePending: false

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.fast; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Motion.fast; easing.type: Easing.InCubic }
    }

    function show(msg) {
        if (msg !== undefined && msg.length > 0) root.message = msg
        root._openTimestamp = Date.now()
        root._hidePending = false
        root.open()
    }

    function hide() {
        const elapsed = Date.now() - root._openTimestamp
        if (elapsed >= root.minDisplayDuration) {
            root._hidePending = false
            root.close()
        } else {
            // 未达到最小显示时长，延迟关闭
            root._hidePending = true
            hideTimer.restart()
        }
    }

    Timer {
        id: hideTimer
        interval: Math.max(1, root.minDisplayDuration - (Date.now() - root._openTimestamp))
        repeat: false
        onTriggered: {
            if (root._hidePending) {
                root._hidePending = false
                root.close()
            }
        }
    }

    // 半透明遮罩
    background: Rectangle {
        color: "#00000080"
    }

    // 居中卡片
    contentItem: Item {
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(280, root.width - 64)
            height: column.implicitHeight + 48
            radius: Theme.radiusLg
            color: Theme.surface
            border.width: 1
            border.color: Theme.strongBorder

            ColumnLayout {
                id: column
                anchors.centerIn: parent
                spacing: 16

                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    running: true
                    width: 36
                    height: 36
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.message
                    color: Theme.foreground
                    font.pixelSize: Theme.fontMd
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }
}
