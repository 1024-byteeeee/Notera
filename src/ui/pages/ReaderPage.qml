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
    property real scrollAccumulator: 0
    property real zoomLevel: 1.0
    property int viewRotation: 0
    property real pinchBaseZoom: 1.0
    property real pinchAnchorX: 0.5
    property real pinchAnchorY: 0.5
    property real pinchViewportX: 0
    property real pinchViewportY: 0
    property bool viewInitializationPending: false
    property int viewInitializationToken: 0
    property var folderScores: []
    readonly property int currentScoreIndex: {
        for (let i = 0; i < root.folderScores.length; i++) {
            if (root.folderScores[i].id === appController.currentScoreId) return i
        }
        return -1
    }
    readonly property bool hasPrev: root.currentScoreIndex > 0
    readonly property bool hasNext: root.currentScoreIndex >= 0 && root.currentScoreIndex < root.folderScores.length - 1

    function refreshFolderScores() {
        root.folderScores = libraryService.scoresInFolder(appController.currentScoreFolderId)
    }
    function goToPrevScore() {
        if (!root.hasPrev) return
        const s = root.folderScores[root.currentScoreIndex - 1]
        appController.openScore(s.id, s.title, s.filePath, s.fileType, s.pageCount, s.folderId)
    }
    function goToNextScore() {
        if (!root.hasNext) return
        const s = root.folderScores[root.currentScoreIndex + 1]
        appController.openScore(s.id, s.title, s.filePath, s.fileType, s.pageCount, s.folderId)
    }

    Connections {
        target: appController
        function onCurrentScoreChanged() { root.refreshFolderScores() }
    }

    Keys.onLeftPressed: function(event) {
        if (event.modifiers & Qt.ControlModifier) return
        root.goToPrevScore()
    }
    Keys.onRightPressed: function(event) {
        if (event.modifiers & Qt.ControlModifier) return
        root.goToNextScore()
    }
    focus: appController.currentPage === "reader"

    function clampScroll(value, contentSize, viewportSize) {
        return Math.max(0, Math.min(value, Math.max(0, contentSize - viewportSize)))
    }

    function restoreAnchor(normalizedX, normalizedY, viewportX, viewportY) {
        readerFlick.contentX = clampScroll(normalizedX * readerFlick.contentWidth - viewportX,
            readerFlick.contentWidth, readerFlick.width)
        readerFlick.contentY = clampScroll(normalizedY * readerFlick.contentHeight - viewportY,
            readerFlick.contentHeight, readerFlick.height)
    }

    function zoomAroundViewport(newZoom, viewportX, viewportY) {
        const safeWidth = Math.max(readerFlick.width, readerFlick.contentWidth)
        const safeHeight = Math.max(readerFlick.height, readerFlick.contentHeight)
        const normalizedX = (readerFlick.contentX + viewportX) / safeWidth
        const normalizedY = (readerFlick.contentY + viewportY) / safeHeight
        root.zoomLevel = Math.max(0.4, Math.min(3.0, newZoom))
        Qt.callLater(function() {
            root.restoreAnchor(normalizedX, normalizedY, viewportX, viewportY)
        })
    }

    function applyDefaultView(initializationToken) {
        root.zoomLevel = 1.0
        Qt.callLater(function() {
            if (initializationToken !== undefined
                && (!root.viewInitializationPending || initializationToken !== root.viewInitializationToken)) {
                return
            }
            readerFlick.contentX = root.clampScroll((readerFlick.contentWidth - readerFlick.width) / 2,
                readerFlick.contentWidth, readerFlick.width)
            readerFlick.contentY = 0
            if (initializationToken !== undefined) {
                root.viewInitializationPending = false
            }
        })
    }

    function beginViewInitialization() {
        root.viewInitializationToken += 1
        root.viewInitializationPending = true
        root.autoScrolling = false
        readerFlick.cancelFlick()
        readerFlick.rotation = 0
        readerFlick.scale = 1
        root.viewRotation = 0
        root.zoomLevel = 1.0
        readerFlick.contentX = 0
        readerFlick.contentY = 0
        root.finishInitialViewIfReady(root.viewInitializationToken)
    }

    function finishInitialViewIfReady(token) {
        const expectedToken = token === undefined ? root.viewInitializationToken : token
        if (!root.viewInitializationPending || expectedToken !== root.viewInitializationToken
            || appController.currentPage !== "reader") {
            return
        }
        const contentReady = root.isImage ? scoreImage.status === Image.Ready
            : root.isPdf ? pdfDocument.pageCount > 0 : true
        if (!contentReady || readerFlick.width <= 0 || readerFlick.height <= 0) {
            return
        }
        root.applyDefaultView(expectedToken)
    }

    function markUserInteraction() {
        root.viewInitializationPending = false
    }

    function resetZoom() { root.markUserInteraction(); applyDefaultView() }
    function resetReaderView() {
        root.markUserInteraction()
        root.autoScrolling = false
        readerFlick.cancelFlick()
        readerFlick.rotation = 0
        readerFlick.scale = 1
        root.viewRotation = 0
        root.applyDefaultView()
    }
    function rotateBy(delta) {
        root.markUserInteraction()
        const normalizedX = (readerFlick.contentX + readerFlick.width / 2)
            / Math.max(readerFlick.width, readerFlick.contentWidth)
        const normalizedY = (readerFlick.contentY + readerFlick.height / 2)
            / Math.max(readerFlick.height, readerFlick.contentHeight)
        root.viewRotation = (root.viewRotation + delta + 360) % 360
        Qt.callLater(function() {
            root.restoreAnchor(normalizedX, normalizedY, readerFlick.width / 2, readerFlick.height / 2)
        })
    }
    function rotateLeft() { rotateBy(-90) }
    function rotateRight() { rotateBy(90) }
    function zoomIn() {
        root.markUserInteraction()
        zoomAroundViewport(root.zoomLevel + 0.25, readerFlick.width / 2, readerFlick.height / 2)
    }
    function zoomOut() {
        root.markUserInteraction()
        zoomAroundViewport(root.zoomLevel - 0.25, readerFlick.width / 2, readerFlick.height / 2)
    }

    function stopAtEnd() {
        const maximum = Math.max(0, readerFlick.contentHeight - readerFlick.height)
        if (readerFlick.contentY >= maximum) {
            readerFlick.contentY = maximum
            autoScrolling = false
        }
    }

    component ToolButton: Rectangle {
        required property string btnText
        property string iconName: ""
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
        opacity: btnEnabled ? 1 : 0.4

        Row {
            anchors.centerIn: parent
            spacing: 6
            scale: btnMouse.pressed ? 0.96 : 1
            Behavior on scale { NumberAnimation { duration: Motion.fast; easing.type: Easing.OutCubic } }
            AppIcon {
                visible: parent.parent.iconName.length > 0
                width: 15
                height: 15
                iconName: parent.parent.iconName
                iconColor: !parent.parent.btnEnabled ? Theme.buttonDisabledText
                    : parent.parent.btnActive ? Theme.selectedText : Theme.buttonText
            }
            Label {
                text: parent.parent.btnText
                color: !parent.parent.btnEnabled ? Theme.buttonDisabledText
                    : parent.parent.btnActive ? Theme.selectedText : Theme.buttonText
                font.pixelSize: Theme.fontSm
                font.weight: parent.parent.btnActive ? Font.DemiBold : Font.Medium
            }
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

                ToolButton {
                    btnText: "乐谱库"
                    iconName: "back"
                    Layout.preferredWidth: 100
                    onBtnClicked: { root.autoScrolling = false; appController.currentPage = "library" }
                }

                ToolButton {
                    objectName: "prevScoreButton"
                    btnText: "上一张"
                    iconName: "previous"
                    btnEnabled: root.hasPrev
                    Layout.preferredWidth: 84
                    onBtnClicked: root.goToPrevScore()
                }

                ToolButton {
                    objectName: "nextScoreButton"
                    btnText: "下一张"
                    iconName: "next"
                    btnEnabled: root.hasNext
                    Layout.preferredWidth: 84
                    onBtnClicked: root.goToNextScore()
                }

                Label {
                    Layout.fillWidth: true
                    text: appController.currentScoreTitle.length > 0 ? appController.currentScoreTitle : "阅读器"
                    color: Theme.foreground
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }

                ToolButton {
                    objectName: "metronomeButton"
                    btnText: "节拍器"
                    iconName: "metronome"
                    btnActive: metronome.running
                    Layout.preferredWidth: 96
                    onBtnClicked: metronome.toggle()
                }

                ToolButton {
                    objectName: "metronomeSettingsButton"
                    btnText: ""
                    iconName: "settings"
                    Layout.preferredWidth: 42
                    onBtnClicked: metronomePanel.open()
                }

                ToolButton {
                    btnText: root.autoScrolling ? "暂停滚动" : "自动滚动"
                    iconName: root.autoScrolling ? "pause" : "play"
                    btnEnabled: root.isPdf || root.isImage
                    btnActive: root.autoScrolling
                    Layout.preferredWidth: 118
                    onBtnClicked: root.autoScrolling = !root.autoScrolling
                }

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
                            from: 1
                            to: 256
                            value: root.scrollSpeed
                            stepSize: 1
                            onMoved: appController.autoScrollSpeed = value

                            background: Rectangle {
                                x: speedSlider.leftPadding
                                y: speedSlider.topPadding + speedSlider.availableHeight / 2 - height / 2
                                width: speedSlider.availableWidth
                                height: 4
                                radius: 2
                                color: Theme.sunkenSurface
                                Rectangle {
                                    width: speedSlider.visualPosition * parent.width
                                    height: parent.height
                                    radius: 2
                                    color: Theme.accent
                                }
                            }
                            handle: Rectangle {
                                x: speedSlider.leftPadding + speedSlider.visualPosition * (speedSlider.availableWidth - width)
                                y: speedSlider.topPadding + speedSlider.availableHeight / 2 - height / 2
                                implicitWidth: 16
                                implicitHeight: 16
                                radius: 8
                                color: Theme.surface
                                border.width: 2
                                border.color: Theme.accent
                            }
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

                ToolButton {
                    objectName: "rotateLeftButton"
                    btnText: ""
                    iconName: "rotate-left"
                    btnEnabled: root.isPdf || root.isImage
                    Layout.preferredWidth: 42
                    onBtnClicked: root.rotateLeft()
                }

                ToolButton {
                    objectName: "rotateRightButton"
                    btnText: ""
                    iconName: "rotate-right"
                    btnEnabled: root.isPdf || root.isImage
                    Layout.preferredWidth: 42
                    onBtnClicked: root.rotateRight()
                }

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

                        Rectangle {
                            Layout.preferredWidth: 30
                            height: 28
                            radius: 6
                            color: zoomOutMouse.containsMouse ? Theme.buttonHover : "transparent"
                            AppIcon { anchors.centerIn: parent; width: 16; height: 16; iconName: "minus"; iconColor: Theme.buttonText }
                            MouseArea {
                                id: zoomOutMouse
                                anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.zoomOut()
                            }
                        }

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

                        Rectangle {
                            Layout.preferredWidth: 30
                            height: 28
                            radius: 6
                            color: zoomInMouse.containsMouse ? Theme.buttonHover : "transparent"
                            AppIcon { anchors.centerIn: parent; width: 16; height: 16; iconName: "plus"; iconColor: Theme.buttonText }
                            MouseArea {
                                id: zoomInMouse
                                anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.zoomIn()
                            }
                        }
                    }
                }

                ToolButton {
                    objectName: "resetReaderButton"
                    btnText: "重置"
                    iconName: "reset"
                    btnEnabled: root.isPdf || root.isImage
                    Layout.preferredWidth: 68
                    onBtnClicked: root.resetReaderView()
                }
            }
        }

        Flickable {
            id: readerFlick
            objectName: "readerFlick"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: documentColumn.width
            contentHeight: documentColumn.height
            boundsBehavior: Flickable.StopAtBounds
            pixelAligned: true
            onWidthChanged: root.finishInitialViewIfReady()
            onHeightChanged: root.finishInitialViewIfReady()
            onMovementStarted: root.markUserInteraction()

            WheelHandler {
                acceptedModifiers: Qt.NoModifier
                onWheel: function(event) {
                    root.markUserInteraction()
                    const delta = event.pixelDelta.y !== 0 ? event.pixelDelta.y : event.angleDelta.y
                    readerFlick.contentY = root.clampScroll(readerFlick.contentY - delta,
                        readerFlick.contentHeight, readerFlick.height)
                    event.accepted = true
                }
            }

            PinchHandler {
                id: pinchZoom
                target: null
                onActiveChanged: {
                    if (pinchZoom.active) {
                        root.markUserInteraction()
                        root.pinchBaseZoom = root.zoomLevel
                        root.pinchViewportX = centroid.position.x
                        root.pinchViewportY = centroid.position.y
                        root.pinchAnchorX = (readerFlick.contentX + root.pinchViewportX)
                            / Math.max(readerFlick.width, readerFlick.contentWidth)
                        root.pinchAnchorY = (readerFlick.contentY + root.pinchViewportY)
                            / Math.max(readerFlick.height, readerFlick.contentHeight)
                    }
                }
                onActiveScaleChanged: {
                    root.zoomLevel = Math.max(0.4, Math.min(3.0,
                        root.pinchBaseZoom * pinchZoom.activeScale))
                    Qt.callLater(function() {
                        root.restoreAnchor(root.pinchAnchorX, root.pinchAnchorY,
                            root.pinchViewportX, root.pinchViewportY)
                    })
                }
            }

            Column {
                id: documentColumn
                readonly property real maxPageWidth: root.isPdf ? 1100 : 1400
                readonly property real basePageWidth: Math.min(readerFlick.width - 48, maxPageWidth)
                readonly property real pageWidth: basePageWidth * root.zoomLevel
                width: Math.max(readerFlick.width, pageWidth + 48)
                spacing: 20
                topPadding: 24
                bottomPadding: 24

                PdfDocument {
                    id: pdfDocument
                    source: root.isPdf ? appController.currentFileUrl : ""
                    onPageCountChanged: root.finishInitialViewIfReady()
                }

                Repeater {
                    model: root.isPdf ? pdfDocument.pageCount : 0
                    delegate: Item {
                        required property int index
                        readonly property size pageSize: pdfDocument.pagePointSize(index)
                        readonly property bool rotated: root.viewRotation % 180 !== 0
                        readonly property real pageRatio: pageSize.height > 0 ? pageSize.width / pageSize.height : 0.7
                        width: documentColumn.pageWidth
                        height: rotated ? width * pageRatio : width / pageRatio
                        x: (documentColumn.width - width) / 2

                        // 懒加载窗口：页面与视口（含预加载余量）相交时才实例化
                        // PdfPageImage。多页 PDF 不再一次性创建/渲染全部页面，
                        // 滚动进出视野时按需渲染（PdfDocument 内部按页缓存渲染结果）。
                        readonly property bool nearViewport: {
                            if (y <= 0 && index < 2) return true   // 布局未完成前先让首页进入
                            const top = y + documentColumn.y
                            const viewTop = readerFlick.contentY - 400
                            const viewBottom = readerFlick.contentY + readerFlick.height + 400
                            return top < viewBottom && top + height > viewTop
                        }

                        Loader {
                            id: pdfPageLoader
                            objectName: "pdfPageLoader"
                            anchors.fill: parent
                            active: nearViewport
                            sourceComponent: Rectangle {
                                width: rotated ? parent.height : parent.width
                                height: rotated ? parent.width : parent.height
                                anchors.centerIn: parent
                                rotation: root.viewRotation
                                color: "white"
                                radius: Theme.radiusSm
                                border.color: Theme.border
                                border.width: 1

                                PdfPageImage {
                                    objectName: "pdfPageImageItem"
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
                    }
                }

                Item {
                    visible: root.isImage
                    width: visible ? documentColumn.pageWidth : 0
                    readonly property bool rotated: root.viewRotation % 180 !== 0
                    readonly property real imageRatio: scoreImage.sourceSize.height > 0
                        ? scoreImage.sourceSize.width / scoreImage.sourceSize.height : 0.7
                    height: visible ? (rotated ? width * imageRatio : width / imageRatio) : 0
                    x: (documentColumn.width - width) / 2

                    Image {
                        id: scoreImage
                        width: parent.rotated ? parent.height : parent.width
                        height: parent.rotated ? parent.width : parent.height
                        anchors.centerIn: parent
                        rotation: root.viewRotation
                        source: root.isImage ? appController.currentFileUrl : ""
                        asynchronous: true
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        onStatusChanged: root.finishInitialViewIfReady()
                    }
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

            // PDF 解析加载指示：QPdfDocument 对多页/复杂 PDF 的解析是后台任务，
            // 解析完成（status=Ready）前页面为空，给出明确提示避免用户误以为卡死。
            Rectangle {
                objectName: "pdfLoadingOverlay"
                visible: root.isPdf && pdfDocument.status !== 2 && pdfDocument.status !== 4
                    && appController.currentFileUrl.toString().length > 0
                anchors.fill: parent
                z: 20
                color: Theme.background

                Column {
                    anchors.centerIn: parent
                    spacing: 14
                    BusyIndicator {
                        anchors.horizontalCenter: parent.horizontalCenter
                        running: true
                        width: 36
                        height: 36
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "正在打开 PDF…"
                        color: Theme.mutedForeground
                        font.pixelSize: Theme.fontMd
                    }
                }
            }
        }

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
            root.scrollAccumulator += root.scrollSpeed * interval / 1000
            const step = Math.floor(root.scrollAccumulator)
            if (step > 0) {
                readerFlick.contentY += step
                root.scrollAccumulator -= step
            }
            root.stopAtEnd()
        }
    }

    Connections {
        target: appController
        function onCurrentScoreChanged() {
            root.beginViewInitialization()
        }
        function onCurrentPageChanged() {
            if (appController.currentPage === "reader") {
                root.finishInitialViewIfReady()
            } else {
                metronome.stop()
            }
        }
    }

    Popup {
        id: metronomePanel
        objectName: "metronomePanel"
        width: 300
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        property int flashBeat: -1
        property var noteValues: [1, 2, 4, 8, 16, 32]

        function nextNoteValue(current) {
            const idx = noteValues.indexOf(current)
            return idx >= 0 && idx < noteValues.length - 1 ? noteValues[idx + 1] : current
        }
        function prevNoteValue(current) {
            const idx = noteValues.indexOf(current)
            return idx > 0 ? noteValues[idx - 1] : current
        }

        onAboutToShow: {
            const btn = metronomeSettingsButton
            if (btn) {
                const pos = btn.mapToItem(root, 0, btn.height + 8)
                x = Math.min(pos.x, root.width - width - 8)
                y = pos.y
            }
        }

        background: Rectangle {
            color: Theme.surface
            border.color: Theme.border
            border.width: 1
            radius: Theme.radiusLg
        }

        contentItem: Item {
            implicitWidth: columnLayout.implicitWidth
            implicitHeight: columnLayout.implicitHeight

            ColumnLayout {
                id: columnLayout
                anchors.fill: parent
                anchors.margins: Theme.spacingMd
                spacing: Theme.spacingMd

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm

                Rectangle {
                    id: beatIndicator
                    Layout.preferredWidth: 12
                    Layout.preferredHeight: 12
                    radius: 6
                    color: metronomePanel.flashBeat === 0 ? Theme.accent
                         : metronomePanel.flashBeat >= 0 ? Theme.strongBorder
                         : Theme.sunkenSurface
                    Behavior on color { ColorAnimation { duration: 60 } }
                }

                AppButton {
                    Layout.fillWidth: true
                    primary: true
                    text: metronome.running ? "暂停节拍器" : "启动节拍器"
                    onClicked: metronome.toggle()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingXs

                Label {
                    text: "BPM"
                    color: Theme.mutedForeground
                    font.pixelSize: Theme.fontSm
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingXs

                    AppButton {
                        text: "−"
                        Layout.preferredWidth: 36
                        onClicked: metronome.bpm = Math.max(1, metronome.bpm - 1)
                    }

                    Label {
                        Layout.fillWidth: true
                        text: metronome.bpm
                        color: Theme.foreground
                        font.pixelSize: Theme.font2xl
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                    }

                    AppButton {
                        text: "+"
                        Layout.preferredWidth: 36
                        onClicked: metronome.bpm = Math.min(500, metronome.bpm + 1)
                    }
                }

                Slider {
                    id: bpmSlider
                    Layout.fillWidth: true
                    from: 1
                    to: 500
                    stepSize: 1
                    value: metronome.bpm
                    onMoved: metronome.bpm = value

                    background: Rectangle {
                        x: bpmSlider.leftPadding
                        y: bpmSlider.topPadding + bpmSlider.availableHeight / 2 - height / 2
                        width: bpmSlider.availableWidth
                        height: 4
                        radius: 2
                        color: Theme.sunkenSurface
                        Rectangle {
                            width: bpmSlider.visualPosition * parent.width
                            height: parent.height
                            radius: 2
                            color: Theme.accent
                        }
                    }
                    handle: Rectangle {
                        x: bpmSlider.leftPadding + bpmSlider.visualPosition * (bpmSlider.availableWidth - width)
                        y: bpmSlider.topPadding + bpmSlider.availableHeight / 2 - height / 2
                        implicitWidth: 16
                        implicitHeight: 16
                        radius: 8
                        color: Theme.surface
                        border.width: 2
                        border.color: Theme.accent
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingXs

                Label {
                    text: "拍号"
                    color: Theme.mutedForeground
                    font.pixelSize: Theme.fontSm
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingXs

                    Label {
                        Layout.preferredWidth: 36
                        text: "节拍"
                        color: Theme.mutedForeground
                        font.pixelSize: Theme.fontXs
                    }

                    AppButton {
                        text: "−"
                        Layout.preferredWidth: 36
                        onClicked: metronome.beatsPerMeasure = Math.max(1, metronome.beatsPerMeasure - 1)
                    }

                    Label {
                        Layout.fillWidth: true
                        text: metronome.beatsPerMeasure
                        color: Theme.foreground
                        font.pixelSize: Theme.fontXl
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }

                    AppButton {
                        text: "+"
                        Layout.preferredWidth: 36
                        onClicked: metronome.beatsPerMeasure = Math.min(16, metronome.beatsPerMeasure + 1)
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    Layout.leftMargin: 36
                    color: Theme.border
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingXs

                    Label {
                        Layout.preferredWidth: 36
                        text: "音符"
                        color: Theme.mutedForeground
                        font.pixelSize: Theme.fontXs
                    }

                    AppButton {
                        text: "−"
                        Layout.preferredWidth: 36
                        onClicked: metronome.beatUnit = metronomePanel.prevNoteValue(metronome.beatUnit)
                    }

                    Label {
                        Layout.fillWidth: true
                        text: metronome.beatUnit
                        color: Theme.foreground
                        font.pixelSize: Theme.fontXl
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }

                    AppButton {
                        text: "+"
                        Layout.preferredWidth: 36
                        onClicked: metronome.beatUnit = metronomePanel.nextNoteValue(metronome.beatUnit)
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingXs

                Label {
                    text: "音量"
                    color: Theme.mutedForeground
                    font.pixelSize: Theme.fontSm
                }

                Slider {
                    id: volumeSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 1
                    stepSize: 0.01
                    value: metronome.volume
                    onMoved: metronome.volume = value

                    background: Rectangle {
                        x: volumeSlider.leftPadding
                        y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                        width: volumeSlider.availableWidth
                        height: 4
                        radius: 2
                        color: Theme.sunkenSurface
                        Rectangle {
                            width: volumeSlider.visualPosition * parent.width
                            height: parent.height
                            radius: 2
                            color: Theme.accent
                        }
                    }
                    handle: Rectangle {
                        x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth - width)
                        y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                        implicitWidth: 16
                        implicitHeight: 16
                        radius: 8
                        color: Theme.surface
                        border.width: 2
                        border.color: Theme.accent
                    }
                }
            }
            }
        }
    }

    Connections {
        target: metronome
        function onBeat(beatIndex) {
            metronomePanel.flashBeat = beatIndex
            beatFlashTimer.restart()
        }
    }

    Timer {
        id: beatFlashTimer
        interval: 90
        onTriggered: metronomePanel.flashBeat = -1
    }
}
