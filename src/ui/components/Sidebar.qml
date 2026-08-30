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

    // 自定义输入弹窗组件
    component InputDialog: Rectangle {
        id: dialogRoot
        required property string dialogTitle
        required property string dialogPlaceholder
        property string dialogValue: ""
        signal accepted(string value)
        signal rejected()

        width: 320
        radius: Theme.radiusLg
        color: Theme.surface
        border.color: Theme.strongBorder
        border.width: 1
        visible: false

        function open() { dialogRoot.visible = true }
        function close() { dialogRoot.visible = false }

        Column {
            width: parent.width
            spacing: 16
            topPadding: 20
            bottomPadding: 16
            leftPadding: 20
            rightPadding: 20

            Label {
                text: dialogRoot.dialogTitle
                color: Theme.foreground
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }

            Rectangle {
                width: parent.width
                height: 38
                radius: Theme.radiusMd
                color: Theme.inputBackground
                border.color: inputField.activeFocus ? Theme.inputFocusBorder : Theme.inputBorder
                border.width: 1

                TextField {
                    id: inputField
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    background: Rectangle { color: "transparent" }
                    placeholderText: dialogRoot.dialogPlaceholder
                    placeholderTextColor: Theme.inputPlaceholder
                    color: Theme.foreground
                    font.pixelSize: 14
                    text: dialogRoot.dialogValue
                    focus: true
                    onAccepted: { dialogRoot.accepted(inputField.text); dialogRoot.close() }
                }
            }

            Row {
                width: parent.width
                spacing: 10

                Item { width: parent.width - 168; height: 1 }

                Rectangle {
                    width: 76; height: 34; radius: Theme.radiusMd
                    color: cancelMouse.containsMouse ? Theme.buttonHover : Theme.buttonBackground
                    border.color: Theme.buttonBorder; border.width: 1
                    Label { anchors.centerIn: parent; text: "取消"; color: Theme.buttonText; font.pixelSize: 13 }
                    MouseArea {
                        id: cancelMouse
                        anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: { dialogRoot.rejected(); dialogRoot.close() }
                    }
                }

                Rectangle {
                    width: 76; height: 34; radius: Theme.radiusMd
                    color: okMouse.containsMouse ? Theme.accentHover : Theme.accent
                    Label { anchors.centerIn: parent; text: "确定"; color: Theme.accentForeground; font.pixelSize: 13; font.weight: Font.DemiBold }
                    MouseArea {
                        id: okMouse
                        anchors.fill: parent; hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: { dialogRoot.accepted(inputField.text); dialogRoot.close() }
                    }
                }
            }
        }
    }

    // 导航项组件
    component NavItem: Rectangle {
        required property string label
        required property string navId
        property string icon: ""
        property string targetPage: "library"
        property bool isSelected: false
        property bool showContextMenu: false
        signal contextRequested(int x, int y)

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
            onPressed: function(mouse) {
                if (mouse.button === Qt.RightButton) {
                    parent.contextRequested(mouse.x, mouse.y)
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
            label: "乐谱库"; navId: "all"; icon: "♪"; targetPage: "library"
            isSelected: appController.currentPage === "library" && appController.libraryFilter === "all"
        }
        NavItem {
            label: "最近使用"; navId: "recent"; icon: "⏱"; targetPage: "library"
            isSelected: appController.currentPage === "library" && appController.libraryFilter === "recent"
        }
        NavItem {
            label: "收藏"; navId: "favorites"; icon: "★"; targetPage: "library"
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
                width: 20; height: 20; radius: 5
                color: newFolderMouse.containsMouse ? Theme.buttonHover : "transparent"
                Label { anchors.centerIn: parent; text: "+"; color: Theme.mutedForeground; font.pixelSize: 14; font.weight: Font.Bold }
                MouseArea { id: newFolderMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: newFolderDialog.open() }
            }
        }

        Repeater {
            model: libraryService.folders
            delegate: NavItem {
                label: modelData.name
                navId: "folder:" + modelData.id
                icon: "📁"
                targetPage: "library"
                isSelected: appController.currentPage === "library" && appController.libraryFilter === ("folder:" + modelData.id)
                onContextRequested: function(x, y) { folderMenu.folderId = modelData.id; folderMenu.folderName = modelData.name; folderMenu.popup() }
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
                width: 20; height: 20; radius: 5
                color: newTagMouse.containsMouse ? Theme.buttonHover : "transparent"
                Label { anchors.centerIn: parent; text: "+"; color: Theme.mutedForeground; font.pixelSize: 14; font.weight: Font.Bold }
                MouseArea { id: newTagMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: newTagDialog.open() }
            }
        }

        Repeater {
            model: libraryService.tags
            delegate: NavItem {
                label: modelData.name
                navId: "tag:" + modelData.id
                icon: "🏷"
                targetPage: "library"
                isSelected: appController.currentPage === "library" && appController.libraryFilter === ("tag:" + modelData.id)
                onContextRequested: function(x, y) { tagMenu.tagId = modelData.id; tagMenu.tagName = modelData.name; tagMenu.popup() }
            }
        }

        Item { Layout.fillHeight: true }

        // 底部设置
        NavItem {
            label: "设置"; navId: "settings"; icon: "⚙"; targetPage: "settings"
            isSelected: appController.currentPage === "settings"
        }
    }

    // ── 遮罩层 + 弹窗 ──────────────────────────────
    Rectangle {
        id: overlay
        anchors.fill: parent
        color: "#00000080"
        visible: newFolderDialog.visible || newTagDialog.visible || renameFolderDialog.visible || renameTagDialog.visible
        z: 100

        InputDialog {
            id: newFolderDialog
            anchors.centerIn: parent
            dialogTitle: "新建文件夹"
            dialogPlaceholder: "输入文件夹名称"
            onAccepted: function(value) { libraryService.createFolder(value) }
        }

        InputDialog {
            id: newTagDialog
            anchors.centerIn: parent
            dialogTitle: "新建标签"
            dialogPlaceholder: "输入标签名称"
            onAccepted: function(value) { libraryService.createTag(value) }
        }

        InputDialog {
            id: renameFolderDialog
            anchors.centerIn: parent
            dialogTitle: "重命名文件夹"
            dialogPlaceholder: "输入新名称"
            property string targetId: ""
            onAccepted: function(value) { libraryService.renameFolder(targetId, value) }
            function openFor(id, name) { targetId = id; dialogValue = name; open() }
        }

        InputDialog {
            id: renameTagDialog
            anchors.centerIn: parent
            dialogTitle: "重命名标签"
            dialogPlaceholder: "输入新名称"
            property string targetId: ""
            onAccepted: function(value) { libraryService.renameTag(targetId, value) }
            function openFor(id, name) { targetId = id; dialogValue = name; open() }
        }
    }

    // 文件夹右键菜单
    Menu {
        id: folderMenu
        property string folderId: ""
        property string folderName: ""
        MenuItem { text: "重命名"; onTriggered: renameFolderDialog.openFor(folderMenu.folderId, folderMenu.folderName) }
        MenuSeparator { }
        MenuItem { text: "删除"; onTriggered: libraryService.deleteFolder(folderMenu.folderId) }
    }

    // 标签右键菜单
    Menu {
        id: tagMenu
        property string tagId: ""
        property string tagName: ""
        MenuItem { text: "重命名"; onTriggered: renameTagDialog.openFor(tagMenu.tagId, tagMenu.tagName) }
        MenuSeparator { }
        MenuItem { text: "删除"; onTriggered: libraryService.deleteTag(tagMenu.tagId) }
    }
}
