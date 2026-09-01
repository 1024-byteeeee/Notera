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
                                Layout.fillWidth: true
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
                                Layout.fillWidth: true
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
                                from: 15
                                to: 160
                                stepSize: 5
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
                                Layout.fillWidth: true
                                spacing: 3
                                Label { text: "关于 Notera"; color: Theme.foreground; font.pixelSize: Theme.fontMd; font.weight: Font.Medium }
                                Label { text: "简单的本地乐谱阅读器"; color: Theme.mutedForeground; font.pixelSize: Theme.fontXs }
                            }
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
        id: migrateResultDialog
        title: "迁移结果"
        confirmText: "确定"
    }
}
