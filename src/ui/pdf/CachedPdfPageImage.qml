








import QtQuick
import QtQuick.Pdf
import Notera

Item {
    id: root


    required property PdfDocument document


    required property int currentFrame


    property real renderScale: 1


    property real pageRotation: 0


    property int status: Image.Null



    property int tileCount: 1


    function _quantizeScale(s) { return Math.round(s * 10000) }




    function _cacheKey(page, scale, tileRow, tileCol) {
        var key = page + "_" + root._quantizeScale(scale) + "_0"
        if (tileRow !== undefined && tileRow >= 0 && tileCol !== undefined && tileCol >= 0)
            key += "_" + tileRow + "_" + tileCol
        return key
    }
    function _pageImageSize() {
        if (!root.document || root.document.status !== PdfDocument.Ready) return Qt.size(0, 0)
        const ps = root.document.pagePointSize(root.currentFrame)

        return Qt.size(Math.max(1, Math.round(ps.width * root.renderScale * Screen.devicePixelRatio)),
                        Math.max(1, Math.round(ps.height * root.renderScale * Screen.devicePixelRatio)))
    }


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


                    if (tileDelegate.pendingRequestId !== 0) {
                        pdfRender.cancelRequest(tileDelegate.pendingRequestId)
                        tileDelegate.pendingRequestId = 0
                    }

                    const tRow = isWholePage ? -1 : tileRow
                    const tCol = isWholePage ? -1 : tileCol


                    if (pdfRender.hasCache(root.currentFrame, root.renderScale,
                                           0, tRow, tCol)) {
                        tileImage.source = "image://pdfcache/" + tileDelegate.tileKey()

                        if (root.status !== Image.Ready) root.status = Image.Ready
                        return
                    }


                    if (isWholePage) {
                        const closest = pdfRender.closestCacheKey(
                            root.currentFrame, root.renderScale, 0)
                        if (closest !== "" && closest !== tileDelegate.tileKey()) {
                            tileImage.source = "image://pdfcache/" + closest
                            root.status = Image.Ready
                        } else {
                            tileImage.source = ""
                            root.status = Image.Loading
                        }
                    } else {

                        tileImage.source = ""
                        if (root.status !== Image.Ready) root.status = Image.Loading
                    }


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


                        tileDelegate.pendingRequestId = 0
                        tileImage.source = "image://pdfcache/" + tileDelegate.tileKey()

                        if (root.status !== Image.Ready) root.status = Image.Ready
                    }
                }

                Component.onCompleted: tileDelegate.refresh()
            }
        }
    }


    onCurrentFrameChanged: refreshAll()
    onRenderScaleChanged: refreshAll()
    onPageRotationChanged: refreshAll()
    onTileCountChanged: refreshAll()

    function refreshAll() {

        if (root.document && root.document.status === PdfDocument.Ready
            && root.currentFrame >= 0) {
            root.status = Image.Loading
        } else {
            root.status = Image.Null
        }



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




    Connections {
        target: pdfRender
        function onDocumentChanged() {
            root.refreshAll()
        }
    }
}
