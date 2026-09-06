import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

// 通用加载遮罩：全屏半透明遮罩 + 居中卡片 + BusyIndicator 旋转动画 + 提示文字。
// 用于文件打开、数据库导入/导出等耗时操作，给用户明确的"正在处理"反馈。
// 即使任务秒开也保证至少显示 minDisplayDuration，避免"一闪而过"的闪烁感缺失。
// 注意：采用普通 Item 而非 Popup——Popup 挂在 Overlay.overlay 下时
// 在本应用（Qt 6.8 / macOS）出现"opened=true 但渲染不可见"的问题，
// 普通 Item + 高 z 值方案在任意窗口/页面层级下都可靠渲染。
Item {
    id: root

    property string message: "正在加载"
    // 最小显示时长（毫秒）：保证加载动画至少可见一段时间，避免秒开时完全无感。
    property int minDisplayDuration: 500

    visible: false
    z: 10000
    anchors.fill: parent

    // 记录打开时间，用于最小显示时长控制
    property real _openTimestamp: 0
    property bool _hidePending: false

    // 全屏半透明遮罩（同时拦截鼠标事件，加载期间阻止误操作）
    Rectangle {
        anchors.fill: parent
        color: "#00000080"

        MouseArea {
            anchors.fill: parent
        }

        // 居中卡片
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(280, parent.width - 64)
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
                    running: root.visible
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

    function show(msg) {
        if (msg !== undefined && msg.length > 0) root.message = msg
        root._openTimestamp = Date.now()
        root._hidePending = false
        root.visible = true
    }

    function hide() {
        const elapsed = Date.now() - root._openTimestamp
        if (elapsed >= root.minDisplayDuration) {
            root._hidePending = false
            root.visible = false
        } else {
            // 未达到最小显示时长，延迟关闭。
            // 注意：interval 必须在 hide() 时动态计算，不能静态绑定——
            // 静态绑定会在组件实例化时计算一次（此时 _openTimestamp=0，
            // 应用已运行数秒后该值会变成 1ms），导致"最小显示时长"失效。
            root._hidePending = true
            hideTimer.interval = Math.max(1, root.minDisplayDuration - elapsed)
            hideTimer.restart()
        }
    }

    Timer {
        id: hideTimer
        interval: root.minDisplayDuration
        repeat: false
        onTriggered: {
            if (root._hidePending) {
                root._hidePending = false
                root.visible = false
            }
        }
    }
}
