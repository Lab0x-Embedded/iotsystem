import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

Rectangle {
    id: root


    property var deviceData
    property var groupData
    property bool isDark: true

    color: isDark ? "#1e1e2e" : "#f5f5f5"

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
            Label { text: "描述:" }
            TextField {
                id: groupDescField
                placeholderText: "请输入描述"
                Layout.fillWidth: true
            }
            Label { text: "上级分组:" }
            ComboBox {
                id: parentGroupCombo
                Layout.fillWidth: true
                model: groupData ? ["无 (顶级分组)"] : ["无 (顶级分组)"]
            }
        }

        onAccepted: {
            if (groupNameField.text && dataManager) {
                dataManager.httpClient().createGroup(
                    groupNameField.text,
                    0,
                    groupDescField.text
                )
                groupNameField.text = ""
                groupDescField.text = ""
            }
        }
    }

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
                text: "+ 新增分组"
                Material.background: "#89b4fa"
                Material.foreground: "#1e1e2e"
                onClicked: addGroupDialog.open()
            }
        }

        // 分组统计卡片
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                color: root.isDark ? "#313244" : "#ffffff"
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1
                radius: 8

                ColumnLayout {
                    anchors.centerIn: parent
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        color: root.isDark ? "#89b4fa" : "#3b82f6"
                        font.pixelSize: 24
                        font.bold: true
                        text: groupData ? groupData.totalCount.toString() : "0"
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        color: root.isDark ? "#a6adc8" : "#666666"
                        font.pixelSize: 12
                        text: "总分组数"
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                color: root.isDark ? "#313244" : "#ffffff"
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1
                radius: 8

                ColumnLayout {
                    anchors.centerIn: parent
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        color: root.isDark ? "#a6e3a1" : "#16a34a"
                        font.pixelSize: 24
                        font.bold: true
                        text: deviceData ? deviceData.totalCount.toString() : "0"
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        color: root.isDark ? "#a6adc8" : "#666666"
                        font.pixelSize: 12
                        text: "总设备数"
                    }
                }
            }
        }

        // 分组列表
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

                        Label { Layout.preferredWidth: 30 }
                        Label { Layout.fillWidth: true; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true; text: "分组名称" }
                        Label { Layout.preferredWidth: 150; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true; text: "描述" }
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
                    model: groupData

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

                            Label {
                                Layout.preferredWidth: 30
                                font.pixelSize: 14
                                text: model.parentId === 0 ? "📁" : "  └"
                            }

                            Label {
                                Layout.fillWidth: true
                                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                font.pixelSize: 13
                                font.bold: model.parentId === 0
                                text: model.groupName
                            }

                            Label {
                                Layout.preferredWidth: 150
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.pixelSize: 12
                                text: model.description || "-"
                            }

                            Label {
                                Layout.preferredWidth: 80
                                color: root.isDark ? "#89b4fa" : "#2563eb"
                                font.pixelSize: 12
                                font.bold: true
                                text: model.deviceCount + " 台"
                            }

                            RowLayout {
                                Layout.preferredWidth: 150
                                spacing: 8

                                Button {
                                    text: "编辑"
                                    flat: true
                                    font.pixelSize: 11
                                    enabled: model.parentId > 0
                                    Material.foreground: "#89b4fa"
                                    onClicked: {
                                        // TODO: 编辑分组
                                    }
                                }

                                Button {
                                    text: "删除"
                                    flat: true
                                    font.pixelSize: 11
                                    enabled: model.parentId > 0 && model.deviceCount === 0
                                    Material.foreground: "#f38ba8"
                                    onClicked: {
                                        if (dataManager) {
                                            dataManager.httpClient().deleteGroup(model.groupId)
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
}
