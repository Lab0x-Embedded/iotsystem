import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

Rectangle {
    id: root

    property int currentIndex: 0
    property bool isDark: true

    signal pageSelected(int index)
    signal themeToggle

    border.color: isDark ? "#313244" : "#e0e0e0"
    border.width: 1
    color: isDark ? "#181825" : "#ffffff"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Logo/Title
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 72
            color: "transparent"

            RowLayout {
                anchors.centerIn: parent
                spacing: 10

                Image {
                    source: "../assets/logo.png"
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                }

                ColumnLayout {
                    spacing: 0

                    Label {
                        color: root.isDark ? "#89b4fa" : "#3b82f6"
                        font.bold: true
                        font.pixelSize: 18
                        text: "IoT Platform"
                    }
                    Label {
                        color: root.isDark ? "#6c7086" : "#999999"
                        font.pixelSize: 10
                        text: "Device Manager v2.0"
                    }
                }
            }
        }

        // Divider
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.preferredHeight: 1
            Layout.rightMargin: 16
            color: root.isDark ? "#313244" : "#e0e0e0"
        }

        // Navigation items
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: 16
            spacing: 4

            Repeater {
                delegate: Rectangle {
                    Layout.fillWidth: true
                    border.color: root.currentIndex === model.idx ? (root.isDark ? "#89b4fa" : "#3b82f6") : "transparent"
                    border.width: root.currentIndex === model.idx ? 1 : 0
                    color: root.currentIndex === model.idx ? (root.isDark ? "#313244" : "#e8f0fe") : "transparent"
                    height: 44
                    radius: 2

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 12

                        Label {
                            font.pixelSize: 18
                            text: model.icon
                        }
                        Label {
                            Layout.fillWidth: true
                            color: root.currentIndex === model.idx ? (root.isDark ? "#89b4fa" : "#3b82f6") : (root.isDark ? "#cdd6f4" : "#1e1e2e")
                            font.bold: root.currentIndex === model.idx
                            font.pixelSize: 13
                            text: model.name
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor

                        onClicked: {
                            root.currentIndex = model.idx;
                            root.pageSelected(model.idx);
                        }
                    }
                }
                model: ListModel {
                    ListElement {
                        icon: "📊"
                        idx: 0
                        name: "设备总览"
                    }
                    ListElement {
                        icon: "📁"
                        idx: 1
                        name: "分组管理"
                    }
                    ListElement {
                        icon: "📈"
                        idx: 3
                        name: "数据面板"
                    }
                    ListElement {
                        icon: "🔔"
                        idx: 4
                        name: "告警中心"
                    }
                }
            }
        }
        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
        }

        // Theme toggle
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: root.isDark ? "#181825" : "#ffffff"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16

                Label {
                    font.pixelSize: 16
                    text: root.isDark ? "🌙" : "☀️"
                }
                Label {
                    Layout.fillWidth: true
                    color: root.isDark ? "#a6adc8" : "#666666"
                    font.pixelSize: 13
                    text: root.isDark ? "深色模式" : "浅色模式"
                }
                Switch {
                    checked: root.isDark
                    scale: 0.6
                    transformOrigin: Item.Center

                    onCheckedChanged: root.themeToggle()
                }
            }
        }
    }
}
