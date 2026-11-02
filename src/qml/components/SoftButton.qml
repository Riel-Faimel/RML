import QtQuick
import QtQuick.Controls
import ".."

// 统一样式的按钮：主操作实心蓝，次操作半透明白 + 蓝色描边。
Button {
    id: control

    property bool primary: false
    property int textSize: 13

    implicitWidth: Math.max(72, contentItem.implicitWidth + 24)
    implicitHeight: 34
    hoverEnabled: true
    padding: 0
    font.pixelSize: textSize

    contentItem: Text {
        text: control.text
        font.pixelSize: control.textSize
        font.bold: control.primary
        color: !control.enabled ? Theme.textSecondary
                                : (control.primary ? Theme.surface : Theme.primary)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        leftPadding: 4
        rightPadding: 4
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: !control.enabled
                   ? (control.primary ? "#99FFFFFF" : "#55FFFFFF")
                   : (control.primary
                          ? (control.pressed ? Theme.primaryPressed : Theme.primary)
                          : (control.hovered || control.down ? Theme.primarySoft : "#88FFFFFF"))
        border.width: control.primary ? 0 : 1
        border.color: control.enabled ? Theme.primary : "#55FFFFFF"
    }
}
