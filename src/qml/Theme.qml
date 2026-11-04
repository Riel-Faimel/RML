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
    readonly property color danger: "#D64545"
    readonly property color primaryText: "#FFFFFF"
    readonly property color glassFill: "#66FFFFFF"
    readonly property color glassBorder: "#55FFFFFF"
    readonly property color glassHoverFill: "#88FFFFFF"
    readonly property color disabledFill: "#99FFFFFF"
    readonly property color divider: "#33808080"

    readonly property int radiusLarge: 16
    readonly property int radius: 10
    readonly property int radiusSmall: 8
    readonly property int gap: 12
    readonly property int headerHeight: 60
    readonly property int pagePadding: 24
}
