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
                { label: "乐谱库", page: "library" },
                { label: "最近使用", page: "library" },
                { label: "收藏", page: "library" }
            ]

            delegate: Button {
                required property var modelData
                text: modelData.label
                Layout.fillWidth: true
                onClicked: root.pageSelected(modelData.page)
            }
        }

        Label { text: "文件夹"; color: Theme.mutedForeground; font.pixelSize: 12; Layout.topMargin: 14 }
        Label { text: "标签"; color: Theme.mutedForeground; font.pixelSize: 12 }

        Item { Layout.fillHeight: true }

        Button {
            text: "设置"
            Layout.fillWidth: true
            onClicked: root.pageSelected("settings")
        }
    }
}
