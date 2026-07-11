import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import "../components"

Rectangle {
    id: root

    property var overPageManager: null
    property var deviceData: null
    property bool isDark: true

    signal deviceSelected(string deviceId)

    color: isDark ? "#1e1e2e" : "#f5f5f5"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // Right: Stats + Table
        ColumnLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
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
                    value: deviceData ? deviceData.totalCount.toString() : "0"
                }
                DashboardCard {
                    Layout.fillWidth: true
                    accentColor: "#a6e3a1"
                    isDark: root.isDark
                    title: "在线设备"
                    value: deviceData ? deviceData.onlineCount.toString() : "0"
                }
                DashboardCard {
                    Layout.fillWidth: true
                    accentColor: "#9399b2"
                    isDark: root.isDark
                    title: "离线设备"
                    value: deviceData ? (deviceData.totalCount - deviceData.onlineCount - deviceData.alarmCount).toString() : "0"
                }
                DashboardCard {
                    Layout.fillWidth: true
                    accentColor: "#f38ba8"
                    isDark: root.isDark
                    title: "告警设备"
                    value: deviceData ? deviceData.alarmCount.toString() : "0"
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

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 0

                    // Table header
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        color: root.isDark ? "#1e1e2e" : "#f8f9fa"
                        radius: 6

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 12

                            Label {
                                Layout.preferredWidth: 50
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.bold: true
                                font.pixelSize: 11
                                text: "状态"
                            }
                            Label {
                                Layout.preferredWidth: 200
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.bold: true
                                font.pixelSize: 11
                                text: "设备ID"
                            }
                            Label {
                                Layout.preferredWidth: 200
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.bold: true
                                font.pixelSize: 11
                                text: "名称"
                            }
                            Label {
                                Layout.preferredWidth: 80
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.bold: true
                                font.pixelSize: 11
                                text: "分组"
                            }
                            Label {
                                Layout.fillWidth: true
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.bold: true
                                font.pixelSize: 11
                                text: "最后上报"
                            }
                            Label {
                                Layout.preferredWidth: 56
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.bold: true
                                font.pixelSize: 11
                                text: "操作"
                            }
                        }
                    }

                    // Table content
                    ListView {
                        id: deviceList

                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        clip: true
                        model: deviceData

                        ScrollBar.vertical: ScrollBar {
                            active: true
                            policy: ScrollBar.AsNeeded
                        }
                        delegate: Rectangle {
                            color: index % 2 === 0 ? (root.isDark ? "#313244" : "#ffffff") : (root.isDark ? "#2a2a3c" : "#f8f9fa")
                            height: 44
                            width: deviceList.width

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                // Status dot
                                Rectangle {
                                    Layout.preferredHeight: 32
                                    Layout.preferredWidth: 50
                                    color: "transparent"

                                    Rectangle {
                                        anchors.centerIn: parent
                                        color: model.status === 1 ? "#4CAF50" : model.status === 2 ? "#FF5722" : "#9E9E9E"
                                        height: 12
                                        radius: 6
                                        width: 12
                                    }
                                }
                                Label {
                                    Layout.preferredWidth: 200
                                    color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                    font.pixelSize: 12
                                    text: model.deviceId || ""
                                }
                                Label {
                                    Layout.preferredWidth: 200
                                    color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                    font.pixelSize: 12
                                    text: model.deviceName || "-"
                                }
                                Label {
                                    Layout.preferredWidth: 80
                                    color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                    font.pixelSize: 12
                                    text: model.group ? groupModel.groupName(Number(model.group)) : "未分组"
                                }
                                Label {
                                    Layout.fillWidth: true
                                    color: root.isDark ? "#a6adc8" : "#666666"
                                    font.pixelSize: 12
                                    text: model.lastSeen || ""
                                }
                                Button {
                                    Layout.preferredHeight: 28
                                    Layout.preferredWidth: 56
                                    flat: true
                                    font.pixelSize: 11
                                    hoverEnabled: true
                                    text: "详情"

                                    background: Rectangle {
                                        border.color: "#89b4fa"
                                        border.width: 1
                                        color: parent.hovered ? (root.isDark ? "#45475a" : "#dbeafe") : "transparent"
                                        radius: 6
                                    }
                                    contentItem: Label {
                                        color: "#89b4fa"
                                        font: parent.font
                                        horizontalAlignment: Text.AlignHCenter
                                        text: parent.text
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: root.deviceSelected(model.deviceId || "")

                                    // 新增 MouseArea 来接管鼠标光标形状
                                    MouseArea {
                                        acceptedButtons: Qt.NoButton // 关键：不拦截鼠标点击，让点击事件正常传递给 Button
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        hoverEnabled: true // 必须开启，否则光标不会在悬停时改变
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
