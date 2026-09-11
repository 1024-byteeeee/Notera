import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.folderlistmodel
import QtCore
import Notera

Dialog {
    id: dialog

    signal importRequested(var folderUrls)

    property var selectedUrls: ({})
    property int selectedCount: 0

    parent: Overlay.overlay
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    width: parent ? Math.min(480, parent.width - 48) : 480
    height: parent ? Math.min(560, parent.height - 96) : 560
    padding: 22
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    transformOrigin: Item.Center

    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: Motion.normal
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "scale"
                from: 0.97
                to: 1
                duration: Motion.normal
                easing.type: Easing.OutCubic
            }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: Motion.fast
                easing.type: Easing.InCubic
            }
            NumberAnimation {
                property: "scale"
                from: 1
                to: 0.985
                duration: Motion.fast
                easing.type: Easing.InCubic
            }
        }
    }

    onOpened: folderModel.folder = "file://" + StandardPaths.standardLocations(StandardPaths.HomeLocation)[0]

    FolderListModel {
        id: folderModel
        showDirs: true
        showFiles: false
        showDotAndDotDot: false
        sortField: FolderListModel.Name
        onFolderChanged: {
            dialog.selectedUrls = ({});
            dialog.selectedCount = 0;
        }
    }

    function toggleFolder(url) {
        if (dialog.selectedUrls[url]) {
            delete dialog.selectedUrls[url];
            dialog.selectedCount--;
        } else {
            dialog.selectedUrls[url] = true;
            dialog.selectedCount++;
        }
    }

    function enterFolder(url) {
        folderModel.folder = url;
    }

    function goUp() {
        const parent = folderModel.parentFolder;
        if (parent.toString().length > 0 && parent !== folderModel.folder)
            folderModel.folder = parent;
    }

    function confirmImport() {
        const urls = [];
        for (const key in dialog.selectedUrls)
            urls.push(key);
        if (urls.length > 0) {
            dialog.importRequested(urls);
            dialog.accept();
        }
    }

    header: Label {
        leftPadding: 22
        rightPadding: 22
        topPadding: 20
        bottomPadding: 8
        text: dialog.title
        color: Theme.foreground
        font.pixelSize: Theme.fontLg
        font.weight: Font.DemiBold
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            spacing: 8

            AppButton {
                text: "上一级"
                enabled: folderModel.parentFolder.toString().length > 0 && folderModel.parentFolder !== folderModel.folder
                onClicked: dialog.goUp()
            }

            Label {
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                horizontalAlignment: Text.AlignRight
                text: folderModel.folder.toString().replace(/^file:\/\//, "")
                color: Theme.mutedForeground
                font.pixelSize: Theme.fontSm
            }
        }

        Item {
            Layout.fillHeight: true
            Layout.topMargin: 14
            Layout.bottomMargin: 14

            ListView {
                id: folderList
                anchors.fill: parent
                clip: true
                model: folderModel
                spacing: 2

                delegate: Rectangle {
                    id: row
                    required property int index
                    readonly property string folderUrl: folderModel.get(index) ? folderModel.get(index).filePath : ""
                    readonly property string folderName: folderModel.get(index) ? folderModel.get(index).fileName : ""
                    readonly property bool isSelected: dialog.selectedUrls[folderUrl] === true

                    width: folderList.width
                    height: 42
                    radius: Theme.radiusMd
                    color: rowArea.containsMouse ? Theme.cardHover : (isSelected ? Theme.accentSoft : "transparent")
                    border.width: isSelected ? 1 : 0
                    border.color: Theme.accent

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10

                        FolderIcon {
                            width: 20
                            height: 20
                            iconColor: isSelected ? Theme.accent : "#c99425"
                        }

                        Label {
                            Layout.fillWidth: true
                            text: row.folderName
                            elide: Text.ElideMiddle
                            color: Theme.foreground
                            font.pixelSize: Theme.fontMd
                        }

                        Rectangle {
                            width: 18
                            height: 18
                            radius: 5
                            color: isSelected ? Theme.accent : "transparent"
                            border.width: 1
                            border.color: isSelected ? Theme.accent : Theme.border

                            Label {
                                anchors.centerIn: parent
                                visible: isSelected
                                text: "✓"
                                color: Theme.selectedText
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }
                    }

                    MouseArea {
                        id: rowArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: dialog.toggleFolder(row.folderUrl)
                        onDoubleClicked: dialog.enterFolder(row.folderUrl)
                    }
                }
            }
        }

        Label {
            visible: folderModel.count === 0
            Layout.fillWidth: true
            Layout.bottomMargin: 14
            horizontalAlignment: Text.AlignHCenter
            text: "此文件夹下没有子文件夹"
            color: Theme.faintForeground
            font.pixelSize: Theme.fontSm
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: dialog.selectedCount > 0 ? ("已选 " + dialog.selectedCount + " 个文件夹") : "单击勾选，双击进入子文件夹"
                color: Theme.mutedForeground
                font.pixelSize: Theme.fontSm
                elide: Text.ElideRight
            }

            AppButton {
                text: "取消"
                onClicked: dialog.reject()
            }
            AppButton {
                text: "导入所选"
                primary: true
                enabled: dialog.selectedCount > 0
                onClicked: dialog.confirmImport()
            }
        }
    }

    background: Rectangle {
        radius: Theme.radiusLg
        color: Theme.surface
        border.width: 1
        border.color: Theme.strongBorder
    }
}
