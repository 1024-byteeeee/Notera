import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Notera

Rectangle {
    color: Theme.background

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingXl
        spacing: Theme.spacingLg

        // ── 顶部工具栏 ───────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd

            ColumnLayout {
                spacing: 2
                Label {
                    text: "乐谱库"
                    color: Theme.foreground
                    font.pixelSize: Theme.font2xl
                    font.weight: Font.Bold
                }
                Label {
                    text: libraryService.scores.count > 0
                        ? libraryService.scores.count + " 份乐谱"
                        : "导入你的第一份乐谱"
                    color: Theme.mutedForeground
                    font.pixelSize: Theme.fontSm
                }
            }

            Item { Layout.fillWidth: true }

            // 搜索框
            Rectangle {
                width: 240
                height: Theme.controlHeight
                radius: Theme.radiusMd
                color: Theme.inputBackground
                border.color: searchField.activeFocus ? Theme.inputFocusBorder : Theme.inputBorder
                border.width: 1
                Behavior on border.color { ColorAnimation { duration: 150 } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 10
                    spacing: 8

                    Label {
                        text: "⌕"
                        color: Theme.mutedForeground
                        font.pixelSize: 14
                    }

                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        background: Rectangle { color: "transparent" }
                        placeholderText: "搜索乐谱…"
                        placeholderTextColor: Theme.inputPlaceholder
                        color: Theme.foreground
                        font.pixelSize: Theme.fontMd
                        selectByMouse: true
                        text: libraryService.searchQuery
                        onTextChanged: libraryService.searchQuery = text
                    }
                }
            }

            // 导入按钮
            Rectangle {
                height: Theme.controlHeight
                radius: Theme.radiusMd
                color: importBtnMouse.containsMouse ? Theme.accentHover : Theme.accent
                border.width: 0
                Behavior on color { ColorAnimation { duration: 120 } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 7

                    Label {
                        text: "+"
                        color: Theme.accentForeground
                        font.pixelSize: 16
                        font.weight: Font.Bold
                    }
                    Label {
                        text: "导入"
                        color: Theme.accentForeground
                        font.pixelSize: Theme.fontMd
                        font.weight: Font.DemiBold
                    }
                }

                MouseArea {
                    id: importBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: fileDialog.open()
                }
            }
        }

        // ── 乐谱网格 ─────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.surface
            radius: Theme.radiusLg
            border.color: Theme.border
            border.width: 1

            GridView {
                id: grid
                anchors.fill: parent
                anchors.margins: 20
                visible: count > 0
                clip: true
                cellWidth: 210
                cellHeight: 296
                model: libraryService.scores

                delegate: Item {
                    required property string scoreId
                    required property string title
                    required property string composer
                    required property int pageCount
                    required property string thumbnailPath
                    required property bool favorite
                    required property string filePath
                    required property string fileType

                    width: grid.cellWidth
                    height: grid.cellHeight

                    // 卡片
                    Rectangle {
                        id: card
                        anchors.fill: parent
                        anchors.margins: 8
                        color: cardMouse.containsMouse ? Theme.cardHover : Theme.cardBackground
                        border.color: cardMouse.containsMouse ? Theme.strongBorder : Theme.cardBorder
                        border.width: 1
                        radius: Theme.radiusMd
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }

                        Column {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 10

                            // 缩略图
                            Rectangle {
                                width: parent.width
                                height: 184
                                color: Theme.sunkenSurface
                                radius: Theme.radiusSm
                                clip: true
                                border.color: Theme.border
                                border.width: 1

                                Image {
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    source: thumbnailPath.length > 0 ? "file://" + thumbnailPath : ""
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    smooth: true
                                }
                                Label {
                                    anchors.centerIn: parent
                                    visible: thumbnailPath.length === 0
                                    text: "♫"
                                    color: Theme.faintForeground
                                    font.pixelSize: 36
                                }

                                // 页数角标
                                Rectangle {
                                    visible: thumbnailPath.length > 0
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 6
                                    width: pageLabel.width + 12
                                    height: 20
                                    radius: 5
                                    color: "#000000aa"
                                    Label {
                                        id: pageLabel
                                        anchors.centerIn: parent
                                        text: pageCount + " 页"
                                        color: "#ffffff"
                                        font.pixelSize: 10
                                        font.weight: Font.Medium
                                    }
                                }
                            }

                            // 标题
                            Label {
                                width: parent.width
                                text: title
                                color: Theme.foreground
                                font.pixelSize: Theme.fontMd
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            // 作曲者
                            Label {
                                width: parent.width
                                text: composer.length > 0 ? composer : "未知作曲者"
                                color: Theme.mutedForeground
                                font.pixelSize: Theme.fontSm
                                elide: Text.ElideRight
                            }
                        }

                        // 收藏按钮
                        Rectangle {
                            id: favBtn
                            width: 28
                            height: 28
                            radius: 8
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 10
                            color: favorite ? Theme.accentSoft : (favMouse.containsMouse ? Theme.buttonHover : "transparent")
                            Behavior on color { ColorAnimation { duration: 120 } }

                            Label {
                                anchors.centerIn: parent
                                text: favorite ? "★" : "☆"
                                color: favorite ? Theme.accent : Theme.mutedForeground
                                font.pixelSize: 14
                            }

                            MouseArea {
                                id: favMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: libraryService.toggleFavorite(scoreId, !favorite)
                            }
                        }
                    }

                    MouseArea {
                        id: cardMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onDoubleClicked: appController.openScore(title, filePath, fileType, pageCount)
                        onPressAndHold: contextMenu.popup()
                    }

                    Menu {
                        id: contextMenu
                        MenuItem { text: favorite ? "取消收藏" : "添加到收藏"; onTriggered: libraryService.toggleFavorite(scoreId, !favorite) }
                        MenuItem { text: "重命名"; onTriggered: renameDialog.openFor(scoreId, title) }
                        MenuSeparator { }
                        MenuItem { text: "删除"; onTriggered: deleteDialog.openFor(scoreId, filePath, thumbnailPath) }
                    }
                }
            }

            // 空状态
            Column {
                anchors.centerIn: parent
                visible: grid.count === 0
                spacing: 14

                Rectangle {
                    width: 72
                    height: 72
                    radius: 20
                    color: Theme.elevatedSurface
                    border.color: Theme.border
                    border.width: 1
                    anchors.horizontalCenter: parent.horizontalCenter
                    Label {
                        anchors.centerIn: parent
                        text: "♪"
                        color: Theme.mutedForeground
                        font.pixelSize: 32
                    }
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: libraryService.searchQuery.length > 0 ? "没有符合搜索条件的乐谱" : "乐谱库为空"
                    color: Theme.foreground
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.DemiBold
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: libraryService.searchQuery.length > 0 ? "换个关键词试试" : "点击右上角导入，或拖拽 PDF / 图片到此处"
                    color: Theme.mutedForeground
                    font.pixelSize: Theme.fontSm
                }
            }

            // 拖拽区域
            DropArea {
                anchors.fill: parent
                onDropped: function(drop) {
                    for (let index = 0; index < drop.urls.length; ++index)
                        libraryService.importLocalFile(drop.urls[index])
                }
            }
        }
    }

    // ── 对话框 ─────────────────────────────────────────
    FileDialog {
        id: fileDialog
        title: "导入乐谱"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["乐谱文件 (*.pdf *.jpg *.jpeg *.png *.bmp *.gif *.webp *.tif *.tiff)", "所有文件 (*)"]
        onAccepted: {
            for (let index = 0; index < selectedFiles.length; ++index)
                libraryService.importLocalFile(selectedFiles[index])
        }
    }

    Dialog {
        id: renameDialog
        property string scoreId: ""
        title: "重命名乐谱"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        function openFor(id, name) { scoreId = id; nameInput.text = name; open() }
        onAccepted: libraryService.renameScore(scoreId, nameInput.text)
        TextField { id: nameInput; width: 320; placeholderText: "乐谱名称" }
    }

    Dialog {
        id: deleteDialog
        property string scoreId: ""
        property string filePath: ""
        property string thumbnailPath: ""
        title: "删除乐谱？"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        function openFor(id, file, thumbnail) { scoreId = id; filePath = file; thumbnailPath = thumbnail; open() }
        onAccepted: libraryService.deleteScore(scoreId, filePath, thumbnailPath)
        Label { text: "导入的文件副本及其元数据将被永久删除。"; wrapMode: Text.WordWrap; width: 300 }
    }
}
