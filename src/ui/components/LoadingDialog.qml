import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera







Item {
    id: root

    property string message: "正在加载"

    visible: false
    z: 10000
    anchors.fill: parent


    Rectangle {
        anchors.fill: parent
        color: "#00000080"

        MouseArea {
            anchors.fill: parent
        }


        Rectangle {
            anchors.centerIn: parent
            width: Math.min(280, parent.width - 64)
            height: column.implicitHeight + 48
            radius: Theme.radiusLg
            color: Theme.surface
            border.width: 1
            border.color: Theme.strongBorder

            ColumnLayout {
                id: column
                anchors.centerIn: parent
                spacing: 16

                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    running: root.visible
                    width: 36
                    height: 36
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.message
                    color: Theme.foreground
                    font.pixelSize: Theme.fontMd
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    function show(msg) {
        if (msg !== undefined && msg.length > 0) root.message = msg
        root.visible = true
    }

    function hide() {

        root.visible = false
    }
}
