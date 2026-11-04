import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../.."
import "../../components"

// 服务器页：server.properties 全量编辑。
SoftDialog {
    id: dialog

    title: qsTr("服务器属性（server.properties）")
    width: 640
    height: 560

    signal saved()

    ListModel { id: propsModel }

    // server.properties 全部条目：读出来填模型，保存时再拼回去
    function openForInstance() {
        propsModel.clear()
        var items = serverBridge.readServerProperties()
        for (var i = 0; i < items.length; ++i) {
            propsModel.append({ key: String(items[i].key), value: String(items[i].value) })
        }
        newKeyField.text = ""
        newValueField.text = ""
        open()
    }

    function saveProperties() {
        var entries = []
        for (var i = 0; i < propsModel.count; ++i) {
            var item = propsModel.get(i)
            entries.push({ key: item.key, value: item.value })
        }
        return serverBridge.saveServerProperties(entries)
    }

    function addProperty() {
        var key = newKeyField.text.trim()
        if (key.length === 0) return
        propsModel.append({ key: key, value: newValueField.text })
        newKeyField.text = ""
        newValueField.text = ""
        newKeyField.forceActiveFocus()
    }

    onAccepted: {
        if (dialog.saveProperties()) dialog.saved()
    }

    ListView {
        id: propsList
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        spacing: 6
        model: propsModel

        delegate: RowLayout {
            width: propsList.width
            height: 38
            spacing: 8

            Text {
                Layout.preferredWidth: 180
                Layout.minimumWidth: 100
                text: model.key
                elide: Text.ElideRight
                color: Theme.text
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
            }

            SoftField {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: model.value
                onTextEdited: propsModel.setProperty(index, "value", text)
            }

            SoftButton {
                text: qsTr("删除")
                onClicked: propsModel.remove(index)
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        SoftField {
            id: newKeyField
            Layout.preferredWidth: 180
            Layout.minimumWidth: 100
            placeholderText: qsTr("新键名")
        }

        SoftField {
            id: newValueField
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            placeholderText: qsTr("值")
            onAccepted: dialog.addProperty()
        }

        SoftButton {
            text: qsTr("添加")
            primary: true
            onClicked: dialog.addProperty()
        }
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("online-mode=true 为正版验证。启动器目前用离线账号启动游戏，请保持 false；改完重启服务器生效。")
        color: Theme.textSecondary
        font.pixelSize: 11
        wrapMode: Text.WordWrap
    }
}
