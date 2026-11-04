import QtQuick
import QtQuick.Layouts
import ".."
import "settings"

Item {
    id: page

    property Item backdrop: Window.window ? Window.window.backdropRef : null

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.gap

        // 左：外观 + Java
        ColumnLayout {
            Layout.preferredWidth: 360
            Layout.maximumWidth: 360
            Layout.fillHeight: true
            spacing: Theme.gap

            AppearancePanel {
                backdrop: page.backdrop
                Layout.fillWidth: true
                Layout.preferredHeight: 184
            }

            JavaPanel {
                backdrop: page.backdrop
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }

        // 右：软件源
        SourcePanel {
            backdrop: page.backdrop
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
