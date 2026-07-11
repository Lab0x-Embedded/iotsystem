import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

Rectangle {
    id: root

    property var dataManager: null
    property var deviceData
    property var groupData
    property bool isDark: true

    signal showGroupDetail(int groupId, string groupName)

    color: isDark ? "#1e1e2e" : "#f5f5f5"

    // 新增分组对话框
    Dialog {
        id: addGroupDialog

        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        title: "新增分组"
        width: 380

        onAccepted: {
            if (groupNameField.text && dataManager) {
                dataManager.httpClient.createGroup(groupNameField.text, 0, groupDescField.text);
                groupNameField.text = "";
                groupDescField.text = "";
            }
        }

        Column {
            anchors.left: parent.left
            anchors.margins: 24
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 12

            Label {
                text: "分组名称:"
            }
            TextField {
                id: groupNameField

                placeholderText: "请输入分组名称"
                width: parent.width
            }
            Label {
                text: "描述:"
            }
            TextField {
                id: groupDescField

                placeholderText: "请输入描述"
                width: parent.width
            }
            Label {
                text: "上级分组:"
            }
            ComboBox {
                id: parentGroupCombo

                model: groupData ? ["无 (顶级分组)"] : ["无 (顶级分组)"]
                width: parent.width
            }
        }
    }

    // 编辑分组对话框
    Dialog {
        id: editGroupDialog

        property int currentGroupId: 0

        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        title: "编辑分组"
        width: 380

        onAccepted: {
            if (editGroupNameField.text && dataManager) {
                dataManager.httpClient.updateGroup(editGroupDialog.currentGroupId, editGroupNameField.text, editGroupDescField.text);
            }
        }

        Column {
            anchors.left: parent.left
            anchors.margins: 24
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 12

            Label {
                text: "分组名称:"
            }
            TextField {
                id: editGroupNameField

                placeholderText: "请输入分组名称"
                width: parent.width
            }
            Label {
                text: "描述:"
            }
            TextField {
                id: editGroupDescField

                placeholderText: "请输入描述"
                width: parent.width
            }
        }
    }

    // 删除确认对话框
    Dialog {
        id: deleteConfirmDialog

        property int targetGroupId: 0
        property string targetGroupName: ""

        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        title: "确认删除"
        width: 340

        onAccepted: {
            if (dataManager && deleteConfirmDialog.targetGroupId > 0) {
                dataManager.httpClient.deleteGroup(deleteConfirmDialog.targetGroupId);
            }
        }

        Column {
            anchors.left: parent.left
            anchors.margins: 24
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 12

            Label {
                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                text: "确定要删除分组 \"" + deleteConfirmDialog.targetGroupName + "\" 吗？"
                width: parent.width
                wrapMode: Text.Wrap
            }
            Label {
                color: root.isDark ? "#a6adc8" : "#666666"
                font.pixelSize: 12
                text: "删除后无法恢复，请谨慎操作。"
            }
        }
    }

    // 设备管理对话框：把设备添加/移入当前组
    Dialog {
        id: assignDeviceDialog

        property int targetGroupId: 0
        property string targetGroupName: ""

        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        title: "管理设备"
        width: 480

        onAccepted: {
            if (dataManager) {
                for (var i = 0; i < devicePicker.selectedIds.length; i++) {
                    dataManager.updateDeviceGroup(devicePicker.selectedIds[i], targetGroupId);
                }
                devicePicker.selectedIds = [];
            }
        }
        onRejected: {
            devicePicker.selectedIds = [];
        }

        Column {
            anchors.left: parent.left
            anchors.margins: 24
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 12

            Label {
                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                font.pixelSize: 14
                text: "将设备添加到分组: \"" + assignDeviceDialog.targetGroupName + "\""
            }
            Label {
                color: root.isDark ? "#a6adc8" : "#666666"
                font.pixelSize: 12
                text: "选择设备:"
            }
            ListView {
                id: devicePicker

                property var selectedIds: []

                clip: true
                height: 240
                model: deviceData
                width: parent.width

                delegate: Rectangle {
                    color: index % 2 === 0 ? (root.isDark ? "#313244" : "#ffffff") : (root.isDark ? "#2a2a3c" : "#f8f9fa")
                    height: 40
                    width: devicePicker.width

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8

                        CheckBox {
                            checked: devicePicker.selectedIds.indexOf(model.deviceId) >= 0

                            onCheckedChanged: {
                                var idx = devicePicker.selectedIds.indexOf(model.deviceId);
                                if (checked && idx < 0) {
                                    devicePicker.selectedIds.push(model.deviceId);
                                } else if (!checked && idx >= 0) {
                                    devicePicker.selectedIds.splice(idx, 1);
                                }
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            font.pixelSize: 12
                            text: model.deviceName || model.deviceId
                        }
                        Label {
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.pixelSize: 11
                            text: model.group || "未分组"
                        }
                    }
                }
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
            Item {
                Layout.fillWidth: true
            }
            Button {
                Material.background: "#89b4fa"
                Material.foreground: "#1e1e2e"
                text: "+ 新增分组"

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
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1
                color: root.isDark ? "#313244" : "#ffffff"
                radius: 8

                ColumnLayout {
                    anchors.centerIn: parent

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        color: root.isDark ? "#89b4fa" : "#3b82f6"
                        font.bold: true
                        font.pixelSize: 24
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
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1
                color: root.isDark ? "#313244" : "#ffffff"
                radius: 8

                ColumnLayout {
                    anchors.centerIn: parent

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        color: root.isDark ? "#a6e3a1" : "#16a34a"
                        font.bold: true
                        font.pixelSize: 24
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

                        Label {
                            Layout.preferredWidth: 30
                        }
                        Label {
                            Layout.preferredWidth: 300
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 11
                            text: "分组名称"
                        }
                        Label {
                            Layout.fillWidth: true
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 11
                            text: "描述"
                        }
                        Label {
                            Layout.preferredWidth: 100
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 11
                            text: "设备数量"
                        }
                        Label {
                            Layout.preferredWidth: 180
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 11
                            text: "操作"
                        }
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
                                Layout.preferredWidth: 300
                                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                font.bold: model.parentId === 0
                                font.pixelSize: 13
                                text: model.groupName
                            }
                            Label {
                                Layout.fillWidth: true
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.pixelSize: 12
                                text: model.description || "-"
                            }
                            Label {
                                Layout.preferredWidth: 80
                                color: root.isDark ? "#89b4fa" : "#2563eb"
                                font.bold: true
                                font.pixelSize: 12
                                text: model.deviceCount + " 台"
                            }
                            RowLayout {
                                Layout.preferredWidth: 180
                                spacing: 8

                                Button {
                                    Material.foreground: "#a6e3a1"
                                    flat: true
                                    font.pixelSize: 11
                                    text: "详情"

                                    onClicked: {
                                        root.showGroupDetail(model.groupId, model.groupName);
                                    }
                                }
                                Button {
                                    Material.foreground: "#89b4fa"
                                    enabled: true
                                    flat: true
                                    font.pixelSize: 11
                                    text: "编辑"

                                    onClicked: {
                                        editGroupDialog.currentGroupId = model.groupId;
                                        editGroupNameField.text = model.groupName;
                                        editGroupDescField.text = model.description;
                                        editGroupDialog.open();
                                    }
                                }
                                Button {
                                    Material.foreground: "#f38ba8"
                                    enabled: model.deviceCount === 0
                                    flat: true
                                    font.pixelSize: 11
                                    text: "删除"

                                    onClicked: {
                                        deleteConfirmDialog.targetGroupId = model.groupId;
                                        deleteConfirmDialog.targetGroupName = model.groupName;
                                        deleteConfirmDialog.open();
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
