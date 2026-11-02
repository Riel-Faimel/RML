pragma Singleton

import QtQuick

// 统一风格：蓝白灰、圆润。
QtObject {
    readonly property color primary: "#2F6FED"
    readonly property color primaryPressed: "#2459C8"
    readonly property color primarySoft: "#E8F0FE"
    readonly property color background: "#F4F6FA"
    readonly property color surface: "#FFFFFF"
    readonly property color border: "#E3E8EF"
    readonly property color text: "#1F2A37"
    readonly property color textSecondary: "#6B7684"

    readonly property int radiusLarge: 16
    readonly property int radius: 10
    readonly property int radiusSmall: 8
    readonly property int gap: 12
    readonly property int headerHeight: 60
    readonly property int pagePadding: 24
}
