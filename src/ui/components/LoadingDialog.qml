import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

// 通用加载遮罩：全屏半透明遮罩 + 居中卡片 + BusyIndicator 旋转动画 + 提示文字。
// 用于文件打开、数据库导入/导出等耗时操作，给用户明确的"正在处理"反馈。
// 不做"最小展示时长"表演：操作真实多快就显示多久，操作完成立即关闭。
// 注意：采用普通 Item 而非 Popup——Popup 挂在 Overlay.overlay 下时
// 在本应用（Qt 6.8 / macOS）出现"opened=true 但渲染不可见"的问题，
// 普通 Item + 高 z 值方案在任意窗口/页面层级下都可靠渲染。
Item {
    id: root

    property string message: "正在加载"

    visible: false
    z: 10000
    anchors.fill: parent

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
        root.visible = true
    }

    function hide() {
        // 立即关闭：真实反映加载完成，不做最小展示时长表演
        root.visible = false
    }
}
