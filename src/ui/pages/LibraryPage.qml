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

    readonly property string filterTitle: {
        const filter = appController.libraryFilter
        if (filter === "all") return "乐谱库"
        if (filter === "recent") return "最近使用"
        if (filter === "favorites") return "收藏"
        if (filter.startsWith("folder:")) return "文件夹"
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

            ColumnLayout {
                spacing: 3
                Label {
                    text: root.filterTitle
                    color: Theme.foreground
                    font.pixelSize: Theme.font2xl
                    font.weight: Font.Bold
                }
                Label {
                    text: libraryService.scores.count > 0 ? libraryService.scores.count + " 份乐谱" : "这里还没有乐谱"
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
                symbol: "+"
                primary: true
                onClicked: fileDialog.open()
            }

            AppButton {
                objectName: "stitchButton"
                Layout.preferredWidth: 120
                text: "拼接导入"
                symbol: "⬚⬚"
                onClicked: stitchDialog.open()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusLg
            color: Theme.surface
            border.width: 1
            border.color: dropArea.containsDrag ? Theme.accent : Theme.border

            GridView {
                id: grid
                anchors.fill: parent
                anchors.margins: 16
                visible: count > 0
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                cellWidth: {
                    const columns = Math.max(1, Math.floor(width / 218))
                    return Math.floor(width / columns)
                }
                cellHeight: 304
                model: libraryService.scores

                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Item {
                    id: scoreDelegate
                    objectName: "scoreDelegate"
                    required property string scoreId
                    required property string title
                    required property string composer
                    required property int pageCount
                    required property string thumbnailPath
                    required property bool favorite
                    required property string filePath
                    required property string fileType
                    readonly property bool contextMenuOpenedOnce: scoreMenu.openedOnce

                    function closeContextMenu() {
                        scoreMenu.close()
                    }

                    width: grid.cellWidth
                    height: grid.cellHeight

                    Rectangle {
                        id: card
                        width: Math.min(218, parent.width - 12)
                        height: 288
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 6
                        radius: Theme.radiusMd
                        color: cardMouse.containsMouse ? Theme.cardHover : Theme.cardBackground
                        border.width: 1
                        border.color: cardMouse.containsMouse ? Theme.strongBorder : Theme.cardBorder

                        Behavior on color { ColorAnimation { duration: 100 } }
                        Behavior on border.color { ColorAnimation { duration: 100 } }

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
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    source: scoreDelegate.thumbnailPath.length > 0 ? "file://" + scoreDelegate.thumbnailPath : ""
                                    asynchronous: true
                                    smooth: true
                                    fillMode: Image.PreserveAspectFit
                                }
                                Label {
                                    anchors.centerIn: parent
                                    visible: scoreDelegate.thumbnailPath.length === 0
                                    text: "♫"
                                    color: Theme.faintForeground
                                    font.pixelSize: 38
                                }
                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 6
                                    implicitWidth: pageBadge.implicitWidth + 12
                                    implicitHeight: 20
                                    radius: 5
                                    color: "#000000b8"
                                    Label {
                                        id: pageBadge
                                        anchors.centerIn: parent
                                        text: scoreDelegate.pageCount + " 页"
                                        color: "white"
                                        font.pixelSize: 10
                                        font.weight: Font.Medium
                                    }
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
                                text: scoreDelegate.composer.length > 0 ? scoreDelegate.composer : "未知作曲者"
                                color: Theme.mutedForeground
                                font.pixelSize: Theme.fontSm
                                elide: Text.ElideRight
                            }
                        }
                    }

                    MouseArea {
                        id: cardMouse
                        objectName: "scoreCardMouse"
                        anchors.fill: card
                        z: 1
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: function(mouse) {
                            if (mouse.button === Qt.RightButton) {
                                scoreMenu.popup()
                            } else {
                                appController.openScore(scoreDelegate.title, scoreDelegate.filePath,
                                    scoreDelegate.fileType, scoreDelegate.pageCount)
                            }
                        }
                    }

                    IconButton {
                        objectName: "favoriteButton"
                        z: 2
                        anchors.right: card.right
                        anchors.top: card.top
                        anchors.margins: 18
                        symbol: scoreDelegate.favorite ? "★" : "☆"
                        selected: scoreDelegate.favorite
                        Accessible.name: scoreDelegate.favorite ? "取消收藏" : "添加到收藏"
                        onClicked: libraryService.toggleFavorite(scoreDelegate.scoreId, !scoreDelegate.favorite)
                    }

                    AppMenu {
                        id: scoreMenu
                        objectName: "scoreContextMenu"
                        AppMenuItem {
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
                            title: "移动到文件夹"
                            AppMenuItem {
                                text: "无（移出文件夹）"
                                onTriggered: libraryService.setScoreFolder(scoreDelegate.scoreId, "")
                            }
                            AppMenuSeparator { }
                            Repeater {
                                model: libraryService.folders
                                delegate: AppMenuItem {
                                    text: modelData.name
                                    onTriggered: libraryService.setScoreFolder(scoreDelegate.scoreId, modelData.id)
                                }
                            }
                        }

                        // 标签子菜单（可多选切换）
                        AppMenu {
                            title: "标签"
                            Repeater {
                                model: libraryService.tags
                                delegate: AppMenuItem {
                                    text: modelData.name
                                    checkable: true
                                    checked: false
                                    onTriggered: {
                                        if (checked) {
                                            libraryService.addScoreTag(scoreDelegate.scoreId, modelData.id)
                                        } else {
                                            libraryService.removeScoreTag(scoreDelegate.scoreId, modelData.id)
                                        }
                                    }
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
                    text: libraryService.searchQuery.length > 0 ? "没有符合条件的乐谱"
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
                    symbol: "+"
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

            // 空白区域右键菜单（z:-1 让卡片右键菜单优先）
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                z: -1
                onClicked: function(mouse) {
                    if (mouse.button === Qt.RightButton) {
                        blankContextMenu.popup()
                    }
                }
            }
        }
    }

    // 空白区域右键菜单
    Menu {
        id: blankContextMenu
        MenuItem { text: "新建文件夹"; onTriggered: newFolderDialog.open() }
        MenuItem { text: "新建标签"; onTriggered: newTagDialog.open() }
        MenuSeparator { }
        MenuItem { text: "导入乐谱"; onTriggered: fileDialog.open() }
    }

    // 新建文件夹对话框
    Dialog {
        id: newFolderDialog
        title: "新建文件夹"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: libraryService.createFolder(folderNameInput.text)
        TextField {
            id: folderNameInput
            width: 280
            placeholderText: "输入文件夹名称"
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
        onAccepted: libraryService.createTag(tagNameInput.text)
        TextField {
            id: tagNameInput
            width: 280
            placeholderText: "输入标签名称"
            focus: true
        }
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
}
