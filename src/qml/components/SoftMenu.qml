import QtQuick
import QtQuick.Controls
import ".."

// 和 SoftComboBox 弹层一致的菜单
Menu {
    id: control

    padding: 4
    width: 180

    background: Rectangle {
        implicitWidth: 180
        radius: Theme.radiusSmall
        color: Theme.popupFill
        border.width: 1
        border.color: Theme.border
    }
}
