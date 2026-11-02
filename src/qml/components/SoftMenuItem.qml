import QtQuick
import QtQuick.Controls
import ".."

// 和 SoftComboBox 弹层一致的菜单项
MenuItem {
    id: control

    width: parent ? parent.width - 8 : 172
    height: 34

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.highlighted ? Theme.primarySoft : "transparent"
    }

    contentItem: Text {
        leftPadding: 8
        rightPadding: 8
        text: control.text
        color: control.highlighted ? Theme.primary : Theme.text
        font.pixelSize: 13
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
