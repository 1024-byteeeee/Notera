import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtCore
import Notera

Rectangle {
    id: root
    objectName: "settingsPage"
    color: Theme.background

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: settingsContent.implicitHeight + 72
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: settingsContent
            objectName: "settingsContent"
            x: Theme.spacingXl
            y: 0
            width: Math.max(0, flick.width - Theme.spacingXl * 2)
            spacing: Theme.spacingLg

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 8
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    objectName: "settingsTitle"
                    text: "设置"
                    color: Theme.foreground
                    font.pixelSize: Theme.font2xl
                    font.weight: Font.Bold
                }
                Label {
                    text: "个性化你的阅读体验"
                    color: Theme.mutedForeground
                    font.pixelSize: Theme.fontSm
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: generalLayout.implicitHeight + 40
                radius: Theme.radiusLg
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                ColumnLayout {
                    id: generalLayout
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12

                    Label {
                        text: "通用"
                        color: Theme.foreground
                        font.pixelSize: Theme.fontLg
                        font.weight: Font.DemiBold
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 72
                        radius: Theme.radiusMd
                        color: Theme.elevatedSurface
                        border.width: 1
                        border.color: Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 16

                            ColumnLayout {
                                spacing: 3
                                Label {
                                    text: "外观主题"
                                    color: Theme.foreground
                                    font.pixelSize: Theme.fontMd
                                    font.weight: Font.Medium
                                }
                                Label {
                                    text: "界面、菜单和弹窗会同时切换"
                                    color: Theme.mutedForeground
                                    font.pixelSize: Theme.fontXs
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Rectangle {
                                id: themeSelector
                                objectName: "themeSelector"
                                Layout.alignment: Qt.AlignRight
                                Layout.preferredWidth: 282
                                Layout.preferredHeight: 38
                                radius: Theme.radiusMd
                                color: Theme.sunkenSurface
                                border.width: 1
                                border.color: Theme.border

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 3
                                    spacing: 3

                                    Repeater {
                                        model: ["浅色", "深色"]
                                        delegate: Button {
                                            required property int index
                                            required property string modelData
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            padding: 0
                                            hoverEnabled: true
                                            contentItem: Label {
                                                text: modelData
                                                color: appController.themeMode === index ? Theme.selectedText : Theme.secondaryForeground
                                                font.pixelSize: Theme.fontSm
                                                font.weight: appController.themeMode === index ? Font.DemiBold : Font.Medium
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                                scale: parent.down ? 0.97 : 1
                                                Behavior on scale { NumberAnimation { duration: Motion.fast; easing.type: Easing.OutCubic } }
                                            }
                                            background: Rectangle {
                                                radius: Theme.radiusSm
                                                color: appController.themeMode === index ? Theme.selectedBackground
                                                    : parent.hovered ? Theme.buttonHover : "transparent"
                                                border.width: appController.themeMode === index ? 1 : 0
                                                border.color: Theme.selectedBorder
                                            }
                                            onClicked: appController.themeMode = index
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 72
                        radius: Theme.radiusMd
                        color: Theme.elevatedSurface
                        border.width: 1
                        border.color: Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 16

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Label {
                                    text: "全局主题色"
                                    color: Theme.foreground
                                    font.pixelSize: Theme.fontMd
                                    font.weight: Font.Medium
                                }
                                Label {
                                    text: appController.accentColor.length > 0
                                        ? "当前颜色 " + appController.accentColor.toUpperCase()
                                        : "使用 Notera 默认金色"
                                    color: Theme.mutedForeground
                                    font.pixelSize: Theme.fontXs
                                }
                            }

                            Rectangle {
                                objectName: "accentColorPreview"
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                radius: 14
                                color: Theme.accent
                                border.width: 2
                                border.color: Theme.strongBorder
                            }

                            AppButton {
                                objectName: "resetAccentColorButton"
                                text: "恢复默认"
                                enabled: appController.accentColor.length > 0
                                onClicked: appController.resetAccentColor()
                            }

                            AppButton {
                                objectName: "changeAccentColorButton"
                                text: "更改"
                                primary: true
                                onClicked: {
                                    accentColorDialog.selectedColor = Theme.accent
                                    accentColorDialog.open()
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 72
                        radius: Theme.radiusMd
                        color: Theme.elevatedSurface
                        border.width: 1
                        border.color: Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 16

                            ColumnLayout {
                                spacing: 3
                                Label {
                                    text: "界面动画"
                                    color: Theme.foreground
                                    font.pixelSize: Theme.fontMd
                                    font.weight: Font.Medium
                                }
                                Label {
                                    text: "控制页面切换、按钮、菜单和弹窗的过渡效果"
                                    color: Theme.mutedForeground
                                    font.pixelSize: Theme.fontXs
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Switch {
                                id: animationsSwitch
                                objectName: "animationsSwitch"
                                readonly property int independentAnimationDuration: 160
                                Layout.alignment: Qt.AlignRight
                                checked: appController.animationsEnabled
                                padding: 0
                                implicitWidth: 46
                                implicitHeight: 26
                                onToggled: appController.animationsEnabled = checked

                                    indicator: Rectangle {
                                        implicitWidth: 46
                                        implicitHeight: 26
                                        radius: 13
                                        color: animationsSwitch.checked ? Theme.accent : Theme.buttonBackground
                                        border.width: 1
                                        border.color: animationsSwitch.checked ? Theme.accent : Theme.strongBorder
                                        Behavior on color { ColorAnimation { duration: animationsSwitch.independentAnimationDuration } }

                                        Rectangle {
                                            x: animationsSwitch.checked ? parent.width - width - 4 : 4
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 18
                                            height: 18
                                            radius: 9
                                            color: animationsSwitch.checked ? Theme.accentForeground : Theme.mutedForeground
                                            Behavior on x { NumberAnimation { duration: animationsSwitch.independentAnimationDuration; easing.type: Easing.OutCubic } }
                                        }
                                    }
                                    contentItem: Item { }
                                }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: readerLayout.implicitHeight + 40
                radius: Theme.radiusLg
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                ColumnLayout {
                    id: readerLayout
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12

                    Label {
                        text: "阅读器"
                        color: Theme.foreground
                        font.pixelSize: Theme.fontLg
                        font.weight: Font.DemiBold
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 72
                        radius: Theme.radiusMd
                        color: Theme.elevatedSurface
                        border.width: 1
                        border.color: Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 18

                            ColumnLayout {
                                spacing: 3
                                Label {
                                    text: "默认滚动速度"
                                    color: Theme.foreground
                                    font.pixelSize: Theme.fontMd
                                    font.weight: Font.Medium
                                }
                                Label {
                                    text: "开启自动滚动时使用的初始速度"
                                    color: Theme.mutedForeground
                                    font.pixelSize: Theme.fontXs
                                }
                            }

                            Slider {
                                id: speedSlider
                                Layout.preferredWidth: 180
                                from: 1
                                to: 256
                                stepSize: 1
                                value: appController.defaultScrollSpeed
                                onMoved: appController.defaultScrollSpeed = value

                                background: Rectangle {
                                    x: speedSlider.leftPadding
                                    y: speedSlider.topPadding + speedSlider.availableHeight / 2 - height / 2
                                    width: speedSlider.availableWidth
                                    height: 4
                                    radius: 2
                                    color: Theme.buttonBackground
                                    Rectangle {
                                        width: speedSlider.visualPosition * parent.width
                                        height: parent.height
                                        radius: 2
                                        color: Theme.accent
                                    }
                                }
                                handle: Rectangle {
                                    x: speedSlider.leftPadding + speedSlider.visualPosition * (speedSlider.availableWidth - width)
                                    y: speedSlider.topPadding + speedSlider.availableHeight / 2 - height / 2
                                    implicitWidth: 18
                                    implicitHeight: 18
                                    radius: 9
                                    color: Theme.surface
                                    border.width: 2
                                    border.color: Theme.accent
                                }
                            }

                            Label {
                                Layout.preferredWidth: 60
                                text: Math.round(appController.defaultScrollSpeed) + " px/s"
                                color: Theme.accent
                                font.pixelSize: Theme.fontSm
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 72
                        radius: Theme.radiusMd
                        color: Theme.elevatedSurface
                        border.width: 1
                        border.color: Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Label {
                                    text: "数据存储位置"
                                    color: Theme.foreground
                                    font.pixelSize: Theme.fontMd
                                    font.weight: Font.Medium
                                }
                                Label {
                                    text: appController.pendingDataDirectory.length > 0
                                        ? "重启后迁移至：" + appController.pendingDataDirectory
                                        : appController.dataDirectory
                                    color: Theme.mutedForeground
                                    font.pixelSize: Theme.fontXs
                                    elide: Text.ElideMiddle
                                    maximumLineCount: 1
                                    Layout.fillWidth: true
                                }
                            }

                            Item { Layout.fillWidth: true }

                            RowLayout {
                                Layout.alignment: Qt.AlignRight
                                spacing: 8
                                AppButton {
                                    objectName: "openDataDirectoryButton"
                                    text: "打开"
                                    onClicked: {
                                        const error = appController.openDataDirectory()
                                        if (error.length > 0) {
                                            migrateResultDialog.message = error
                                            migrateResultDialog.open()
                                        }
                                    }
                                }
                                AppButton {
                                    objectName: "changeDataDirectoryButton"
                                    text: "更改"
                                    onClicked: dataDirDialog.open()
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: backupLayout.implicitHeight + 40
                        radius: Theme.radiusLg
                        color: Theme.surface
                        border.width: 1
                        border.color: Theme.border

                        ColumnLayout {
                            id: backupLayout
                            anchors.fill: parent
                            anchors.margins: 20
                            spacing: 12
                            Label {
                                text: "数据备份"
                                color: Theme.foreground
                                font.pixelSize: Theme.fontLg
                                font.weight: Font.DemiBold
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 72
                                radius: Theme.radiusMd
                                color: Theme.elevatedSurface
                                border.width: 1
                                border.color: Theme.border

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 18
                                    anchors.rightMargin: 18
                                    spacing: 16
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 3
                                        Label { text: "导出数据库备份"; color: Theme.foreground; font.pixelSize: Theme.fontMd; font.weight: Font.Medium }
                                        Label { text: "导出完整备份包（数据库、乐谱、缩略图），可跨设备恢复"; color: Theme.secondaryForeground; font.pixelSize: Theme.fontXs }
                                    }
                                    Item { Layout.fillWidth: true }
                                    AppButton {
                                        objectName: "exportBackupButton"
                                        text: "导出"
                                        onClicked: exportBackupDialog.open()
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 72
                                radius: Theme.radiusMd
                                color: Theme.elevatedSurface
                                border.width: 1
                                border.color: Theme.border

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 18
                                    anchors.rightMargin: 18
                                    spacing: 16
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 3
                                        Label { text: "导入数据库备份"; color: Theme.foreground; font.pixelSize: Theme.fontMd; font.weight: Font.Medium }
                                        Label { text: "从备份包导入，可合并到当前库或替换当前所有数据"; color: Theme.secondaryForeground; font.pixelSize: Theme.fontXs }
                                    }
                                    Item { Layout.fillWidth: true }
                                    AppButton {
                                        objectName: "importBackupButton"
                                        text: "导入"
                                        onClicked: importBackupDialog.open()
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 64
                        radius: Theme.radiusMd
                        color: Theme.elevatedSurface
                        border.width: 1
                        border.color: Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 12
                            ColumnLayout {
                                spacing: 3
                                Label { text: "关于 Notera"; color: Theme.foreground; font.pixelSize: Theme.fontMd; font.weight: Font.Medium }
                                Label { text: "简单的本地乐谱阅读器"; color: Theme.mutedForeground; font.pixelSize: Theme.fontXs }
                            }
                            Item { Layout.fillWidth: true }
                            Label {
                                objectName: "versionLabel"
                                Layout.alignment: Qt.AlignRight
                                text: "v0.1.2"
                                color: Theme.mutedForeground
                                font.pixelSize: Theme.fontSm
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: dangerLayout.implicitHeight + 40
                radius: Theme.radiusLg
                color: Theme.surface
                border.width: 1
                border.color: Theme.danger

                ColumnLayout {
                    id: dangerLayout
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12
                    Label {
                        text: "危险操作"
                        color: Theme.danger
                        font.pixelSize: Theme.fontLg
                        font.weight: Font.DemiBold
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 72
                        radius: Theme.radiusMd
                        color: Theme.dangerSoft
                        border.width: 1
                        border.color: Theme.danger
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 18
                            anchors.rightMargin: 18
                            spacing: 16
                            ColumnLayout {
                                spacing: 3
                                Label { text: "清空所有数据"; color: Theme.danger; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                                Label { text: "永久删除全部乐谱、文件夹、标签、收藏、缓存和设置"; color: Theme.secondaryForeground; font.pixelSize: Theme.fontXs }
                            }
                            Item { Layout.fillWidth: true }
                            AppButton {
                                objectName: "clearAllDataButton"
                                text: "清空所有数据"
                                danger: true
                                onClicked: clearWarningDialog.open()
                            }
                        }
                    }
                }
            }
        }
    }

    FolderDialog {
        id: dataDirDialog
        objectName: "dataDirectoryDialog"
        title: "选择数据存储位置"
        currentFolder: StandardPaths.standardLocations(StandardPaths.HomeLocation)[0]
        onAccepted: {
            migrateConfirmDialog.message = "将把所有数据迁移到：\n" + selectedFolder + "\n\n迁移完成后需要重启应用才能生效。是否继续？"
            migrateConfirmDialog.newDirectory = selectedFolder
            migrateConfirmDialog.open()
        }
    }

    FileDialog {
        id: exportBackupDialog
        objectName: "exportBackupDialog"
        title: "导出数据库备份"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "zip"
        nameFilters: ["Notera 备份 (*.notera-backup *.zip)", "所有文件 (*)"]
        currentFolder: StandardPaths.standardLocations(StandardPaths.HomeLocation)[0]
        onAccepted: {
            const error = appController.exportDatabaseBackup(selectedFile)
            backupResultDialog.title = error.length > 0 ? "导出失败" : "导出成功"
            backupResultDialog.message = error.length > 0
                ? error
                : "备份已导出到所选位置。"
            backupResultDialog.open()
        }
    }

    FileDialog {
        id: importBackupDialog
        objectName: "importBackupDialog"
        title: "选择备份文件（.notera-backup / .zip）"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Notera 备份 (*.notera-backup *.zip)", "所有文件 (*)"]
        currentFolder: StandardPaths.standardLocations(StandardPaths.HomeLocation)[0]
        onAccepted: {
            importModeDialog.backupFile = selectedFile
            importModeDialog.backupInfo = libraryService.probeDatabaseBackup(selectedFile)
            importModeDialog.open()
        }
    }

    Dialog {
        id: importModeDialog
        objectName: "importModeDialog"
        property url backupFile: ""
        property var backupInfo: ({})
        parent: Overlay.overlay
        x: parent ? Math.round((parent.width - width) / 2) : 0
        y: parent ? Math.round((parent.height - height) / 2) : 0
        width: parent ? Math.min(460, parent.width - 48) : 460
        modal: true
        focus: true
        padding: 22
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        header: Label {
            leftPadding: 22; rightPadding: 22; topPadding: 20; bottomPadding: 4
            text: "导入数据库备份"
            color: Theme.foreground
            font.pixelSize: Theme.fontLg
            font.weight: Font.DemiBold
        }

        contentItem: ColumnLayout {
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: {
                    const info = importModeDialog.backupInfo
                    if (info && info.valid) {
                        let summary = "备份包含 "
                            + (info.scoreCount !== undefined ? info.scoreCount : "?") + " 份乐谱、"
                            + (info.folderCount !== undefined ? info.folderCount : "?") + " 个文件夹、"
                            + (info.tagCount !== undefined ? info.tagCount : "?") + " 个标签"
                        if (info.createdAt) summary += "\n创建于 " + info.createdAt
                        return summary
                    }
                    return info && info.error ? "备份信息不可用：" + info.error : "备份信息不可用。"
                }
                color: Theme.secondaryForeground
                font.pixelSize: Theme.fontMd
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                text: "合并：把备份中的乐谱、文件夹、标签并入当前库，保留现有数据；重复乐谱按内容逐项处理。\n\n替换：删除当前所有数据，用备份整体恢复并自动重启。"
                color: Theme.mutedForeground
                font.pixelSize: Theme.fontSm
                wrapMode: Text.WordWrap
            }
        }

        footer: Item {
            implicitHeight: 62
            RowLayout {
                anchors.right: parent.right
                anchors.rightMargin: 22
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10
                AppButton {
                    text: "取消"
                    onClicked: importModeDialog.reject()
                }
                AppButton {
                    text: "替换所有数据"
                    danger: true
                    onClicked: {
                        importConfirmDialog.backupFile = importModeDialog.backupFile
                        importConfirmDialog.message = "导入将替换当前所有乐谱、文件夹、标签和设置，此操作不可撤销。\n\n是否继续？"
                        importModeDialog.close()
                        importConfirmDialog.open()
                    }
                }
                AppButton {
                    text: "合并到当前库"
                    primary: true
                    onClicked: {
                        const error = libraryService.importDatabaseBackupMerged(importModeDialog.backupFile)
                        importModeDialog.close()
                        if (error && error.length > 0) {
                            backupResultDialog.title = "合并失败"
                            backupResultDialog.message = error
                            backupResultDialog.open()
                        }
                    }
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

    ColorDialog {
        id: accentColorDialog
        objectName: "accentColorDialog"
        title: "选择全局主题色"
        onAccepted: appController.accentColor = selectedColor.toString()
    }

    ConfirmDialog {
        id: importConfirmDialog
        objectName: "importConfirmDialog"
        property url backupFile: ""
        title: "导入数据库备份？"
        confirmText: "开始导入"
        onAccepted: {
            const error = appController.importDatabaseBackup(backupFile)
            if (error.length > 0) {
                backupResultDialog.title = "导入失败"
                backupResultDialog.message = error
                backupResultDialog.open()
            }
        }
    }

    ConfirmDialog {
        id: backupResultDialog
        objectName: "backupResultDialog"
        title: "备份结果"
        confirmText: "确定"
    }

    ConfirmDialog {
        id: migrateConfirmDialog
        objectName: "migrationConfirmDialog"
        property url newDirectory: ""
        title: "迁移数据？"
        confirmText: "开始迁移"
        onAccepted: {
            const error = appController.migrateDataDirectory(newDirectory)
            if (error === "" || error === undefined || error === null) {
                appController.requestRestart()
            } else {
                migrateResultDialog.message = "迁移失败：\n" + error
                migrateResultDialog.open()
            }
        }
    }

    ConfirmDialog {
        id: clearWarningDialog
        objectName: "clearWarningDialog"
        title: "确定清空所有数据？"
        confirmText: "继续"
        message: "将永久删除所有乐谱文件、缩略图、文件夹、标签、收藏、阅读记录、缓存和应用设置。此操作无法撤销。"
        onAccepted: {
            clearConfirmInput.text = ""
            clearTypedDialog.open()
        }
    }

    Dialog {
        id: clearTypedDialog
        objectName: "clearTypedDialog"
        parent: Overlay.overlay
        width: parent ? Math.min(460, parent.width - 48) : 460
        x: parent ? Math.round((parent.width - width) / 2) : 0
        y: parent ? Math.round((parent.height - height) / 2) : 0
        modal: true
        focus: true
        padding: 22
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        header: Label {
            leftPadding: 22; rightPadding: 22; topPadding: 20; bottomPadding: 4
            text: "输入确认文字"
            color: Theme.foreground
            font.pixelSize: Theme.fontLg
            font.weight: Font.DemiBold
        }
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: "请手动输入“确认清空所有数据”以继续。应用会在清空后自动重启。"
                color: Theme.secondaryForeground
                font.pixelSize: Theme.fontMd
                wrapMode: Text.WordWrap
            }
            TextField {
                id: clearConfirmInput
                objectName: "clearConfirmInput"
                Layout.fillWidth: true
                placeholderText: "确认清空所有数据"
                color: Theme.foreground
                selectByMouse: true
                background: Rectangle {
                    implicitHeight: 42
                    radius: Theme.radiusMd
                    color: Theme.inputBackground
                    border.width: 1
                    border.color: clearConfirmInput.activeFocus ? Theme.inputFocusBorder : Theme.inputBorder
                }
            }
        }
        footer: Item {
            implicitHeight: 62
            RowLayout {
                anchors.right: parent.right
                anchors.rightMargin: 22
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10
                AppButton { text: "取消"; onClicked: clearTypedDialog.reject() }
                AppButton {
                    objectName: "confirmClearAllDataButton"
                    text: "永久清空"
                    danger: true
                    enabled: clearConfirmInput.text === "确认清空所有数据"
                    onClicked: {
                        const error = appController.clearAllData(clearConfirmInput.text)
                        if (error.length > 0) {
                            migrateResultDialog.message = error
                            migrateResultDialog.open()
                        } else {
                            clearTypedDialog.close()
                        }
                    }
                }
            }
        }
        background: Rectangle {
            radius: Theme.radiusLg
            color: Theme.surface
            border.width: 1
            border.color: Theme.danger
        }
    }

    ConfirmDialog {
        id: migrateResultDialog
        title: "迁移结果"
        confirmText: "确定"
    }
}
