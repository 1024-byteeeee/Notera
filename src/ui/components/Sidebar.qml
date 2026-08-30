import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

Rectangle {
    id: root
    width: Theme.sidebarWidth
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    property string currentPage: "library"

    signal pageSelected(string page)

    // 导航项组件
    component NavItem: Rectangle {
        required property string label
        required property string targetPage
        property string icon: ""
        property bool selected: root.currentPage === targetPage

        width: parent.width
        height: 38
        radius: Theme.radiusMd
        color: selected ? Theme.selectedBackground : (mouseArea.containsMouse ? Theme.buttonHover : "transparent")
        border.width: selected ? 1 : 0
        border.color: selected ? Theme.selectedBorder : "transparent"

        Behavior on color { ColorAnimation { duration: 120 } }

        Rectangle {
            visible: selected
            width: 3
            height: 18
            radius: 2
            color: Theme.accent
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.verticalCenter: parent.verticalCenter
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: selected ? 16 : 18
            anchors.rightMargin: 14
            spacing: 10

            Label {
                text: icon
                color: selected ? Theme.selectedText : Theme.mutedForeground
                font.pixelSize: 15
                font.weight: selected ? Font.DemiBold : Font.Normal
                Layout.preferredWidth: 18
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                text: label
                color: selected ? Theme.selectedText : Theme.foreground
                font.pixelSize: Theme.fontMd
                font.weight: selected ? Font.DemiBold : Font.Medium
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.pageSelected(targetPage)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 4

        // Logo
        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 18
            Layout.leftMargin: 6
            spacing: 10

            Rectangle {
                width: 32
                height: 32
                radius: 9
                color: Theme.accent
                Label {
                    anchors.centerIn: parent
                    text: "N"
                    color: Theme.accentForeground
                    font.pixelSize: 17
                    font.weight: Font.Bold
                }
            }

            Label {
                text: "Notera"
                color: Theme.foreground
                font.pixelSize: 18
                font.weight: Font.Bold
                font.letterSpacing: 0.3
            }
        }

        // 主导航
        NavItem { label: "乐谱库"; targetPage: "library"; icon: "♪" }
        NavItem { label: "最近使用"; targetPage: "library"; icon: "⏱" }
        NavItem { label: "收藏"; targetPage: "library"; icon: "★" }

        // 分组
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 16
            Layout.bottomMargin: 4
            height: 1
            color: Theme.border
        }

        Label {
            text: "文件夹"
            color: Theme.mutedForeground
            font.pixelSize: Theme.fontXs
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.8
            Layout.leftMargin: 8
            Layout.topMargin: 8
            Layout.bottomMargin: 4
        }
        Label {
            text: "暂无文件夹"
            color: Theme.faintForeground
            font.pixelSize: Theme.fontSm
            Layout.leftMargin: 8
            Layout.bottomMargin: 8
        }

        Label {
            text: "标签"
            color: Theme.mutedForeground
            font.pixelSize: Theme.fontXs
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.8
            Layout.leftMargin: 8
            Layout.topMargin: 8
            Layout.bottomMargin: 4
        }
        Label {
            text: "暂无标签"
            color: Theme.faintForeground
            font.pixelSize: Theme.fontSm
            Layout.leftMargin: 8
        }

        Item { Layout.fillHeight: true }

        // 底部设置
        NavItem {
            label: "设置"
            targetPage: "settings"
            icon: "⚙"
            selected: root.currentPage === "settings"
        }
    }
}
