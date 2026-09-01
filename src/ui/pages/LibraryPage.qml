import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Notera
import "../components"

Rectangle {
    id: root
    objectName: "libraryPage"
    color: Theme.background

    readonly property int selectedCount: libraryService.selection.count

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
                text: "← 上一级"
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
                        : (libraryService.scores.count > 0 ? libraryService.scores.count + " 份乐谱" : "这里还没有乐谱")
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
                    Label { text: "⌕"; color: Theme.mutedForeground; font.pixelSize: 16 }
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
                Layout.preferredWidth: 108
                text: "导入"
                primary: true
                onClicked: fileDialog.open()
            }

            AppButton {
                objectName: "stitchButton"
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
                anchors.fill: parent
                anchors.margins: 16
                visible: count > 0
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                interactive: true
                cellWidth: {
                    const columns = Math.max(1, Math.floor(width / 218))
                    return Math.floor(width / columns)
                }
                cellHeight: 304
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
                    readonly property string scoreId: itemId
                    readonly property bool contextMenuOpenedOnce: scoreMenu.openedOnce
                    readonly property real contextMenuWidth: scoreMenu.implicitWidth
                    readonly property bool folderSubmenuEnabled: folderSubmenu.enabled
                    readonly property bool tagSubmenuEnabled: tagSubmenu.enabled
                    readonly property int folderSubmenuItemCount: folderSubmenu.count
                    readonly property int tagSubmenuItemCount: tagSubmenu.count
                    readonly property int normalMenuArrowCount: favoriteMenuItem.visibleArrowCount
                    readonly property int folderSubmenuArrowCount: scoreMenu.openedOnce && scoreMenu.count > 3
                        && scoreMenu.itemAt(3) ? scoreMenu.itemAt(3).visibleArrowCount : -1
                    readonly property real folderSubmenuArrowWidth: scoreMenu.openedOnce && scoreMenu.count > 3
                        && scoreMenu.itemAt(3) ? scoreMenu.itemAt(3).arrowVisualWidth : -1
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
                        height: 288
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 6
                        radius: Theme.radiusMd
                        color: root.isSelected(scoreDelegate.itemId) ? Theme.accentSoft : (cardHover.hovered ? Theme.cardHover : Theme.cardBackground)
                        border.width: root.isSelected(scoreDelegate.itemId) ? 2 : 1
                        border.color: root.isSelected(scoreDelegate.itemId) ? Theme.accent : (cardHover.hovered ? Theme.strongBorder : Theme.cardBorder)

                        Behavior on color { ColorAnimation { duration: Motion.fast } }
                        Behavior on border.color { ColorAnimation { duration: Motion.fast } }

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
                                    source: scoreDelegate.thumbnailPath.length > 0 ? "file://" + scoreDelegate.thumbnailPath : ""
                                    asynchronous: true
                                    smooth: true
                                    fillMode: Image.PreserveAspectFit
                                }
                                Label {
                                    anchors.centerIn: parent
                                    visible: scoreDelegate.itemType !== "folder" && scoreDelegate.thumbnailPath.length === 0
                                    text: "♫"
                                    color: Theme.faintForeground
                                    font.pixelSize: 38
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
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: {
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
                    }

                    // 左上角勾选框始终可用，并位于卡片点击层之上。
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
                        Label {
                            anchors.centerIn: parent
                            text: root.isSelected(scoreDelegate.itemId) ? "✓" : ""
                            color: "white"
                            font.pixelSize: 14
                            font.weight: Font.Bold
                        }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: libraryService.selection.toggle(scoreDelegate.itemId)
                        }
                    }

                    // 收藏按钮 - 悬浮放大五角星，无框
                    Item {
                        id: favoriteBtn
                        objectName: scoreDelegate.itemType === "score" ? "favoriteButton" : ""
                        visible: scoreDelegate.itemType === "score"
                        z: 2
                        anchors.right: card.right
                        anchors.top: card.top
                        anchors.rightMargin: 7
                        anchors.topMargin: 7
                        width: 28
                        height: 28
                        readonly property bool hovered: favoriteHover.hovered

                        Label {
                            anchors.centerIn: parent
                            text: scoreDelegate.favorite ? "★" : "☆"
                            color: scoreDelegate.favorite ? Theme.accent : Theme.mutedForeground
                            font.pixelSize: favoriteBtn.hovered ? 22 : 18
                            font.weight: Font.DemiBold
                            Behavior on font.pixelSize { NumberAnimation { duration: Motion.fast; easing.type: Easing.OutCubic } }
                        }
                        HoverHandler {
                            id: favoriteHover
                            cursorShape: Qt.PointingHandCursor
                        }
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: libraryService.toggleFavorite(scoreDelegate.scoreId, !scoreDelegate.favorite)
                        }
                    }

                    AppMenu {
                        id: scoreMenu
                        objectName: scoreDelegate.itemType === "score" ? "scoreContextMenu" : ""
                        AppMenuItem {
                            id: favoriteMenuItem
                            text: scoreDelegate.favorite ? "取消收藏" : "添加到收藏"
                            onTriggered: libraryService.toggleFavorite(scoreDelegate.scoreId, !scoreDelegate.favorite)
                        }
                        AppMenuItem {
                            text: "重命名"
                            onTriggered: {
                                renameDialog.scoreId = scoreDelegate.scoreId
                                renameDialog.value = scoreDelegate.title
                                renameDialog.open()
                            }
                        }
                        AppMenuSeparator { }

                        // 移动到文件夹子菜单
                        AppMenu {
                            id: folderSubmenu
                            title: "移动到文件夹"
                            enabled: libraryService.folders.count > 0
                            AppMenuItem {
                                text: "无（移出文件夹）"
                                onTriggered: libraryService.setScoreFolder(scoreDelegate.scoreId, "")
                            }
                            AppMenuSeparator { }
                            Instantiator {
                                model: libraryService.folders
                                delegate: AppMenuItem {
                                    required property string itemId
                                    required property string name
                                    text: name
                                    onTriggered: libraryService.setScoreFolder(scoreDelegate.scoreId, itemId)
                                }
                                onObjectAdded: function(index, object) {
                                    folderSubmenu.insertItem(index + 2, object)
                                }
                                onObjectRemoved: function(index, object) {
                                    folderSubmenu.removeItem(object)
                                }
                            }
                        }

                        // 标签子菜单（可多选切换）
                        AppMenu {
                            id: tagSubmenu
                            title: "标签"
                            enabled: libraryService.tags.count > 0
                            Instantiator {
                                model: libraryService.tags
                                delegate: AppMenuItem {
                                    required property string itemId
                                    required property string name
                                    text: name
                                    checkable: true
                                    checked: libraryService.scoreHasTag(scoreDelegate.scoreId, itemId)
                                    onTriggered: {
                                        if (checked) {
                                            libraryService.addScoreTag(scoreDelegate.scoreId, itemId)
                                        } else {
                                            libraryService.removeScoreTag(scoreDelegate.scoreId, itemId)
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
                            text: "打开"
                            onTriggered: libraryService.enterFolder(scoreDelegate.itemId)
                        }
                        AppMenuItem {
                            text: "重命名"
                            onTriggered: {
                                renameFolderDialog.targetId = scoreDelegate.itemId
                                renameFolderDialog.value = scoreDelegate.title
                                renameFolderDialog.open()
                            }
                        }
                        AppMenuSeparator { }
                        AppMenuItem {
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

            // 鼠标拖过空白或卡片后直接框选；矩形缩小时同步取消离开选区的项目。
            Rectangle {
                id: selectionBox
                objectName: "selectionBox"
                visible: rubberBand.active
                z: 10
                color: Theme.accent + "22"
                border.width: 1
                border.color: Theme.accent
                radius: 4
            }

            DragHandler {
                id: rubberBand
                target: null
                acceptedButtons: Qt.LeftButton
                acceptedDevices: PointerDevice.Mouse
                grabPermissions: PointerHandler.CanTakeOverFromItems
                    | PointerHandler.ApprovesTakeOverByAnything

                onActiveChanged: {
                    if (!active) return
                    selectionBox.x = centroid.pressPosition.x
                    selectionBox.y = centroid.pressPosition.y
                    selectionBox.width = 0
                    selectionBox.height = 0
                }
                onActiveTranslationChanged: {
                    selectionBox.x = Math.min(centroid.pressPosition.x,
                        centroid.pressPosition.x + activeTranslation.x)
                    selectionBox.y = Math.min(centroid.pressPosition.y,
                        centroid.pressPosition.y + activeTranslation.y)
                    selectionBox.width = Math.abs(activeTranslation.x)
                    selectionBox.height = Math.abs(activeTranslation.y)
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
                    Label { anchors.centerIn: parent; text: "♫"; color: Theme.mutedForeground; font.pixelSize: 32 }
                }
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: libraryService.searchQuery.length > 0 ? "没有符合条件的项目"
                        : appController.libraryFilter === "favorites" ? "还没有收藏的乐谱" : "乐谱库为空"
                    color: Theme.foreground
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: libraryService.searchQuery.length > 0 ? "换个关键词试试" : "导入 PDF 或图片，开始建立你的乐谱库"
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
                visible: dropArea.containsDrag
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
                anchors.fill: parent
                z: 11
                onDropped: function(drop) {
                    for (let index = 0; index < drop.urls.length; ++index)
                        libraryService.importLocalFile(drop.urls[index])
                }
            }

            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: blankContextMenu.popup()
            }
        }

        // 底部批量操作栏
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

    // 空白区域右键菜单
    AppMenu {
        id: blankContextMenu
        objectName: "blankContextMenu"
        AppMenuItem { text: "新建文件夹"; onTriggered: newFolderDialog.open() }
        AppMenuItem { text: "新建标签"; onTriggered: newTagDialog.open() }
        AppMenuSeparator { }
        AppMenuItem { text: "导入乐谱"; onTriggered: fileDialog.open() }
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
}
