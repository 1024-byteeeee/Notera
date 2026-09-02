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

    property var expandedFolderIds: ({})
    property ListModel visibleFoldersModel: ListModel {}

    function folderHasChildren(folderId) {
        for (let i = 0; i < libraryService.folders.count; i++) {
            if (libraryService.folders.get(i).parentId === folderId) {
                return true
            }
        }
        return false
    }

    function isFolderExpanded(folderId) {
        return !!root.expandedFolderIds[folderId]
    }

    function toggleFolder(folderId) {
        if (root.expandedFolderIds[folderId]) {
            delete root.expandedFolderIds[folderId]
        } else {
            root.expandedFolderIds[folderId] = true
        }
        root.refreshVisibleFolders()
    }

    function refreshVisibleFolders() {
        root.visibleFoldersModel.clear()
        function addChildren(parentId, depth) {
            for (let i = 0; i < libraryService.folders.count; i++) {
                const f = libraryService.folders.get(i)
                if (f.parentId === parentId) {
                    root.visibleFoldersModel.append({
                        itemId: f.itemId,
                        name: f.name,
                        parentId: f.parentId,
                        depth: depth,
                        hasChildren: root.folderHasChildren(f.itemId),
                        expanded: root.isFolderExpanded(f.itemId)
                    })
                    if (root.isFolderExpanded(f.itemId)) {
                        addChildren(f.itemId, depth + 1)
                    }
                }
            }
        }
        addChildren("", 0)
    }

    function draggedIds(drag) {
        return drag.source && drag.source.dragIds ? drag.source.dragIds : []
    }
    function canMoveAll(ids, folderId) {
        if (!ids || ids.length === 0) return false
        for (let i = 0; i < ids.length; ++i) {
            if (!libraryService.canMoveItemToFolder(ids[i], folderId)) return false
        }
        return true
    }

    Connections {
        target: libraryService.folders
        function onCountChanged() {
            root.refreshVisibleFolders()
        }
    }

    Connections {
        target: libraryService
        function onFoldersChanged() {
            root.refreshVisibleFolders()
        }
    }

    Component.onCompleted: root.refreshVisibleFolders()

    component NavItem: Rectangle {
        id: navItem
        required property string label
        required property string navId
        property string symbol: ""
        property string targetPage: "library"
        property bool selected: false
        property bool contextEnabled: false
        property int indent: 0
        property bool hasChildren: false
        property bool expanded: false
        property bool tagEntry: false
        readonly property bool acceptsLibraryDrop: navId === "all" || navId === "favorites"
            || navId.startsWith("folder:") || navId.startsWith("tag:")
        readonly property int hoverTransitionDuration: 0
        signal contextRequested()
        signal toggleExpand()

        Layout.fillWidth: true
        implicitHeight: 40
        radius: Theme.radiusMd
        color: navDrop.containsDrag ? Theme.accentSoft
            : selected ? Theme.selectedBackground : (navMouse.containsMouse ? Theme.buttonHover : Qt.rgba(Theme.buttonHover.r, Theme.buttonHover.g, Theme.buttonHover.b, 0))
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
            id: navContent
            anchors.fill: parent
            anchors.leftMargin: (navItem.selected ? 16 : 18) + navItem.indent
            anchors.rightMargin: 12
            spacing: 10
            scale: navMouse.pressed ? 0.985 : 1
            Behavior on scale { NumberAnimation { duration: Motion.fast; easing.type: Easing.OutCubic } }

            AppIcon {
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                visible: navItem.hasChildren
                iconName: navItem.expanded ? "chevron-down" : "chevron-right"
                iconColor: navItem.selected ? Theme.selectedText : Theme.mutedForeground
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: navItem.toggleExpand()
                }
            }
            AppIcon {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                visible: navItem.symbol.length > 0 && !navItem.tagEntry
                iconName: navItem.symbol
                iconColor: navItem.selected ? Theme.selectedText : Theme.mutedForeground
            }
            TagIcon {
                objectName: navItem.tagEntry ? "tagEntryIcon" : ""
                visible: navItem.tagEntry
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                iconColor: navItem.selected ? Theme.selectedText : Theme.mutedForeground
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
            hoverEnabled: true
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


        DropArea {
            id: navDrop
            anchors.fill: parent
            z: 5
            enabled: navItem.acceptsLibraryDrop
            onEntered: function(drag) {
                const ids = root.draggedIds(drag)
                if (navItem.navId === "all") drag.accepted = root.canMoveAll(ids, "")
                else if (navItem.navId.startsWith("folder:"))
                    drag.accepted = root.canMoveAll(ids, navItem.navId.substring(7))
                else drag.accepted = ids.length > 0
            }
            onDropped: function(drop) {
                const ids = root.draggedIds(drop)
                if (ids.length === 0) return
                if (navItem.navId === "all") libraryService.moveItems(ids, "")
                else if (navItem.navId === "favorites") libraryService.favoriteItems(ids)
                else if (navItem.navId.startsWith("folder:"))
                    libraryService.moveItems(ids, navItem.navId.substring(7))
                else if (navItem.navId.startsWith("tag:"))
                    libraryService.tagItems(ids, navItem.navId.substring(4))
                drop.acceptProposedAction()
            }
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

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            Layout.leftMargin: 6
            Layout.rightMargin: 6
            Layout.bottomMargin: 8
            color: Theme.border
        }

        NavItem {
            objectName: "libraryNavItem"
            label: "乐谱库"; navId: "all"; symbol: "music"
            selected: appController.currentPage === "library" && appController.libraryFilter === "all"
        }
        NavItem {
            label: "最近使用"; navId: "recent"; symbol: "recent"
            selected: appController.currentPage === "library" && appController.libraryFilter === "recent"
        }
        NavItem {
            label: "收藏"; navId: "favorites"; symbol: "star-filled"
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
            objectName: "collectionsFlick"
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: collectionsLayout.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            ColumnLayout {
                id: collectionsLayout
                objectName: "collectionsLayout"
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
                        symbol: "plus"
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
                    model: root.visibleFoldersModel
                    delegate: NavItem {
                        objectName: "folderNavItem"
                        required property string itemId
                        required property string name
                        required property int depth
                        required property bool hasChildren
                        required property bool expanded
                        label: name
                        navId: "folder:" + itemId
                        symbol: ""
                        indent: depth * 20
                        hasChildren: hasChildren
                        expanded: expanded
                        contextEnabled: true
                        selected: appController.currentPage === "library" && appController.libraryFilter === navId
                        onToggleExpand: root.toggleFolder(itemId)
                        onContextRequested: {
                            folderMenu.targetId = itemId
                            folderMenu.targetName = name
                            folderMenu.popup()
                        }
                    }
                }

                Label {
                    visible: libraryService.folders.count === 0
                    Layout.leftMargin: 12
                    text: "当前没有文件夹"
                    color: Theme.faintForeground
                    font.pixelSize: Theme.fontXs
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
                        symbol: "plus"
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
                        symbol: ""
                        tagEntry: true
                        contextEnabled: true
                        selected: appController.currentPage === "library" && appController.libraryFilter === navId
                        onContextRequested: {
                            tagMenu.targetId = itemId
                            tagMenu.targetName = name
                            tagMenu.popup()
                        }
                    }
                }
                Label {
                    visible: libraryService.tags.count === 0
                    Layout.leftMargin: 12
                    text: "当前没有标签"
                    color: Theme.faintForeground
                    font.pixelSize: Theme.fontXs
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
            label: "设置"; navId: "settings"; symbol: "settings"; targetPage: "settings"
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
            symbol: "edit"
            text: "重命名"
            onTriggered: {
                renameFolderDialog.targetId = folderMenu.targetId
                renameFolderDialog.value = folderMenu.targetName
                renameFolderDialog.open()
            }
        }
        AppMenuSeparator { }
        AppMenuItem {
            symbol: "trash"
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
            symbol: "edit"
            text: "重命名"
            onTriggered: {
                renameTagDialog.targetId = tagMenu.targetId
                renameTagDialog.value = tagMenu.targetName
                renameTagDialog.open()
            }
        }
        AppMenuSeparator { }
        AppMenuItem {
            symbol: "trash"
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
