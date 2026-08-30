import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    Connections {
        target: libraryService
        function onErrorOccurred(message) { toast.show(message) }
        function onNoticeOccurred(message) { toast.show(message) }
    }

    Sidebar {
        id: sidebar
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        onPageSelected: appController.currentPage = page
    }

    StackLayout {
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        currentIndex: appController.currentPage === "library" ? 0
                    : appController.currentPage === "reader" ? 1 : 2

        LibraryPage { }
        ReaderPage { }
        SettingsPage { }
    }

    Rectangle {
        id: toast
        property alias text: toastLabel.text
        function show(message) {
            text = message
            visible = true
            hideTimer.restart()
        }

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 28
        visible: false
        color: Theme.elevatedSurface
        border.color: Theme.border
        radius: 6
        implicitWidth: toastLabel.implicitWidth + 28
        implicitHeight: toastLabel.implicitHeight + 18
        z: 10

        Label {
            id: toastLabel
            anchors.centerIn: parent
            color: Theme.foreground
        }

        Timer {
            id: hideTimer
            interval: 3600
            onTriggered: toast.visible = false
        }
    }
}
