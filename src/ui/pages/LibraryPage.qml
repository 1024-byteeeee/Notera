import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtCore
import Notera
import "../components"

Rectangle {
    id: root
    objectName: "libraryPage"
    color: Theme.background

    readonly property int selectedCount: libraryService.selection.count
    property bool dragInProgress: false
    property var dragItemIds: []
    property string dragThumbnailPath: ""

    Rectangle {
        id: dragPreview
        objectName: "dragPreview"
        z: 100
        x: -10000
        y: -10000
        width: 110
        height: 145
        visible: root.dragInProgress
        radius: Theme.radiusMd
        opacity: 0.88
        color: Theme.elevatedSurface
        border.width: 1
        border.color: Theme.accent
        property var dragIds: []
        Drag.active: root.dragInProgress
        Drag.dragType: Drag.Internal
        Drag.supportedActions: Qt.MoveAction
        Drag.keys: ["notera-library-items"]
        Drag.hotSpot.x: width / 2
        Drag.hotSpot.y: height / 2

        Image {
            objectName: "dragPreviewImage"
            anchors.fill: parent
            anchors.margins: 6
            source: root.localFileUrl(root.dragThumbnailPath)
            fillMode: Image.PreserveAspectFit
            asynchronous: true
        }

        Rectangle {
            visible: root.dragItemIds.length > 1
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: -8
            width: 26
            height: 26
            radius: 13
            color: Theme.accent
            Label {
                anchors.centerIn: parent
                text: root.dragItemIds.length > 9 ? "9+" : root.dragItemIds.length
                color: Theme.selectedText
                font.pixelSize: Theme.fontXs
                font.weight: Font.Bold
            }
        }
    }

    function dragIds(drag) {
        return drag.source && drag.source.dragIds ? drag.source.dragIds : []
    }
    function localFileUrl(path) {
        if (!path || path.length === 0) return ""
        const normalized = path.replace(/\\/g, "/")
        return normalized.startsWith("/") ? "file://" + normalized : "file:///" + normalized
    }
    function finishInternalDrag() {
        if (root.dragInProgress) {
            try { dragPreview.Drag.drop() } catch (e) {}
        }
        root.dragInProgress = false
        Qt.callLater(function() {
            root.dragItemIds = []
            dragPreview.dragIds = []
            dragPreview.x = -10000
            dragPreview.y = -10000
        })
    }
    function canMoveAll(ids, folderId) {
        if (!ids || ids.length === 0) return false
        for (let i = 0; i < ids.length; ++i) {
            if (!libraryService.canMoveItemToFolder(ids[i], folderId)) return false
        }
        return true
    }

    function selectAll() { libraryService.selection.replace(libraryService.entries.itemIds()) }
    function clearSelection() { libraryService.selection.clear() }
    function isSelected(id) {
        const selectionRevision = root.selectedCount
        return selectionRevision >= 0 && libraryService.selection.contains(id)
    }

    function updateRubberSelection() {
        const ids = []
        const selectionRect = Qt.rect(selectionBox.x, selectionBox.y, selectionBox.width, selectionBox.height)
        for (let i = 0; i < grid.count; i++) {
            const item = grid.itemAtIndex(i)
            if (!item) continue
            const topLeft = item.card.mapToItem(librarySurface, 0, 0)
            const itemRect = Qt.rect(topLeft.x, topLeft.y, item.card.width, item.card.height)
            const intersects = itemRect.x < selectionRect.x + selectionRect.width
                && itemRect.x + itemRect.width > selectionRect.x
                && itemRect.y < selectionRect.y + selectionRect.height
                && itemRect.y + itemRect.height > selectionRect.y
            if (intersects) ids.push(item.itemId)
        }
        libraryService.selection.replace(ids)
    }

    readonly property string filterTitle: {
        const filter = appController.libraryFilter
        if (filter === "all" || filter.startsWith("folder:")) return libraryService.currentFolderName
        if (filter === "recent") return "最近使用"
        if (filter === "favorites") return "收藏"
        if (filter.startsWith("tag:")) return "标签"
        return "乐谱库"
    }

    readonly property string emptyTitle: {
        if (libraryService.searchQuery.length > 0) return "没有符合条件的项目"
        const filter = appController.libraryFilter
        if (filter === "favorites") return "当前没有收藏内容"
        if (filter === "recent") return "当前没有最近使用的项目"
        if (filter.startsWith("tag:")) return "当前标签下没有项目"
        if (filter.startsWith("folder:")) return "当前文件夹为空"
        return libraryService.tags.count === 0 ? "乐谱库为空" : "乐谱库为空"
    }
    readonly property string emptyDescription: libraryService.searchQuery.length > 0
        ? "换个关键词试试"
        : appController.libraryFilter === "favorites" ? "收藏的文件夹和乐谱会显示在这里"
        : appController.libraryFilter === "recent" ? "打开过的文件夹和乐谱会显示在这里"
        : appController.libraryFilter.startsWith("tag:") ? "为文件夹或乐谱添加此标签后会显示在这里"
        : appController.libraryFilter.startsWith("folder:") ? "可在这里新建文件夹或导入乐谱"
        : "导入 PDF 或图片，开始建立你的乐谱库"

    Connections {
        target: appController
        function onLibraryFilterChanged() { libraryService.filterMode = appController.libraryFilter }
    }
    Connections {
        target: libraryService
        function onImportRequested() { fileDialog.open() }
    }
    Component.onCompleted: libraryService.filterMode = appController.libraryFilter

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingXl
        spacing: Theme.spacingLg

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd

            AppButton {
                visible: libraryService.canGoUp
                Layout.preferredWidth: 78
                text: "上一级"
                symbol: "back"
                onClicked: libraryService.goUp()
            }

            ColumnLayout {
                spacing: 3
                Label {
                    text: root.filterTitle
                    color: Theme.foreground
                    font.pixelSize: Theme.font2xl
                    font.weight: Font.Bold
                }
                Label {
                    text: (appController.libraryFilter === "all" || appController.libraryFilter.startsWith("folder:"))
                        ? libraryService.currentFolderBreadcrumb + "  ·  " + libraryService.entries.count + " 个项目"
                        : (libraryService.entries.count > 0 ? libraryService.entries.count + " 个项目" : root.emptyTitle)
                    color: Theme.mutedForeground
                    font.pixelSize: Theme.fontSm
                }
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                Layout.preferredWidth: 240
                Layout.preferredHeight: 38
                radius: Theme.radiusMd
                color: Theme.inputBackground
                border.width: 1
                border.color: searchField.activeFocus ? Theme.inputFocusBorder : Theme.inputBorder

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 8
                    spacing: 8
                    AppIcon { width: 16; height: 16; iconName: "search"; iconColor: Theme.mutedForeground }
                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        color: Theme.foreground
                        placeholderText: "搜索乐谱"
                        placeholderTextColor: Theme.inputPlaceholder
                        font.pixelSize: Theme.fontMd
                        selectByMouse: true
                        text: libraryService.searchQuery
                        onTextChanged: libraryService.searchQuery = text
                        background: Item { }
                    }
                }
            }

            AppButton {
                objectName: "importButton"
                visible: !appController.libraryFilter.startsWith("tag:")
                Layout.preferredWidth: 108
                text: "导入"
                primary: true
                onClicked: fileDialog.open()
            }

            AppButton {
                objectName: "stitchButton"
                visible: !appController.libraryFilter.startsWith("tag:")
                Layout.preferredWidth: 120
                text: "拼接导入"
                primary: true
                onClicked: stitchDialog.open()
            }

        }

        Rectangle {
            id: librarySurface
            objectName: "librarySurface"
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusLg
            color: Theme.surface
            border.width: 1
            border.color: dropArea.containsDrag ? Theme.accent : Theme.border

            GridView {
                id: grid
                objectName: "browserGrid"
                z: 2
                anchors.fill: parent
                anchors.margins: 16
                visible: count > 0
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                interactive: !gridRubberBand.active
                cellWidth: {
                    const columns = Math.max(1, Math.floor(width / 218))
                    return Math.floor(width / columns)
                }
                cellHeight: 326
                model: libraryService.entries

                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Item {
                    id: scoreDelegate
                    objectName: itemType === "score" ? "scoreDelegate" : "folderDelegate"
                    required property string itemType
                    required property string itemId
                    required property string title
                    required property string createdDate
                    required property int pageCount
                    required property string thumbnailPath
                    required property bool favorite
                    required property string filePath
                    required property string fileType
                    required property var tags
                    readonly property string scoreId: itemId
                    readonly property bool contextMenuOpenedOnce: scoreMenu.openedOnce
                    readonly property real contextMenuWidth: scoreMenu.implicitWidth
                    readonly property bool folderSubmenuEnabled: folderSubmenu.enabled
                    readonly property bool tagSubmenuEnabled: tagSubmenu.enabled
                    readonly property int folderSubmenuItemCount: folderSubmenu.count
                    readonly property int tagSubmenuItemCount: tagSubmenu.count
                    readonly property int normalMenuArrowCount: favoriteMenuItem.visibleArrowCount
                    readonly property int folderSubmenuArrowCount: scoreMenu.openedOnce && scoreMenu.count > 4
                        && scoreMenu.itemAt(4) ? scoreMenu.itemAt(4).visibleArrowCount : -1
                    readonly property real folderSubmenuArrowWidth: scoreMenu.openedOnce && scoreMenu.count > 4
                        && scoreMenu.itemAt(4) ? scoreMenu.itemAt(4).arrowVisualWidth : -1
                    readonly property real folderSubmenuArrowRightInset: scoreMenu.openedOnce && scoreMenu.count > 4
                        && scoreMenu.itemAt(4) ? scoreMenu.itemAt(4).arrowRightInset : -1
                    readonly property bool tagMenuHasDefaultCheckIndicator: tagSubmenu.count > 0
                        && tagSubmenu.itemAt(0).indicator.visible
                        && tagSubmenu.itemAt(0).indicator.implicitWidth > 0
                    readonly property alias card: card

                    function closeContextMenu() {
                        scoreMenu.close()
                    }

                    width: grid.cellWidth
                    height: grid.cellHeight

                    Rectangle {
                        id: card
                        objectName: scoreDelegate.itemType === "score" ? "scoreCardMouse" : "folderCardMouse"
                        width: Math.min(218, parent.width - 12)
                        height: 310
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 6
                        radius: Theme.radiusMd
                        color: folderDrop.containsDrag ? Theme.accentSoft : (root.isSelected(scoreDelegate.itemId) ? Theme.accentSoft : (cardHover.hovered ? Theme.cardHover : Theme.cardBackground))
                        Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        border.width: folderDrop.containsDrag ? 2 : (root.isSelected(scoreDelegate.itemId) ? 2 : 1)
                        border.color: folderDrop.containsDrag ? Theme.accent
                            : root.isSelected(scoreDelegate.itemId) ? Theme.accent : (cardHover.hovered ? Theme.strongBorder : Theme.cardBorder)
                        Behavior on border.color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }


                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 13
                            spacing: 9

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 196
                                radius: Theme.radiusSm
                                color: Theme.sunkenSurface
                                border.width: 1
                                border.color: Theme.border
                                clip: true

                                Image {
                                    visible: scoreDelegate.itemType === "score"
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    source: root.localFileUrl(scoreDelegate.thumbnailPath)
                                    asynchronous: true
                                    smooth: true
                                    fillMode: Image.PreserveAspectFit
                                }
                                AppIcon {
                                    anchors.centerIn: parent
                                    visible: scoreDelegate.itemType !== "folder" && scoreDelegate.thumbnailPath.length === 0
                                    width: 38
                                    height: 38
                                    iconName: "music"
                                    iconColor: Theme.faintForeground
                                }
                                FolderIcon {
                                    anchors.centerIn: parent
                                    visible: scoreDelegate.itemType === "folder"
                                    width: 72
                                    height: 58
                                    iconColor: Theme.accent
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: scoreDelegate.title
                                color: Theme.foreground
                                font.pixelSize: Theme.fontMd
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: scoreDelegate.tags.length > 0 ? 22 : 0
                                visible: scoreDelegate.tags.length > 0
                                spacing: 5
                                Repeater {
                                    model: Math.min(2, scoreDelegate.tags.length)
                                    delegate: Rectangle {
                                        required property int index
                                        Layout.preferredWidth: tagText.implicitWidth + 20
                                        Layout.preferredHeight: 22
                                        radius: 8
                                        color: Theme.accentSoft
                                        Row {
                                            anchors.centerIn: parent
                                            spacing: 4
                                            TagIcon { width: 11; height: 11; iconColor: Theme.accent }
                                            Label { id: tagText; text: scoreDelegate.tags[index]; color: Theme.secondaryForeground; font.pixelSize: Theme.fontXs }
                                        }
                                    }
                                }
                                Label {
                                    visible: scoreDelegate.tags.length > 2
                                    text: "+" + (scoreDelegate.tags.length - 2)
                                    color: Theme.mutedForeground
                                    font.pixelSize: Theme.fontXs
                                }
                                Item { Layout.fillWidth: true }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: scoreDelegate.itemType === "folder" ? "文件夹" : "添加于 " + scoreDelegate.createdDate
                                color: Theme.mutedForeground
                                font.pixelSize: Theme.fontSm
                                elide: Text.ElideRight
                            }
                        }

                        HoverHandler {
                            id: cardHover
                            cursorShape: Qt.PointingHandCursor
                        }
                        MouseArea {
                            id: cardMouseArea
                            anchors.fill: parent
                            objectName: scoreDelegate.itemType === "score" ? "scoreCardMouse" : "folderCardMouse"
                            acceptedButtons: Qt.LeftButton
                            preventStealing: true
                            drag.target: dragPreview
                            drag.threshold: Qt.styleHints.startDragDistance
                            property bool preparedDrag: false

                            onPressed: function(mouse) {
                                if (root.selectedCount > 0 && root.isSelected(scoreDelegate.itemId)) {
                                    root.dragItemIds = libraryService.selection.selectedIds
                                } else {
                                    root.dragItemIds = [scoreDelegate.itemId]
                                }
                                root.dragThumbnailPath = scoreDelegate.thumbnailPath
                                dragPreview.dragIds = root.dragItemIds
                                const point = card.mapToItem(root, mouse.x, mouse.y)
                                dragPreview.x = point.x - dragPreview.Drag.hotSpot.x
                                dragPreview.y = point.y - dragPreview.Drag.hotSpot.y
                            }
                            onPositionChanged: {
                                if (!drag.active || preparedDrag) return
                                preparedDrag = true
                                root.dragInProgress = true
                            }
                            onReleased: {
                                preparedDrag = false
                                root.finishInternalDrag()
                            }
                            onCanceled: {
                                preparedDrag = false
                                root.finishInternalDrag()
                            }
                            onClicked: {
                                if (root.selectedCount > 0) {
                                    libraryService.selection.toggle(scoreDelegate.itemId)
                                    return
                                }
                                if (scoreDelegate.itemType === "folder") {
                                    libraryService.enterFolder(scoreDelegate.itemId)
                                } else {
                                    appController.openScore(scoreDelegate.scoreId, scoreDelegate.title, scoreDelegate.filePath,
                                        scoreDelegate.fileType, scoreDelegate.pageCount,
                                        libraryService.scoreFolderId(scoreDelegate.scoreId))
                                }
                            }
                        }
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: {
                                if (scoreDelegate.itemType === "folder") folderCardMenu.popup()
                                else scoreMenu.popup()
                            }
                        }

                        DropArea {
                            id: folderDrop
                            anchors.fill: parent
                            z: 5
                            enabled: scoreDelegate.itemType === "folder"
                            onEntered: function(drag) {
                                const accepted = root.canMoveAll(root.dragIds(drag), scoreDelegate.itemId)
                                drag.accepted = accepted
                            }
                            onDropped: function(drop) {
                                if (!root.canMoveAll(root.dragIds(drop), scoreDelegate.itemId)) return
                                libraryService.moveItems(root.dragIds(drop), scoreDelegate.itemId)
                                drop.acceptProposedAction()
                            }
                        }
                    }


                    Rectangle {
                        id: checkBox
                        objectName: "entryCheckBox"
                        anchors.left: card.left
                        anchors.top: card.top
                        anchors.margins: 10
                        width: 22
                        height: 22
                        radius: 6
                        color: root.isSelected(scoreDelegate.itemId) ? Theme.accent : Theme.surface
                        border.width: 1.5
                        border.color: root.isSelected(scoreDelegate.itemId) ? Theme.accent : Theme.strongBorder
                        z: 3
                        AppIcon {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            iconName: root.isSelected(scoreDelegate.itemId) ? "check" : ""
                            iconColor: "white"
                        }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: libraryService.selection.toggle(scoreDelegate.itemId)
                        }
                    }

                    Item {
                        id: favoriteBtn
                        objectName: scoreDelegate.itemType === "score" ? "favoriteButton" : "folderFavoriteButton"
                        visible: true
                        z: 2
                        anchors.right: card.right
                        anchors.top: card.top
                        anchors.rightMargin: 7
                        anchors.topMargin: 7
                        width: 28
                        height: 28
                        readonly property bool hovered: favoriteHover.hovered

                        AppIcon {
                            anchors.centerIn: parent
                            width: 20
                            height: 20
                            iconName: scoreDelegate.favorite ? "star-filled" : "star"
                            iconColor: scoreDelegate.favorite ? Theme.accent : Theme.mutedForeground
                            scale: favoriteBtn.hovered ? 1.12 : 1
                            Behavior on scale { NumberAnimation { duration: Motion.fast; easing.type: Easing.OutCubic } }
                        }
                        HoverHandler {
                            id: favoriteHover
                            cursorShape: Qt.PointingHandCursor
                        }
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: libraryService.toggleItemFavorite(scoreDelegate.itemId, !scoreDelegate.favorite)
                        }
                    }

                    AppMenu {
                        id: scoreMenu
                        objectName: scoreDelegate.itemType === "score" ? "scoreContextMenu" : ""
                        AppMenuItem {
                            id: favoriteMenuItem
                            symbol: scoreDelegate.favorite ? "star-filled" : "star"
                            text: scoreDelegate.favorite ? "取消收藏" : "添加到收藏"
                            onTriggered: libraryService.toggleItemFavorite(scoreDelegate.itemId, !scoreDelegate.favorite)
                        }
                        AppMenuItem {
                            symbol: "edit"
                            text: "重命名"
                            onTriggered: {
                                renameDialog.scoreId = scoreDelegate.scoreId
                                renameDialog.value = scoreDelegate.title
                                renameDialog.open()
                            }
                        }
                        AppMenuItem {
                            symbol: "export"
                            text: "另存为"
                            onTriggered: {
                                saveAsDialog.scoreId = scoreDelegate.scoreId
                                saveAsDialog.defaultName = scoreDelegate.filePath.split("/").pop()
                                saveAsDialog.open()
                            }
                        }
                        AppMenuSeparator { }

                        AppMenu {
                            id: folderSubmenu
                            title: "移动到文件夹"
                            symbol: "folder"
                            enabled: libraryService.folders.count > 0
                            AppMenuItem {
                                symbol: "folder-up"
                                text: "无（移出文件夹）"
                                onTriggered: libraryService.setItemFolder(scoreDelegate.itemId, "")
                            }
                            AppMenuSeparator { }
                            Instantiator {
                                model: libraryService.folders
                                delegate: AppMenuItem {
                                    required property string itemId
                                    required property string name
                                    text: name
                                    symbol: "folder"
                                    enabled: libraryService.canMoveItemToFolder(scoreDelegate.itemId, itemId)
                                    onTriggered: libraryService.setItemFolder(scoreDelegate.itemId, itemId)
                                }
                                onObjectAdded: function(index, object) {
                                    folderSubmenu.insertItem(index + 2, object)
                                }
                                onObjectRemoved: function(index, object) {
                                    folderSubmenu.removeItem(object)
                                }
                            }
                        }

                        AppMenu {
                            id: tagSubmenu
                            title: "标签"
                            tagIcon: true
                            enabled: libraryService.tags.count > 0
                            Instantiator {
                                model: libraryService.tags
                                delegate: AppMenuItem {
                                    required property string itemId
                                    required property string name
                                    text: name
                                    tagIcon: true
                                    checkable: true
                                    checked: libraryService.itemHasTag(scoreDelegate.itemId, itemId)
                                    onTriggered: {
                                        if (checked) {
                                            libraryService.addItemTag(scoreDelegate.itemId, itemId)
                                        } else {
                                            libraryService.removeItemTag(scoreDelegate.itemId, itemId)
                                        }
                                    }
                                }
                                onObjectAdded: function(index, object) {
                                    tagSubmenu.insertItem(index, object)
                                }
                                onObjectRemoved: function(index, object) {
                                    tagSubmenu.removeItem(object)
                                }
                            }
                        }

                        AppMenuSeparator { }
                        AppMenuItem {
                            symbol: "trash"
                            text: "删除乐谱"
                            danger: true
                            onTriggered: {
                                deleteDialog.scoreId = scoreDelegate.scoreId
                                deleteDialog.filePath = scoreDelegate.filePath
                                deleteDialog.thumbnailPath = scoreDelegate.thumbnailPath
                                deleteDialog.message = "将“" + scoreDelegate.title + "”从 Notera 乐谱库中删除。此操作无法撤销。"
                                deleteDialog.open()
                            }
                        }
                    }

                    AppMenu {
                        id: folderCardMenu
                        AppMenuItem {
                            symbol: scoreDelegate.favorite ? "star-filled" : "star"
                            text: scoreDelegate.favorite ? "取消收藏" : "添加到收藏"
                            onTriggered: libraryService.toggleItemFavorite(scoreDelegate.itemId, !scoreDelegate.favorite)
                        }
                        AppMenuItem {
                            symbol: "open"
                            text: "打开"
                            onTriggered: libraryService.enterFolder(scoreDelegate.itemId)
                        }
                        AppMenuItem {
                            symbol: "edit"
                            text: "重命名"
                            onTriggered: {
                                renameFolderDialog.targetId = scoreDelegate.itemId
                                renameFolderDialog.value = scoreDelegate.title
                                renameFolderDialog.open()
                            }
                        }
                        AppMenuItem {
                            symbol: "export"
                            text: "另存为"
                            onTriggered: {
                                saveFolderAsDialog.folderId = scoreDelegate.itemId
                                saveFolderAsDialog.folderName = scoreDelegate.title
                                saveFolderAsDialog.open()
                            }
                        }
                        AppMenuSeparator { }
                        AppMenu {
                            id: folderMoveSubmenu
                            title: "移动到文件夹"
                            symbol: "folder"
                            enabled: libraryService.folders.count > 0
                            AppMenuItem {
                                symbol: "folder-up"
                                text: "无（移出文件夹）"
                                onTriggered: libraryService.setItemFolder(scoreDelegate.itemId, "")
                            }
                            AppMenuSeparator { }
                            Instantiator {
                                model: libraryService.folders
                                delegate: AppMenuItem {
                                    required property string itemId
                                    required property string name
                                    text: name
                                    symbol: "folder"
                                    enabled: libraryService.canMoveItemToFolder(scoreDelegate.itemId, itemId)
                                    onTriggered: libraryService.setItemFolder(scoreDelegate.itemId, itemId)
                                }
                                onObjectAdded: function(index, object) { folderMoveSubmenu.insertItem(index + 2, object) }
                                onObjectRemoved: function(index, object) { folderMoveSubmenu.removeItem(object) }
                            }
                        }
                        AppMenu {
                            id: folderTagSubmenu
                            title: "标签"
                            tagIcon: true
                            enabled: libraryService.tags.count > 0
                            Instantiator {
                                model: libraryService.tags
                                delegate: AppMenuItem {
                                    required property string itemId
                                    required property string name
                                    text: name
                                    tagIcon: true
                                    checkable: true
                                    checked: libraryService.itemHasTag(scoreDelegate.itemId, itemId)
                                    onTriggered: checked ? libraryService.addItemTag(scoreDelegate.itemId, itemId)
                                        : libraryService.removeItemTag(scoreDelegate.itemId, itemId)
                                }
                                onObjectAdded: function(index, object) { folderTagSubmenu.insertItem(index, object) }
                                onObjectRemoved: function(index, object) { folderTagSubmenu.removeItem(object) }
                            }
                        }
                        AppMenuSeparator { }
                        AppMenuItem {
                            symbol: "trash"
                            text: "删除文件夹"
                            danger: true
                            onTriggered: {
                                deleteFolderDialog.targetId = scoreDelegate.itemId
                                deleteFolderDialog.folderName = scoreDelegate.title
                                deleteFolderDialog.open()
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: selectionBox
                objectName: "selectionBox"
                readonly property color appliedBorderColor: Theme.marqueeBorder
                visible: gridRubberBand.active && gridRubberBand.validStart
                z: 10
                color: Theme.marqueeFill
                border.width: 1
                border.color: appliedBorderColor
                radius: 4
            }

            DragHandler {
                id: gridRubberBand
                objectName: "gridRubberBand"
                property bool validStart: false
                enabled: !root.dragInProgress && grid.count > 0
                target: null
                acceptedButtons: Qt.LeftButton
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                grabPermissions: PointerHandler.CanTakeOverFromItems
                    | PointerHandler.ApprovesTakeOverByAnything

                onActiveChanged: {
                    if (!active) {
                        validStart = false
                        return
                    }
                    const p = centroid.pressPosition
                    validStart = p.x >= 0 && p.y >= 0
                        && p.x <= librarySurface.width && p.y <= librarySurface.height
                    if (!validStart) return
                    selectionBox.x = p.x
                    selectionBox.y = p.y
                    selectionBox.width = 0
                    selectionBox.height = 0
                }
                onActiveTranslationChanged: {
                    if (!active || !validStart) return
                    const start = centroid.pressPosition
                    const curX = centroid.position.x
                    const curY = centroid.position.y
                    selectionBox.x = Math.min(start.x, curX)
                    selectionBox.y = Math.min(start.y, curY)
                    selectionBox.width = Math.abs(curX - start.x)
                    selectionBox.height = Math.abs(curY - start.y)
                    root.updateRubberSelection()
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                visible: grid.count === 0
                spacing: 10

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 72
                    radius: 22
                    color: Theme.elevatedSurface
                    border.width: 1
                    border.color: Theme.border
                    AppIcon { anchors.centerIn: parent; width: 32; height: 32; iconName: "music"; iconColor: Theme.mutedForeground }
                }
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.emptyTitle
                    color: Theme.foreground
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.emptyDescription
                    color: Theme.mutedForeground
                    font.pixelSize: Theme.fontSm
                }
                AppButton {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 6
                    visible: libraryService.searchQuery.length === 0 && appController.libraryFilter === "all"
                    text: "导入第一份乐谱"
                    primary: true
                    onClicked: fileDialog.open()
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 8
                visible: dropArea.containsDrag && dropArea.hasExternalUrls
                radius: Theme.radiusLg
                color: Theme.accentSoft
                border.width: 2
                border.color: Theme.accent
                z: 10
                Label {
                    anchors.centerIn: parent
                    text: "松开即可导入乐谱"
                    color: Theme.selectedText
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.DemiBold
                }
            }

            DropArea {
                id: dropArea
                property bool hasExternalUrls: false
                anchors.fill: parent
                z: 1
                onEntered: function(drag) {
                    hasExternalUrls = drag.urls.length > 0
                }
                onExited: hasExternalUrls = false
                onDropped: function(drop) {
                    hasExternalUrls = false
                    // 内部拖放（移动乐谱/文件夹）仅在文件夹卡片上生效；
                    // 松开在空白区域时不修改数据库，项目自动回到原位置。
                    if (root.dragIds(drop).length > 0) {
                        drop.acceptProposedAction()
                        return
                    }
                    for (let index = 0; index < drop.urls.length; ++index)
                        libraryService.importLocalFile(drop.urls[index])
                }
            }

            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: blankContextMenu.popup()
            }
        }

        Rectangle {
            visible: root.selectedCount > 0
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            radius: Theme.radiusMd
            color: Theme.elevatedSurface
            border.width: 1
            border.color: Theme.border

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Label {
                    text: root.selectedCount + " 项已选中"
                    color: Theme.foreground
                    font.pixelSize: Theme.fontMd
                    font.weight: Font.DemiBold
                }

                Item { Layout.fillWidth: true }

                AppButton {
                    text: "全选"
                    Layout.preferredWidth: 70
                    onClicked: root.selectAll()
                }

                AppButton {
                    text: "取消全选"
                    Layout.preferredWidth: 84
                    onClicked: root.clearSelection()
                }

                AppButton {
                    text: "删除"
                    danger: true
                    Layout.preferredWidth: 70
                    enabled: root.selectedCount > 0
                    onClicked: {
                        batchDeleteDialog.selectedIds = libraryService.selection.selectedIds
                        batchDeleteDialog.open()
                    }
                }

                AppButton {
                    text: "完成"
                    primary: true
                    Layout.preferredWidth: 70
                    onClicked: root.clearSelection()
                }
            }
        }
    }

    AppMenu {
        id: blankContextMenu
        objectName: "blankContextMenu"
        AppMenuItem { text: "新建文件夹"; symbol: "folder"; visible: !appController.libraryFilter.startsWith("tag:"); onTriggered: newFolderDialog.open() }
        AppMenuItem { text: "新建标签"; tagIcon: true; onTriggered: newTagDialog.open() }
        AppMenuSeparator { visible: !appController.libraryFilter.startsWith("tag:") }
        AppMenuItem { text: "导入乐谱"; symbol: "import"; visible: !appController.libraryFilter.startsWith("tag:"); onTriggered: fileDialog.open() }
    }

    AppDialog {
        id: newFolderDialog
        title: "新建文件夹"
        placeholderText: "输入文件夹名称"
        onSubmitted: function(text) { libraryService.createFolder(text) }
    }

    AppDialog {
        id: renameFolderDialog
        property string targetId: ""
        title: "重命名文件夹"
        placeholderText: "输入新名称"
        onSubmitted: function(text) { libraryService.renameFolder(targetId, text) }
    }

    ConfirmDialog {
        id: deleteFolderDialog
        property string targetId: ""
        property string folderName: ""
        title: "删除文件夹？"
        message: "将删除“" + folderName + "”及其中的所有子文件夹和乐谱。此操作无法撤销。"
        onAccepted: libraryService.deleteFolder(targetId)
    }

    AppDialog {
        id: newTagDialog
        title: "新建标签"
        placeholderText: "输入标签名称"
        onSubmitted: function(text) { libraryService.createTag(text) }
    }

    FileDialog {
        id: fileDialog
        title: "导入本地乐谱"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["支持的乐谱 (*.pdf *.jpg *.jpeg *.png *.bmp *.gif *.webp *.tif *.tiff)", "所有文件 (*)"]
        onAccepted: {
            for (let index = 0; index < selectedFiles.length; ++index)
                libraryService.importLocalFile(selectedFiles[index])
        }
    }

    FileDialog {
        id: stitchDialog
        title: "选择多张图片拼接导入（按选择顺序垂直拼接）"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["图片文件 (*.jpg *.jpeg *.png *.bmp *.gif *.webp *.tif *.tiff)", "所有文件 (*)"]
        onAccepted: {
            var paths = []
            for (var i = 0; i < selectedFiles.length; i++) {
                paths.push(selectedFiles[i].toString())
            }
            libraryService.importAndStitchImages(paths)
        }
    }

    AppDialog {
        id: renameDialog
        property string scoreId: ""
        title: "重命名乐谱"
        placeholderText: "输入乐谱名称"
        onSubmitted: function(text) { libraryService.renameScore(scoreId, text) }
    }

    ConfirmDialog {
        id: deleteDialog
        property string scoreId: ""
        property string filePath: ""
        property string thumbnailPath: ""
        title: "删除乐谱？"
        onAccepted: libraryService.deleteScore(scoreId, filePath, thumbnailPath)
    }

    ConfirmDialog {
        id: batchDeleteDialog
        property var selectedIds: []
        title: "批量删除？"
        message: "将删除选中的 " + batchDeleteDialog.selectedIds.length + " 个项目。此操作无法撤销。"
        onAccepted: {
            libraryService.deleteItems(batchDeleteDialog.selectedIds)
            root.clearSelection()
        }
    }

    FileDialog {
        id: saveAsDialog
        property string scoreId: ""
        property string defaultName: ""
        title: "另存为"
        fileMode: FileDialog.SaveFile
        defaultSuffix: ""
        onAccepted: {
            const error = libraryService.saveScoreAs(scoreId, selectedFile)
            saveAsResultDialog.title = error.length > 0 ? "另存为失败" : "另存为成功"
            saveAsResultDialog.message = error.length > 0 ? error : "乐谱已保存到所选位置。"
            saveAsResultDialog.open()
        }
    }

    FolderDialog {
        id: saveFolderAsDialog
        property string folderId: ""
        property string folderName: ""
        title: "选择导出目标目录"
        currentFolder: StandardPaths.standardLocations(StandardPaths.HomeLocation)[0]
        onAccepted: {
            const error = libraryService.saveFolderAs(folderId, selectedFolder)
            saveAsResultDialog.title = error.length > 0 ? "导出失败" : "导出成功"
            saveAsResultDialog.message = error.length > 0 ? error : "文件夹已导出到所选位置。"
            saveAsResultDialog.open()
        }
    }

    ConfirmDialog {
        id: saveAsResultDialog
        title: "另存为结果"
        confirmText: "确定"
    }
}
