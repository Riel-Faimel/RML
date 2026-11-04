import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../.."
import "../../components"

// 启动页：当前版本的模组管理（已安装 / 仓库）。
SoftDialog {
    id: dialog

    title: qsTr("模组管理")
    okText: qsTr("关闭")
    showCancel: false
    width: 640
    height: 520

    property bool repoMode: false

    function reloadInstalled() {
        installedModel.clear()
        var items = bridge.installedMods()
        for (var i = 0; i < items.length; ++i) installedModel.append(items[i])
    }

    function loadRepository() {
        repoModel.clear()
        var items = bridge.repositoryMods()
        for (var i = 0; i < items.length; ++i) repoModel.append(items[i])
    }

    function installRepoMod(index) {
        if (bridge.installRepositoryMod(index)) {
            loadRepository()
            reloadInstalled()
        }
    }

    function removeInstalledMod(index) {
        if (bridge.removeInstalledMod(index)) reloadInstalled()
    }

    onOpened: {
        dialog.repoMode = false
        dialog.reloadInstalled()
    }

    ListModel { id: installedModel }
    ListModel { id: repoModel }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: bridge.selectedName
            color: Theme.text
            font.pixelSize: 14
            font.bold: true
            elide: Text.ElideRight
        }

        SoftButton {
            text: dialog.repoMode ? qsTr("返回") : qsTr("+")
            primary: !dialog.repoMode
            onClicked: {
                if (dialog.repoMode) {
                    dialog.repoMode = false
                    dialog.reloadInstalled()
                } else {
                    dialog.repoMode = true
                    dialog.loadRepository()
                }
            }
        }
    }

    Text {
        Layout.fillWidth: true
        visible: !dialog.repoMode && installedModel.count === 0
        text: qsTr("当前版本还没有安装模组")
        color: Theme.textSecondary
        font.pixelSize: 12
    }

    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: !dialog.repoMode
        clip: true
        spacing: 6
        model: installedModel

        delegate: RowLayout {
            width: ListView.view.width
            height: 38
            spacing: 8

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: model.name
                color: Theme.text
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Text {
                Layout.preferredWidth: 130
                text: model.version
                color: Theme.textSecondary
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            SoftButton {
                text: qsTr("删除")
                onClicked: dialog.removeInstalledMod(index)
            }
        }
    }

    Text {
        Layout.fillWidth: true
        visible: dialog.repoMode && repoModel.count === 0
        text: qsTr("Mod 仓库里没有适合当前版本的模组")
        color: Theme.textSecondary
        font.pixelSize: 12
    }

    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: dialog.repoMode
        clip: true
        spacing: 6
        model: repoModel

        delegate: RowLayout {
            width: ListView.view.width
            height: 46
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: model.name
                    color: Theme.text
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: model.version || qsTr("已下载")
                    color: Theme.textSecondary
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }

            SoftButton {
                text: qsTr("安装")
                primary: true
                onClicked: dialog.installRepoMod(index)
            }
        }
    }

    Text {
        Layout.fillWidth: true
        visible: dialog.repoMode
        text: bridge.downloadStatus
        color: Theme.textSecondary
        font.pixelSize: 11
        wrapMode: Text.WordWrap
    }
}
