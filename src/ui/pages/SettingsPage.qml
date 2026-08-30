import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
            y: 32
            width: Math.max(0, flick.width - Theme.spacingXl * 2)
            spacing: Theme.spacingLg

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
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
                            anchors.rightMargin: 14
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
                                        model: ["跟随系统", "浅色", "深色"]
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
                                value: appController.autoScrollSpeed
                                onMoved: appController.autoScrollSpeed = value

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
                                text: Math.round(appController.autoScrollSpeed) + " px/s"
                                color: Theme.accent
                                font.pixelSize: Theme.fontSm
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignRight
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
                                Label { text: "本地优先的中文乐谱阅读器"; color: Theme.mutedForeground; font.pixelSize: Theme.fontXs }
                            }
                            Label { text: "v0.1.2"; color: Theme.mutedForeground; font.pixelSize: Theme.fontSm }
                        }
                    }
                }
            }
        }
    }
}
