import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

Rectangle {
    id: root
    objectName: "sidebar"
    width: Theme.sidebarWidth
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    component NavItem: Rectangle {
        id: navItem
        required property string label
        required property string navId
        property string symbol: ""
        property string targetPage: "library"
        property bool selected: false
        property bool contextEnabled: false
        readonly property int hoverTransitionDuration: 0
        signal contextRequested()

        Layout.fillWidth: true
        implicitHeight: 40
        radius: Theme.radiusMd
        color: selected ? Theme.selectedBackground : (navHover.hovered ? Theme.buttonHover : "transparent")
        border.width: selected ? 1 : 0
        border.color: selected ? Theme.selectedBorder : "transparent"

        Rectangle {
            visible: navItem.selected
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            width: 3
            height: 20
            radius: 2
            color: Theme.accent
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: navItem.selected ? 16 : 18
            anchors.rightMargin: 12
            spacing: 10

            Label {
                Layout.preferredWidth: 18
                visible: navItem.symbol.length > 0
                text: navItem.symbol
                color: navItem.selected ? Theme.selectedText : Theme.mutedForeground
                font.pixelSize: 15
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }
            FolderIcon {
                visible: navItem.navId.startsWith("folder:")
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                iconColor: navItem.selected ? Theme.selectedText : Theme.mutedForeground
            }
            Label {
                Layout.fillWidth: true
                text: navItem.label
                color: navItem.selected ? Theme.selectedText : Theme.foreground
                font.pixelSize: Theme.fontMd
                font.weight: navItem.selected ? Font.DemiBold : Font.Medium
                elide: Text.ElideRight
            }
        }

        MouseArea {
            id: navMouse
            anchors.fill: parent
            acceptedButtons: navItem.contextEnabled
                ? Qt.LeftButton | Qt.RightButton
                : Qt.LeftButton
            hoverEnabled: false
            preventStealing: true
            cursorShape: Qt.PointingHandCursor
            onPressed: function(mouse) {
                if (mouse.button === Qt.RightButton && navItem.contextEnabled) {
                    navItem.contextRequested()
                }
            }
            onClicked: function(mouse) {
                if (mouse.button === Qt.RightButton) return
                if (navItem.targetPage === "library") {
                    if (navItem.navId === "all") libraryService.goToLibraryRoot()
                    appController.libraryFilter = navItem.navId
                    appController.currentPage = "library"
                } else {
                    appController.currentPage = navItem.targetPage
                }
            }
        }
        HoverHandler {
            id: navHover
            cursorShape: Qt.PointingHandCursor
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 6
            Layout.bottomMargin: 12
            spacing: 10

            Image {
                objectName: "appLogo"
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34
                source: "qrc:/src/assets/notera-icon.png"
                sourceSize.width: 68
                sourceSize.height: 68
                fillMode: Image.PreserveAspectFit
                smooth: true
            }
            Label {
                objectName: "brandLabel"
                text: "Notera"
                color: Theme.foreground
                font.pixelSize: 19
                font.weight: Font.Bold
                font.letterSpacing: 0.2
            }
        }

        AppButton {
            objectName: "sidebarImportButton"
            Layout.fillWidth: true
            Layout.bottomMargin: 12
            text: "导入乐谱"
            primary: true
            onClicked: {
                appController.libraryFilter = "all"
                appController.currentPage = "library"
                libraryService.requestImport()
            }
        }

        NavItem {
            objectName: "libraryNavItem"
            label: "乐谱库"; navId: "all"; symbol: "♪"
            selected: appController.currentPage === "library" && appController.libraryFilter === "all"
        }
        NavItem {
            label: "最近使用"; navId: "recent"; symbol: "◷"
            selected: appController.currentPage === "library" && appController.libraryFilter === "recent"
        }
        NavItem {
            label: "收藏"; navId: "favorites"; symbol: "★"
            selected: appController.currentPage === "library" && appController.libraryFilter === "favorites"
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.bottomMargin: 6
            implicitHeight: 1
            color: Theme.border
        }

        Flickable {
            id: collectionsFlick
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: collectionsLayout.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            ColumnLayout {
                id: collectionsLayout
                width: collectionsFlick.width
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 8
                    Layout.rightMargin: 4
                    Layout.topMargin: 4
                    Layout.bottomMargin: 2

                    Label {
                        text: "文件夹"
                        color: Theme.mutedForeground
                        font.pixelSize: Theme.fontXs
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.7
                    }
                    Item { Layout.fillWidth: true }
                    IconButton {
                        objectName: "newFolderButton"
                        symbol: "+"
                        implicitWidth: 26
                        implicitHeight: 26
                        Accessible.name: "新建文件夹"
                        onClicked: {
                            newFolderDialog.value = ""
                            newFolderDialog.open()
                        }
                    }
                }

                Repeater {
                    model: libraryService.folders
                    delegate: NavItem {
                        objectName: "folderNavItem"
                        required property string itemId
                        required property string name
                        label: name
                        navId: "folder:" + itemId
                        symbol: ""
                        contextEnabled: true
                        selected: appController.currentPage === "library" && appController.libraryFilter === navId
                        onContextRequested: {
                            folderMenu.targetId = itemId
                            folderMenu.targetName = name
                            folderMenu.popup()
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 8
                    Layout.rightMargin: 4
                    Layout.topMargin: 12
                    Layout.bottomMargin: 2

                    Label {
                        text: "标签"
                        color: Theme.mutedForeground
                        font.pixelSize: Theme.fontXs
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.7
                    }
                    Item { Layout.fillWidth: true }
                    IconButton {
                        objectName: "newTagButton"
                        symbol: "+"
                        implicitWidth: 26
                        implicitHeight: 26
                        Accessible.name: "新建标签"
                        onClicked: {
                            newTagDialog.value = ""
                            newTagDialog.open()
                        }
                    }
                }

                Repeater {
                    model: libraryService.tags
                    delegate: NavItem {
                        objectName: "tagNavItem"
                        required property string itemId
                        required property string name
                        label: name
                        navId: "tag:" + itemId
                        symbol: "#"
                        contextEnabled: true
                        selected: appController.currentPage === "library" && appController.libraryFilter === navId
                        onContextRequested: {
                            tagMenu.targetId = itemId
                            tagMenu.targetName = name
                            tagMenu.popup()
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 6
            Layout.bottomMargin: 6
            implicitHeight: 1
            color: Theme.border
        }

        NavItem {
            label: "设置"; navId: "settings"; symbol: "⚙"; targetPage: "settings"
            selected: appController.currentPage === "settings"
        }
    }

    AppDialog {
        id: newFolderDialog
        objectName: "folderEditorDialog"
        title: "新建文件夹"
        placeholderText: "输入文件夹名称"
        onSubmitted: function(text) { libraryService.createFolder(text) }
    }
    AppDialog {
        id: newTagDialog
        title: "新建标签"
        placeholderText: "输入标签名称"
        onSubmitted: function(text) { libraryService.createTag(text) }
    }
    AppDialog {
        id: renameFolderDialog
        property string targetId: ""
        title: "重命名文件夹"
        placeholderText: "输入新名称"
        onSubmitted: function(text) { libraryService.renameFolder(targetId, text) }
    }
    AppDialog {
        id: renameTagDialog
        property string targetId: ""
        title: "重命名标签"
        placeholderText: "输入新名称"
        onSubmitted: function(text) { libraryService.renameTag(targetId, text) }
    }

    ConfirmDialog {
        id: deleteCollectionDialog
        property bool deletingFolder: true
        property string targetId: ""
        title: deletingFolder ? "删除文件夹？" : "删除标签？"
        message: deletingFolder
            ? "文件夹、子文件夹及其中的所有乐谱都会被删除。此操作无法撤销。"
            : "删除标签不会删除任何乐谱。"
        onAccepted: {
            if (deletingFolder) libraryService.deleteFolder(targetId)
            else libraryService.deleteTag(targetId)
            appController.libraryFilter = "all"
        }
    }

    AppMenu {
        id: folderMenu
        objectName: "folderContextMenu"
        property string targetId: ""
        property string targetName: ""
        AppMenuItem {
            text: "重命名"
            onTriggered: {
                renameFolderDialog.targetId = folderMenu.targetId
                renameFolderDialog.value = folderMenu.targetName
                renameFolderDialog.open()
            }
        }
        AppMenuSeparator { }
        AppMenuItem {
            text: "删除文件夹"
            danger: true
            onTriggered: {
                deleteCollectionDialog.deletingFolder = true
                deleteCollectionDialog.targetId = folderMenu.targetId
                deleteCollectionDialog.open()
            }
        }
    }

    AppMenu {
        id: tagMenu
        objectName: "tagContextMenu"
        property string targetId: ""
        property string targetName: ""
        AppMenuItem {
            text: "重命名"
            onTriggered: {
                renameTagDialog.targetId = tagMenu.targetId
                renameTagDialog.value = tagMenu.targetName
                renameTagDialog.open()
            }
        }
        AppMenuSeparator { }
        AppMenuItem {
            text: "删除标签"
            danger: true
            onTriggered: {
                deleteCollectionDialog.deletingFolder = false
                deleteCollectionDialog.targetId = tagMenu.targetId
                deleteCollectionDialog.open()
            }
        }
    }
}
