import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

Rectangle {
    id: root

    property var alarmModel: null
    property bool isDark: true

    color: isDark ? "#1e1e2e" : "#f5f5f5"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // Header
        RowLayout {
            Layout.fillWidth: true

            Label {
                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                font.bold: true
                font.pixelSize: 20
                text: "告警中心"
            }
            Item {
                Layout.fillWidth: true
            }
            Label {
                color: "#f38ba8"
                font.pixelSize: 13
                text: "未确认: " + (alarmModel ? alarmModel.unacknowledgedCount : 0)
            }
        }

        // Filter row
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Label {
                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                font.pixelSize: 13
                text: "严重级别:"
            }
            ComboBox {
                id: severityFilter

                Layout.preferredWidth: 140
                Layout.preferredHeight: 36
                Material.foreground: root.isDark ? "#cdd6f4" : "#1e1e2e"
                model: ["全部", "CRITICAL", "WARNING", "INFO"]
            }
            Item {
                Layout.fillWidth: true
            }
            Button {
                Material.background: "#89b4fa"
                Material.foreground: "white"
                enabled: alarmList.currentIndex >= 0
                text: "确认选中"

                onClicked: {
                    if (alarmList.currentIndex >= 0) {
                        alarmModel.acknowledge(alarmList.currentIndex);
                    }
                }
            }
        }

        // Alarm table
        Rectangle {
            Layout.fillHeight: true
            Layout.fillWidth: true
            border.color: root.isDark ? "#45475a" : "#e0e0e0"
            border.width: 1
            color: root.isDark ? "#313244" : "#ffffff"
            radius: 12

            ListView {
                id: alarmList

                anchors.fill: parent
                anchors.margins: 1
                clip: true
                highlightFollowsCurrentItem: true
                model: alarmModel

                delegate: Rectangle {
                    color: index % 2 === 0 ? (root.isDark ? "#313244" : "#ffffff") : (root.isDark ? "#2a2a3c" : "#f8f9fa")
                    height: 48
                    width: alarmList.width

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 0

                        // Severity badge
                        Rectangle {
                            Layout.preferredHeight: 24
                            Layout.preferredWidth: 80
                            color: model.severity === 0 ? "#2196F3" : model.severity === 1 ? "#FFC107" : "#FF5722"
                            radius: 4

                            Label {
                                anchors.centerIn: parent
                                color: "white"
                                font.bold: true
                                font.pixelSize: 10
                                text: model.severityText
                            }
                        }
                        Label {
                            Layout.preferredWidth: 120
                            color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            font.pixelSize: 12
                            text: model.deviceId
                        }
                        Label {
                            Layout.preferredWidth: 100
                            color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            font.pixelSize: 12
                            text: model.metric
                        }
                        Label {
                            Layout.preferredWidth: 80
                            color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            font.pixelSize: 12
                            text: model.value.toFixed(1)
                        }
                        Label {
                            Layout.fillWidth: true
                            color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            elide: Text.ElideRight
                            font.pixelSize: 12
                            text: model.message
                        }
                        Label {
                            Layout.preferredWidth: 100
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.pixelSize: 12
                            text: model.triggeredAt
                        }

                        // Ack status
                        Label {
                            Layout.preferredWidth: 60
                            color: model.acknowledged ? "#a6e3a1" : "#f38ba8"
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            text: model.acknowledged ? "✓" : "●"
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor

                        onClicked: alarmList.currentIndex = index
                    }
                }
                header: Rectangle {
                    color: root.isDark ? "#181825" : "#f8f9fa"
                    height: 48
                    width: alarmList.width

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 0

                        Label {
                            Layout.preferredWidth: 80
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 12
                            text: "级别"
                        }
                        Label {
                            Layout.preferredWidth: 120
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 12
                            text: "设备"
                        }
                        Label {
                            Layout.preferredWidth: 100
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 12
                            text: "指标"
                        }
                        Label {
                            Layout.preferredWidth: 80
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 12
                            text: "值"
                        }
                        Label {
                            Layout.fillWidth: true
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 12
                            text: "消息"
                        }
                        Label {
                            Layout.preferredWidth: 100
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 12
                            text: "时间"
                        }
                        Label {
                            Layout.preferredWidth: 60
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 12
                            text: "状态"
                        }
                    }
                }
                highlight: Rectangle {
                    color: root.isDark ? "#45475a" : "#e8f0fe"
                    radius: 4
                }
            }
        }
    }
}
