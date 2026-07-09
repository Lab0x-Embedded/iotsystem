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
            Layout.preferredHeight: 60
            color: "transparent"

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 4

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    color: root.isDark ? "#89b4fa" : "#3b82f6"
                    font.bold: true
                    font.pixelSize: 24
                    text: "IoT"
                }
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    color: root.isDark ? "#a6adc8" : "#666666"
                    font.pixelSize: 11
                    text: "Device Manager"
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
                            font.pixelSize: 14
                            font.weight: root.currentIndex === model.idx ? Font.Medium : Font.Normal
                            text: model.name
                        }

                        // Badge for alarm
                        Rectangle {
                            Layout.preferredHeight: 20
                            Layout.preferredWidth: 20
                            color: "#f38ba8"
                            radius: 10
                            visible: model.idx === 3 && alarmModel && alarmModel.unacknowledgedCount > 0

                            Label {
                                anchors.centerIn: parent
                                color: "white"
                                font.bold: true
                                font.pixelSize: 10
                                text: alarmModel ? (alarmModel.unacknowledgedCount > 99 ? "99+" : alarmModel.unacknowledgedCount.toString()) : "0"
                            }
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
                        icon: "📱"
                        idx: 1
                        name: "设备详情"
                    }
                    ListElement {
                        icon: "📈"
                        idx: 2
                        name: "数据面板"
                    }
                    ListElement {
                        icon: "🔔"
                        idx: 3
                        name: "告警中心"
                    }
                }
            }
        }
        Item {
            Layout.fillHeight: true
        }

        // Theme toggle
        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 12
            Layout.preferredHeight: 48
            color: root.isDark ? "#313244" : "#f0f0f0"
            radius: 8

            RowLayout {
                anchors.centerIn: parent
                spacing: 8

                Label {
                    font.pixelSize: 16
                    text: root.isDark ? "🌙" : "☀️"
                }
                Label {
                    color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                    font.pixelSize: 12
                    text: root.isDark ? "深色模式" : "浅色模式"
                }
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor

                onClicked: root.themeToggle()
            }
        }

        // Version info
        Label {
            Layout.bottomMargin: 12
            Layout.fillWidth: true
            color: root.isDark ? "#585b70" : "#999999"
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
            text: "v2.0.0"
        }
    }
}
