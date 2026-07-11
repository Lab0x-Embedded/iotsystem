import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

Rectangle {
    id: root

    property var deviceData: null
    property var dataManager: null
    property bool isDark: true
    property int groupId: 0
    property string groupName: ""

    // 本分组下的设备数量
    property int groupDeviceCount: {
        if (!deviceData) return 0
        var count = 0
        for (var i = 0; i < deviceData.rowCount(); i++) {
            var dev = deviceData.deviceAt(i)
            if (dev && String(dev.group) === String(root.groupId)) count++
        }
        return count
    }

    // 本分组在线设备数量
    property int groupOnlineCount: {
        if (!deviceData) return 0
        var count = 0
        for (var i = 0; i < deviceData.rowCount(); i++) {
            var dev = deviceData.deviceAt(i)
            if (dev && String(dev.group) === String(root.groupId) && dev.status === 1) count++
        }
        return count
    }

    function showGroup(gid, gname) {
        root.groupId = gid
        root.groupName = gname
    }

    color: root.isDark ? "#1e1e2e" : "#f5f5f5"

    // ===== 添加设备对话框 =====
    Dialog {
        id: addDeviceDialog
        title: "添加设备到分组"
        width: 480
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        Column {
            spacing: 12
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 24

            Label {
                text: "选择未分组设备添加到 \"" + root.groupName + "\""
                font.pixelSize: 14
                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
            }

            ListView {
                id: ungroupedPicker
                width: parent.width
                height: 280
                clip: true
                property var selectedIds: []

                model: ListModel { id: ungroupedModel }

                delegate: Rectangle {
                    width: ungroupedPicker.width
                    height: 40
                    color: index % 2 === 0 ? (root.isDark ? "#313244" : "#ffffff") : (root.isDark ? "#2a2a3c" : "#f8f9fa")

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8

                        CheckBox {
                            checked: ungroupedPicker.selectedIds.indexOf(model.deviceId) >= 0
                            onCheckedChanged: {
                                var idx = ungroupedPicker.selectedIds.indexOf(model.deviceId)
                                if (checked && idx < 0) {
                                    ungroupedPicker.selectedIds.push(model.deviceId)
                                } else if (!checked && idx >= 0) {
                                    ungroupedPicker.selectedIds.splice(idx, 1)
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: model.deviceName || model.deviceId
                            color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            font.pixelSize: 12
                        }

                        Label {
                            text: model.deviceId
                            color: root.isDark ? "#a6adc8" : "#666666"
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }

        onOpened: {
            // 刷新未分组设备列表
            ungroupedModel.clear()
            ungroupedPicker.selectedIds = []
            if (deviceData) {
                for (var i = 0; i < deviceData.rowCount(); i++) {
                    var dev = deviceData.deviceAt(i)
                    if (dev && (dev.group === "0" || dev.group === "")) {
                        ungroupedModel.append({
                            "deviceId": dev.id,
                            "deviceName": dev.name
                        })
                    }
                }
            }
        }

        onAccepted: {
            if (dataManager) {
                for (var i = 0; i < ungroupedPicker.selectedIds.length; i++) {
                    dataManager.updateDeviceGroup(ungroupedPicker.selectedIds[i], root.groupId)
                }
                ungroupedPicker.selectedIds = []
            }
        }
        onRejected: {
            ungroupedPicker.selectedIds = []
        }
    }

    // ===== 移除确认对话框 =====
    Dialog {
        id: removeConfirmDialog
        title: "确认移除"
        width: 340
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string targetDeviceId: ""
        property string targetDeviceName: ""

        Column {
            spacing: 12
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 24
            Label {
                text: "确定要将 \"" + removeConfirmDialog.targetDeviceName + "\" 从分组中移除吗？"
                wrapMode: Text.Wrap
                width: parent.width
                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
            }
            Label {
                text: "移除后设备将变为未分组状态。"
                font.pixelSize: 12
                color: root.isDark ? "#a6adc8" : "#666666"
            }
        }

        onAccepted: {
            if (dataManager && removeConfirmDialog.targetDeviceId !== "") {
                dataManager.removeDeviceFromGroup(removeConfirmDialog.targetDeviceId)
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
                    stackView.currentIndex = 1
                    sidebar.currentIndex = 1
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
                text: "+ 添加设备"
                Material.background: "#89b4fa"
                Material.foreground: "#1e1e2e"
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
                color: root.isDark ? "#313244" : "#ffffff"
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1
                radius: 8

                ColumnLayout {
                    anchors.centerIn: parent
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        color: root.isDark ? "#f38ba8" : "#dc2626"
                        font.pixelSize: 24
                        font.bold: true
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

                        Label { Layout.preferredWidth: 100; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true; text: "设备ID" }
                        Label { Layout.preferredWidth: 120; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true; text: "设备名称" }
                        Label { Layout.preferredWidth: 80; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true; text: "状态" }
                        Label { Layout.fillWidth: true; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true; text: "产品Key" }
                        Label { Layout.preferredWidth: 80; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 11; font.bold: true; text: "操作" }
                    }
                }

                // 设备列表 - 使用 Repeater 过滤本分组设备
                ListView {
                    id: groupDeviceList
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    clip: true

                    model: ListModel { id: filteredDeviceModel }

                    ScrollBar.vertical: ScrollBar {
                        active: true
                        policy: ScrollBar.AsNeeded
                    }

                    // 空状态提示
                    Label {
                        anchors.centerIn: parent
                        visible: filteredDeviceModel.count === 0
                        color: root.isDark ? "#a6adc8" : "#666666"
                        font.pixelSize: 14
                        text: "该分组暂无设备，点击上方\"添加设备\"按钮"
                    }

                    delegate: Rectangle {
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
                                text: model.deviceId
                            }

                            Label {
                                Layout.preferredWidth: 120
                                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                font.pixelSize: 12
                                text: model.deviceName || "-"
                            }

                            // 状态指示
                            RowLayout {
                                Layout.preferredWidth: 80
                                spacing: 6

                                Rectangle {
                                    width: 8
                                    height: 8
                                    radius: 4
                                    color: model.statusValue === 1 ? "#4CAF50" : model.statusValue === 2 ? "#FF5722" : model.statusValue === 3 ? "#FFC107" : "#9E9E9E"
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
                                text: "移除"
                                flat: true
                                font.pixelSize: 11
                                Material.foreground: "#f38ba8"
                                onClicked: {
                                    removeConfirmDialog.targetDeviceId = model.deviceId
                                    removeConfirmDialog.targetDeviceName = model.deviceName
                                    removeConfirmDialog.open()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 刷新过滤后的设备列表
    function refreshFilteredDevices() {
        filteredDeviceModel.clear()
        if (!deviceData) return
        for (var i = 0; i < deviceData.rowCount(); i++) {
            var dev = deviceData.deviceAt(i)
            if (dev && String(dev.group) === String(root.groupId)) {
                filteredDeviceModel.append({
                    "deviceId": dev.id,
                    "deviceName": dev.name,
                    "productKey": dev.productKey,
                    "statusValue": dev.status,
                    "statusText": dev.statusText ? dev.statusText() : (dev.status === 1 ? "在线" : dev.status === 2 ? "告警" : dev.status === 3 ? "维护" : "离线")
                })
            }
        }
    }

    // 数据变化时自动刷新
    Connections {
        target: deviceData
        function onCountsChanged() {
            refreshFilteredDevices()
        }
    }

    onGroupIdChanged: refreshFilteredDevices()
    Component.onCompleted: refreshFilteredDevices()
}
