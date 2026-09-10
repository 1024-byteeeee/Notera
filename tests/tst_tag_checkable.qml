import QtQuick
import QtQuick.Controls
import QtTest












TestCase {
    id: root
    name: "TagCheckableMenu"
    when: windowShown
    width: 420
    height: 420


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

            MenuItem {
                id: tagMenuItem
                objectName: "tagMenuItem"
                text: "TAG-1"
                checkable: true
                checked: fakeService.itemHasTag(root.folderItemId, root.tagItemId)
                onTriggered: {

                    if (fakeService.itemHasTag(root.folderItemId, root.tagItemId)) {
                        fakeService.removeItemTag(root.folderItemId, root.tagItemId)
                    } else {
                        fakeService.addItemTag(root.folderItemId, root.tagItemId)
                    }
                }
            }
        }
    }


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


    function test_click_tagged_calls_remove() {
        fakeService.reset()
        fakeService.addItemTag(folderItemId, tagItemId)
        const addCallsBeforeClick = fakeService.addCalls
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
