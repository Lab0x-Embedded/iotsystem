import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property bool isDark: true

    color: "transparent"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.preferredHeight: 36
            spacing: 12

            Label {
                Layout.preferredWidth: 60
                color: root.isDark ? "#a6adc8" : "#666666"
                font.bold: true
                font.pixelSize: 12
                text: "排名"
            }
            Label {
                Layout.preferredWidth: 100
                color: root.isDark ? "#a6adc8" : "#666666"
                font.bold: true
                font.pixelSize: 12
                text: "设备ID"
            }
            Label {
                Layout.fillWidth: true
                color: root.isDark ? "#a6adc8" : "#666666"
                font.bold: true
                font.pixelSize: 12
                text: "名称"
            }
            Label {
                Layout.preferredWidth: 80
                color: root.isDark ? "#a6adc8" : "#666666"
                font.bold: true
                font.pixelSize: 12
                text: "温度"
            }
        }

        // Divider
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.isDark ? "#45475a" : "#e0e0e0"
        }

        // Data rows
        Repeater {
            model: 5

            delegate: Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                color: index % 2 === 0 ? "transparent" : (root.isDark ? "#2a2a3c" : "#f8f9fa")

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 12

                    // Rank badge - match header width
                    Rectangle {
                        Layout.preferredWidth: 60
                        Layout.preferredHeight: 24
                        radius: 4
                        color: index === 0 ? "#f9e2af" : index === 1 ? "#c0c0c0" : index === 2 ? "#cd7f32" : "transparent"

                        Label {
                            anchors.centerIn: parent
                            color: index < 3 ? "#1e1e2e" : (root.isDark ? "#cdd6f4" : "#1e1e2e")
                            font.bold: index < 3
                            font.pixelSize: 11
                            text: (index + 1).toString()
                        }
                    }
                    Label {
                        Layout.preferredWidth: 100
                        color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                        font.pixelSize: 12
                        text: "DEV-" + (1001 + index)
                    }
                    Label {
                        Layout.fillWidth: true
                        color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                        font.pixelSize: 12
                        text: "传感器 " + (index + 1)
                    }
                    Label {
                        Layout.preferredWidth: 80
                        color: "#f38ba8"
                        font.pixelSize: 12
                        text: (28.5 - index * 1.2).toFixed(1) + "°C"
                    }
                }
            }
        }
    }
}
