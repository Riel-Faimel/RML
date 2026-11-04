import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../.."
import "../../components"

// 服务器页：新建服务器对话框。
SoftDialog {
    id: dialog

    title: qsTr("新建服务器")
    width: 460

    property var gamemodes: ["survival", "creative", "adventure", "spectator"]
    property var gamemodeLabels: [qsTr("生存"), qsTr("创造"), qsTr("冒险"), qsTr("旁观")]
    property var difficulties: ["peaceful", "easy", "normal", "hard"]
    property var difficultyLabels: [qsTr("和平"), qsTr("简单"), qsTr("普通"), qsTr("困难")]
    property var installationLabels: []

    // SoftComboBox 的 delegate 用 modelData 显示文本，这里把 installations 映射成 label；
    // 下拉下标仍与 serverBridge.installations 的下标一一对应。
    function refreshInstallations() {
        var items = serverBridge.installations
        var labels = []
        for (var i = 0; i < items.length; ++i) {
            labels.push(items[i].label ? String(items[i].label) : String(items[i].id))
        }
        dialog.installationLabels = labels
    }

    function createServer() {
        var name = createNameField.text.trim()
        if (name.length === 0 || dialog.installationLabels.length === 0) return

        var port = parseInt(createPortField.text)
        if (isNaN(port)) port = 25565
        var memory = parseInt(createMemoryField.text)
        if (isNaN(memory)) memory = 2048
        var maxPlayers = parseInt(createMaxPlayersField.text)
        if (isNaN(maxPlayers)) maxPlayers = 20

        var installation = Math.max(0, createInstallationBox.currentIndex)
        serverBridge.createServer(name, installation, port, memory,
                                  dialog.gamemodes[createGamemodeBox.currentIndex],
                                  dialog.difficulties[createDifficultyBox.currentIndex], maxPlayers)
    }

    onOpened: {
        serverBridge.reloadInstallations()
        dialog.refreshInstallations()
        createNameField.text = ""
        createPortField.text = "25565"
        createMemoryField.text = "2048"
        createMaxPlayersField.text = "20"
        createInstallationBox.currentIndex = 0
        createGamemodeBox.currentIndex = 0
        createDifficultyBox.currentIndex = 1
        createNameField.forceActiveFocus()
    }

    onAccepted: dialog.createServer()

    ColumnLayout {
        width: parent.width
        spacing: 10

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 10
            rowSpacing: 8

            Text {
                text: qsTr("名称")
                font.pixelSize: 12
                color: Theme.textSecondary
                Layout.preferredWidth: 68
            }

            SoftField {
                id: createNameField
                Layout.fillWidth: true
                placeholderText: qsTr("例如 朋友服")
                onAccepted: dialog.accept()
            }

            Text {
                text: qsTr("服务端")
                font.pixelSize: 12
                color: Theme.textSecondary
                Layout.preferredWidth: 68
            }

            SoftComboBox {
                id: createInstallationBox
                Layout.fillWidth: true
                model: dialog.installationLabels
            }

            Text {
                text: qsTr("端口")
                font.pixelSize: 12
                color: Theme.textSecondary
                Layout.preferredWidth: 68
            }

            SoftField {
                id: createPortField
                Layout.fillWidth: true
                text: "25565"
                validator: IntValidator { bottom: 1; top: 65535 }
            }

            Text {
                text: qsTr("内存 MB")
                font.pixelSize: 12
                color: Theme.textSecondary
                Layout.preferredWidth: 68
            }

            SoftField {
                id: createMemoryField
                Layout.fillWidth: true
                text: "2048"
                validator: IntValidator { bottom: 512; top: 32768 }
            }

            Text {
                text: qsTr("游戏模式")
                font.pixelSize: 12
                color: Theme.textSecondary
                Layout.preferredWidth: 68
            }

            SoftComboBox {
                id: createGamemodeBox
                Layout.fillWidth: true
                model: dialog.gamemodeLabels
            }

            Text {
                text: qsTr("难度")
                font.pixelSize: 12
                color: Theme.textSecondary
                Layout.preferredWidth: 68
            }

            SoftComboBox {
                id: createDifficultyBox
                Layout.fillWidth: true
                model: dialog.difficultyLabels
            }

            Text {
                text: qsTr("最大玩家")
                font.pixelSize: 12
                color: Theme.textSecondary
                Layout.preferredWidth: 68
            }

            SoftField {
                id: createMaxPlayersField
                Layout.fillWidth: true
                text: "20"
                validator: IntValidator { bottom: 1; top: 1000 }
            }

            Text {
                Layout.fillWidth: true
                Layout.columnSpan: 2
                visible: dialog.installationLabels.length === 0
                text: qsTr("没有可用的服务端安装，请先在下载页安装服务端。")
                font.pixelSize: 11
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
        }
    }
}
