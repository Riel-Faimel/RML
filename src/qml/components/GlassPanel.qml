import QtQuick
import QtQuick.Effects

// 毛玻璃面板：把背景图对应区域裁出来 → 模糊 → 白色薄涂层 → 圆角
Item {
    id: root

    property Item backdrop: null
    property real corner: 16
    property bool squareTopLeft: false
    property bool squareTopRight: false
    property bool squareBottomLeft: false
    property bool squareBottomRight: false
    property bool showBorder: true
    property bool blurred: false
    property real tintOpacity: 0.30
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

    Item {
        id: maskShape
        anchors.fill: parent
        visible: false
        layer.enabled: true

        Rectangle {
            anchors.fill: parent
            radius: root.corner
            color: "white"
        }

        // 需要直角的角落用同色方块补齐，遮罩就变成非圆角
        Rectangle {
            width: root.corner
            height: root.corner
            color: "white"
            visible: root.squareTopLeft
            x: 0; y: 0
        }
        Rectangle {
            width: root.corner
            height: root.corner
            color: "white"
            visible: root.squareTopRight
            x: parent.width - width; y: 0
        }
        Rectangle {
            width: root.corner
            height: root.corner
            color: "white"
            visible: root.squareBottomLeft
            x: 0; y: parent.height - height
        }
        Rectangle {
            width: root.corner
            height: root.corner
            color: "white"
            visible: root.squareBottomRight
            x: parent.width - width; y: parent.height - height
        }
    }

    MultiEffect {
        anchors.fill: parent
        visible: root.blurred
        enabled: root.blurred
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
        color: Theme.surface
        opacity: root.blurred ? root.tintOpacity : Math.min(root.tintOpacity, 0.24)
    }

    Rectangle {
        anchors.fill: parent
        radius: root.corner
        color: "transparent"
        border.width: root.showBorder ? 1 : 0
        border.color: "#88FFFFFF"
    }

    Item {
        id: contentHost
        anchors.fill: parent
        clip: true
    }

    onWidthChanged: crop.scheduleUpdate()
    onHeightChanged: crop.scheduleUpdate()
    onXChanged: crop.scheduleUpdate()
    onYChanged: crop.scheduleUpdate()
    onBackdropChanged: crop.scheduleUpdate()

    // ShaderEffectSource 是 live=false；背景图换源后必须重新抓取，否则面板还是旧图。
    Connections {
        target: root.backdrop
        ignoreUnknownSignals: true
        function onSourceChanged() { crop.scheduleUpdate() }
        function onStatusChanged() {
            if (root.backdrop.status === Image.Ready) crop.scheduleUpdate()
        }
    }
}
