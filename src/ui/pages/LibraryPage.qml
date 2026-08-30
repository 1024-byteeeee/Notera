import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: Theme.background

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 36
        spacing: 24

        RowLayout {
            Layout.fillWidth: true
            Label { text: "Library"; color: Theme.foreground; font.pixelSize: 28; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
            TextField { placeholderText: "Search scores"; implicitWidth: 220 }
            Button { text: "Import"; enabled: false; ToolTip.visible: hovered; ToolTip.text: "Available in Phase 2" }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.surface
            radius: 8
            border.color: Theme.border

            Column {
                anchors.centerIn: parent
                spacing: 10
                Label { anchors.horizontalCenter: parent.horizontalCenter; text: "Your score library is empty"; color: Theme.foreground; font.pixelSize: 18 }
                Label { anchors.horizontalCenter: parent.horizontalCenter; text: "Import PDF, JPG or PNG scores to begin."; color: Theme.mutedForeground }
            }
        }
    }
}
