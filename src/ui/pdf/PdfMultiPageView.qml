import QtQuick
import QtQuick.Controls
import QtQuick.Pdf

Item {
    id: root

    required property PdfDocument document

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

    signal viewMovementStarted

    signal ctrlWheelZoomRequested(real deltaY, real viewportX, real viewportY)

    readonly property int pageCount: root.document ? root.document.pageCount : 0

    function goToPage(page) {
        if (page < 0 || page >= root.pageCount)
            return;
        if (page === pageNavigator.currentPage)
            return;

        tableView.positionViewAtRow(page, TableView.AlignTop);

        pageNavigator.update(page, Qt.point(-1, -1), root.renderScale);
    }

    function resetView() {
        tableView.contentX = 0;
        tableView.contentY = 0;
    }

    function cancelFlick() {
        tableView.cancelFlick();
    }

    TableView {
        id: tableView
        anchors.fill: parent

        model: root.pageCount
        rowSpacing: root.rowSpacing
        topMargin: root.topMargin
        bottomMargin: root.bottomMargin
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        onMovementStarted: {
            root.viewMovementStarted();
            prefetchTimer.stop();
            pdfRender.cancelLowPriority();
        }
        onMovementEnded: prefetchTimer.restart()

        property real rotationNorm: Math.round((360 + (root.pageRotation % 360)) % 360)
        property bool rot90: rotationNorm == 90 || rotationNorm == 270
        onRot90Changed: forceLayout()

        property size firstPagePointSize: root.document && root.document.status === PdfDocument.Ready ? root.document.pagePointSize(0) : Qt.size(1, 1)
        columnWidthProvider: function (col) {
            if (!root.document)
                return 0;
            const maxW = rot90 ? root.document.maxPageHeight : root.document.maxPageWidth;
            return Math.max(root.width, maxW * root.renderScale);
        }
        rowHeightProvider: function (row) {
            const s = root.document ? root.document.pagePointSize(row) : Qt.size(1, 1);
            return (rot90 ? s.width : s.height) * root.renderScale;
        }

        onRowsChanged: {
            if (rows > 0 && pageNavigator.currentPage < 0)
                pageNavigator.update(0, Qt.point(-1, -1), 1);
        }

        delegate: Rectangle {
            id: pageHolder
            required property int index
            color: "transparent"

            Rectangle {
                id: paper
                width: image.width
                height: image.height

                x: Math.max(0, (pageHolder.width - width) / 2)
                y: Math.max(0, (pageHolder.height - height) / 2)
                rotation: root.pageRotation
                color: "white"
                radius: 2
                border.color: "#D5D4CC"
                border.width: 1
                property size pagePointSize: root.document ? root.document.pagePointSize(pageHolder.index) : Qt.size(1, 1)

                CachedPdfPageImage {
                    id: image
                    objectName: "pdfPageImageItem"
                    document: root.document
                    currentFrame: pageHolder.index
                    renderScale: root.renderScale
                    pageRotation: root.pageRotation

                    tileCount: 1

                    width: paper.pagePointSize.width * root.renderScale
                    height: paper.pagePointSize.height * root.renderScale
                    onStatusChanged: {
                        if (pageHolder.index === root.currentPage || (root.currentPage < 0 && pageHolder.index === 0)) {
                            root.currentPageRenderingStatus = status;
                        }
                    }
                }
            }
        }

        ScrollBar.vertical: ScrollBar {
            id: vscroll
        }
        ScrollBar.horizontal: ScrollBar {}

        WheelHandler {
            target: null
            objectName: "pdfWheelCtrl"
            acceptedModifiers: Qt.ControlModifier
            onWheel: function (wheel) {
                root.ctrlWheelZoomRequested(wheel.angleDelta.y, wheel.x, wheel.y);
            }
        }
        WheelHandler {
            target: null
            objectName: "pdfWheelMeta"
            acceptedModifiers: Qt.MetaModifier
            onWheel: function (wheel) {
                root.ctrlWheelZoomRequested(wheel.angleDelta.y, wheel.x, wheel.y);
            }
        }

        onContentYChanged: {
            if (pageNavigator.currentPage >= 0 && !syncTimer.running)
                syncTimer.start();
        }
    }

    Timer {
        id: syncTimer
        interval: 100
        repeat: false
        onTriggered: {
            if (pageNavigator.currentPage < 0)
                return;
            const cell = tableView.cellAtPos(root.width / 2, root.height / 2);
            if (cell.y >= 0 && cell.y !== pageNavigator.currentPage)
                pageNavigator.update(cell.y, Qt.point(-1, -1), root.renderScale);

            prefetchTimer.restart();
        }
    }

    Timer {
        id: prefetchTimer
        interval: 150
        repeat: false
        onTriggered: {
            if (!root.document || root.document.status !== PdfDocument.Ready)
                return;
            const cur = root.currentPage;
            if (cur < 0)
                return;
            const pageCount = root.document.pageCount;
            const rot90 = Math.round(root.pageRotation) % 180 !== 0;

            const offsets = [1, -1, 2, -2];
            for (const off of offsets) {
                const p = cur + off;
                if (p < 0 || p >= pageCount)
                    continue;
                if (pdfRender.hasCache(p, root.renderScale, root.pageRotation))
                    continue;
                const ps = root.document.pagePointSize(p);
                const w = rot90 ? ps.height : ps.width;
                const h = rot90 ? ps.width : ps.height;
                pdfRender.requestRender(p, root.renderScale, root.pageRotation, Qt.size(Math.max(1, Math.round(w * root.renderScale * Screen.devicePixelRatio)), Math.max(1, Math.round(h * root.renderScale * Screen.devicePixelRatio))), 1);
            }
        }
    }

    onRenderScaleChanged: {
        if (pageNavigator.jumping)
            return;

        const savedContentY = tableView.contentY;
        tableView.model = 0;
        tableView.model = Qt.binding(function () {
            return root.pageCount;
        });
        tableView.contentY = savedContentY;

        if (pageNavigator.currentPage < 0)
            return;
        const cell = tableView.cellAtPos(root.width / 2, root.height / 2);
        const currentItem = cell.x >= 0 ? tableView.itemAtCell(cell) : null;
        if (currentItem) {
            const currentLocation = Qt.point(tableView.contentX - currentItem.x, tableView.contentY - currentItem.y);
            pageNavigator.update(cell.y, currentLocation, renderScale);
        }

        prefetchTimer.restart();
    }

    PdfPageNavigator {
        id: pageNavigator
        property bool jumping: false
        property int previousPage: 0
        onJumped: function (current) {
            jumping = true;

            if (current.location.y < 0) {
                const previousPageDelegate = tableView.itemAtCell(0, previousPage);
                const currentYOffset = previousPageDelegate ? tableView.contentY - previousPageDelegate.y : 0;
                tableView.positionViewAtRow(current.page, Qt.AlignTop, currentYOffset);
            } else {
                const pageSize = root.document.pagePointSize(current.page);
                const rectPx = Qt.rect(current.location.x * root.renderScale, current.location.y * root.renderScale, 1, 1);
                tableView.positionViewAtCell(0, current.page, TableView.AlignLeft | TableView.AlignTop, Qt.point(0, 0), rectPx);
            }
            jumping = false;
            previousPage = current.page;
        }

        property url documentSource: root.document ? root.document.source : ""
        onDocumentSourceChanged: {
            pageNavigator.clear();
            root.resetView();
        }
    }
}
