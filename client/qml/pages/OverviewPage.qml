import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import "../components"

Rectangle {
    id: root

    property string currentGroup: "全部设备"
    property var deviceModel: null
    property var dataManager: null
    property bool isDark: true

    signal deviceSelected(string deviceId)

    color: isDark ? "#1e1e2e" : "#f5f5f5"

    // 从设备列表中提取分组统计
    ListModel {
        id: groupModel
    }

    function updateGroupModel() {
        if (!deviceModel) return;
        
        groupModel.clear();
        
        // 统计各分组设备数
        var groups = {};
        var totalCount = 0;
        
        for (var i = 0; i < deviceModel.rowCount(); i++) {
            var idx = deviceModel.index(i, 0);
            var group = deviceModel.data(deviceModel.index(i, 3), Qt.DisplayRole) || "未分组";
            
            totalCount++;
            if (!groups[group]) {
                groups[group] = { count: 0, online: 0 };
            }
            groups[group].count++;
            // 检查是否在线 (status列)
            var status = deviceModel.data(deviceModel.index(i, 0), Qt.UserRole);
            if (status === 1) { // Online
                groups[group].online++;
            }
        }
        
        // 添加"全部设备"
        groupModel.append({
            name: "全部设备",
            count: totalCount,
            depth: 0,
            hasChildren: Object.keys(groups).length > 0,
            expanded: true
        });
        
        // 添加各分组
        for (var groupName in groups) {
            groupModel.append({
                name: groupName,
                count: groups[groupName].count,
                depth: 1,
                hasChildren: false,
                expanded: false
            });
        }
    }

    // 监听模型变化
    Connections {
        target: deviceModel
        function onModelReset() { updateGroupModel() }
        function onRowsInserted() { updateGroupModel() }
        function onRowsRemoved() { updateGroupModel() }
    }

    Component.onCompleted: {
        updateGroupModel()
    }

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

                            Label { Layout.preferredWidth: 50; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true; text: "状态" }
                            Label { Layout.preferredWidth: 100; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true; text: "设备ID" }
                            Label { Layout.preferredWidth: 80; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true; text: "名称" }
                            Label { Layout.preferredWidth: 80; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true; text: "分组" }
                            Label { Layout.fillWidth: true; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true; text: "最后上报" }
                            Label { Layout.preferredWidth: 56; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true; text: "操作" }
                        }
                    }

                    // Table content
                    ListView {
                        id: deviceList
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        clip: true
                        model: deviceModel

                        // 过滤当前分组
                        // 注意: 简化处理，显示所有设备

                        ScrollBar.vertical: ScrollBar {
                            active: true
                            policy: ScrollBar.AsNeeded
                        }

                        header: Rectangle { height: 6; width: deviceList.width; color: "transparent" }

                        delegate: Rectangle {
                            color: index % 2 === 0 ? (root.isDark ? "#313244" : "#ffffff") : (root.isDark ? "#2a2a3c" : "#f8f9fa")
                            height: 44
                            width: deviceList.width

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

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
}
