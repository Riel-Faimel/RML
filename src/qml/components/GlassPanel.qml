import QtQuick
import QtQuick.Effects

// 毛玻璃面板：把背景图对应区域裁出来 → 模糊 → 白色薄涂层 → 圆角
Item {
    id: root

    property Item backdrop: null
    property real corner: 16
    property real tintOpacity: 0.52
    property real blurAmount: 0.85
    default property alias content: contentHost.data

    ShaderEffectSource {
        id: crop
        anchors.fill: parent
        sourceItem: root.backdrop
        visible: false
        live: false
        sourceRect: {
            if (!root.backdrop) return Qt.rect(0, 0, root.width, root.height)
            const p = root.mapToItem(root.backdrop, 0, 0)
            return Qt.rect(p.x, p.y, root.width, root.height)
        }
    }

    Rectangle {
        id: maskShape
        anchors.fill: parent
        radius: root.corner
        color: "white"
        visible: false
        layer.enabled: true
    }

    MultiEffect {
        anchors.fill: parent
        source: crop
        blurEnabled: true
        blur: root.blurAmount
        blurMax: 48
        maskEnabled: true
        maskSource: maskShape
    }

    Rectangle {
        anchors.fill: parent
        radius: root.corner
        color: "#FFFFFF"
        opacity: root.tintOpacity
    }

    Rectangle {
        anchors.fill: parent
        radius: root.corner
        color: "transparent"
        border.width: 1
        border.color: "#88FFFFFF"
    }

    Item {
        id: contentHost
        anchors.fill: parent
    }

    onWidthChanged: crop.scheduleUpdate()
    onHeightChanged: crop.scheduleUpdate()
    onXChanged: crop.scheduleUpdate()
    onYChanged: crop.scheduleUpdate()
}
