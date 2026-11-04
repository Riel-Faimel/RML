import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

ApplicationWindow {
    id: window

    width: 1100
    height: 720
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: qsTr("RML")
    color: "#0B1220"

    // 页面通过 Window.window.backdropRef 拿到背景，做毛玻璃
    property Item backdropRef: backdrop

    Image {
        id: backdrop
        anchors.fill: parent
        source: bridge.backgroundUrl
        fillMode: Image.PreserveAspectCrop
        asynchronous: false
    }

    ColumnLayout {
        id: shell
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: headerBar
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.headerHeight
            color: Theme.primary  // 顶栏：不透明蓝

            Row {
                id: navigation
                anchors.centerIn: parent
                spacing: 4

                Repeater {
                    model: [qsTr("启动"), qsTr("下载"), qsTr("服务器"), qsTr("设置")]

                    delegate: Button {
                        id: tab
                        text: modelData
                        checkable: true
                        checked: stack.currentIndex === index
                        hoverEnabled: true
                        implicitHeight: 38
                        implicitWidth: Math.max(84, contentItem.implicitWidth + 32)
                        padding: 0
                        onClicked: stack.currentIndex = index

                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: tab.checked ? "#FFFFFF" : (tab.hovered ? "#33FFFFFF" : "transparent")
                        }

                        contentItem: Text {
                            text: tab.text
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            color: tab.checked ? Theme.primary : "#FFFFFF"
                        }
                    }
                }
            }

            Button {
                id: taskButton
                anchors.right: accountButton.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                implicitWidth: 38
                implicitHeight: 38
                padding: 0
                hoverEnabled: true

                contentItem: Text {
                    text: "↓"
                    font.pixelSize: 22
                    color: taskButton.hovered || stack.currentIndex === 4 ? "#FFFFFF" : "#DDEAFF"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: stack.currentIndex === 4 ? "#33FFFFFF"
                                                    : (taskButton.hovered ? "#22FFFFFF" : "transparent")
                }

                onClicked: stack.currentIndex = 4
            }

            Button {
                id: accountButton
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("账号")
                flat: true
                implicitHeight: 38

                contentItem: Text {
                    text: accountButton.text
                    font.pixelSize: 14
                    color: "#FFFFFF"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: accountButton.hovered ? "#33FFFFFF" : "transparent"
                }
            }
        }

        StackLayout {
            id: stack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: 0

            Loader { source: "pages/LaunchPage.qml" }
            Loader { source: "pages/DownloadPage.qml" }
            Loader { source: "pages/ServerPage.qml" }
            Loader { source: "pages/SettingsPage.qml" }
            Loader { source: "pages/TaskPage.qml" }
        }
    }

    // 调试构建：等界面渲染完截一张图，方便在没有显示器的环境里核对
    Timer {
        running: typeof rmlDebug !== "undefined" && rmlDebug
        interval: 3500
        repeat: false
        onTriggered: shell.grabToImage(function(result) {
            result.saveToFile(rmlShotPath)
        })
    }
}
