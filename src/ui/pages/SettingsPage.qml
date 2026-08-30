import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: Theme.background

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 36
        spacing: 24

        Label { text: "Settings"; color: Theme.foreground; font.pixelSize: 28; font.weight: Font.DemiBold }

        GroupBox {
            title: "General"
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                Label { text: "Theme"; color: Theme.foreground }
                ComboBox { model: ["System", "Light", "Dark"]; currentIndex: 0 }
            }
        }

        GroupBox {
            title: "Reader"
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                Label { text: "Default reader mode"; color: Theme.foreground }
                ComboBox { model: ["Single", "Double", "Continuous", "Horizontal"]; currentIndex: 0 }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
