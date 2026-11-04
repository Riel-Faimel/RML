import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../.."
import "../../components"

// 下载页内容结果：图标、标题、简介、安装。
Rectangle {
    id: root

    property int index: -1
    property string title: ""
    property string iconUrl: ""
    property string description: ""

    signal installRequested()

    width: 0
    height: 74
    radius: Theme.radius
    color: Theme.glassFill
    border.width: 1
    border.color: Theme.glassBorder

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 8
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            radius: Theme.radiusSmall
            color: Theme.glassHoverFill
            visible: root.iconUrl.length === 0

            Text {
                anchors.centerIn: parent
                text: root.title.length > 0 ? root.title.charAt(0) : "?"
                font.pixelSize: 16
                font.bold: true
                color: Theme.primary
            }
        }

        Image {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            visible: root.iconUrl.length > 0
            source: root.iconUrl
            sourceSize.width: 40
            sourceSize.height: 40
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: root.title
                elide: Text.ElideRight
                font.pixelSize: 14
                color: Theme.text
            }

            Text {
                Layout.fillWidth: true
                text: root.description
                visible: root.description.length > 0
                elide: Text.ElideRight
                maximumLineCount: 2
                wrapMode: Text.Wrap
                font.pixelSize: 11
                color: Theme.textSecondary
            }
        }

        Button {
            text: qsTr("安装")
            enabled: !bridge.downloadBusy
            implicitWidth: 76
            implicitHeight: 30
            contentItem: Text {
                text: parent.text
                color: Theme.primaryText
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: Theme.radiusSmall
                color: parent.enabled ? Theme.primary : Theme.disabledFill
            }
            onClicked: root.installRequested()
        }
    }
}
