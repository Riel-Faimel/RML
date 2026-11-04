import QtQuick
import QtQuick.Layouts
import "../.."
import "../../components"

// 模组管理：把本机 jar 导入全局仓库，并写明适用版本。
SoftDialog {
    id: dialog

    title: qsTr("导入 Mod")
    width: 460

    property var files: []
    property string errorText: ""

    signal imported()

    function openFor(list, mc, loader) {
        dialog.files = list || []
        gameVersionField.text = mc || ""
        var index = loaderBox.model.indexOf(loader)
        loaderBox.currentIndex = index >= 0 ? index : 0
        modVersionField.text = ""
        dialog.errorText = ""
        open()
    }

    function submit() {
        if (dialog.files.length === 0) {
            dialog.errorText = qsTr("没有选择文件")
            return false
        }
        var mc = gameVersionField.text.trim()
        var version = modVersionField.text.trim()
        if (mc.length === 0 || version.length === 0) {
            dialog.errorText = qsTr("游戏版本和 Mod 版本不能为空")
            return false
        }

        for (var i = 0; i < dialog.files.length; ++i) {
            if (!bridge.importMod(dialog.files[i], mc, loaderBox.currentText, version)) {
                dialog.errorText = bridge.downloadStatus
                return false
            }
        }
        dialog.imported()
        return true
    }

    onAccepted: {
        if (!dialog.submit()) dialog.open()
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("已选择 %1 个文件").arg(dialog.files.length)
        color: Theme.textSecondary
        font.pixelSize: 11
        elide: Text.ElideRight
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 10
        rowSpacing: 8

        Text {
            text: qsTr("游戏版本")
            font.pixelSize: 12
            color: Theme.textSecondary
            Layout.preferredWidth: 68
        }

        SoftField {
            id: gameVersionField
            Layout.fillWidth: true
            placeholderText: qsTr("例如 1.21.1")
        }

        Text {
            text: qsTr("加载器")
            font.pixelSize: 12
            color: Theme.textSecondary
            Layout.preferredWidth: 68
        }

        SoftComboBox {
            id: loaderBox
            Layout.fillWidth: true
            model: ["Fabric", "Forge", "NeoForge", "Quilt"]
        }

        Text {
            text: qsTr("Mod 版本")
            font.pixelSize: 12
            color: Theme.textSecondary
            Layout.preferredWidth: 68
        }

        SoftField {
            id: modVersionField
            Layout.fillWidth: true
            placeholderText: qsTr("例如 1.2.3")
        }
    }

    Text {
        Layout.fillWidth: true
        visible: dialog.errorText.length > 0
        text: dialog.errorText
        color: Theme.danger
        font.pixelSize: 11
        wrapMode: Text.WordWrap
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("导入只复制到全局 mods 仓库并写版本描述；要真正加载，再到仓库列表安装到当前版本。")
        color: Theme.textSecondary
        font.pixelSize: 11
        wrapMode: Text.WordWrap
    }
}
