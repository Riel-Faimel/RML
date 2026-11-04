import QtQuick
import QtQuick.Controls

// 全局动作反馈：左下角浮层，类似 Edge 的链接提示。
Rectangle {
    id: toast

    property string message: ""
    property bool isError: false
    property string lastMessage: ""
    property int maxWidth: 480

    implicitWidth: Math.min(maxWidth, Math.max(180, label.implicitWidth + 24))
    implicitHeight: label.implicitHeight + 18
    width: implicitWidth
    height: implicitHeight
    radius: 8
    color: isError ? "#E6B42323" : "#E61F2937"
    border.width: 1
    border.color: isError ? "#FF8A8A" : "#44FFFFFF"
    opacity: 0
    visible: opacity > 0
    z: 1000

    function show(text, error) {
        if (!text || text.length === 0) return
        if (text === lastMessage) return
        lastMessage = text
        message = text
        isError = !!error
        opacity = 1
        hideTimer.restart()
    }

    function hide() {
        opacity = 0
    }

    Text {
        id: label
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        text: toast.message
        color: "#FFFFFF"
        font.pixelSize: 12
        wrapMode: Text.WordWrap
        maximumLineCount: 4
        elide: Text.ElideRight
    }

    Behavior on opacity {
        NumberAnimation { duration: 140 }
    }

    Timer {
        id: hideTimer
        interval: 2800
        onTriggered: toast.hide()
    }
}
