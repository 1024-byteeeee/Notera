// Notera 缓存版 PDF 页面图像组件。
// 替换 PdfPageImage：先查 PdfRenderCache，命中直接显示（0ms）；
// miss 则通过 QPdfPageRenderer 异步渲染并写入缓存；
// 缩放时先用 closest 缓存（旧分辨率拉伸）显示，零空白（参考 Sioyek try_closest_rendered_page）。

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

    // 内部跟踪：当前进行中的渲染请求 ID（用于属性变化时取消）
    property var _pendingRequestId: 0

    // 与 C++ PdfRenderCache::makeKey / quantizeScale 保持一致
    function _quantizeScale(s) { return Math.round(s * 10000) }
    function _cacheKey(page, scale, rot) {
        return page + "_" + root._quantizeScale(scale) + "_" + Math.round(rot)
    }
    function _myKey() { return root._cacheKey(root.currentFrame, root.renderScale, root.pageRotation) }

    function _pageImageSize() {
        if (!root.document || root.document.status !== PdfDocument.Ready) return Qt.size(0, 0)
        const ps = root.document.pagePointSize(root.currentFrame)
        const rot = Math.round(root.pageRotation) % 180 !== 0
        const w = rot ? ps.height : ps.width
        const h = rot ? ps.width : ps.height
        return Qt.size(Math.max(1, Math.round(w * root.renderScale * Screen.devicePixelRatio)),
                        Math.max(1, Math.round(h * root.renderScale * Screen.devicePixelRatio)))
    }

    function refresh() {
        if (!root.document || root.document.status !== PdfDocument.Ready
            || root.currentFrame < 0 || root.currentFrame >= root.document.pageCount) {
            display.source = ""
            root.status = Image.Null
            return
        }

        // 取消旧请求（快速滚动/缩放时不再需要的渲染）
        if (root._pendingRequestId !== 0) {
            pdfRender.cancelRequest(root._pendingRequestId)
            root._pendingRequestId = 0
        }

        const myKey = root._myKey()

        // 1. 精确命中：直接显示缓存
        if (pdfRender.hasCache(root.currentFrame, root.renderScale, root.pageRotation)) {
            display.source = "image://pdfcache/" + myKey
            root.status = Image.Ready
            return
        }

        // 2. closest 命中（缩放时先用旧分辨率拉伸显示，零空白）
        const closest = pdfRender.closestCacheKey(root.currentFrame, root.renderScale, root.pageRotation)
        if (closest !== "" && closest !== myKey) {
            display.source = "image://pdfcache/" + closest
            root.status = Image.Ready // 临时显示，后台渲染新分辨率
        } else {
            display.source = ""
            root.status = Image.Loading
        }

        // 3. 请求渲染新分辨率
        const imgSize = root._pageImageSize()
        if (imgSize.width > 0 && imgSize.height > 0) {
            const reqId = pdfRender.requestRender(
                root.currentFrame, root.renderScale, root.pageRotation, imgSize)
            if (reqId !== 0) {
                root._pendingRequestId = reqId
            }
        }
    }

    Image {
        id: display
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        asynchronous: false
        cache: false // 不经过 QQuickPixmap 缓存，直接从 provider 取（缓存已在 PdfRenderCache）
        source: ""
    }

    Connections {
        target: pdfRender
        function onRenderFinished(reqId, page, scale, rotation) {
            // 匹配到当前页/缩放/旋转才更新显示
            if (page !== root.currentFrame) return
            if (Math.abs(scale - root.renderScale) > 0.0001) return
            if (Math.abs(rotation - root.pageRotation) > 0.5) return

            if (root._pendingRequestId === reqId) {
                root._pendingRequestId = 0
            }
            display.source = "image://pdfcache/" + root._myKey()
            root.status = Image.Ready
        }
    }

    Component.onCompleted: root.refresh()
    onCurrentFrameChanged: root.refresh()
    onRenderScaleChanged: root.refresh()
    onPageRotationChanged: root.refresh()

    Connections {
        target: root.document
        function onStatusChanged() {
            if (root.document && root.document.status === PdfDocument.Ready)
                root.refresh()
        }
    }
}
