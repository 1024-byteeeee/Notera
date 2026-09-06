// Notera 缓存版 PDF 页面图像组件。
// 替换 PdfPageImage：先查 PdfRenderCache，命中直接显示（0ms）；
// miss 则通过 QPdfPageRenderer 异步渲染并写入缓存。
//
// Phase 2：可见页 High 优先级插队渲染，预渲染 Low 优先级可批量取消。
// Phase 3：分块渲染——tileCount>1 时大页面分成 N×N 块，每块独立缓存/渲染，
//          首块快速显示（参考 SumatraPDF TilePosition + Okular TilesManager）。
//          整页模式(tileCount=1)支持 closest 缓存（Sioyek 技巧，缩放零空白）。

import QtQuick
import QtQuick.Pdf
import Notera

Item {
    id: root

    /*! 外部传入的 PdfDocument（必须已设置 source） */
    required property PdfDocument document

    /*! 页码（0-based） */
    required property int currentFrame

    /*! 渲染缩放（与 PdfMultiPageView.renderScale 一致） */
    property real renderScale: 1

    /*! 页面旋转角度（0/90/180/270） */
    property real pageRotation: 0

    /*! 渲染状态（兼容 PdfPageImage.status：Image.Null/Loading/Ready） */
    property int status: Image.Null

    /*! 分块网格大小：1=整页渲染(默认)，2=2×2分块，3=3×3分块。
        大页面(渲染宽度>1500px)建议设为 2 或 3，首块显示更快。 */
    property int tileCount: 1

    // 与 C++ PdfRenderCache::makeKey / quantizeScale 保持一致
    function _quantizeScale(s) { return Math.round(s * 10000) }
    // 注意：缓存 key 中不包含 pageRotation，因为内部渲染时始终传入 rotation=0，
    // 渲染图像为原始方向。页面旋转完全由外层 paper.rotation 负责（与 Qt 官方
    // PdfPageImage 行为一致）。这样渲染图像宽高比与布局容器一致，
    // Image.PreserveAspectFit 不会压缩显示。
    function _cacheKey(page, scale, tileRow, tileCol) {
        var key = page + "_" + root._quantizeScale(scale) + "_0"
        if (tileRow !== undefined && tileRow >= 0 && tileCol !== undefined && tileCol >= 0)
            key += "_" + tileRow + "_" + tileCol
        return key
    }
    function _pageImageSize() {
        if (!root.document || root.document.status !== PdfDocument.Ready) return Qt.size(0, 0)
        const ps = root.document.pagePointSize(root.currentFrame)
        // 渲染时不旋转，始终使用原始宽高
        return Qt.size(Math.max(1, Math.round(ps.width * root.renderScale * Screen.devicePixelRatio)),
                        Math.max(1, Math.round(ps.height * root.renderScale * Screen.devicePixelRatio)))
    }

    // 分块网格：tileCount=1 时退化为单页
    Grid {
        id: tileGrid
        anchors.fill: parent
        columns: Math.max(1, root.tileCount)
        rows: Math.max(1, root.tileCount)
        spacing: 0

        Repeater {
            id: tileRepeater
            model: Math.max(1, root.tileCount) * Math.max(1, root.tileCount)

            delegate: Item {
                id: tileDelegate
                required property int index

                // Grid 的 delegate 必须显式设置尺寸，否则 Grid 无法计算 cell 尺寸，
                // 会导致 delegate 尺寸为 0，Image 不显示（全白）。
                width: tileGrid.width / Math.max(1, root.tileCount)
                height: tileGrid.height / Math.max(1, root.tileCount)

                readonly property int tileRow: Math.floor(index / Math.max(1, root.tileCount))
                readonly property int tileCol: index % Math.max(1, root.tileCount)
                readonly property bool isWholePage: root.tileCount <= 1

                property var pendingRequestId: 0

                function tileKey() {
                    return root._cacheKey(root.currentFrame, root.renderScale,
                        isWholePage ? -1 : tileRow,
                        isWholePage ? -1 : tileCol)
                }

                function refresh() {
                    if (!root.document || root.document.status !== PdfDocument.Ready
                        || root.currentFrame < 0 || root.currentFrame >= root.document.pageCount) {
                        tileImage.source = ""
                        if (isWholePage) root.status = Image.Null
                        return
                    }

                    // 取消旧请求
                    if (tileDelegate.pendingRequestId !== 0) {
                        pdfRender.cancelRequest(tileDelegate.pendingRequestId)
                        tileDelegate.pendingRequestId = 0
                    }

                    const tRow = isWholePage ? -1 : tileRow
                    const tCol = isWholePage ? -1 : tileCol

                    // 1. 精确命中（渲染时 rotation 始终为 0）
                    if (pdfRender.hasCache(root.currentFrame, root.renderScale,
                                           0, tRow, tCol)) {
                        tileImage.source = "image://pdfcache/" + tileDelegate.tileKey()
                        // 分块模式下第一个块命中即标记 Ready（首块可见=PDF已打开）
                        if (root.status !== Image.Ready) root.status = Image.Ready
                        return
                    }

                    // 2. closest 命中（仅整页模式：缩放时先用旧分辨率拉伸显示，零空白）
                    if (isWholePage) {
                        const closest = pdfRender.closestCacheKey(
                            root.currentFrame, root.renderScale, 0)
                        if (closest !== "" && closest !== tileDelegate.tileKey()) {
                            tileImage.source = "image://pdfcache/" + closest
                            root.status = Image.Ready // 临时显示
                        } else {
                            tileImage.source = ""
                            root.status = Image.Loading
                        }
                    } else {
                        // 分块模式 miss：标记 Loading，等待第一个块渲染完成
                        tileImage.source = ""
                        if (root.status !== Image.Ready) root.status = Image.Loading
                    }

                    // 3. 请求渲染（High 优先级：可见页立即插队，rotation 始终为 0）
                    const imgSize = root._pageImageSize()
                    if (imgSize.width > 0 && imgSize.height > 0) {
                        const reqId = pdfRender.requestRender(
                            root.currentFrame, root.renderScale, 0,
                            imgSize, 0, tRow, tCol, root.tileCount)
                        if (reqId !== 0) {
                            tileDelegate.pendingRequestId = reqId
                        }
                    }
                }

                Image {
                    id: tileImage
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    asynchronous: false
                    cache: false
                    source: ""
                }

                Connections {
                    target: pdfRender
                    function onRenderFinished(reqId, page, scale, rotation) {
                        if (reqId !== tileDelegate.pendingRequestId) return
                        if (page !== root.currentFrame) return
                        if (Math.abs(scale - root.renderScale) > 0.0001) return
                        // 渲染时 rotation 始终为 0，无需检查

                        tileDelegate.pendingRequestId = 0
                        tileImage.source = "image://pdfcache/" + tileDelegate.tileKey()
                        // 分块模式下第一个块渲染完成即标记 Ready（首块可见=PDF已打开）
                        if (root.status !== Image.Ready) root.status = Image.Ready
                    }
                }

                Component.onCompleted: tileDelegate.refresh()
            }
        }
    }

    // 属性变化时刷新所有分块
    onCurrentFrameChanged: refreshAll()
    onRenderScaleChanged: refreshAll()
    onPageRotationChanged: refreshAll()
    onTileCountChanged: refreshAll()

    function refreshAll() {
        // 切换页面/缩放/旋转/分块模式时重置 status（分块需要重新渲染）
        if (root.document && root.document.status === PdfDocument.Ready
            && root.currentFrame >= 0) {
            root.status = Image.Loading
        } else {
            root.status = Image.Null
        }
        // 注意：必须用 tileRepeater.itemAt(i) 遍历 delegate，
        // 不能用 tileGrid.children——Grid 的直接子项只有 Repeater 对象，
        // 不包含 Repeater 创建的 delegate。
        for (var i = 0; i < tileRepeater.count; i++) {
            var child = tileRepeater.itemAt(i)
            if (child && child.refresh) child.refresh()
        }
    }

    Connections {
        target: root.document
        function onStatusChanged() {
            if (root.document && root.document.status === PdfDocument.Ready)
                root.refreshAll()
        }
    }

    // pdfRender 文档设置完成（此时 document 已 Ready），触发刷新。
    // 这是最可靠的刷新触发点：pdfRender.setDocument 在 ReaderPage 的
    // pdfDocument.onStatusChanged(Ready) 中调用，此时 document 一定是 Ready。
    Connections {
        target: pdfRender
        function onDocumentChanged() {
            root.refreshAll()
        }
    }
}
