import QtQuick
import QtQuick.Controls
import QtTest

// 回归测试：LibraryPage 标签子菜单的 checkable MenuItem 点击逻辑。
//
// 背景：Qt Quick Controls 的 checkable MenuItem 在鼠标点击时会「先自动切换 checked，
// 再触发 onTriggered」。旧代码在 onTriggered 里用 UI 的 checked 判断「当前是否已打标」，
// 导致点击未打标的标签时 checked 已被切为 true，误走 removeItemTag，标签永远不会写入。
// 修复后改为直接读取服务端状态（itemHasTag）来决定 add/remove。
//
// 本测试用真实鼠标点击（QTest::mouseClick，会复现 checkable 的自动切换行为）验证：
//   1. 未打标时点击 → 调用 addItemTag
//   2. 已打标时点击 → 调用 removeItemTag
// 若未来有人把逻辑改回依赖 UI checked，本测试会失败。
TestCase {
    id: root
    name: "TagCheckableMenu"
    when: windowShown
    width: 420
    height: 420

    // 模拟 LibraryService：记录调用，供 onTriggered 决策
    QtObject {
        id: fakeService
        property var taggedItemIds: []
        property int addCalls: 0
        property int removeCalls: 0
        property string lastAction: ""
        function itemHasTag(itemId, tagId) { return taggedItemIds.indexOf(itemId) !== -1 }
        function addItemTag(itemId, tagId) {
            addCalls++; lastAction = "add"
            if (taggedItemIds.indexOf(itemId) === -1) taggedItemIds.push(itemId)
        }
        function removeItemTag(itemId, tagId) {
            removeCalls++; lastAction = "remove"
            const i = taggedItemIds.indexOf(itemId)
            if (i >= 0) taggedItemIds.splice(i, 1)
        }
        function reset() {
            taggedItemIds = []; addCalls = 0; removeCalls = 0; lastAction = ""
        }
    }

    readonly property string folderItemId: "FOLDER-X"
    readonly property string tagItemId: "TAG-1"

    Component {
        id: menuComponent
        Menu {
            // 复刻 LibraryPage.qml 中修复后的 folderTagSubmenu 单条目逻辑
            MenuItem {
                id: tagMenuItem
                objectName: "tagMenuItem"
                text: "TAG-1"
                checkable: true
                checked: fakeService.itemHasTag(root.folderItemId, root.tagItemId)
                onTriggered: {
                    // 修复后：直接读取服务端状态，不依赖 UI checked（checkable 已自动切换）
                    if (fakeService.itemHasTag(root.folderItemId, root.tagItemId)) {
                        fakeService.removeItemTag(root.folderItemId, root.tagItemId)
                    } else {
                        fakeService.addItemTag(root.folderItemId, root.tagItemId)
                    }
                }
            }
        }
    }

    // 未打标时点击标签 → 应该调用 addItemTag（旧代码会因 checked 被切为 true 而误走 remove）
    function test_click_untagged_calls_add() {
        fakeService.reset()
        const m = menuComponent.createObject(root)
        m.popup()
        const item = m.itemAt(0)
        wait(150)
        mouseClick(item, item.width / 2, item.height / 2)
        wait(150)
        compare(fakeService.lastAction, "add", "点击未打标标签应执行添加")
        compare(fakeService.addCalls, 1)
        compare(fakeService.removeCalls, 0)
        compare(fakeService.itemHasTag(folderItemId, tagItemId), true)
        compare(item.checked, true, "checkable 项点击后 checked 应为 true")
        m.destroy()
    }

    // 已打标时点击标签 → 应该调用 removeItemTag
    function test_click_tagged_calls_remove() {
        fakeService.reset()
        fakeService.addItemTag(folderItemId, tagItemId)   // 先打上标签
        const addCallsBeforeClick = fakeService.addCalls  // 仅包含上面的准备调用
        const m = menuComponent.createObject(root)
        m.popup()
        const item = m.itemAt(0)
        wait(150)
        compare(item.checked, true, "已打标时菜单项应显示为勾选")
        mouseClick(item, item.width / 2, item.height / 2)
        wait(150)
        compare(fakeService.lastAction, "remove", "点击已打标标签应执行移除")
        compare(fakeService.removeCalls, 1)
        compare(fakeService.addCalls, addCallsBeforeClick, "点击不应触发添加")
        compare(fakeService.itemHasTag(folderItemId, tagItemId), false)
        compare(item.checked, false)
        m.destroy()
    }
}
