import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

Rectangle {
    id: root

    property var deviceModel: null
    property bool isDark: true

    color: isDark ? "#1e1e2e" : "#f5f5f5"

    // 分组数据模型
    ListModel {
        id: groupModel
    }

    // 新增分组对话框
    Dialog {
        id: addGroupDialog
        title: "新增分组"
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        ColumnLayout {
            spacing: 12
            Label { text: "分组名称:" }
            TextField {
                id: groupNameField
                placeholderText: "请输入分组名称"
                Layout.fillWidth: true
            }
            Label { text: "上级分组:" }
            ComboBox {
                id: parentGroupCombo
                Layout.fillWidth: true
                model: ["无 (顶级分组)", "工厂A", "工厂B"]
            }
        }

        onAccepted: {
            if (groupNameField.text) {
                groupModel.append({
                    "name": groupNameField.text,
                    "parent": parentGroupCombo.currentText,
                    "deviceCount": 0,
                    "depth": parentGroupCombo.currentIndex === 0 ? 0 : 1
                })
                groupNameField.text = ""
            }
        }
    }

    function updateGroupStats() {
        if (!deviceModel) return
        
        groupModel.clear()
        
        var groups = {}
        
        // 统计每个分组的设备数
        for (var i = 0; i < deviceModel.rowCount(); i++) {
            var group = deviceModel.data(deviceModel.index(i, 3), Qt.DisplayRole) || "未分组"
            if (!groups[group]) {
                groups[group] = 0
            }
            groups[group]++
        }
        
        // 添加到模型
        groupModel.append({
            "name": "全部设备",
            "parent": "",
            "deviceCount": deviceModel.totalCount,
            "depth": 0
        })
        
        for (var groupName in groups) {
            groupModel.append({
                "name": groupName,
                "parent": "全部设备",
                "deviceCount": groups[groupName],
                "depth": 1
            })
        }
    }

    Connections {
        target: deviceModel
        function onCountsChanged() { updateGroupStats() }
    }

    Component.onCompleted: updateGroupStats()

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
                text: "分组管理"
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "新增分组"
                Material.background: "#89b4fa"
                Material.foreground: "#1e1e2e"
                
                onClicked: addGroupDialog.open()
            }
        }

        // 分组树
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

                // 表头
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: root.isDark ? "#1e1e2e" : "#f8f9fa"
                    radius: 6

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16

                        Label { Layout.preferredWidth: 30; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true }
                        Label { Layout.fillWidth: true; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true; text: "分组名称" }
                        Label { Layout.preferredWidth: 100; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true; text: "上级分组" }
                        Label { Layout.preferredWidth: 80; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true; text: "设备数量" }
                        Label { Layout.preferredWidth: 150; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true; text: "操作" }
                    }
                }

                // 分组列表
                ListView {
                    id: groupList
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    clip: true
                    model: groupModel

                    ScrollBar.vertical: ScrollBar {
                        active: true
                        policy: ScrollBar.AsNeeded
                    }

                    delegate: Rectangle {
                        color: index % 2 === 0 ? (root.isDark ? "#313244" : "#ffffff") : (root.isDark ? "#2a2a3c" : "#f8f9fa")
                        height: 50
                        width: groupList.width

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 12

                            // 展开图标
                            Label {
                                Layout.preferredWidth: 30
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.pixelSize: 12
                                text: model.depth === 0 ? "📁" : "  └"
                            }

                            // 分组名称
                            Label {
                                Layout.fillWidth: true
                                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                font.pixelSize: 13
                                font.bold: model.depth === 0
                                text: model.name
                            }

                            // 上级分组
                            Label {
                                Layout.preferredWidth: 100
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.pixelSize: 12
                                text: model.parent || "-"
                            }

                            // 设备数量
                            Label {
                                Layout.preferredWidth: 80
                                color: root.isDark ? "#89b4fa" : "#2563eb"
                                font.pixelSize: 12
                                font.bold: true
                                text: model.deviceCount + " 台"
                            }

                            // 操作按钮
                            RowLayout {
                                Layout.preferredWidth: 150
                                spacing: 8

                                Button {
                                    text: "编辑"
                                    flat: true
                                    font.pixelSize: 11
                                    enabled: model.depth > 0
                                    Material.foreground: "#89b4fa"
                                }

                                Button {
                                    text: "删除"
                                    flat: true
                                    font.pixelSize: 11
                                    enabled: model.depth > 0 && model.deviceCount === 0
                                    Material.foreground: "#f38ba8"
                                }

                                Button {
                                    text: "添加设备"
                                    flat: true
                                    font.pixelSize: 11
                                    enabled: model.depth > 0
                                    Material.foreground: "#a6e3a1"
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
