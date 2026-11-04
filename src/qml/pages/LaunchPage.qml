import QtQuick
import QtQuick.Layouts
import ".."
import "../components"
import "launch"

Item {
    id: page

    property Item backdrop: Window.window ? Window.window.backdropRef : null
    property int pendingDelete: -1
    property int pendingRename: -1

    SoftDialog {
        id: confirmDelete
        title: qsTr("删除版本")
        okText: qsTr("删除")
        width: 360

        Text {
            Layout.fillWidth: true
            text: qsTr("确定删除「%1」？只删除版本入口，存档/模组随入口一起删除；共享核心会保留，可在设置页清理。").arg(page.pendingDelete >= 0 ? bridge.versions[page.pendingDelete] : "")
            wrapMode: Text.WrapAnywhere
            color: Theme.text
            font.pixelSize: 13
        }

        onAccepted: bridge.deleteVersionAt(page.pendingDelete)
    }

    SoftDialog {
        id: renameDialog
        title: qsTr("重命名版本")
        width: 360

        onOpened: renameField.forceActiveFocus()

        SoftField {
            id: renameField
            Layout.fillWidth: true
            placeholderText: qsTr("新名字")
            onAccepted: renameDialog.accept()
        }

        onAccepted: bridge.renameVersionAt(page.pendingRename, renameField.text)
    }

    ModsDialog {
        id: modsDialog
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.gap

        VersionPanel {
            backdrop: page.backdrop
            Layout.preferredWidth: Theme.sidebarWidth
            Layout.maximumWidth: Theme.sidebarWidth
            Layout.fillHeight: true

            onRenameRequested: function(index, name) {
                page.pendingRename = index
                renameField.text = name
                renameDialog.open()
            }

            onDeleteRequested: function(index, name) {
                page.pendingDelete = index
                confirmDelete.open()
            }
        }

        DetailPanel {
            backdrop: page.backdrop
            Layout.fillWidth: true
            Layout.fillHeight: true

            onModsRequested: modsDialog.open()
        }
    }
}
