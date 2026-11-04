import QtQuick
import QtQuick.Layouts
import "../.."
import "../../components"

// 下载页左栏：分类。
GlassPanel {
    id: root

    property string category: "version"

    signal activated(string key, string kind)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 40

            Text {
                text: qsTr("下载")
                font.pixelSize: 20
                font.bold: true
                color: Theme.text
                Layout.fillWidth: true
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.divider
        }

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
                color: root.category === modelData.key ? Theme.controlFill : Theme.glassFill
                border.width: 1
                border.color: root.category === modelData.key ? Theme.primary : Theme.glassBorder

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    verticalAlignment: Text.AlignVCenter
                    text: modelData.label
                    font.pixelSize: 14
                    color: root.category === modelData.key ? Theme.primary : Theme.text
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.activated(modelData.key, modelData.kind)
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
