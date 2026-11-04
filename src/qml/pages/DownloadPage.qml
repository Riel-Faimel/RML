import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: page

    property Item backdrop: Window.window ? Window.window.backdropRef : null
    property string category: "version"
    property string kind: "mod"
    property int versionKind: 0
    property var loaderChoice: ({})
    property var loaderVersionChoice: ({})
    property int pendingKind: 0
    property int pendingIndex: -1
    property string pendingLoader: ""
    property string pendingMcVersion: ""

    function loaderFor(id) { return page.loaderChoice[id] || "原版" }
    function loaderVersionFor(id) { return page.loaderVersionChoice[id] || "" }

    function setLoaderVersion(id, version) {
        var copy = {}
        for (var key in page.loaderVersionChoice) copy[key] = page.loaderVersionChoice[key]
        copy[id] = version
        page.loaderVersionChoice = copy
    }

    function chooseLoader(id, name) {
        page.setLoader(id, name)
        page.setLoaderVersion(id, "")
        page.pendingMcVersion = id
        page.pendingLoader = name
        if (name === "原版") return
        bridge.loadLoaderVersions(id, name)
        loaderDialog.selectedVersion = ""
        loaderDialog.open()
    }

    function installVersion(kind, index, loader, mcVersion) {
        if (loader === "原版") {
            bridge.installGame(kind, index, loader)
            return
        }
        const version = page.loaderVersionFor(mcVersion)
        if (version.length === 0) {
            page.chooseLoader(mcVersion, loader)
            return
        }
        bridge.installGameWithLoader(kind, index, loader, version)
    }

    function setLoader(id, name) {
        var copy = {}
        for (var key in page.loaderChoice) copy[key] = page.loaderChoice[key]
        copy[id] = name
        page.loaderChoice = copy
    }

    function activate(key, targetKind) {
        page.category = key
        page.kind = targetKind
        if (key === "version") {
            bridge.loadGameVersions()
        } else {
            bridge.searchContent(searchField.text, targetKind)
        }
    }

    // 默认就拉一次版本清单，不再依赖手动刷新
    Component.onCompleted: bridge.loadGameVersions()

    SoftDialog {
        id: loaderDialog
        title: qsTr("选择 %1 版本").arg(page.pendingLoader)
        okText: qsTr("确定")
        side: true
        width: 380
        height: page.height
        property string selectedVersion: ""

        onOpened: selectedVersion = page.loaderVersionFor(page.pendingMcVersion)
        onAccepted: {
            if (selectedVersion.length > 0) {
                page.setLoaderVersion(page.pendingMcVersion, selectedVersion)
            }
        }

        Text {
            Layout.fillWidth: true
            text: bridge.loaderVersionsBusy
                  ? qsTr("正在获取 %1 %2 的版本…").arg(page.pendingLoader).arg(page.pendingMcVersion)
                  : bridge.loaderVersionsError
            color: bridge.loaderVersionsError.length > 0 ? Theme.danger : Theme.textSecondary
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            visible: bridge.loaderVersionsBusy || bridge.loaderVersionsError.length > 0
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: bridge.loaderVersionOptions

            delegate: Rectangle {
                width: ListView.view.width
                height: 38
                radius: Theme.radiusSmall
                color: modelData === loaderDialog.selectedVersion ? Theme.primarySoft : "#66FFFFFF"
                border.width: 1
                border.color: modelData === loaderDialog.selectedVersion ? Theme.primary : Theme.glassBorder

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    verticalAlignment: Text.AlignVCenter
                    text: modelData
                    color: modelData === loaderDialog.selectedVersion ? Theme.primary : Theme.text
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: loaderDialog.selectedVersion = modelData
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.gap

        // 左：分类
        GlassPanel {
            backdrop: page.backdrop
            Layout.preferredWidth: 200
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                Repeater {
                    model: [
                        { key: "version", kind: "", label: qsTr("游戏版本") },
                        { key: "mod", kind: "mod", label: qsTr("Mod") },
                        { key: "shader", kind: "shader", label: qsTr("光影") },
                        { key: "resourcepack", kind: "resourcepack", label: qsTr("材质包") }
                    ]

                    delegate: Rectangle {
                        id: categoryItem
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        radius: Theme.radius
                        color: page.category === modelData.key ? Theme.primarySoft : "#66FFFFFF"
                        border.width: 1
                        border.color: page.category === modelData.key ? Theme.primary : "#55FFFFFF"

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            verticalAlignment: Text.AlignVCenter
                            text: modelData.label
                            font.pixelSize: 14
                            color: page.category === modelData.key ? Theme.primary : Theme.text
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: page.activate(modelData.key, modelData.kind)
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        // 右：内容
        GlassPanel {
            backdrop: page.backdrop
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 12

                // 版本分类
                RowLayout {
                    Layout.fillWidth: true
                    visible: page.category === "version"

                    Text {
                        text: qsTr("游戏版本")
                        font.pixelSize: 20
                        font.bold: true
                        color: Theme.text
                        Layout.fillWidth: true
                    }

                    Repeater {
                        model: [qsTr("正式版"), qsTr("测试版"), qsTr("愚人节版")]

                        delegate: Button {
                            id: kindTab
                            text: modelData
                            checkable: true
                            checked: page.versionKind === index
                            implicitHeight: 34
                            implicitWidth: 88
                            padding: 0
                            onClicked: page.versionKind = index

                            contentItem: Text {
                                text: kindTab.text
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                color: kindTab.checked ? "#FFFFFF" : Theme.primary
                            }

                            background: Rectangle {
                                radius: Theme.radiusSmall
                                color: kindTab.checked ? Theme.primary : "#88FFFFFF"
                                border.width: 1
                                border.color: Theme.primary
                            }
                        }
                    }
                }

                // 其它分类：搜索
                RowLayout {
                    Layout.fillWidth: true
                    visible: page.category !== "version"
                    spacing: 10

                    SoftComboBox {
                        id: sourceBox
                        Layout.preferredWidth: 170
                        model: bridge.contentSources
                        onActivated: bridge.selectContentSource(index)
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        radius: Theme.radiusSmall
                        color: "#88FFFFFF"
                        border.width: 1
                        border.color: "#55FFFFFF"

                        TextField {
                            id: searchField
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            placeholderText: qsTr("搜索 Mod / 光影 / 材质包")
                            background: null
                            placeholderTextColor: Theme.textSecondary
                            font.pixelSize: 14
                            color: Theme.text
                            onAccepted: bridge.searchContent(text, page.kind)
                        }
                    }

                    Button {
                        text: qsTr("搜索")
                        enabled: !bridge.downloadBusy
                        implicitWidth: 100
                        implicitHeight: 40
                        contentItem: Text {
                            text: parent.text
                            color: "#FFFFFF"
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: parent.enabled ? Theme.primary : "#99FFFFFF"
                        }
                        onClicked: bridge.searchContent(searchField.text, page.kind)
                    }
                }

                // 列表
                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: page.category !== "version"
                           ? bridge.contentResults
                           : (page.versionKind === 0 ? bridge.releaseVersions
                                                     : (page.versionKind === 1 ? bridge.snapshotVersions
                                                                               : bridge.aprilVersions))

                    delegate: Rectangle {
                        id: versionRow
                        property string versionId: modelData
                        width: ListView.view.width
                        height: 46
                        radius: Theme.radius
                        color: "#66FFFFFF"
                        border.width: 1
                        border.color: "#55FFFFFF"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 8
                            spacing: 8

                            Text {
                                Layout.fillWidth: true
                                text: modelData
                                elide: Text.ElideRight
                                font.pixelSize: 14
                                color: Theme.text
                            }

                            // 加载器：默认原版，点开可切换（菜单可扩展）
                            Button {
                                id: loaderButton
                                visible: page.category === "version"
                                text: page.loaderFor(versionRow.versionId)
                                enabled: !bridge.downloadBusy
                                implicitWidth: 84
                                implicitHeight: 30
                                padding: 0
                                onClicked: loaderMenu.popup()

                                contentItem: Text {
                                    text: loaderButton.text
                                    color: Theme.primary
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }

                                background: Rectangle {
                                    radius: Theme.radiusSmall
                                    color: loaderButton.hovered ? Theme.primarySoft : "#88FFFFFF"
                                    border.width: 1
                                    border.color: Theme.primary
                                }

                                SoftMenu {
                                    id: loaderMenu
                                    y: loaderButton.height
                                    width: 160

                                    Repeater {
                                        model: ["原版", "Fabric", "Forge", "NeoForge"]

                                        delegate: SoftMenuItem {
                                            text: modelData
                                            onTriggered: page.chooseLoader(versionRow.versionId, modelData)
                                        }
                                    }
                                }
                            }

                            SoftButton {
                                text: qsTr("服务端")
                                visible: page.category === "version"
                                enabled: !bridge.downloadBusy
                                implicitWidth: 68
                                implicitHeight: 30
                                textSize: 12
                                onClicked: bridge.installServer(page.versionKind, index,
                                                               page.loaderFor(versionRow.versionId))
                            }

                            Button {
                                text: qsTr("安装")
                                enabled: !bridge.downloadBusy
                                implicitWidth: 76
                                implicitHeight: 30
                                contentItem: Text {
                                    text: parent.text
                                    color: "#FFFFFF"
                                    font.pixelSize: 13
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: Theme.radiusSmall
                                    color: parent.enabled ? Theme.primary : "#99FFFFFF"
                                }
                                onClicked: page.category === "version"
                                               ? page.installVersion(page.versionKind, index,
                                                                    page.loaderFor(versionRow.versionId),
                                                                    versionRow.versionId)
                                               : bridge.installContent(index)
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: list.count === 0
                        text: page.category === "version"
                              ? (bridge.downloadBusy ? qsTr("正在获取版本清单…")
                                                     : qsTr("没有拿到版本清单，检查网络或软件源"))
                              : qsTr("输入关键词搜索")
                        color: Theme.textSecondary
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: bridge.downloadStatus
                    font.pixelSize: 13
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 6
                    radius: 3
                    color: Theme.border
                    visible: bridge.downloadBusy || bridge.downloadFailed

                    Rectangle {
                        width: bridge.downloadFailed ? parent.width
                                                     : parent.width * Math.max(0, Math.min(1, bridge.downloadProgress))
                        height: parent.height
                        radius: 3
                        color: bridge.downloadFailed ? Theme.danger : Theme.primary
                    }
                }
            }
        }
    }
}
