import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

Rectangle {
    color: Theme.background

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.height
        clip: true

        ColumnLayout {
            id: column
            width: parent.width
            spacing: Theme.spacingLg

            // 顶部间距
            Item { Layout.fillWidth: true; Layout.preferredHeight: Theme.spacingXl }

            // 标题
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingXl
                Layout.rightMargin: Theme.spacingXl
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

            // ── 通用设置卡片 ─────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingXl
                Layout.rightMargin: Theme.spacingXl
                color: Theme.surface
                radius: Theme.radiusLg
                border.color: Theme.border
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingLg
                    spacing: Theme.spacingSm

                    Label {
                        text: "通用"
                        color: Theme.foreground
                        font.pixelSize: Theme.fontLg
                        font.weight: Font.DemiBold
                        Layout.bottomMargin: Theme.spacingSm
                    }

                    // 主题选择行
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        color: Theme.elevatedSurface
                        radius: Theme.radiusMd
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

                            // 分段选择器
                            Row {
                                spacing: 0
                                Repeater {
                                    id: themeRepeater
                                    model: ["跟随系统", "浅色", "深色"]
                                    delegate: Rectangle {
                                        required property int index
                                        required property string modelData
                                        width: 84
                                        height: 34
                                        radius: 0
                                        color: appController.themeMode === index ? Theme.accent : Theme.buttonBackground
                                        border.width: 1
                                        border.color: appController.themeMode === index ? Theme.accent : Theme.buttonBorder

                                        // 第一个左圆角，最后一个右圆角
                                        Rectangle {
                                            visible: index === 0
                                            anchors.fill: parent
                                            radius: Theme.radiusSm
                                            color: parent.color
                                            border.width: 1
                                            border.color: parent.border.color
                                            z: -1
                                        }

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

            // ── 阅读器设置卡片 ───────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingXl
                Layout.rightMargin: Theme.spacingXl
                color: Theme.surface
                radius: Theme.radiusLg
                border.color: Theme.border
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingLg
                    spacing: Theme.spacingSm

                    Label {
                        text: "阅读器"
                        color: Theme.foreground
                        font.pixelSize: Theme.fontLg
                        font.weight: Font.DemiBold
                        Layout.bottomMargin: Theme.spacingSm
                    }

                    // 默认滚动速度
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        color: Theme.elevatedSurface
                        radius: Theme.radiusMd
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

                    // 关于
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        color: Theme.elevatedSurface
                        radius: Theme.radiusMd
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
                                    text: "本地优先的乐谱阅读器"
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

            Item { Layout.fillWidth: true; Layout.preferredHeight: Theme.spacingXl }
        }
    }
}
