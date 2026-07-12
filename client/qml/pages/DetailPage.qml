import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import "../components"

Rectangle {
    id: root

    property var currentDevice: null
    property bool isDark: true

    function addDataPoint(metric, value, timestamp) {
        if (currentDevice) {
            chart.addDataPoint(metric, value, timestamp);
        }
    }

    function showDevice(deviceId) {
        for (var i = 0; i < deviceModel.rowCount(); i++) {
            var device = deviceModel.deviceAt(i);
            if (device.id === deviceId) {
                currentDevice = device;
                // 加载该设备的告警
                alarmModel.setDeviceFilter(deviceId);
                // 加载影子数据
                if (dataManager && dataManager.online)
                    dataManager.httpClient.getShadow(deviceId);
                return;
            }
        }
    }

    color: isDark ? "#1e1e2e" : "#f5f5f5"

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        padding: 16

        ColumnLayout {
            spacing: 16
            width: parent.width

            // ===== 顶栏 =====
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    Material.foreground: root.isDark ? "#a6adc8" : "#666666"
                    flat: true
                    font.pixelSize: 13
                    text: "← 返回"
                    onClicked: {
                        currentDevice = null;
                        alarmModel.setDeviceFilter("");
                        stackView.currentIndex = 0;
                        sidebar.currentIndex = 0;
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                        font.bold: true
                        font.pixelSize: 18
                        text: currentDevice ? currentDevice.name : ""
                    }
                    Label {
                        color: root.isDark ? "#a6adc8" : "#666666"
                        font.pixelSize: 12
                        text: currentDevice ? currentDevice.id + " · " + currentDevice.group : ""
                    }
                }

                RowLayout {
                    spacing: 6
                    StatusIndicator {
                        height: 10; width: 10
                        status: currentDevice ? currentDevice.status : 0
                    }
                    Label {
                        color: currentDevice ? (currentDevice.status === 0 ? "#9E9E9E" : currentDevice.status === 1 ? "#4CAF50" : currentDevice.status === 2 ? "#FF5722" : "#FFC107") : "#9E9E9E"
                        font.pixelSize: 13
                        text: currentDevice ? (currentDevice.status === 0 ? "离线" : currentDevice.status === 1 ? "在线" : currentDevice.status === 2 ? "告警" : "维护") : ""
                    }
                }

                Rectangle {
                    Layout.preferredHeight: 20; Layout.preferredWidth: 1
                    color: root.isDark ? "#45475a" : "#e0e0e0"
                }

                Label {
                    color: root.isDark ? "#a6adc8" : "#666666"
                    font.pixelSize: 12
                    text: currentDevice ? "上报次数: " + currentDevice.reportCount + " · 最后: " + Qt.formatDateTime(currentDevice.lastSeen, "MM-dd HH:mm:ss") : ""
                }
            }

            // ===== 基础信息卡片 =====
            Rectangle {
                Layout.fillWidth: true
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1
                color: root.isDark ? "#313244" : "#ffffff"
                radius: 12

                GridLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    columnSpacing: 16; columns: 4; rowSpacing: 12

                    MetricCard {
                        Layout.fillWidth: true
                        icon: "🌡️"; isDark: root.isDark; title: "温度"
                        value: currentDevice ? currentDevice.temperature.toFixed(1) + "°C" : "--"
                    }
                    MetricCard {
                        Layout.fillWidth: true
                        icon: "💧"; isDark: root.isDark; title: "湿度"
                        value: currentDevice ? currentDevice.humidity.toFixed(0) + "%" : "--"
                    }
                    MetricCard {
                        Layout.fillWidth: true
                        icon: "🔋"; isDark: root.isDark; title: "电量"
                        value: currentDevice ? currentDevice.battery.toFixed(0) + "%" : "--"
                    }
                    MetricCard {
                        Layout.fillWidth: true
                        icon: "📦"; isDark: root.isDark; title: "产品密钥"
                        value: currentDevice ? currentDevice.productKey : "--"
                    }
                }
            }

            // ===== 实时趋势图 =====
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 260
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1
                color: root.isDark ? "#313244" : "#ffffff"
                radius: 12

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8

                    Label {
                        color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                        font.bold: true; font.pixelSize: 15
                        text: "实时监控"
                    }
                    RealtimeChart {
                        id: chart
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        isDark: root.isDark
                    }
                }
            }

            // ===== Tab 区域 =====
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 420
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1
                color: root.isDark ? "#313244" : "#ffffff"
                radius: 12

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 0
                    spacing: 0

                    TabBar {
                        id: detailTabBar
                        Layout.fillWidth: true
                        background: Rectangle {
                            color: "transparent"
                            Rectangle {
                                width: parent.width; height: 1
                                anchors.bottom: parent.bottom
                                color: root.isDark ? "#45475a" : "#e0e0e0"
                            }
                        }

                        TabButton {
                            text: "设备影子"
                            font.pixelSize: 13
                            Material.foreground: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            width: implicitWidth
                        }
                        TabButton {
                            text: "告警记录"
                            font.pixelSize: 13
                            Material.foreground: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            width: implicitWidth
                        }
                        TabButton {
                            text: "指令下发"
                            font.pixelSize: 13
                            Material.foreground: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            width: implicitWidth
                        }
                        TabButton {
                            text: "数据点历史"
                            font.pixelSize: 13
                            Material.foreground: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            width: implicitWidth
                        }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: 16
                        currentIndex: detailTabBar.currentIndex

                        // ---- Tab 0: 设备影子 ----
                        Item {
                            RowLayout {
                                anchors.fill: parent
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true; Layout.fillHeight: true
                                    spacing: 6

                                    Label {
                                        color: root.isDark ? "#a6adc8" : "#666666"
                                        font.bold: true; font.pixelSize: 11
                                        text: "Desired"
                                    }
                                    Rectangle {
                                        Layout.fillHeight: true; Layout.fillWidth: true
                                        border.color: root.isDark ? "#45475a" : "#e0e0e0"
                                        border.width: 1
                                        color: root.isDark ? "#1e1e2e" : "#f8f9fa"
                                        radius: 8

                                        ScrollView {
                                            anchors.fill: parent; anchors.margins: 10; clip: true
                                            Label {
                                                color: root.isDark ? "#a6e3a1" : "#2e7d32"
                                                font.family: "Monaco"; font.pixelSize: 12
                                                text: shadowDesiredText
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true; Layout.fillHeight: true
                                    spacing: 6

                                    Label {
                                        color: root.isDark ? "#a6adc8" : "#666666"
                                        font.bold: true; font.pixelSize: 11
                                        text: "Reported"
                                    }
                                    Rectangle {
                                        Layout.fillHeight: true; Layout.fillWidth: true
                                        border.color: root.isDark ? "#45475a" : "#e0e0e0"
                                        border.width: 1
                                        color: root.isDark ? "#1e1e2e" : "#f8f9fa"
                                        radius: 8

                                        ScrollView {
                                            anchors.fill: parent; anchors.margins: 10; clip: true
                                            Label {
                                                color: root.isDark ? "#89b4fa" : "#1565c0"
                                                font.family: "Monaco"; font.pixelSize: 12
                                                text: shadowReportedText
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // ---- Tab 1: 告警记录 ----
                        Item {
                            ListView {
                                anchors.fill: parent
                                clip: true
                                model: alarmListModel

                                header: RowLayout {
                                    width: parent.width
                                    height: 28
                                    spacing: 8

                                    Label { Layout.preferredWidth: 60; color: root.isDark ? "#a6adc8" : "#666"; font.pixelSize: 11; font.bold: true; text: "级别" }
                                    Label { Layout.preferredWidth: 100; color: root.isDark ? "#a6adc8" : "#666"; font.pixelSize: 11; font.bold: true; text: "指标" }
                                    Label { Layout.preferredWidth: 80; color: root.isDark ? "#a6adc8" : "#666"; font.pixelSize: 11; font.bold: true; text: "当前值" }
                                    Label { Layout.preferredWidth: 80; color: root.isDark ? "#a6adc8" : "#666"; font.pixelSize: 11; font.bold: true; text: "阈值" }
                                    Label { Layout.preferredWidth: 80; color: root.isDark ? "#a6adc8" : "#666"; font.pixelSize: 11; font.bold: true; text: "状态" }
                                    Label { Layout.fillWidth: true; color: root.isDark ? "#a6adc8" : "#666"; font.pixelSize: 11; font.bold: true; text: "时间" }
                                }

                                delegate: Rectangle {
                                    width: parent.width; height: 36
                                    color: "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 4; spacing: 8

                                        Rectangle {
                                            Layout.preferredWidth: 52; Layout.preferredHeight: 20
                                            radius: 4
                                            color: model.severity === 0 ? "#2196F3" : model.severity === 1 ? "#FFC107" : "#FF5722"
                                            Label {
                                                anchors.centerIn: parent
                                                color: "white"; font.pixelSize: 9; font.bold: true
                                                text: model.severityText
                                            }
                                        }
                                        Label { Layout.preferredWidth: 100; color: root.isDark ? "#cdd6f4" : "#1e1e2e"; font.pixelSize: 11; text: model.metric }
                                        Label { Layout.preferredWidth: 80; color: root.isDark ? "#cdd6f4" : "#1e1e2e"; font.pixelSize: 11; text: model.value.toFixed(1) }
                                        Label { Layout.preferredWidth: 80; color: root.isDark ? "#cdd6f4" : "#1e1e2e"; font.pixelSize: 11; text: model.threshold.toFixed(1) }
                                        Label { Layout.preferredWidth: 80; color: model.status === 0 ? "#f38ba8" : model.status === 1 ? "#89b4fa" : "#a6e3a1"; font.pixelSize: 11; text: model.statusText }
                                        Label { Layout.fillWidth: true; color: root.isDark ? "#a6adc8" : "#666"; font.pixelSize: 11; text: model.triggeredAt }
                                    }
                                }
                            }
                        }

                        // ---- Tab 2: 指令下发 ----
                        Item {
                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 12

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    TextField {
                                        id: cmdNameField
                                        Layout.preferredWidth: 120
                                        placeholderText: "指令名"
                                        font.pixelSize: 12
                                    }
                                    TextField {
                                        id: cmdPayloadField
                                        Layout.fillWidth: true
                                        placeholderText: 'Payload JSON (如 {"speed":"high"})'
                                        font.pixelSize: 12
                                    }
                                    Button {
                                        Material.background: "#89b4fa"
                                        Material.foreground: "white"
                                        text: "发送"
                                        onClicked: {
                                            if (cmdNameField.text && currentDevice) {
                                                var payload = {};
                                                try { payload = JSON.parse(cmdPayloadField.text || "{}"); } catch(e) {}
                                                dataManager.httpClient.sendCommand(currentDevice.id, cmdNameField.text, payload);
                                            }
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true; spacing: 8
                                    Repeater {
                                        model: ListModel {
                                            ListElement { cmd: "fan_on"; name: "开启风扇" }
                                            ListElement { cmd: "fan_off"; name: "关闭风扇" }
                                            ListElement { cmd: "set_temp"; name: "设置温度" }
                                            ListElement { cmd: "reboot"; name: "重启设备"; danger: true }
                                        }
                                        Button {
                                            Layout.fillWidth: true
                                            text: model.name
                                            background: Rectangle {
                                                border.color: model.danger ? "#f38ba8" : (root.isDark ? "#45475a" : "#cbd5e1")
                                                border.width: 1
                                                color: parent.hovered ? (root.isDark ? "#3b3b4f" : "#e8edf5") : (root.isDark ? "#1e1e2e" : "#f8f9fa")
                                                implicitHeight: 36; radius: 8
                                            }
                                            contentItem: Label {
                                                color: model.danger ? "#f38ba8" : (root.isDark ? "#cdd6f4" : "#1e1e2e")
                                                font.bold: model.danger; font.pixelSize: 12
                                                horizontalAlignment: Text.AlignHCenter
                                                text: parent.text
                                            }
                                            onClicked: {
                                                if (currentDevice)
                                                    dataManager.httpClient.sendCommand(currentDevice.id, model.cmd)
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true; Layout.fillHeight: true
                                    border.color: root.isDark ? "#45475a" : "#e0e0e0"
                                    border.width: 1
                                    color: root.isDark ? "#1e1e2e" : "#f8f9fa"
                                    radius: 8

                                    Label {
                                        anchors.centerIn: parent
                                        color: root.isDark ? "#a6adc8" : "#666"
                                        font.pixelSize: 12
                                        text: "指令结果将在此显示"
                                    }
                                }
                            }
                        }

                        // ---- Tab 3: 数据点历史 ----
                        Item {
                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 12

                                RowLayout {
                                    Layout.fillWidth: true; spacing: 8

                                    Label { color: root.isDark ? "#a6adc8" : "#666"; font.pixelSize: 12; text: "指标:" }
                                    ComboBox {
                                        id: historyMetric
                                        model: ["temperature", "humidity", "battery"]
                                        Layout.preferredWidth: 130
                                    }
                                    Label { color: root.isDark ? "#a6adc8" : "#666"; font.pixelSize: 12; text: "时间:" }
                                    ComboBox {
                                        id: historyRange
                                        model: ["最近1小时", "最近6小时", "最近24小时", "最近7天"]
                                        Layout.preferredWidth: 130
                                    }
                                    Item { Layout.fillWidth: true }
                                    Button {
                                        Material.background: "#89b4fa"
                                        Material.foreground: "white"
                                        text: "查询"
                                        onClicked: {
                                            if (currentDevice) {
                                                var now = Math.floor(Date.now() / 1000);
                                                var ranges = [3600, 21600, 86400, 604800];
                                                var start = now - ranges[historyRange.currentIndex];
                                                dataManager.fetchDataPointHistory(currentDevice.id, historyMetric.currentText, start, now, 200);
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true; Layout.fillHeight: true
                                    border.color: root.isDark ? "#45475a" : "#e0e0e0"
                                    border.width: 1
                                    color: root.isDark ? "#1e1e2e" : "#f8f9fa"
                                    radius: 8

                                    ListView {
                                        id: historyList
                                        anchors.fill: parent; anchors.margins: 8
                                        clip: true
                                        model: historyDataModel

                                        header: RowLayout {
                                            width: parent.width; height: 28; spacing: 8
                                            Label { Layout.fillWidth: true; color: root.isDark ? "#a6adc8" : "#666"; font.pixelSize: 11; font.bold: true; text: "时间" }
                                            Label { Layout.preferredWidth: 120; color: root.isDark ? "#a6adc8" : "#666"; font.pixelSize: 11; font.bold: true; text: "数值" }
                                        }

                                        delegate: Rectangle {
                                            width: parent.width; height: 28
                                            color: index % 2 === 0 ? "transparent" : (root.isDark ? "#ffffff08" : "#00000005")

                                            RowLayout {
                                                anchors.fill: parent; spacing: 8
                                                Label { Layout.fillWidth: true; color: root.isDark ? "#cdd6f4" : "#1e1e2e"; font.pixelSize: 11; text: model.time }
                                                Label { Layout.preferredWidth: 120; color: root.isDark ? "#a6e3a1" : "#2e7d32"; font.pixelSize: 11; text: model.value.toFixed(2) }
                                            }
                                        }

                                        Label {
                                            anchors.centerIn: parent
                                            visible: historyDataModel.count === 0
                                            color: root.isDark ? "#a6adc8" : "#666"
                                            font.pixelSize: 12
                                            text: "点击查询获取历史数据"
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

    // Shadow 文本 (简化，实际可从 C++ model 获取)
    property string shadowDesiredText: '{\n  "temperature": 25,\n  "fan_speed": "high"\n}'
    property string shadowReportedText: '{\n  "temperature": 23.5,\n  "humidity": 65,\n  "fan_speed": "low",\n  "battery": 85\n}'
}
