import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "settings"

Item {
    id: page

    property Item backdrop: Window.window ? Window.window.backdropRef : null
    property int contentWidth: Math.max(0, width - Theme.pagePadding * 2)

    // 设置页单列，从上到下排列；内容再多也只纵向滚动。
    Flickable {
        id: settingsScroll
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(height, content.implicitHeight + Theme.pagePadding * 2)
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: content
            x: Theme.pagePadding
            y: Theme.pagePadding
            width: page.contentWidth
            height: implicitHeight
            spacing: Theme.gap

            AppearancePanel {
                backdrop: page.backdrop
                Layout.fillWidth: true
                Layout.preferredHeight: 210
            }

            JavaPanel {
                backdrop: page.backdrop
                Layout.fillWidth: true
                Layout.preferredHeight: 330
            }

            ServerPanel {
                backdrop: page.backdrop
                Layout.fillWidth: true
                Layout.preferredHeight: 320
            }

            MaintenancePanel {
                backdrop: page.backdrop
                Layout.fillWidth: true
                Layout.preferredHeight: 310
            }

            SourcePanel {
                backdrop: page.backdrop
                Layout.fillWidth: true
                Layout.preferredHeight: 700
            }
        }
    }
}
