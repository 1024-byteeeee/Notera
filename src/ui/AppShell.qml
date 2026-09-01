import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera
import "components"
import "pages"

Item {
    id: root

    function pageIndex(page) {
        return page === "library" ? 0 : page === "reader" ? 1 : 2
    }

    function switchPage(page) {
        const nextIndex = pageIndex(page)
        if (nextIndex === pageStack.currentIndex) return
        pageTransition.stop()
        pageStack.currentIndex = nextIndex
        if (Motion.enabled) {
            pageStack.opacity = 0.72
            pageTransition.start()
        } else {
            pageStack.opacity = 1
        }
    }

    Connections {
        target: appController
        function onCurrentPageChanged() { root.switchPage(appController.currentPage) }
    }
    Connections {
        target: libraryService
        function onErrorOccurred(message) { toast.show(message, false) }
        function onNoticeOccurred(message) { toast.show(message, true) }
    }

    Sidebar {
        id: sidebar
        visible: appController.currentPage !== "reader"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
    }

    StackLayout {
        id: pageStack
        anchors.left: appController.currentPage === "reader" ? parent.left : sidebar.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        currentIndex: 0

        LibraryPage { }
        ReaderPage { }
        SettingsPage { }
    }

    NumberAnimation {
        id: pageTransition
        target: pageStack
        property: "opacity"
        to: 1
        duration: Motion.normal
        easing.type: Easing.OutCubic
    }

    // Toast 通知
    Rectangle {
        id: toast
        property alias text: toastLabel.text
        property bool isSuccess: true

        function show(message, success) {
            text = message
            isSuccess = success
            opacity = 1
            visible = true
            hideTimer.restart()
        }

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 32
        visible: false
        opacity: 0
        color: isSuccess ? Theme.elevatedSurface : Theme.dangerSoft
        border.color: isSuccess ? Theme.strongBorder : Theme.danger
        border.width: 1
        radius: Theme.radiusMd
        implicitWidth: Math.min(toastLabel.implicitWidth + 44, 420)
        implicitHeight: toastLabel.implicitHeight + 22
        z: 100

        Behavior on opacity { NumberAnimation { duration: Motion.normal } }

        Label {
            id: toastLabel
            anchors.centerIn: parent
            color: toast.isSuccess ? Theme.foreground : Theme.danger
            font.pixelSize: Theme.fontMd
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            width: parent.width - 36
        }

        Timer {
            id: hideTimer
            interval: 2800
            onTriggered: {
                toast.opacity = 0
                hideCompleteTimer.start()
            }
        }
        Timer {
            id: hideCompleteTimer
            interval: 220
            onTriggered: toast.visible = false
        }
    }
}
