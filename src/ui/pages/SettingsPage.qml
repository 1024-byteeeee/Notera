import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

Rectangle {
    color: Theme.background

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 36
        spacing: 24

        Label { text: "设置"; color: Theme.foreground; font.pixelSize: 28; font.weight: Font.DemiBold }

        GroupBox {
            title: "通用"
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                Label { text: "主题"; color: Theme.foreground }
                ComboBox {
                    model: ["跟随系统", "浅色", "深色"]
                    currentIndex: appController.themeMode
                    onActivated: function(index) { appController.themeMode = index }
                }
            }
        }

        GroupBox {
            title: "阅读器"
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                Label { text: "默认阅读模式"; color: Theme.foreground }
                ComboBox { model: ["单页", "双页", "连续", "横向"]; currentIndex: 0 }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
