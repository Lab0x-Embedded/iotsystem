import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

import "../components"

Rectangle {
    id: root

    property var alarmListModel: null
    property var ruleListModel: null
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
            Item { Layout.fillWidth: true }
            Label {
                color: "#f38ba8"
                font.pixelSize: 13
                text: "未确认: " + (alarmListModel ? alarmListModel.unacknowledgedCount : 0)
            }
        }

        // Tab bar + Add Rule button
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            TabBar {
                id: tabBar
                background: Rectangle { color: "transparent" }

                TabButton {
                    text: "告警记录"
                    Material.foreground: root.isDark ? "#cdd6f4" : "#1e1e2e"
                }
                TabButton {
                    text: "告警规则"
                    Material.foreground: root.isDark ? "#cdd6f4" : "#1e1e2e"
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                Material.background: "#a6e3a1"
                Material.foreground: "#1e1e2e"
                text: "添加规则"
                onClicked: addRuleDialog.open()
            }
        }

        // Content area
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            // ====== Tab 1: 告警记录 ======
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

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
                        Item { Layout.fillWidth: true }
                        Button {
                            Material.background: "#89b4fa"
                            Material.foreground: "white"
                            enabled: alarmList.currentIndex >= 0
                            text: "确认选中"
                            onClicked: {
                                if (alarmList.currentIndex >= 0) {
                                    alarmListModel.acknowledge(alarmList.currentIndex);
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
                        radius: 8

                        ListView {
                            id: alarmList
                            anchors.fill: parent
                            anchors.margins: 1
                            clip: true
                            highlightFollowsCurrentItem: true
                            model: alarmListModel

                            delegate: Rectangle {
                                color: index % 2 === 0 ? (root.isDark ? "#313244" : "#ffffff") : (root.isDark ? "#2a2a3c" : "#f8f9fa")
                                height: 48
                                width: alarmList.width

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 16
                                    anchors.rightMargin: 16
                                    spacing: 0

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
                                    Label { Layout.preferredWidth: 80; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 12; text: "级别" }
                                    Label { Layout.preferredWidth: 120; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 12; text: "设备" }
                                    Label { Layout.preferredWidth: 100; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 12; text: "指标" }
                                    Label { Layout.preferredWidth: 80; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 12; text: "值" }
                                    Label { Layout.fillWidth: true; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 12; text: "消息" }
                                    Label { Layout.preferredWidth: 100; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 12; text: "时间" }
                                    Label { Layout.preferredWidth: 60; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 12; text: "状态" }
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

            // ====== Tab 2: 告警规则 ======
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"

                Rectangle {
                    anchors.fill: parent
                    border.color: root.isDark ? "#45475a" : "#e0e0e0"
                    border.width: 1
                    color: root.isDark ? "#313244" : "#ffffff"
                    radius: 8

                    ListView {
                        id: ruleList
                        anchors.fill: parent
                        anchors.margins: 1
                        clip: true
                        model: ruleListModel

                        delegate: Rectangle {
                            color: index % 2 === 0 ? (root.isDark ? "#313244" : "#ffffff") : (root.isDark ? "#2a2a3c" : "#f8f9fa")
                            height: 48
                            width: ruleList.width

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 0

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
                                    Layout.preferredWidth: 60
                                    color: root.isDark ? "#89b4fa" : "#4a6fa5"
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    text: model.op
                                }
                                Label {
                                    Layout.preferredWidth: 80
                                    color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignRight
                                    text: model.threshold
                                }
                                Label {
                                    Layout.preferredWidth: 60
                                    color: model.severity === "严重" ? "#f38ba8" : model.severity === "警告" ? "#fab387" : "#89b4fa"
                                    font.bold: true
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    text: model.severity
                                }
                                Label {
                                    Layout.fillWidth: true
                                    color: model.enabled === "启用" ? "#a6e3a1" : "#6c7086"
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    text: model.enabled
                                }
                            }
                        }
                        header: Rectangle {
                            color: root.isDark ? "#181825" : "#f8f9fa"
                            height: 48
                            width: ruleList.width
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 0
                                Label { Layout.preferredWidth: 120; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 12; text: "设备" }
                                Label { Layout.preferredWidth: 100; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 12; text: "指标" }
                                Label { Layout.preferredWidth: 60; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 12; text: "条件" }
                                Label { Layout.preferredWidth: 80; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 12; text: "阈值" }
                                Label { Layout.preferredWidth: 60; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 12; text: "级别" }
                                Label { Layout.fillWidth: true; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 12; text: "状态" }
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
    }

    // Add Rule Dialog
    AddAlarmRuleDialog {
        id: addRuleDialog
        isDark: root.isDark
        width: 420
        x: (root.width - width) / 2
        y: (root.height - height) / 2
    }
}
