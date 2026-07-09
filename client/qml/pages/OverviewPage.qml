import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import "../components"

Rectangle {
    id: root

    property string currentGroup: "全部设备"
    property var deviceModel: null
    property bool isDark: true

    signal deviceSelected(string deviceId)

    color: isDark ? "#1e1e2e" : "#f5f5f5"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // Left: Group tree
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 200
            border.color: root.isDark ? "#45475a" : "#e0e0e0"
            border.width: 1
            color: root.isDark ? "#313244" : "#ffffff"
            radius: 12

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 0

                Label {
                    Layout.bottomMargin: 12
                    color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                    font.bold: true
                    font.pixelSize: 14
                    text: "设备分组"
                }
                ListView {
                    id: groupTree

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    clip: true
                    model: groupModel

                    delegate: Rectangle {
                        color: model.name === root.currentGroup ? (root.isDark ? "#45475a" : "#dbeafe") : "transparent"
                        height: 32
                        radius: 6
                        width: groupTree.width

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8 + model.depth * 16
                            spacing: 6

                            Label {
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.pixelSize: 10
                                text: model.expanded ? "▼" : (model.hasChildren ? "▶" : "")
                                visible: model.hasChildren
                            }
                            Label {
                                Layout.fillWidth: true
                                color: model.name === root.currentGroup ? (root.isDark ? "#89b4fa" : "#2563eb") : (root.isDark ? "#cdd6f4" : "#1e1e2e")
                                font.bold: model.name === root.currentGroup
                                font.pixelSize: 13
                                text: model.name
                            }
                            Label {
                                color: root.isDark ? "#a6adc8" : "#999999"
                                font.pixelSize: 11
                                text: model.count.toString()
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor

                            onClicked: {
                                root.currentGroup = model.name;
                            }
                        }
                    }
                }
            }
        }

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

                    Layout.preferredHeight: 36
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

                    Layout.preferredHeight: 36
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

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Table header
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        color: root.isDark ? "#181825" : "#f8f9fa"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 12

                            Label {
                                Layout.preferredWidth: 50
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.bold: true
                                font.pixelSize: 12
                                text: "状态"
                            }
                            Label {
                                Layout.preferredWidth: 100
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.bold: true
                                font.pixelSize: 12
                                text: "设备ID"
                            }
                            Label {
                                Layout.preferredWidth: 80
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.bold: true
                                font.pixelSize: 12
                                text: "类型"
                            }
                            Label {
                                Layout.preferredWidth: 80
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.bold: true
                                font.pixelSize: 12
                                text: "分组"
                            }
                            Label {
                                Layout.fillWidth: true
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.bold: true
                                font.pixelSize: 12
                                text: "最后上报"
                            }
                            Label {
                                Layout.preferredWidth: 60
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.bold: true
                                font.pixelSize: 12
                                text: "操作"
                            }
                        }
                    }

                    // Divider
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: root.isDark ? "#45475a" : "#e0e0e0"
                    }

                    // Table data
                    ListView {
                        id: deviceList

                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        clip: true
                        model: deviceModel

                        ScrollBar.vertical: ScrollBar {
                            parent: deviceList
                            policy: ScrollBar.AsNeeded
                            width: 6
                            x: deviceList.width - width - 4

                            background: Rectangle {
                                color: "transparent"
                            }
                            contentItem: Rectangle {
                                color: parent.parent.pressed ? "#CBD5E1" : parent.parent.hovered ? "#94A3B8" : '#DBEAFE'
                                implicitWidth: 6
                                radius: 3

                                Behavior on color {
                                    ColorAnimation {
                                        duration: 200
                                    }
                                }
                            }
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
                                StatusIndicator {
                                    Layout.preferredHeight: 32
                                    Layout.preferredWidth: 50
                                    status: model.status
                                }
                                Label {
                                    Layout.preferredWidth: 100
                                    color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                    font.pixelSize: 12
                                    text: model.id
                                }
                                Label {
                                    Layout.preferredWidth: 80
                                    color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                    font.pixelSize: 12
                                    text: model.name
                                }
                                Label {
                                    Layout.preferredWidth: 80
                                    color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                    font.pixelSize: 12
                                    text: model.group
                                }
                                Label {
                                    Layout.fillWidth: true
                                    color: root.isDark ? "#a6adc8" : "#666666"
                                    font.pixelSize: 12
                                    text: model.lastSeen
                                }

                                // Detail button
                                Button {
                                    Layout.preferredHeight: 28
                                    Layout.preferredWidth: 56
                                    flat: true
                                    font.pixelSize: 11
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

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor

                                        onClicked: root.deviceSelected(model.id)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Group tree data model
    ListModel {
        id: groupModel

        ListElement {
            count: 847
            depth: 0
            expanded: true
            hasChildren: true
            name: "全部设备"
        }
        ListElement {
            count: 523
            depth: 1
            expanded: true
            hasChildren: true
            name: "工厂A"
        }
        ListElement {
            count: 312
            depth: 2
            expanded: false
            hasChildren: false
            name: "车间1"
        }
        ListElement {
            count: 211
            depth: 2
            expanded: false
            hasChildren: false
            name: "车间2"
        }
        ListElement {
            count: 324
            depth: 1
            expanded: false
            hasChildren: true
            name: "工厂B"
        }
    }
}
