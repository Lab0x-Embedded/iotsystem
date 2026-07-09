import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import "../components"

Rectangle {
    id: root

    property var deviceModel: null
    property bool isDark: true

    signal deviceSelected(string deviceId)

    color: isDark ? "#1e1e2e" : "#f5f5f5"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // Stats cards row
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            DashboardCard {
                Layout.fillWidth: true
                accentColor: "#89b4fa"
                isDark: root.isDark
                title: "设备总数"
                value: deviceModel ? deviceModel.totalCount.toString() : "0"
            }
            DashboardCard {
                Layout.fillWidth: true
                accentColor: "#a6e3a1"
                isDark: root.isDark
                title: "在线设备"
                value: deviceModel ? deviceModel.onlineCount.toString() : "0"
            }
            DashboardCard {
                Layout.fillWidth: true
                accentColor: "#9399b2"
                isDark: root.isDark
                title: "离线设备"
                value: deviceModel ? (deviceModel.totalCount - deviceModel.onlineCount - deviceModel.alarmCount).toString() : "0"
            }
            DashboardCard {
                Layout.fillWidth: true
                accentColor: "#f38ba8"
                isDark: root.isDark
                title: "告警设备"
                value: deviceModel ? deviceModel.alarmCount.toString() : "0"
            }
        }

        // Filter row
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Label {
                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                font.pixelSize: 13
                text: "筛选:"
            }
            ComboBox {
                id: statusFilter

                Layout.preferredWidth: 120
                Material.foreground: root.isDark ? "#cdd6f4" : "#1e1e2e"
                model: ["全部状态", "在线", "离线", "告警"]
            }
            Item {
                Layout.fillWidth: true
            }
            Label {
                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                font.pixelSize: 13
                text: "搜索:"
            }
            TextField {
                id: searchField

                Layout.preferredWidth: 250
                Material.foreground: root.isDark ? "#cdd6f4" : "#1e1e2e"
                placeholderText: "搜索设备 ID/名称..."
            }
        }

        // Device table
        Rectangle {
            Layout.fillHeight: true
            Layout.fillWidth: true
            border.color: root.isDark ? "#45475a" : "#e0e0e0"
            border.width: 1
            color: root.isDark ? "#313244" : "#ffffff"
            radius: 12

            ListView {
                id: deviceList

                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: deviceModel

                delegate: Rectangle {
                    color: index % 2 === 0 ? (root.isDark ? "#313244" : "#ffffff") : (root.isDark ? "#2a2a3c" : "#f8f9fa")
                    height: 48
                    width: deviceList.width

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 0

                        // Status indicator
                        StatusIndicator {
                            Layout.preferredHeight: 32
                            Layout.preferredWidth: 60
                            status: model.status
                        }
                        Label {
                            Layout.preferredWidth: 150
                            color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            font.pixelSize: 12
                            text: model.id
                        }
                        Label {
                            Layout.preferredWidth: 120
                            color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            font.pixelSize: 12
                            text: model.name
                        }
                        Label {
                            Layout.preferredWidth: 100
                            color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            font.pixelSize: 12
                            text: model.group
                        }
                        Label {
                            Layout.preferredWidth: 80
                            color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            font.pixelSize: 12
                            text: model.temperature + "°C"
                        }
                        Label {
                            Layout.preferredWidth: 80
                            color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            font.pixelSize: 12
                            text: model.humidity + "%"
                        }
                        Label {
                            Layout.preferredWidth: 80
                            color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            font.pixelSize: 12
                            text: model.battery + "%"
                        }
                        Label {
                            Layout.fillWidth: true
                            color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            font.pixelSize: 12
                            text: model.lastSeen
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor

                        onDoubleClicked: root.deviceSelected(model.id)
                    }
                }
                header: Rectangle {
                    color: root.isDark ? "#181825" : "#f8f9fa"
                    height: 48
                    width: deviceList.width

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 0

                        Label {
                            Layout.preferredWidth: 60
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 12
                            text: "状态"
                        }
                        Label {
                            Layout.preferredWidth: 150
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 12
                            text: "设备ID"
                        }
                        Label {
                            Layout.preferredWidth: 120
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 12
                            text: "名称"
                        }
                        Label {
                            Layout.preferredWidth: 100
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 12
                            text: "分组"
                        }
                        Label {
                            Layout.preferredWidth: 80
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 12
                            text: "温度"
                        }
                        Label {
                            Layout.preferredWidth: 80
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 12
                            text: "湿度"
                        }
                        Label {
                            Layout.preferredWidth: 80
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 12
                            text: "电量"
                        }
                        Label {
                            Layout.fillWidth: true
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 12
                            text: "最后上报"
                        }
                    }
                }
            }
        }
    }
}
