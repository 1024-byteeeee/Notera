import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
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
}
