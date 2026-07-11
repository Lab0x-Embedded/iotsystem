import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

Rectangle {
    id: root

    property var dataManager: null
    property var deviceData: null

    // 本分组下的设备数量
    property int groupDeviceCount: 0
    property int groupId: 0
    property string groupName: ""

    // 本分组在线设备数量
    property int groupOnlineCount: 0
    property bool isDark: true

    // 刷新过滤后的设备列表
    function refreshFilteredDevices() {
        filteredDeviceModel.clear();
        if (!deviceData) {
            root.groupDeviceCount = 0;
            root.groupOnlineCount = 0;
            return;
        }
        var devices = deviceData.devicesByGroup(root.groupId);
        var online = 0;
        for (var i = 0; i < devices.length; i++) {
            var d = devices[i];
            if (d.status === 1)
                online++;
            filteredDeviceModel.append({
                "deviceId": d.id || "",
                "deviceName": d.name || "",
                "productKey": d.productKey || "",
                "statusValue": d.status,
                "statusText": d.statusText || "离线"
            });
        }
        root.groupDeviceCount = devices.length;
        root.groupOnlineCount = online;
    }
    function showGroup(gid, gname) {
        root.groupId = gid;
        root.groupName = gname;
    }

    color: root.isDark ? "#1e1e2e" : "#f5f5f5"

    Component.onCompleted: refreshFilteredDevices()
    onGroupIdChanged: refreshFilteredDevices()

    // ===== 添加设备对话框 =====
    Dialog {
        id: addDeviceDialog

        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        title: "添加设备到分组"
        width: 480

        onAccepted: {
            if (dataManager) {
                for (var i = 0; i < ungroupedPicker.selectedIds.length; i++) {
                    dataManager.updateDeviceGroup(ungroupedPicker.selectedIds[i], root.groupId);
                }
                ungroupedPicker.selectedIds = [];
            }
        }
        onOpened: {
            // 刷新未分组设备列表
            ungroupedModel.clear();
            ungroupedPicker.selectedIds = [];
            if (deviceData) {
                var allDevices = deviceData.devicesByGroup(-1);
                for (var i = 0; i < allDevices.length; i++) {
                    if (String(allDevices[i].group) === String(root.groupId))
                        continue;
                    ungroupedModel.append({
                        "deviceId": allDevices[i].id,
                        "deviceName": allDevices[i].name
                    });
                }
            }
        }
        onRejected: {
            ungroupedPicker.selectedIds = [];
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
                text: "选择未分组设备添加到 \"" + root.groupName + "\""
            }
            ListView {
                id: ungroupedPicker

                property var selectedIds: []

                clip: true
                height: 280
                width: parent.width

                delegate: Rectangle {
                    color: index % 2 === 0 ? (root.isDark ? "#313244" : "#ffffff") : (root.isDark ? "#2a2a3c" : "#f8f9fa")
                    height: 40
                    width: ungroupedPicker.width

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8

                        CheckBox {
                            checked: ungroupedPicker.selectedIds.indexOf(model.deviceId) >= 0

                            onCheckedChanged: {
                                var idx = ungroupedPicker.selectedIds.indexOf(model.deviceId);
                                if (checked && idx < 0) {
                                    ungroupedPicker.selectedIds.push(model.deviceId);
                                } else if (!checked && idx >= 0) {
                                    ungroupedPicker.selectedIds.splice(idx, 1);
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
                            text: model.deviceId
                        }
                    }
                }
                model: ListModel {
                    id: ungroupedModel
                }
            }
        }
    }

    // ===== 移除确认对话框 =====
    Dialog {
        id: removeConfirmDialog

        property string targetDeviceId: ""
        property string targetDeviceName: ""

        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        title: "确认移除"
        width: 340

        onAccepted: {
            if (dataManager && removeConfirmDialog.targetDeviceId !== "") {
                dataManager.removeDeviceFromGroup(removeConfirmDialog.targetDeviceId);
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
                text: "确定要将 \"" + removeConfirmDialog.targetDeviceName + "\" 从分组中移除吗？"
                width: parent.width
                wrapMode: Text.Wrap
            }
            Label {
                color: root.isDark ? "#a6adc8" : "#666666"
                font.pixelSize: 12
                text: "移除后设备将变为未分组状态。"
            }
        }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // ===== Header: 返回 + 分组名 + 统计 + 添加按钮 =====
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Button {
                Material.foreground: root.isDark ? "#a6adc8" : "#666666"
                flat: true
                font.pixelSize: 13
                text: "← 返回"

                onClicked: {
                    stackView.currentIndex = 1;
                    sidebar.currentIndex = 1;
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                    font.bold: true
                    font.pixelSize: 18
                    text: root.groupName
                }
                Label {
                    color: root.isDark ? "#a6adc8" : "#666666"
                    font.pixelSize: 12
                    text: "共 " + root.groupDeviceCount + " 台设备 · 在线 " + root.groupOnlineCount
                }
            }
            Button {
                Material.background: "#89b4fa"
                Material.foreground: "#1e1e2e"
                text: "+ 添加设备"

                onClicked: addDeviceDialog.open()
            }
        }

        // ===== 统计卡片 =====
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
                        text: root.groupDeviceCount.toString()
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        color: root.isDark ? "#a6adc8" : "#666666"
                        font.pixelSize: 12
                        text: "设备总数"
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
                        text: root.groupOnlineCount.toString()
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        color: root.isDark ? "#a6adc8" : "#666666"
                        font.pixelSize: 12
                        text: "在线设备"
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
                        color: root.isDark ? "#f38ba8" : "#dc2626"
                        font.bold: true
                        font.pixelSize: 24
                        text: (root.groupDeviceCount - root.groupOnlineCount).toString()
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        color: root.isDark ? "#a6adc8" : "#666666"
                        font.pixelSize: 12
                        text: "离线设备"
                    }
                }
            }
        }

        // ===== 设备列表 =====
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
                            Layout.preferredWidth: 100
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 11
                            text: "设备ID"
                        }
                        Label {
                            Layout.preferredWidth: 120
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 11
                            text: "设备名称"
                        }
                        Label {
                            Layout.preferredWidth: 80
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 11
                            text: "状态"
                        }
                        Label {
                            Layout.fillWidth: true
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 11
                            text: "产品Key"
                        }
                        Label {
                            Layout.preferredWidth: 80
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.bold: true
                            font.pixelSize: 11
                            text: "操作"
                        }
                    }
                }

                // 设备列表 - 使用 Repeater 过滤本分组设备
                ListView {
                    id: groupDeviceList

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    clip: true

                    ScrollBar.vertical: ScrollBar {
                        active: true
                        policy: ScrollBar.AsNeeded
                    }
                    delegate: Rectangle {
                        id: deviceDelegate

                        color: index % 2 === 0 ? (root.isDark ? "#313244" : "#ffffff") : (root.isDark ? "#2a2a3c" : "#f8f9fa")
                        height: 50
                        width: groupDeviceList.width

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 12

                            Label {
                                Layout.preferredWidth: 100
                                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                font.pixelSize: 12
                                text: deviceId
                            }
                            Label {
                                Layout.preferredWidth: 120
                                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                font.pixelSize: 12
                                text: deviceName || "-"
                            }

                            // 状态指示
                            RowLayout {
                                Layout.preferredWidth: 80
                                spacing: 6

                                Rectangle {
                                    color: model.statusValue === 1 ? "#4CAF50" : model.statusValue === 2 ? "#FF5722" : model.statusValue === 3 ? "#FFC107" : "#9E9E9E"
                                    height: 8
                                    radius: 4
                                    width: 8
                                }
                                Label {
                                    color: root.isDark ? "#a6adc8" : "#666666"
                                    font.pixelSize: 11
                                    text: model.statusText
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.pixelSize: 12
                                text: model.productKey || "-"
                            }
                            Button {
                                Material.foreground: "#f38ba8"
                                flat: true
                                font.pixelSize: 11
                                text: "移除"

                                onClicked: {
                                    removeConfirmDialog.targetDeviceId = deviceId;
                                    removeConfirmDialog.targetDeviceName = deviceName;
                                    removeConfirmDialog.open();
                                }
                            }
                        }
                    }
                    model: ListModel {
                        id: filteredDeviceModel
                    }

                    // 空状态提示
                    Label {
                        anchors.centerIn: parent
                        color: root.isDark ? "#a6adc8" : "#666666"
                        font.pixelSize: 14
                        text: "该分组暂无设备，点击上方\"添加设备\"按钮"
                        visible: filteredDeviceModel.count === 0
                    }
                }
            }
        }
    }

    // 设备数据变化后也刷新
    Connections {
        function onConnectionStatusChanged(status) {
            if (status === "connected")
                Qt.callLater(refreshFilteredDevices);
        }
        function onDeviceUpdated() {
            Qt.callLater(refreshFilteredDevices);
        }

        target: dataManager
    }

    // 数据变化时自动刷新
    Connections {
        function onCountsChanged() {
            refreshFilteredDevices();
        }

        target: deviceData
    }
}
