import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import "../components"

Rectangle {
    id: root

    property var currentDevice: null
    property bool isDark: true

    function addDataPoint(metric, value, timestamp) {
        if (currentDevice) {
            chart.addDataPoint(metric, value, timestamp);
        }
    }
    function showDevice(deviceId) {
        // Find device from model
        for (var i = 0; i < deviceModel.rowCount(); i++) {
            var device = deviceModel.deviceAt(i);
            if (device.id === deviceId) {
                currentDevice = device;
                return;
            }
        }
    }

    color: isDark ? "#1e1e2e" : "#f5f5f5"

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            anchors.margins: 16
            spacing: 16
            width: parent.width

            // Back button
            RowLayout {
                Layout.fillWidth: true

                Button {
                    Material.foreground: root.isDark ? "#89b4fa" : "#3b82f6"
                    flat: true
                    text: "← 返回"

                    onClicked: {
                        currentDevice = null;
                        stackView.currentIndex = 0;
                        sidebar.currentIndex = 0;
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
                Label {
                    color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                    font.bold: true
                    font.pixelSize: 20
                    text: currentDevice ? currentDevice.name : "设备详情"
                }
                Item {
                    Layout.fillWidth: true
                }
            }

            // Device status card
            Rectangle {
                Layout.fillWidth: true
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1
                color: root.isDark ? "#313244" : "#ffffff"
                radius: 12

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 16

                    // Device info header
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        // Status indicator
                        StatusIndicator {
                            height: 12
                            status: currentDevice ? currentDevice.status : 0
                            width: 12
                        }
                        ColumnLayout {
                            spacing: 4

                            Label {
                                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                font.bold: true
                                font.pixelSize: 18
                                text: currentDevice ? currentDevice.name : ""
                            }
                            Label {
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.pixelSize: 12
                                text: currentDevice ? "ID: " + currentDevice.id : ""
                            }
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Label {
                            color: currentDevice ? (currentDevice.status === 0 ? "#9E9E9E" : currentDevice.status === 1 ? "#4CAF50" : currentDevice.status === 2 ? "#FF5722" : "#FFC107") : "#9E9E9E"
                            font.bold: true
                            font.pixelSize: 14
                            text: currentDevice ? (currentDevice.status === 0 ? "离线" : currentDevice.status === 1 ? "在线" : currentDevice.status === 2 ? "告警" : "维护") : ""
                        }
                    }

                    // Divider
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: root.isDark ? "#45475a" : "#e0e0e0"
                    }

                    // Device metrics
                    GridLayout {
                        Layout.fillWidth: true
                        columnSpacing: 24
                        columns: 4
                        rowSpacing: 12

                        MetricCard {
                            Layout.fillWidth: true
                            icon: "🌡️"
                            isDark: root.isDark
                            title: "温度"
                            value: currentDevice ? currentDevice.temperature.toFixed(1) + "°C" : "--"
                        }
                        MetricCard {
                            Layout.fillWidth: true
                            icon: "💧"
                            isDark: root.isDark
                            title: "湿度"
                            value: currentDevice ? currentDevice.humidity.toFixed(0) + "%" : "--"
                        }
                        MetricCard {
                            Layout.fillWidth: true
                            icon: "🔋"
                            isDark: root.isDark
                            title: "电量"
                            value: currentDevice ? currentDevice.battery.toFixed(0) + "%" : "--"
                        }
                        MetricCard {
                            Layout.fillWidth: true
                            icon: "📊"
                            isDark: root.isDark
                            title: "上报次数"
                            value: currentDevice ? currentDevice.reportCount.toString() : "--"
                        }
                    }

                    // Last update
                    Label {
                        color: root.isDark ? "#a6adc8" : "#666666"
                        font.pixelSize: 12
                        text: "最后更新: " + (currentDevice ? currentDevice.lastSeen : "--")
                    }
                }
            }

            // Shadow panel
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1
                color: root.isDark ? "#313244" : "#ffffff"
                radius: 12

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Label {
                        color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                        font.bold: true
                        font.pixelSize: 16
                        text: "设备影子 (Shadow)"
                    }
                    RowLayout {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        spacing: 16

                        // Desired state
                        ColumnLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                color: "#89b4fa"
                                font.bold: true
                                font.pixelSize: 12
                                text: "期望状态 (Desired)"
                            }
                            Rectangle {
                                Layout.fillHeight: true
                                Layout.fillWidth: true
                                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                                border.width: 1
                                color: root.isDark ? "#1e1e2e" : "#f8f9fa"
                                radius: 8

                                ScrollView {
                                    anchors.fill: parent
                                    anchors.margins: 8

                                    Label {
                                        color: root.isDark ? "#a6e3a1" : "#2e7d32"
                                        font.family: "Monaco"
                                        font.pixelSize: 12
                                        text: '{\n  "temperature": 24.5,\n  "humidity": 65,\n  "mode": "auto"\n}'
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                        }

                        // Reported state
                        ColumnLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                color: "#a6e3a1"
                                font.bold: true
                                font.pixelSize: 12
                                text: "上报状态 (Reported)"
                            }
                            Rectangle {
                                Layout.fillHeight: true
                                Layout.fillWidth: true
                                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                                border.width: 1
                                color: root.isDark ? "#1e1e2e" : "#f8f9fa"
                                radius: 8

                                ScrollView {
                                    anchors.fill: parent
                                    anchors.margins: 8

                                    Label {
                                        color: root.isDark ? "#89b4fa" : "#1565c0"
                                        font.family: "Monaco"
                                        font.pixelSize: 12
                                        text: '{\n  "temperature": 24.3,\n  "humidity": 64,\n  "mode": "auto",\n  "battery": 85\n}'
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Chart panel
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 250
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1
                color: root.isDark ? "#313244" : "#ffffff"
                radius: 12

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Label {
                        color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                        font.bold: true
                        font.pixelSize: 16
                        text: "实时监控"
                    }
                    RealtimeChart {
                        id: chart

                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        isDark: root.isDark
                    }
                }
            }

            // Control panel
            Rectangle {
                Layout.fillWidth: true
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1
                color: root.isDark ? "#313244" : "#ffffff"
                radius: 12

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Label {
                        color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                        font.bold: true
                        font.pixelSize: 16
                        text: "发送命令"
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        TextField {
                            id: cmdInput

                            Layout.fillWidth: true
                            Material.foreground: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            placeholderText: "输入命令 JSON..."
                        }
                        Button {
                            Material.background: "#89b4fa"
                            Material.foreground: "white"
                            text: "发送"

                            onClicked: {
                                console.log("Sending command:", cmdInput.text);
                                cmdInput.text = "";
                            }
                        }
                    }
                }
            }
        }
    }
}
