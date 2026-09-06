// Notera 定制版 PdfMultiPageView
// 基于 Qt 6.8 官方 QtQuick.Pdf/PdfMultiPageView.qml 删减定制：
//  - 保留 TableView 虚拟化（万页只实例化可见行）与 PdfPageImage 异步渲染
//  - 删除：文本选择 / 搜索高亮 / 内部链接 / PdfStyle / 导航栈（back/forward）/
//          内置 PinchHandler（缩放由外部 zoomLevel 单一真源驱动）
//  - 新增：暴露 contentX/contentY（自动滚动）、resetView()、viewMovementStarted 信号、
//          currentPageRenderingStatus 在首行可渲染时即更新（打开 overlay 用）
// 官方组件允许复制修改（见官方 QML 文件头注释）。

import QtQuick
import QtQuick.Controls
import QtQuick.Pdf

Item {
    id: root

    /*! 外部传入的 PdfDocument（必须已设置 source） */
    required property PdfDocument document

    // ---------- Notera 定制：视图控制 ----------
    property alias contentX: tableView.contentX
    property alias contentY: tableView.contentY
    property alias contentWidth: tableView.contentWidth
    property alias contentHeight: tableView.contentHeight
    property alias currentPage: pageNavigator.currentPage
    property int currentPageRenderingStatus: Image.Null
    property real renderScale: 1
    property real pageRotation: 0
    property real rowSpacing: 20
    property real topMargin: 24
    property real bottomMargin: 24

    signal viewMovementStarted()

    readonly property int pageCount: root.document ? root.document.pageCount : 0

    function goToPage(page) {
        if (page === pageNavigator.currentPage)
            return
        pageNavigator.jump(page, Qt.point(-1, -1), 0)
    }

    function resetView() {
        // 只复位滚动位置；renderScale / pageRotation 由外部绑定控制，
        // 此处禁止赋值（赋值会断开外部绑定，导致缩放/旋转失效）。
        tableView.contentX = 0
        tableView.contentY = 0
    }

    function cancelFlick() { tableView.cancelFlick() }

    TableView {
        id: tableView
        anchors.fill: parent
        anchors.leftMargin: 2
        model: root.pageCount
        rowSpacing: root.rowSpacing
        topMargin: root.topMargin
        bottomMargin: root.bottomMargin
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        onMovementStarted: root.viewMovementStarted()

        property real rotationNorm: Math.round((360 + (root.pageRotation % 360)) % 360)
        property bool rot90: rotationNorm == 90 || rotationNorm == 270
        onRot90Changed: forceLayout()
        property size firstPagePointSize: root.document && root.document.status === PdfDocument.Ready
            ? root.document.pagePointSize(0) : Qt.size(1, 1)
        columnWidthProvider: function(col) {
            // 直接计算，不依赖中间绑定（TableView 内部 property 绑定曾出现不随 renderScale 更新的问题）。
            // 内容宽 = max(视口宽, 最大页显示宽)：页面不超视口时无水平滚动条，放大超出时才出现。
            if (!root.document) return 0
            const maxW = rot90 ? root.document.maxPageHeight : root.document.maxPageWidth
            return Math.max(root.width, maxW * root.renderScale)
        }
        rowHeightProvider: function(row) {
            const s = root.document ? root.document.pagePointSize(row) : Qt.size(1, 1)
            return (rot90 ? s.width : s.height) * root.renderScale
        }

        // 文档就绪后初始化当前页为 0（打开 overlay 依赖 currentPageRenderingStatus）
        onRowsChanged: {
            if (rows > 0 && pageNavigator.currentPage < 0)
                pageNavigator.update(0, Qt.point(-1, -1), 1)
        }

        delegate: Rectangle {
            id: pageHolder
            required property int index
            color: "transparent"

            Rectangle {
                id: paper
                width: image.width
                height: image.height
                rotation: root.pageRotation
                anchors.centerIn: parent
                color: "white"
                radius: 2
                border.color: "#D5D4CC"
                border.width: 1
                property size pagePointSize: root.document ? root.document.pagePointSize(pageHolder.index) : Qt.size(1, 1)

                PdfPageImage {
                    id: image
                    objectName: "pdfPageImageItem"
                    document: root.document
                    currentFrame: pageHolder.index
                    asynchronous: true
                    fillMode: Image.PreserveAspectFit
                    width: paper.pagePointSize.width * root.renderScale
                    height: paper.pagePointSize.height * root.renderScale
                    property real renderScale: root.renderScale
                    onRenderScaleChanged: {
                        image.sourceSize.width = paper.pagePointSize.width * renderScale * Screen.devicePixelRatio
                        image.sourceSize.height = 0
                    }
                    onStatusChanged: {
                        if (pageHolder.index === root.currentPage
                            || (root.currentPage < 0 && pageHolder.index === 0)) {
                            root.currentPageRenderingStatus = status
                        }
                    }
                }
            }
        }

        ScrollBar.vertical: ScrollBar { id: vscroll }
        ScrollBar.horizontal: ScrollBar { }

        // 滚动时跟随当前页（节流）：自动滚动/滚轮/拖拽均经过 contentY
        onContentYChanged: {
            if (pageNavigator.currentPage >= 0 && !syncTimer.running)
                syncTimer.start()
        }
    }

    Timer {
        id: syncTimer
        interval: 100
        repeat: false
        onTriggered: {
            if (pageNavigator.currentPage < 0)
                return
            const cell = tableView.cellAtPos(root.width / 2, root.height / 2)
            if (cell.y >= 0 && cell.y !== pageNavigator.currentPage)
                pageNavigator.update(cell.y, Qt.point(-1, -1), root.renderScale)
        }
    }

    onRenderScaleChanged: {
        if (pageNavigator.jumping)
            return
        tableView.forceLayout()
        const cell = tableView.cellAtPos(root.width / 2, root.height / 2)
        const currentItem = cell.x >= 0 ? tableView.itemAtCell(cell) : null
        if (currentItem) {
            const currentLocation = Qt.point(tableView.contentX - currentItem.x,
                                             tableView.contentY - currentItem.y)
            pageNavigator.update(cell.y, currentLocation, renderScale)
        }
    }

    PdfPageNavigator {
        id: pageNavigator
        property bool jumping: false
        property int previousPage: 0
        onJumped: function(current) {
            jumping = true
            if (current.zoom > 0)
                root.renderScale = current.zoom
            if (current.location.y < 0) {
                const previousPageDelegate = tableView.itemAtCell(0, previousPage)
                const currentYOffset = previousPageDelegate
                    ? tableView.contentY - previousPageDelegate.y
                    : 0
                tableView.positionViewAtRow(current.page, Qt.AlignTop, currentYOffset)
            } else {
                const pageSize = root.document.pagePointSize(current.page)
                const rectPx = Qt.rect(current.location.x * root.renderScale,
                                       current.location.y * root.renderScale,
                                       1, 1)
                tableView.positionViewAtCell(0, current.page,
                    TableView.AlignLeft | TableView.AlignTop, Qt.point(0, 0), rectPx)
            }
            jumping = false
            previousPage = current.page
        }

        property url documentSource: root.document ? root.document.source : ""
        onDocumentSourceChanged: {
            pageNavigator.clear()
            root.resetView()
        }
    }
}
