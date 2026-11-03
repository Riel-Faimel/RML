import QtQuick
import QtQuick.Controls
import ".."

// 统一样式的输入框
TextField {
    id: control

    implicitHeight: 34
    leftPadding: 10
    rightPadding: 10
    font.pixelSize: 12
    color: Theme.text
    placeholderTextColor: Theme.textSecondary
    selectionColor: Theme.primary
    selectedTextColor: "#FFFFFF"

    background: Rectangle {
        radius: Theme.radiusSmall
        color: "#E6FFFFFF"
        border.width: 1
        border.color: control.activeFocus ? Theme.primary : Theme.border
    }
}
