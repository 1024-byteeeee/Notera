import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Notera

Rectangle {
    color: Theme.background

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 36
        spacing: 24

        RowLayout {
            Layout.fillWidth: true
            Label { text: "乐谱库"; color: Theme.foreground; font.pixelSize: 28; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
            TextField {
                placeholderText: "搜索乐谱"
                implicitWidth: 220
                text: libraryService.searchQuery
                onTextChanged: libraryService.searchQuery = text
            }
            Button { text: "导入"; onClicked: fileDialog.open() }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.surface
            radius: 8
            border.color: Theme.border

            GridView {
                id: grid
                anchors.fill: parent
                anchors.margins: 18
                visible: count > 0
                clip: true
                cellWidth: 208
                cellHeight: 286
                model: libraryService.scores

                delegate: Item {
                    required property string scoreId
                    required property string title
                    required property string composer
                    required property int pageCount
                    required property string thumbnailPath
                    required property bool favorite
                    required property string filePath

                    width: grid.cellWidth
                    height: grid.cellHeight

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 8
                        color: Theme.elevatedSurface
                        border.color: Theme.border
                        radius: 6

                        Column {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            Rectangle {
                                width: parent.width
                                height: 182
                                color: "#fafafa"
                                radius: 3
                                clip: true
                                Image {
                                    anchors.fill: parent
                                    anchors.margins: 3
                                    source: thumbnailPath.length > 0 ? "file://" + thumbnailPath : ""
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                }
                                Label {
                                    anchors.centerIn: parent
                                    visible: thumbnailPath.length === 0
                                    text: "乐谱"
                                    color: "#737373"
                                }
                            }

                            Label { width: parent.width; text: title; color: Theme.foreground; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Label { width: parent.width; text: composer.length > 0 ? composer : "未知作曲者"; color: Theme.mutedForeground; elide: Text.ElideRight }
                            Label { text: pageCount + " 页"; color: Theme.mutedForeground; font.pixelSize: 12 }
                        }

                        Button {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 12
                            text: favorite ? "★" : "☆"
                            onClicked: libraryService.toggleFavorite(scoreId, !favorite)
                        }

                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onDoubleTapped: appController.currentPage = "reader"
                        }
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: contextMenu.popup()
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
            }

            Column {
                anchors.centerIn: parent
                visible: grid.count === 0
                spacing: 10
                Label { anchors.horizontalCenter: parent.horizontalCenter; text: libraryService.searchQuery.length > 0 ? "没有符合搜索条件的乐谱" : "乐谱库为空"; color: Theme.foreground; font.pixelSize: 18 }
                Label { anchors.horizontalCenter: parent.horizontalCenter; text: "请导入 PDF、JPG 或 PNG 乐谱。"; color: Theme.mutedForeground }
            }

            DropArea {
                anchors.fill: parent
                onDropped: drop => libraryService.importUrls(drop.urls)
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: "导入乐谱"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["乐谱文件 (*.pdf *.jpg *.jpeg *.png)"]
        onAccepted: libraryService.importUrls(selectedFiles)
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
        TextField { id: nameInput; width: 300; placeholderText: "乐谱名称" }
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
        Label { text: "导入的文件副本及其元数据将被删除。" }
    }
}
