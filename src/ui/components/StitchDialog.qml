import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Notera

// 拼接对话框：让用户指定拼接方向（纵向/横向）、拼接顺序（列表上移/下移）、
// 拼接后的文件名。确认后发出 submitted(orderedPaths, direction, outputName)。
Dialog {
    id: dialog

    // 待拼接图片路径列表（QUrl 字符串），打开时按文件名排序显示
    property var paths: []
    property string defaultName: "拼接图片"
    signal submitted(var orderedPaths, string direction, string outputName)

    // 当前拼接方向
    property string stitchDirection: "vertical"

    parent: Overlay.overlay
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    width: parent ? Math.min(460, parent.width - 48) : 460
    popupType: Popup.Item
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    transformOrigin: Item.Center

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Motion.normal; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Motion.normal; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Motion.fast; easing.type: Easing.InCubic }
            NumberAnimation { property: "scale"; from: 1; to: 0.985; duration: Motion.fast; easing.type: Easing.InCubic }
        }
    }

    // 打开时：按显示名排序重建列表（批量/右键传 {path,name} 用乐谱标题，
    // 拼接导入传路径字符串则用文件名），文件名输入框填入默认名
    onOpened: {
        fileModel.clear()
        var sorted = []
        for (var i = 0; i < dialog.paths.length; ++i) {
            var raw = dialog.paths[i]
            var isMap = (typeof raw === "object" && raw !== null && "path" in raw)
            var p = isMap ? raw.path.toString() : raw.toString()
            var name = isMap && raw.name ? raw.name.toString() : p.split("/").pop()
            sorted.push({ path: p, name: name })
        }
        sorted.sort(function(a, b) { return a.name.localeCompare(b.name, "zh") })
        for (var j = 0; j < sorted.length; ++j) {
            fileModel.append(sorted[j])
        }
        listView.currentIndex = 0
        dialog.stitchDirection = "vertical"
        if (nameField.text.trim().length === 0) {
            nameField.text = dialog.defaultName
        }
        nameField.selectAll()
        nameField.forceActiveFocus()
    }

    ListModel { id: fileModel }

    function collectOrderedPaths() {
        var arr = []
        for (var i = 0; i < fileModel.count; ++i) {
            arr.push(fileModel.get(i).path)
        }
        return arr
    }

    function moveSelected(offset) {
        var idx = listView.currentIndex
        var target = idx + offset
        if (idx < 0 || target < 0 || target >= fileModel.count) return
        fileModel.move(idx, target, 1)
        listView.currentIndex = target
    }

    header: Label {
        leftPadding: 22
        rightPadding: 22
        topPadding: 20
        bottomPadding: 4
        text: dialog.title
        color: Theme.foreground
        font.pixelSize: Theme.fontLg
        font.weight: Font.DemiBold
    }

    contentItem: ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 22
        spacing: 14

        // 拼接方向
        RowLayout {
            spacing: 10

            Label {
                text: "拼接方向"
                color: Theme.foreground
                font.pixelSize: Theme.fontMd
                Layout.preferredWidth: 72
            }

            DirectionCard {
                id: verticalCard
                Layout.fillWidth: true
                selected: dialog.stitchDirection === "vertical"
                title: "纵向拼接"
                hint: "上下排列"
                onClicked: dialog.stitchDirection = "vertical"
            }

            DirectionCard {
                Layout.fillWidth: true
                selected: dialog.stitchDirection === "horizontal"
                title: "横向拼接"
                hint: "左右排列"
                onClicked: dialog.stitchDirection = "horizontal"
            }
        }

        // 拼接顺序
        RowLayout {
            spacing: 10

            Label {
                text: "拼接顺序"
                color: Theme.foreground
                font.pixelSize: Theme.fontMd
                Layout.preferredWidth: 72
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                radius: Theme.radiusMd
                color: Theme.inputBackground
                border.width: 1
                border.color: Theme.inputBorder
                clip: true

                ListView {
                    id: listView
                    anchors.fill: parent
                    anchors.margins: 4
                    model: fileModel
                    clip: true
                    delegate: ItemDelegate {
                        id: row
                        required property string path
                        required property string name
                        required property int index
                        width: listView.width
                        height: 28
                        highlighted: ListView.isCurrentItem
                        onClicked: listView.currentIndex = index

                        background: Rectangle {
                            radius: 6
                            color: row.highlighted ? Theme.accentSoft : "transparent"
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            Label {
                                text: (row.index + 1) + "."
                                color: Theme.secondaryForeground
                                font.pixelSize: Theme.fontSm
                            }
                            Label {
                                text: row.name
                                color: Theme.foreground
                                font.pixelSize: Theme.fontSm
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                spacing: 8

                AppButton {
                    text: "上移"
                    Layout.preferredWidth: 64
                    enabled: listView.currentIndex > 0
                    onClicked: dialog.moveSelected(-1)
                }
                AppButton {
                    text: "下移"
                    Layout.preferredWidth: 64
                    enabled: listView.currentIndex >= 0 && listView.currentIndex < fileModel.count - 1
                    onClicked: dialog.moveSelected(1)
                }
            }
        }

        // 文件名
        RowLayout {
            spacing: 10

            Label {
                text: "文件名"
                color: Theme.foreground
                font.pixelSize: Theme.fontMd
                Layout.preferredWidth: 72
            }

            TextField {
                id: nameField
                Layout.fillWidth: true
                implicitHeight: 42
                color: Theme.foreground
                placeholderText: "拼接图片"
                placeholderTextColor: Theme.inputPlaceholder
                selectByMouse: true
                leftPadding: 12
                rightPadding: 12
                onAccepted: dialog.confirm()
                background: Rectangle {
                    radius: Theme.radiusMd
                    color: Theme.inputBackground
                    border.width: 1
                    border.color: nameField.activeFocus ? Theme.inputFocusBorder : Theme.inputBorder
                }
            }
        }
    }

    function confirm() {
        if (fileModel.count < 2 || nameField.text.trim().length === 0) return
        dialog.submitted(dialog.collectOrderedPaths(), dialog.stitchDirection, nameField.text.trim())
        dialog.accept()
    }

    footer: Item {
        implicitHeight: 62

        RowLayout {
            anchors.right: parent.right
            anchors.rightMargin: 22
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            AppButton {
                text: "取消"
                onClicked: dialog.reject()
            }
            AppButton {
                text: "确定"
                primary: true
                enabled: fileModel.count >= 2 && nameField.text.trim().length > 0
                onClicked: dialog.confirm()
            }
        }
    }

    background: Rectangle {
        radius: Theme.radiusLg
        color: Theme.surface
        border.width: 1
        border.color: Theme.strongBorder
    }

    // 方向选择卡片
    component DirectionCard: Rectangle {
        id: card
        property bool selected: false
        property string title: ""
        property string hint: ""
        signal clicked()

        Layout.preferredHeight: 56
        radius: Theme.radiusMd
        color: card.selected ? Theme.accentSoft : Theme.inputBackground
        border.width: card.selected ? 2 : 1
        border.color: card.selected ? Theme.accent : Theme.inputBorder

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: card.clicked()
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 2

            Label {
                text: card.title
                color: Theme.foreground
                font.pixelSize: Theme.fontSm
                font.weight: card.selected ? Font.DemiBold : Font.Normal
            }
            Label {
                text: card.hint
                color: Theme.secondaryForeground
                font.pixelSize: 11
            }
        }
    }
}
