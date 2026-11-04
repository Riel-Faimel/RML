import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../.."
import "../../components"

// 服务器页右栏：实例详情、参数、控制台。
GlassPanel {
    id: root

    property var gamemodes: ["survival", "creative", "adventure", "spectator"]
    property var gamemodeLabels: [qsTr("生存"), qsTr("创造"), qsTr("冒险"), qsTr("旁观")]
    property var difficulties: ["peaceful", "easy", "normal", "hard"]
    property var difficultyLabels: [qsTr("和平"), qsTr("简单"), qsTr("普通"), qsTr("困难")]

    property string lastSelectionKey: ""

    signal renameRequested()
    signal propertiesRequested()

    function syncParams() {
        if (serverBridge.selectedIndex < 0) return
        portField.text = String(serverBridge.port)
        memoryField.text = String(serverBridge.memoryMb)
        maxPlayersField.text = String(serverBridge.maxPlayers)

        var gamemode = root.gamemodes.indexOf(serverBridge.gamemode)
        gamemodeBox.currentIndex = gamemode >= 0 ? gamemode : 0
        var difficulty = root.difficulties.indexOf(serverBridge.difficulty)
        difficultyBox.currentIndex = difficulty >= 0 ? difficulty : 0
    }

    function applyParams() {
        var port = parseInt(portField.text)
        if (isNaN(port)) port = serverBridge.port
        var memory = parseInt(memoryField.text)
        if (isNaN(memory)) memory = serverBridge.memoryMb
        var maxPlayers = parseInt(maxPlayersField.text)
        if (isNaN(maxPlayers)) maxPlayers = serverBridge.maxPlayers

        serverBridge.applyParams(port, memory, root.gamemodes[gamemodeBox.currentIndex],
                                 root.difficulties[difficultyBox.currentIndex], maxPlayers)
    }

    function sendCommand() {
        if (!serverBridge.running) return
        var command = commandField.text
        if (command.trim().length === 0) return
        serverBridge.sendCommand(command)
        commandField.text = ""
        commandField.forceActiveFocus()
    }

    function scrollConsoleToBottom() {
        var flickable = consoleScroll.contentItem
        if (!flickable) return
        flickable.contentY = Math.max(0, flickable.contentHeight - flickable.height)
    }

    // selectedIndex 的 NOTIFY 是 changed（和其它属性共用），所以必须自己判断是否换了一项，
    // 避免状态/控制台刷新时把用户正在编辑的参数覆盖掉。删除非末尾项时下标可能不变，
    // 因此同时把实例数量和当前实例名作为身份的一部分。
    function selectionKey() {
        var items = serverBridge.instances
        var name = ""
        if (serverBridge.selectedIndex >= 0 && serverBridge.selectedIndex < items.length) {
            name = String(items[serverBridge.selectedIndex].name)
        }
        return String(serverBridge.selectedIndex) + "|" + String(items.length) + "|" + name
    }

    function syncSelection() {
        var key = selectionKey()
        if (key === root.lastSelectionKey) return
        root.lastSelectionKey = key
        syncParams()
    }

    Component.onCompleted: {
        root.lastSelectionKey = selectionKey()
        syncParams()
    }

    Connections {
        target: serverBridge
        function onChanged() { root.syncSelection() }
    }

    ColumnLayout {
        id: detailColumn
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10
        visible: serverBridge.selectedIndex >= 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: serverBridge.name
                    font.pixelSize: 20
                    font.bold: true
                    color: Theme.text
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: serverBridge.status
                    font.pixelSize: 12
                    color: serverBridge.running ? Theme.primary : Theme.textSecondary
                    elide: Text.ElideRight
                }
            }

            SoftButton {
                text: qsTr("启动")
                primary: true
                visible: !serverBridge.running
                enabled: serverBridge.selectedIndex >= 0
                onClicked: serverBridge.startServer()
            }

            SoftButton {
                text: qsTr("停止")
                primary: true
                visible: serverBridge.running
                onClicked: serverBridge.stopServer()
            }

            SoftButton {
                text: qsTr("强制停止")
                visible: serverBridge.running
                onClicked: serverBridge.forceStopServer()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.divider
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 3

                Text {
                    Layout.fillWidth: true
                    text: qsTr("端口")
                    font.pixelSize: 11
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                }

                SoftField {
                    id: portField
                    Layout.fillWidth: true
                    validator: IntValidator { bottom: 1; top: 65535 }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 3

                Text {
                    Layout.fillWidth: true
                    text: qsTr("内存 MB")
                    font.pixelSize: 11
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                }

                SoftField {
                    id: memoryField
                    Layout.fillWidth: true
                    validator: IntValidator { bottom: 512; top: 32768 }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 3

                Text {
                    Layout.fillWidth: true
                    text: qsTr("游戏模式")
                    font.pixelSize: 11
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                }

                SoftComboBox {
                    id: gamemodeBox
                    Layout.fillWidth: true
                    model: root.gamemodeLabels
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 3

                Text {
                    Layout.fillWidth: true
                    text: qsTr("难度")
                    font.pixelSize: 11
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                }

                SoftComboBox {
                    id: difficultyBox
                    Layout.fillWidth: true
                    model: root.difficultyLabels
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 3

                Text {
                    Layout.fillWidth: true
                    text: qsTr("最大玩家")
                    font.pixelSize: 11
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                }

                SoftField {
                    id: maxPlayersField
                    Layout.fillWidth: true
                    validator: IntValidator { bottom: 1; top: 1000 }
                }
            }

            SoftButton {
                text: qsTr("应用")
                primary: true
                Layout.alignment: Qt.AlignBottom
                onClicked: root.applyParams()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            SoftButton {
                text: qsTr("打开目录")
                onClicked: serverBridge.openFolder()
            }

            SoftButton {
                text: qsTr("重命名")
                onClicked: root.renameRequested()
            }

            SoftButton {
                text: qsTr("详细信息")
                onClicked: root.propertiesRequested()
            }

            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                text: qsTr("控制台")
                font.pixelSize: 12
                color: Theme.textSecondary
            }

            Text {
                text: serverBridge.running ? qsTr("运行中") : qsTr("未运行")
                font.pixelSize: 11
                color: serverBridge.running ? Theme.primary : Theme.textSecondary
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 100
            radius: Theme.radius
            color: Theme.text
            border.width: 1
            border.color: Theme.border
            clip: true

            ScrollView {
                id: consoleScroll
                anchors.fill: parent
                anchors.margins: 10
                clip: true
                background: null
                ScrollBar.horizontal.policy: ScrollBar.AsNeeded
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                TextArea {
                    id: consoleArea
                    width: Math.max(consoleScroll.availableWidth, contentWidth)
                    text: serverBridge.consoleText
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.NoWrap
                    textFormat: TextEdit.PlainText
                    color: Theme.background
                    selectionColor: Theme.primary
                    selectedTextColor: Theme.surface
                    font.family: "Consolas"
                    font.pixelSize: 12
                    padding: 0
                    background: null
                    onTextChanged: Qt.callLater(root.scrollConsoleToBottom)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            SoftField {
                id: commandField
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                enabled: serverBridge.running
                placeholderText: serverBridge.running ? qsTr("输入命令，回车发送")
                                                      : qsTr("服务器未运行")
                onAccepted: root.sendCommand()
            }

            SoftButton {
                text: qsTr("发送")
                primary: true
                enabled: serverBridge.running && commandField.text.trim().length > 0
                onClicked: root.sendCommand()
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 360)
        spacing: 8
        visible: serverBridge.selectedIndex < 0

        Text {
            Layout.fillWidth: true
            text: qsTr("点左上角 + 新建服务器")
            horizontalAlignment: Text.AlignHCenter
            color: Theme.textSecondary
            font.pixelSize: 14
        }
    }
}
