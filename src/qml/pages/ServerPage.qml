import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ".."
import "../components"

Item {
    id: page

    property Item backdrop: Window.window ? Window.window.backdropRef : null

    GlassPanel {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        backdrop: page.backdrop

        Text {
            anchors.centerIn: parent
            text: qsTr("服务器")
            color: Theme.textSecondary
            font.pixelSize: 18
        }
    }
}
