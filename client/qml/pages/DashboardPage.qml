import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import "../components"

Rectangle {
    id: root

    property bool isDark: true

    function addDataPoint(metric, value, timestamp) {
        realtimeChart.addDataPoint(metric, value, timestamp);
    }

    color: isDark ? "#1e1e2e" : "#f5f5f5"

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        padding: 16

        ColumnLayout {
            spacing: 16
            width: parent.width

            // Gauge cards row
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                GaugeWidget {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    color: "#89b4fa"
                    isDark: root.isDark
                    label: "平均温度"
                    maxValue: 50
                    minValue: 0
                    unit: "°C"
                    value: 24.5
                }
                GaugeWidget {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    color: "#a6e3a1"
                    isDark: root.isDark
                    label: "平均湿度"
                    maxValue: 100
                    minValue: 0
                    unit: "%"
                    value: 65
                }
                GaugeWidget {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    color: "#f9e2af"
                    isDark: root.isDark
                    label: "平均电量"
                    maxValue: 100
                    minValue: 0
                    unit: "%"
                    value: 78
                }
                GaugeWidget {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    color: "#cba6f7"
                    isDark: root.isDark
                    label: "在线率"
                    maxValue: 100
                    minValue: 0
                    unit: "%"
                    value: 85
                }
            }

            // Realtime chart
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 300
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
                        text: "实时数据"
                    }
                    RealtimeChart {
                        id: realtimeChart

                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        isDark: root.isDark
                    }
                }
            }

            // Top devices table
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
                        text: "温度 Top 5"
                    }
                    TopDevicesTable {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        isDark: root.isDark
                    }
                }
            }
        }
    }
}
