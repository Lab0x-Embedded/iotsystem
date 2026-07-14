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
            width: parent.width
            spacing: 16

            // Gauge cards row
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Repeater {
                    model: ListModel {
                        ListElement { title: "平均温度"; val: 24.5; unit: "°C"; min: 0; max: 50; accent: "#3874F7" }
                        ListElement { title: "平均湿度"; val: 65; unit: "%"; min: 0; max: 100; accent: "#4CAF50" }
                        ListElement { title: "平均电量"; val: 78; unit: "%"; min: 0; max: 100; accent: "#f9e2af" }
                        ListElement { title: "在线率"; val: 85; unit: "%"; min: 0; max: 100; accent: "#cba6f7" }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 200
                        radius: 4
                        color: root.isDark ? "#313244" : "#ffffff"
                        border.color: root.isDark ? "#45475a" : "#e0e0e0"
                        border.width: 1

                        GaugeWidget {
                            anchors.fill: parent
                            anchors.margins: 8
                            label: model.title
                            value: model.val
                            unit: model.unit
                            minValue: model.min
                            maxValue: model.max
                            color: model.accent
                            isDark: root.isDark
                        }
                    }
                }
            }

            // Realtime chart
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                radius: 12
                color: root.isDark ? "#313244" : "#ffffff"
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Label {
                        text: "实时数据"
                        font.pixelSize: 16
                        font.bold: true
                        color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                    }

                    RealtimeChart {
                        id: realtimeChart
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        isDark: root.isDark
                    }
                }
            }

            // Top devices table
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 280
                radius: 12
                color: root.isDark ? "#313244" : "#ffffff"
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Label {
                        text: "温度 Top 5"
                        font.pixelSize: 16
                        font.bold: true
                        color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                    }

                    TopDevicesTable {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        isDark: root.isDark
                    }
                }
            }
        }
    }
}
