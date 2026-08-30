import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

Rectangle {
    color: Theme.background

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingXl
        spacing: Theme.spacingLg

        // 标题
        ColumnLayout {
            spacing: 2
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

        // ── 通用设置卡片 ─────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            color: Theme.surface
            radius: Theme.radiusLg
            border.color: Theme.border
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingLg
                spacing: Theme.spacingMd

                Label {
                    text: "通用"
                    color: Theme.foreground
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.DemiBold
                }

                // 主题选择行
                Rectangle {
                    Layout.fillWidth: true
                    height: 52
                    radius: Theme.radiusMd
                    color: Theme.elevatedSurface
                    border.color: Theme.border
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 12

                        ColumnLayout {
                            spacing: 2
                            Label {
                                text: "主题"
                                color: Theme.foreground
                                font.pixelSize: Theme.fontMd
                                font.weight: Font.Medium
                            }
                            Label {
                                text: "选择浅色、深色或跟随系统"
                                color: Theme.mutedForeground
                                font.pixelSize: Theme.fontXs
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // 主题分段选择器
                        Row {
                            spacing: 0
                            Repeater {
                                model: ["跟随系统", "浅色", "深色"]
                                delegate: Rectangle {
                                    required property int index
                                    required property string modelData
                                    width: 82
                                    height: 32
                                    radius: index === 0 ? Theme.radiusSm : 0
                                    color: appController.themeMode === index ? Theme.accent : Theme.buttonBackground
                                    border.width: 1
                                    border.color: appController.themeMode === index ? Theme.accent : Theme.buttonBorder

                                    Label {
                                        anchors.centerIn: parent
                                        text: modelData
                                        color: appController.themeMode === index ? Theme.accentForeground : Theme.buttonText
                                        font.pixelSize: Theme.fontSm
                                        font.weight: appController.themeMode === index ? Font.DemiBold : Font.Medium
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: appController.themeMode = index
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ── 阅读器设置卡片 ───────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            color: Theme.surface
            radius: Theme.radiusLg
            border.color: Theme.border
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingLg
                spacing: Theme.spacingMd

                Label {
                    text: "阅读器"
                    color: Theme.foreground
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 52
                    radius: Theme.radiusMd
                    color: Theme.elevatedSurface
                    border.color: Theme.border
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 12

                        ColumnLayout {
                            spacing: 2
                            Label {
                                text: "默认滚动速度"
                                color: Theme.foreground
                                font.pixelSize: Theme.fontMd
                                font.weight: Font.Medium
                            }
                            Label {
                                text: "自动滚动时的初始速度"
                                color: Theme.mutedForeground
                                font.pixelSize: Theme.fontXs
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            text: Math.round(appController.autoScrollSpeed) + " px/s"
                            color: Theme.accent
                            font.pixelSize: Theme.fontSm
                            font.weight: Font.DemiBold
                            Layout.preferredWidth: 64
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 52
                    radius: Theme.radiusMd
                    color: Theme.elevatedSurface
                    border.color: Theme.border
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 12

                        ColumnLayout {
                            spacing: 2
                            Label {
                                text: "关于"
                                color: Theme.foreground
                                font.pixelSize: Theme.fontMd
                                font.weight: Font.Medium
                            }
                            Label {
                                text: "Notera 本地乐谱阅读器"
                                color: Theme.mutedForeground
                                font.pixelSize: Theme.fontXs
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            text: "v0.1.2"
                            color: Theme.mutedForeground
                            font.pixelSize: Theme.fontSm
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
