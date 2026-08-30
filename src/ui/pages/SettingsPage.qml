import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

Rectangle {
    color: Theme.background

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentCol.height + 80
        clip: true

        Column {
            id: contentCol
            width: parent.width
            spacing: 20

            Item { width: parent.width; height: 36 }

            // 标题
            Column {
                width: parent.width
                spacing: 4
                leftPadding: 36
                Label {
                    text: "设置"
                    color: Theme.foreground
                    font.pixelSize: 26
                    font.weight: Font.Bold
                }
                Label {
                    text: "个性化你的阅读体验"
                    color: Theme.mutedForeground
                    font.pixelSize: 13
                }
            }

            // ── 通用设置卡片 ─────────────────────────────
            Rectangle {
                width: parent.width - 72
                x: 36
                color: Theme.surface
                radius: Theme.radiusLg
                border.color: Theme.border
                border.width: 1

                Column {
                    id: generalCol
                    width: parent.width
                    spacing: 10
                    topPadding: 20
                    bottomPadding: 20
                    leftPadding: 20
                    rightPadding: 20

                    Label {
                        text: "通用"
                        color: Theme.foreground
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        bottomPadding: 6
                    }

                    // 主题选择行
                    Rectangle {
                        width: parent.width
                        height: 56
                        color: Theme.elevatedSurface
                        radius: Theme.radiusMd
                        border.color: Theme.border
                        border.width: 1

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 12

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2
                                Label {
                                    text: "主题"
                                    color: Theme.foreground
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                }
                                Label {
                                    text: "选择浅色、深色或跟随系统"
                                    color: Theme.mutedForeground
                                    font.pixelSize: 11
                                }
                            }

                            Item { width: parent.width - 32 - themeCol.width - themeRow.width - 12; height: 1 }

                            // 分段选择器
                            Row {
                                id: themeRow
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 0
                                Repeater {
                                    model: ["跟随系统", "浅色", "深色"]
                                    delegate: Rectangle {
                                        required property int index
                                        required property string modelData
                                        width: 84
                                        height: 34
                                        color: appController.themeMode === index ? Theme.accent : Theme.buttonBackground
                                        border.width: 1
                                        border.color: appController.themeMode === index ? Theme.accent : Theme.buttonBorder
                                        radius: index === 0 ? Theme.radiusSm : (index === 2 ? Theme.radiusSm : 0)

                                        Label {
                                            anchors.centerIn: parent
                                            text: modelData
                                            color: appController.themeMode === index ? Theme.accentForeground : Theme.buttonText
                                            font.pixelSize: 12
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
                width: parent.width - 72
                x: 36
                color: Theme.surface
                radius: Theme.radiusLg
                border.color: Theme.border
                border.width: 1

                Column {
                    width: parent.width
                    spacing: 10
                    topPadding: 20
                    bottomPadding: 20
                    leftPadding: 20
                    rightPadding: 20

                    Label {
                        text: "阅读器"
                        color: Theme.foreground
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        bottomPadding: 6
                    }

                    // 默认滚动速度
                    Rectangle {
                        width: parent.width
                        height: 56
                        color: Theme.elevatedSurface
                        radius: Theme.radiusMd
                        border.color: Theme.border
                        border.width: 1

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 12

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2
                                Label {
                                    text: "默认滚动速度"
                                    color: Theme.foreground
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                }
                                Label {
                                    text: "自动滚动时的初始速度"
                                    color: Theme.mutedForeground
                                    font.pixelSize: 11
                                }
                            }

                            Item { width: parent.width - 32 - speedCol.width - 60 - 12; height: 1 }

                            Label {
                                anchors.verticalCenter: parent.verticalCenter
                                text: Math.round(appController.autoScrollSpeed) + " px/s"
                                color: Theme.accent
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }

                    // 关于
                    Rectangle {
                        width: parent.width
                        height: 56
                        color: Theme.elevatedSurface
                        radius: Theme.radiusMd
                        border.color: Theme.border
                        border.width: 1

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 12

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2
                                Label {
                                    text: "关于"
                                    color: Theme.foreground
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                }
                                Label {
                                    text: "本地优先的乐谱阅读器"
                                    color: Theme.mutedForeground
                                    font.pixelSize: 11
                                }
                            }

                            Item { width: parent.width - 32 - aboutCol.width - 50 - 12; height: 1 }

                            Label {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "v0.1.2"
                                color: Theme.mutedForeground
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }

            Item { width: parent.width; height: 40 }
        }
    }
}
