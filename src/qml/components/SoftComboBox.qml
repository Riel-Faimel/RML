import QtQuick
import QtQuick.Controls
import ".."

// 统一风格的下拉框：默认 Qt 样式和这套界面不搭，会把弹层也一起重画
ComboBox {
    id: control

    implicitHeight: 36

    palette.text: Theme.text
    palette.window: "#FFFFFF"
    palette.base: "#FFFFFF"
    palette.button: "#FFFFFF"
    palette.highlight: Theme.primary
    palette.highlightedText: "#FFFFFF"

    background: Rectangle {
        radius: Theme.radiusSmall
        color: "#88FFFFFF"
        border.width: 1
        border.color: control.hovered ? Theme.primary : "#55FFFFFF"
    }

    contentItem: Text {
        leftPadding: 12
        rightPadding: control.indicator ? control.indicator.width + 8 : 12
        text: control.displayText
        color: Theme.text
        font.pixelSize: 13
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - width - 10
        y: (control.height - height) / 2
        text: "▾"
        color: Theme.textSecondary
        font.pixelSize: 12
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 320)
        padding: 4

        background: Rectangle {
            radius: Theme.radiusSmall
            color: "#F7F9FC"
            border.width: 1
            border.color: Theme.border
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }

    delegate: ItemDelegate {
        id: entry
        width: control.width - 8
        height: 34
        highlighted: control.highlightedIndex === index

        background: Rectangle {
            radius: Theme.radiusSmall
            color: entry.highlighted ? Theme.primarySoft : "transparent"
        }

        contentItem: Text {
            leftPadding: 8
            text: entry.text
            color: entry.highlighted ? Theme.primary : Theme.text
            font.pixelSize: 13
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
}
