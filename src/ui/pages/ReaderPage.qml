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

    function stopAtEnd() {
        const maximum = Math.max(0, readerFlick.contentHeight - readerFlick.height)
        if (readerFlick.contentY >= maximum) {
            readerFlick.contentY = maximum
            autoScrolling = false
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 64
            color: Theme.surface
            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                Button { text: "← 乐谱库"; onClicked: { root.autoScrolling = false; appController.currentPage = "library" } }
                Label {
                    Layout.fillWidth: true
                    text: appController.currentScoreTitle.length > 0 ? appController.currentScoreTitle : "阅读器"
                    color: Theme.foreground
                    font.pixelSize: 17
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }
                Button {
                    text: root.autoScrolling ? "暂停滚动" : "自动滚动"
                    enabled: root.isPdf || root.isImage
                    onClicked: root.autoScrolling = !root.autoScrolling
                }
                Label { text: "速度"; color: Theme.mutedForeground }
                Slider {
                    from: 15
                    to: 160
                    value: root.scrollSpeed
                    stepSize: 5
                    implicitWidth: 130
                    onMoved: appController.autoScrollSpeed = value
                }
                Label { text: Math.round(root.scrollSpeed) + " 像素/秒"; color: Theme.mutedForeground; Layout.minimumWidth: 82 }
            }
        }

        Flickable {
            id: readerFlick
            objectName: "readerFlick"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: documentColumn.height
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: documentColumn
                width: readerFlick.width
                spacing: 18
                topPadding: 18
                bottomPadding: 18

                PdfDocument {
                    id: pdfDocument
                    source: root.isPdf ? appController.currentFileUrl : ""
                }

                Repeater {
                    model: root.isPdf ? pdfDocument.pageCount : 0
                    delegate: Rectangle {
                        required property int index
                        readonly property size pageSize: pdfDocument.pagePointSize(index)
                        width: Math.min(documentColumn.width - 36, 1100)
                        height: pageSize.width > 0 ? width * pageSize.height / pageSize.width : 800
                        x: (documentColumn.width - width) / 2
                        color: "white"
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
                    width: visible ? Math.min(documentColumn.width - 36, 1400) : 0
                    height: visible ? (sourceSize.width > 0 ? width * sourceSize.height / sourceSize.width : 700) : 0
                    x: (documentColumn.width - width) / 2
                    source: root.isImage ? appController.currentFileUrl : ""
                    asynchronous: true
                    fillMode: Image.PreserveAspectFit
                }

                Label {
                    visible: appController.currentFileUrl.toString().length === 0
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "请从乐谱库双击打开一份乐谱"
                    color: Theme.mutedForeground
                    font.pixelSize: 18
                }

                Label {
                    visible: appController.currentFileUrl.toString().length > 0 && !root.isPdf && !root.isImage
                    width: documentColumn.width - 72
                    anchors.horizontalCenter: parent.horizontalCenter
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: "文件已安全导入资料库，但当前版本暂不支持预览此格式：" + (appController.currentFileType.length > 0 ? appController.currentFileType : "未知")
                    color: Theme.mutedForeground
                    font.pixelSize: 18
                }
            }

            ScrollBar.vertical: ScrollBar { }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 42
            color: Theme.surface
            Label {
                anchors.centerIn: parent
                text: root.isPdf ? "共 " + pdfDocument.pageCount + " 页" : (root.isImage ? "图片乐谱" : "附件")
                color: Theme.foreground
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
        }
    }
}
