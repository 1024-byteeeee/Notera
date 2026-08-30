import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    width: 244
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    signal pageSelected(string page)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 10

        Label {
            text: "Notera"
            color: Theme.foreground
            font.pixelSize: 23
            font.weight: Font.DemiBold
            Layout.bottomMargin: 18
        }

        Repeater {
            model: [
                { label: "Library", page: "library" },
                { label: "Recent", page: "library" },
                { label: "Favorites", page: "library" }
            ]

            delegate: Button {
                required property var modelData
                text: modelData.label
                Layout.fillWidth: true
                onClicked: root.pageSelected(modelData.page)
            }
        }

        Label { text: "Folders"; color: Theme.mutedForeground; font.pixelSize: 12; Layout.topMargin: 14 }
        Label { text: "Tags"; color: Theme.mutedForeground; font.pixelSize: 12 }

        Item { Layout.fillHeight: true }

        Button {
            text: "Settings"
            Layout.fillWidth: true
            onClicked: root.pageSelected("settings")
        }
    }
}
