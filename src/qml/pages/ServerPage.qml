import QtQuick
import QtQuick.Layouts
import ".."
import "../components"
import "server"

Item {
    id: page

    property Item backdrop: Window.window ? Window.window.backdropRef : null

    // serverBridge 里的参数值是英文枚举，界面展示中文。
    property var gamemodes: ["survival", "creative", "adventure", "spectator"]
    property var gamemodeLabels: [qsTr("生存"), qsTr("创造"), qsTr("冒险"), qsTr("旁观")]
    property var difficulties: ["peaceful", "easy", "normal", "hard"]
    property var difficultyLabels: [qsTr("和平"), qsTr("简单"), qsTr("普通"), qsTr("困难")]
    property int pendingDelete: -1
    property string pendingDeleteName: ""

    function deleteServerAt(index, name) {
        page.pendingDelete = index
        page.pendingDeleteName = String(name)
        confirmDelete.open()
    }

    property int pendingRename: -1
    property string pendingRenameName: ""

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.gap

        InstancePanel {
            backdrop: page.backdrop
            Layout.preferredWidth: Theme.sidebarWidth
            Layout.maximumWidth: Theme.sidebarWidth
            Layout.fillHeight: true

            onAddRequested: createDialog.open()
            onOpenFolderRequested: function(index) {
                serverBridge.selectAt(index)
                serverBridge.openFolder()
            }
            onRenameRequested: function(index, name) {
                serverBridge.selectAt(index)
                page.pendingRename = index
                page.pendingRenameName = String(name)
                renameDialog.open()
            }
            onDeleteRequested: function(index, name) { page.deleteServerAt(index, name) }
        }

        DetailPanel {
            id: detail
            backdrop: page.backdrop
            Layout.fillWidth: true
            Layout.fillHeight: true

            gamemodes: page.gamemodes
            gamemodeLabels: page.gamemodeLabels
            difficulties: page.difficulties
            difficultyLabels: page.difficultyLabels

            onRenameRequested: renameDialog.open()
            onPropertiesRequested: propertyDialog.openForInstance()
        }
    }

    CreateDialog {
        id: createDialog
        gamemodes: page.gamemodes
        gamemodeLabels: page.gamemodeLabels
        difficulties: page.difficulties
        difficultyLabels: page.difficultyLabels
    }

    SoftDialog {
        id: renameDialog
        title: qsTr("重命名服务器")
        width: 400

        onOpened: {
            renameField.text = page.pendingRenameName.length > 0 ? page.pendingRenameName
                                                                  : serverBridge.name
            renameField.forceActiveFocus()
            renameField.selectAll()
        }

        onAccepted: {
            var newName = renameField.text.trim()
            if (newName.length > 0) serverBridge.renameServer(newName)
        }

        SoftField {
            id: renameField
            width: parent.width
            placeholderText: qsTr("新名字")
            onAccepted: renameDialog.accept()
        }
    }

    SoftDialog {
        id: confirmDelete
        title: qsTr("删除服务器")
        okText: qsTr("删除")
        width: 400

        Text {
            Layout.fillWidth: true
            text: qsTr("确定删除「%1」？服务器目录会被永久删除。").arg(page.pendingDeleteName)
            wrapMode: Text.WrapAnywhere
            color: Theme.text
            font.pixelSize: 13
        }

        onAccepted: serverBridge.removeServerAt(page.pendingDelete)
    }

    PropertyDialog {
        id: propertyDialog
        onSaved: detail.syncParams()
    }
}
