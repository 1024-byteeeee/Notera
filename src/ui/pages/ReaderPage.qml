import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Pdf
import Notera

Rectangle {
    id: root
    objectName: "readerPage"
    color: Theme.background

    readonly property bool isPdf: appController.currentFileType === "pdf"
    readonly property bool isImage: ["jpg", "jpeg", "png", "bmp", "gif", "webp", "tif", "tiff"].indexOf(appController.currentFileType) !== -1
    property bool autoScrolling: false
    property real scrollSpeed: appController.autoScrollSpeed
    property real zoomLevel: 1.0

    function resetZoom() { root.zoomLevel = 1.0 }
    function zoomIn() { root.zoomLevel = Math.min(3.0, root.zoomLevel + 0.25) }
    function zoomOut() { root.zoomLevel = Math.max(0.4, root.zoomLevel - 0.25) }

    function stopAtEnd() {
        const maximum = Math.max(0, readerFlick.contentHeight - readerFlick.height)
        if (readerFlick.contentY >= maximum) {
            readerFlick.contentY = maximum
            autoScrolling = false
        }
    }

    // 工具按钮组件
    component ToolButton: Rectangle {
        required property string btnText
        property bool btnEnabled: true
        property bool btnActive: false
        signal btnClicked()

        height: Theme.controlHeight
        radius: Theme.radiusMd
        color: !btnEnabled ? Theme.buttonDisabled
             : btnActive ? Theme.selectedBackground
             : btnMouse.containsMouse ? Theme.buttonHover : Theme.buttonBackground
        border.width: 1
        border.color: !btnEnabled ? "transparent"
                    : btnActive ? Theme.selectedBorder
                    : btnMouse.containsMouse ? Theme.strongBorder : Theme.buttonBorder
        Behavior on color { ColorAnimation { duration: 120 } }
        opacity: btnEnabled ? 1 : 0.4

        Label {
            anchors.centerIn: parent
            text: parent.btnText
            color: !btnEnabled ? Theme.buttonDisabledText
                 : btnActive ? Theme.selectedText : Theme.buttonText
            font.pixelSize: Theme.fontSm
            font.weight: btnActive ? Font.DemiBold : Font.Medium
        }

        MouseArea {
            id: btnMouse
            anchors.fill: parent
            hoverEnabled: true
            enabled: parent.btnEnabled
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.btnClicked()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── 顶部工具栏 ───────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 60
            color: Theme.surface
            border.color: Theme.border
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                // 返回按钮
                ToolButton {
                    btnText: "← 乐谱库"
                    Layout.preferredWidth: 100
                    onBtnClicked: { root.autoScrolling = false; appController.currentPage = "library" }
                }

                // 标题
                Label {
                    Layout.fillWidth: true
                    text: appController.currentScoreTitle.length > 0 ? appController.currentScoreTitle : "阅读器"
                    color: Theme.foreground
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }

                // 自动滚动按钮
                ToolButton {
                    btnText: root.autoScrolling ? "⏸ 暂停滚动" : "▶ 自动滚动"
                    btnEnabled: root.isPdf || root.isImage
                    btnActive: root.autoScrolling
                    Layout.preferredWidth: 118
                    onBtnClicked: root.autoScrolling = !root.autoScrolling
                }

                // 速度控制
                Rectangle {
                    Layout.preferredWidth: 180
                    height: Theme.controlHeight
                    radius: Theme.radiusMd
                    color: Theme.buttonBackground
                    border.color: Theme.buttonBorder
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Label {
                            text: "速度"
                            color: Theme.mutedForeground
                            font.pixelSize: Theme.fontSm
                        }

                        Slider {
                            id: speedSlider
                            Layout.fillWidth: true
                            from: 15
                            to: 160
                            value: root.scrollSpeed
                            stepSize: 5
                            onMoved: appController.autoScrollSpeed = value
                        }

                        Label {
                            text: Math.round(root.scrollSpeed)
                            color: Theme.foreground
                            font.pixelSize: Theme.fontSm
                            font.weight: Font.Medium
                            Layout.preferredWidth: 28
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }

                // 缩放控制
                Rectangle {
                    Layout.preferredWidth: 170
                    height: Theme.controlHeight
                    radius: Theme.radiusMd
                    color: Theme.buttonBackground
                    border.color: Theme.buttonBorder
                    border.width: 1
                    visible: root.isPdf || root.isImage

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 2

                        // 缩小
                        Rectangle {
                            Layout.preferredWidth: 30
                            height: 28
                            radius: 6
                            color: zoomOutMouse.containsMouse ? Theme.buttonHover : "transparent"
                            Label { anchors.centerIn: parent; text: "−"; color: Theme.buttonText; font.pixelSize: 16; font.weight: Font.Bold }
                            MouseArea {
                                id: zoomOutMouse
                                anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.zoomOut()
                            }
                        }

                        // 百分比（点击重置）
                        Rectangle {
                            Layout.fillWidth: true
                            height: 28
                            radius: 6
                            color: zoomResetMouse.containsMouse ? Theme.buttonHover : "transparent"
                            Label {
                                anchors.centerIn: parent
                                text: Math.round(root.zoomLevel * 100) + "%"
                                color: Theme.buttonText
                                font.pixelSize: 12
                                font.weight: Font.Medium
                            }
                            MouseArea {
                                id: zoomResetMouse
                                anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.resetZoom()
                            }
                        }

                        // 放大
                        Rectangle {
                            Layout.preferredWidth: 30
                            height: 28
                            radius: 6
                            color: zoomInMouse.containsMouse ? Theme.buttonHover : "transparent"
                            Label { anchors.centerIn: parent; text: "+"; color: Theme.buttonText; font.pixelSize: 16; font.weight: Font.Bold }
                            MouseArea {
                                id: zoomInMouse
                                anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.zoomIn()
                            }
                        }
                    }
                }
            }
        }

        // ── 阅读区域 ─────────────────────────────────────
        Flickable {
            id: readerFlick
            objectName: "readerFlick"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: documentColumn.height
            boundsBehavior: Flickable.StopAtBounds

            // Ctrl+滚轮缩放（普通滚轮仍用于滚动）
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                propagateComposedEvents: true
                z: 5
                onWheel: function(wheel) {
                    if (wheel.modifiers & Qt.ControlModifier) {
                        wheel.accepted = true
                        const delta = wheel.angleDelta.y / 1200
                        root.zoomLevel = Math.max(0.4, Math.min(3.0, root.zoomLevel + delta))
                    }
                }
            }

            Column {
                id: documentColumn
                width: readerFlick.width
                spacing: 20
                topPadding: 24
                bottomPadding: 24

                PdfDocument {
                    id: pdfDocument
                    source: root.isPdf ? appController.currentFileUrl : ""
                }

                Repeater {
                    model: root.isPdf ? pdfDocument.pageCount : 0
                    delegate: Rectangle {
                        required property int index
                        readonly property size pageSize: pdfDocument.pagePointSize(index)
                        width: Math.min(documentColumn.width - 48, 1100) * root.zoomLevel
                        height: pageSize.width > 0 ? width * pageSize.height / pageSize.width : 800
                        x: (documentColumn.width - width) / 2
                        color: "white"
                        radius: Theme.radiusSm
                        border.color: Theme.border
                        border.width: 1

                        PdfPageImage {
                            anchors.fill: parent
                            document: pdfDocument
                            currentFrame: index
                            asynchronous: true
                            fillMode: Image.PreserveAspectFit
                            sourceSize.width: width
                            sourceSize.height: height
                        }
                    }
                }

                Image {
                    visible: root.isImage
                    width: visible ? Math.min(documentColumn.width - 48, 1400) * root.zoomLevel : 0
                    height: visible ? (sourceSize.width > 0 ? width * sourceSize.height / sourceSize.width : 700) : 0
                    x: (documentColumn.width - width) / 2
                    source: root.isImage ? appController.currentFileUrl : ""
                    asynchronous: true
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }

                Label {
                    visible: appController.currentFileUrl.toString().length === 0
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "请从乐谱库单击打开一份乐谱"
                    color: Theme.mutedForeground
                    font.pixelSize: Theme.fontLg
                }

                Label {
                    visible: appController.currentFileUrl.toString().length > 0 && !root.isPdf && !root.isImage
                    width: documentColumn.width - 72
                    anchors.horizontalCenter: parent.horizontalCenter
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: "文件已安全导入资料库，但当前版本暂不支持预览此格式：" + (appController.currentFileType.length > 0 ? appController.currentFileType : "未知")
                    color: Theme.mutedForeground
                    font.pixelSize: Theme.fontMd
                }
            }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }

        // ── 底部状态栏 ───────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 36
            color: Theme.surface
            border.color: Theme.border
            border.width: 1

            Label {
                anchors.centerIn: parent
                text: root.isPdf ? "共 " + pdfDocument.pageCount + " 页" : (root.isImage ? "图片乐谱" : "附件")
                color: Theme.mutedForeground
                font.pixelSize: Theme.fontSm
            }
        }
    }

    Timer {
        interval: 16
        repeat: true
        running: root.autoScrolling
        onTriggered: {
            readerFlick.contentY += root.scrollSpeed * interval / 1000
            root.stopAtEnd()
        }
    }

    Connections {
        target: appController
        function onCurrentScoreChanged() {
            readerFlick.contentY = 0
            root.autoScrolling = false
            root.zoomLevel = 1.0
        }
    }
}
