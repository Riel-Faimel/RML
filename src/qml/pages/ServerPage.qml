import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: page

    property Item backdrop: Window.window ? Window.window.backdropRef : null

    // serverBridge 里的参数值是英文枚举，界面展示中文。
    property var gamemodes: ["survival", "creative", "adventure", "spectator"]
    property var gamemodeLabels: [qsTr("生存"), qsTr("创造"), qsTr("冒险"), qsTr("旁观")]
    property var difficulties: ["peaceful", "easy", "normal", "hard"]
    property var difficultyLabels: [qsTr("和平"), qsTr("简单"), qsTr("普通"), qsTr("困难")]
    property var installationLabels: []
    property string lastSelectionKey: ""

    function syncParamFields() {
        if (serverBridge.selectedIndex < 0) return
        portField.text = String(serverBridge.port)
        memoryField.text = String(serverBridge.memoryMb)
        maxPlayersField.text = String(serverBridge.maxPlayers)

        var gamemode = page.gamemodes.indexOf(serverBridge.gamemode)
        gamemodeBox.currentIndex = gamemode >= 0 ? gamemode : 0
        var difficulty = page.difficulties.indexOf(serverBridge.difficulty)
        difficultyBox.currentIndex = difficulty >= 0 ? difficulty : 0
    }

    function applyParams() {
        var port = parseInt(portField.text)
        if (isNaN(port)) port = serverBridge.port
        var memory = parseInt(memoryField.text)
        if (isNaN(memory)) memory = serverBridge.memoryMb
        var maxPlayers = parseInt(maxPlayersField.text)
        if (isNaN(maxPlayers)) maxPlayers = serverBridge.maxPlayers

        serverBridge.applyParams(port, memory, page.gamemodes[gamemodeBox.currentIndex],
                                 page.difficulties[difficultyBox.currentIndex], maxPlayers)
    }

    function sendCommand() {
        if (!serverBridge.running) return
        var command = commandField.text
        if (command.trim().length === 0) return
        serverBridge.sendCommand(command)
        commandField.text = ""
        commandField.forceActiveFocus()
    }

    // SoftComboBox 的 delegate 用 modelData 显示文本，这里把 installations 映射成 label；
    // 下拉下标仍与 serverBridge.installations 的下标一一对应。
    function refreshInstallations() {
        var items = serverBridge.installations
        var labels = []
        for (var i = 0; i < items.length; ++i) {
            labels.push(items[i].label ? String(items[i].label) : String(items[i].id))
        }
        page.installationLabels = labels
    }

    function createServer() {
        var name = createNameField.text.trim()
        if (name.length === 0 || page.installationLabels.length === 0) return

        var port = parseInt(createPortField.text)
        if (isNaN(port)) port = 25565
        var memory = parseInt(createMemoryField.text)
        if (isNaN(memory)) memory = 2048
        var maxPlayers = parseInt(createMaxPlayersField.text)
        if (isNaN(maxPlayers)) maxPlayers = 20

        var installation = Math.max(0, createInstallationBox.currentIndex)
        serverBridge.createServer(name, installation, port, memory,
                                  page.gamemodes[createGamemodeBox.currentIndex],
                                  page.difficulties[createDifficultyBox.currentIndex], maxPlayers)
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
        if (key === page.lastSelectionKey) return
        page.lastSelectionKey = key
        syncParamFields()
    }

    Component.onCompleted: {
        page.lastSelectionKey = selectionKey()
        syncParamFields()
    }

    Connections {
        target: serverBridge
        function onChanged() { page.syncSelection() }
    }

    Dialog {
        id: createDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        title: qsTr("新建服务器")
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 440

        onOpened: {
            serverBridge.reloadInstallations()
            page.refreshInstallations()
            createNameField.text = ""
            createPortField.text = "25565"
            createMemoryField.text = "2048"
            createMaxPlayersField.text = "20"
            createInstallationBox.currentIndex = 0
            createGamemodeBox.currentIndex = 0
            createDifficultyBox.currentIndex = 1
            createNameField.forceActiveFocus()
        }

        onAccepted: page.createServer()

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
                    onAccepted: createDialog.accept()
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
                    model: page.installationLabels
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
                    model: page.gamemodeLabels
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
                    model: page.difficultyLabels
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
                    visible: page.installationLabels.length === 0
                    text: qsTr("没有可用的服务端安装，请先在下载页安装服务端。")
                    font.pixelSize: 11
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Dialog {
        id: renameDialog
        modal: true
        anchors.centerIn: Overlay.overlay
        title: qsTr("重命名服务器")
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 380

        onOpened: {
            renameField.text = serverBridge.name
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

    Dialog {
        id: confirmDelete
        modal: true
        anchors.centerIn: Overlay.overlay
        title: qsTr("删除服务器")
        standardButtons: Dialog.Yes | Dialog.Cancel
        width: 380

        Text {
            width: parent.width
            text: qsTr("确定删除「%1」？服务器目录会被永久删除。").arg(serverBridge.name)
            wrapMode: Text.WrapAnywhere
            color: Theme.text
            font.pixelSize: 13
        }

        onAccepted: serverBridge.removeServer()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.gap

        // 左：服务器实例
        GlassPanel {
            backdrop: page.backdrop
            Layout.preferredWidth: 300
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("服务器")
                        font.pixelSize: 20
                        font.bold: true
                        color: Theme.text
                    }

                    Button {
                        id: addButton
                        implicitWidth: 32
                        implicitHeight: 32
                        padding: 0
                        onClicked: createDialog.open()

                        contentItem: Text {
                            text: "+"
                            font.pixelSize: 20
                            color: Theme.surface
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 999
                            color: addButton.pressed ? Theme.primaryPressed : Theme.primary
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.border
                }

                ListView {
                    id: serverList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: serverBridge.instances

                    delegate: Rectangle {
                        id: serverRow
                        width: ListView.view.width
                        height: 54
                        radius: Theme.radius
                        color: index === serverBridge.selectedIndex ? Theme.primarySoft : "#66FFFFFF"
                        border.width: 1
                        border.color: index === serverBridge.selectedIndex ? Theme.primary : "#55FFFFFF"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 10
                            spacing: 8

                            Text {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: modelData.name
                                elide: Text.ElideRight
                                font.pixelSize: 14
                                color: index === serverBridge.selectedIndex ? Theme.primary : Theme.text
                            }

                            Text {
                                Layout.maximumWidth: 120
                                text: modelData.status
                                elide: Text.ElideRight
                                font.pixelSize: 12
                                color: modelData.running ? Theme.primary : Theme.textSecondary
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: serverBridge.selectAt(index)
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: serverList.count === 0
                        text: qsTr("还没有服务器")
                        color: Theme.textSecondary
                    }
                }
            }
        }

        // 右：实例详情
        GlassPanel {
            backdrop: page.backdrop
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                id: detailColumn
                anchors.fill: parent
                anchors.margins: 20
                spacing: 10
                visible: serverBridge.selectedIndex >= 0

                RowLayout {
                    Layout.fillWidth: true
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
                    color: Theme.border
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
                            model: page.gamemodeLabels
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
                            model: page.difficultyLabels
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
                        onClicked: page.applyParams()
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
                        onClicked: renameDialog.open()
                    }

                    SoftButton {
                        text: qsTr("删除")
                        onClicked: confirmDelete.open()
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
                            onTextChanged: Qt.callLater(page.scrollConsoleToBottom)
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
                        onAccepted: page.sendCommand()
                    }

                    SoftButton {
                        text: qsTr("发送")
                        primary: true
                        enabled: serverBridge.running && commandField.text.trim().length > 0
                        onClicked: page.sendCommand()
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: serverBridge.message
                    font.pixelSize: 11
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
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

                Text {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: serverBridge.message
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
