import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."

// 项目风格的对话框：白底圆角、标题、右下角“取消 / 确定”。
// 调用方直接往里面加内容即可（会进 contentItem 的 ColumnLayout）。
Dialog {
    id: control

    property string okText: qsTr("确定")
    property string cancelText: qsTr("取消")
    property bool showCancel: true
    property bool side: false

    modal: true
    anchors.centerIn: side ? undefined : Overlay.overlay
    x: side ? Overlay.overlay.width - width : 0
    y: 0
    height: side ? Overlay.overlay.height : implicitHeight
    padding: 18
    width: 440

    Overlay.modal: Rectangle { color: "#55000000" }

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.surface
        border.width: 1
        border.color: Theme.border
    }

    header: Item {
        implicitHeight: 38

        Text {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            text: control.title
            color: Theme.text
            font.pixelSize: 18
            font.bold: true
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    footer: Item {
        implicitHeight: 46

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            spacing: 8

            Item { Layout.fillWidth: true }

            SoftButton {
                text: control.cancelText
                visible: control.showCancel
                onClicked: control.reject()
            }

            SoftButton {
                text: control.okText
                primary: true
                onClicked: control.accept()
            }
        }
    }

    contentItem: ColumnLayout {
        width: control.availableWidth
        spacing: 10
    }
}
