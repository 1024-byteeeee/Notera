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

    // 导航项组件
    component NavItem: Rectangle {
        required property string label
        required property string navId
        property string icon: ""
        property string targetPage: "library"
        property bool isSelected: false

        width: parent.width
        height: 38
        radius: Theme.radiusMd
        color: isSelected ? Theme.selectedBackground : (mouseArea.containsMouse ? Theme.buttonHover : "transparent")
        border.width: isSelected ? 1 : 0
        border.color: isSelected ? Theme.selectedBorder : "transparent"

        Behavior on color { ColorAnimation { duration: 120 } }

        Rectangle {
            visible: isSelected
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
            anchors.leftMargin: isSelected ? 16 : 18
            anchors.rightMargin: 14
            spacing: 10

            Label {
                text: icon
                color: isSelected ? Theme.selectedText : Theme.mutedForeground
                font.pixelSize: 15
                font.weight: isSelected ? Font.DemiBold : Font.Normal
                Layout.preferredWidth: 18
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                text: label
                color: isSelected ? Theme.selectedText : Theme.foreground
                font.pixelSize: Theme.fontMd
                font.weight: isSelected ? Font.DemiBold : Font.Medium
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (targetPage === "library") {
                    appController.libraryFilter = navId
                    appController.currentPage = "library"
                } else {
                    appController.currentPage = targetPage
                }
            }
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
        NavItem {
            label: "乐谱库"
            navId: "all"
            icon: "♪"
            targetPage: "library"
            isSelected: appController.currentPage === "library" && appController.libraryFilter === "all"
        }
        NavItem {
            label: "最近使用"
            navId: "recent"
            icon: "⏱"
            targetPage: "library"
            isSelected: appController.currentPage === "library" && appController.libraryFilter === "recent"
        }
        NavItem {
            label: "收藏"
            navId: "favorites"
            icon: "★"
            targetPage: "library"
            isSelected: appController.currentPage === "library" && appController.libraryFilter === "favorites"
        }

        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 16
            Layout.bottomMargin: 4
            height: 1
            color: Theme.border
        }

        // 文件夹分组
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.topMargin: 8
            Layout.bottomMargin: 4
            spacing: 6

            Label {
                text: "文件夹"
                color: Theme.mutedForeground
                font.pixelSize: Theme.fontXs
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.8
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                width: 20
                height: 20
                radius: 5
                color: newFolderMouse.containsMouse ? Theme.buttonHover : "transparent"
                Label {
                    anchors.centerIn: parent
                    text: "+"
                    color: Theme.mutedForeground
                    font.pixelSize: 14
                    font.weight: Font.Bold
                }
                MouseArea {
                    id: newFolderMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: newFolderDialog.open()
                }
            }
        }

        Repeater {
            model: libraryService.folders
            delegate: NavItem {
                label: model.name
                navId: "folder:" + model.id
                icon: "📁"
                targetPage: "library"
                isSelected: appController.currentPage === "library" && appController.libraryFilter === ("folder:" + model.id)
            }
        }

        // 标签分组
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            Layout.topMargin: 12
            Layout.bottomMargin: 4
            spacing: 6

            Label {
                text: "标签"
                color: Theme.mutedForeground
                font.pixelSize: Theme.fontXs
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.8
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                width: 20
                height: 20
                radius: 5
                color: newTagMouse.containsMouse ? Theme.buttonHover : "transparent"
                Label {
                    anchors.centerIn: parent
                    text: "+"
                    color: Theme.mutedForeground
                    font.pixelSize: 14
                    font.weight: Font.Bold
                }
                MouseArea {
                    id: newTagMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: newTagDialog.open()
                }
            }
        }

        Repeater {
            model: libraryService.tags
            delegate: NavItem {
                label: model.name
                navId: "tag:" + model.id
                icon: "🏷"
                targetPage: "library"
                isSelected: appController.currentPage === "library" && appController.libraryFilter === ("tag:" + model.id)
            }
        }

        Item { Layout.fillHeight: true }

        // 底部设置
        NavItem {
            label: "设置"
            navId: "settings"
            icon: "⚙"
            targetPage: "settings"
            isSelected: appController.currentPage === "settings"
        }
    }

    // 新建文件夹对话框
    Dialog {
        id: newFolderDialog
        title: "新建文件夹"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: libraryService.createFolder(folderName.text)
        TextField {
            id: folderName
            width: 280
            placeholderText: "文件夹名称"
            focus: true
        }
    }

    // 新建标签对话框
    Dialog {
        id: newTagDialog
        title: "新建标签"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: libraryService.createTag(tagName.text)
        TextField {
            id: tagName
            width: 280
            placeholderText: "标签名称"
            focus: true
        }
    }
}
