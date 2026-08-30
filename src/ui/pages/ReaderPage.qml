import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: Theme.background

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 64
            color: Theme.surface
            RowLayout {
                anchors.fill: parent
                anchors.margins: 16
                Button { text: "← Library"; onClicked: appController.currentPage = "library" }
                Item { Layout.fillWidth: true }
                Label { text: "Reader"; color: Theme.foreground; font.pixelSize: 17 }
                Item { Layout.fillWidth: true }
                Label { text: "100%"; color: Theme.mutedForeground }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#303030"
            Label { anchors.centerIn: parent; text: "PDF and image rendering arrives in Phase 3"; color: Theme.mutedForeground }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 54
            color: Theme.surface
            Label { anchors.centerIn: parent; text: "←     1 / 1     →"; color: Theme.foreground }
        }
    }
}
