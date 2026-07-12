import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import "../components"

Rectangle {
    id: root

    property var currentDevice: null
    property bool isDark: true
    property int realtimeCount: 0

    function addDataPoint(metric, value, timestamp) {
        if (currentDevice) {
            realtimeCount++;
            chart.addDataPoint(metric, value, timestamp);
        }
    }

    function showDevice(deviceId) {
        for (var i = 0; i < deviceModel.rowCount(); i++) {
            var device = deviceModel.deviceAt(i);
            if (device.id === deviceId) {
                currentDevice = device;
                realtimeCount = 0;
                alarmModel.setDeviceFilter(deviceId);
                // 加载影子数据
                if (dataManager && dataManager.online)
                    dataManager.httpClient.getShadow(deviceId);
                return;
            }
        }
    }

    color: isDark ? "#1e1e2e" : "#f5f5f5"

    // Toast notification
    property string toastText: ""
    property color toastColor: "#89b4fa"

    function showToast(msg, color) {
        toastText = msg;
        toastColor = color || "#89b4fa";
        toastTimer.restart();
    }

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
                    text: currentDevice ? "上报次数: " + (currentDevice.reportCount + realtimeCount) + " · 最后: " + Qt.formatDateTime(currentDevice.lastSeen, "MM-dd HH:mm:ss") : ""
                }

                Item { Layout.fillWidth: true }

                Button {
                    visible: currentDevice && currentDevice.status === 1
                    Material.foreground: "#f38ba8"
                    flat: true
                    font.pixelSize: 12
                    text: "⟳ 重启设备"

                    background: Rectangle {
                        border.color: "#f38ba8"
                        border.width: 1
                        color: parent.hovered ? (root.isDark ? "#3b3b4f" : "#fef2f2") : "transparent"
                        implicitHeight: 30; radius: 6
                    }

                    onClicked: {
                        if (currentDevice)
                            dataManager.httpClient.sendCommand(currentDevice.id, "reboot")
                    }
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

                                // ---- Reported (left) ----
                                ColumnLayout {
                                    Layout.fillWidth: true; Layout.fillHeight: true
                                    spacing: 6

                                    Label {
                                        color: root.isDark ? "#a6adc8" : "#666666"
                                        font.bold: true; font.pixelSize: 11
                                        text: "Reported（报告状态）"
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

                                // ---- Desired (right) ----
                                ColumnLayout {
                                    Layout.fillWidth: true; Layout.fillHeight: true
                                    spacing: 6

                                    RowLayout {
                                        Layout.fillWidth: true

                                        Label {
                                            color: root.isDark ? "#a6adc8" : "#666666"
                                            font.bold: true; font.pixelSize: 11
                                            text: "Desired（期望状态）"
                                        }
                                        Item { Layout.fillWidth: true }
                                        Button {
                                            Material.foreground: "#a6e3a1"
                                            flat: true
                                            font.pixelSize: 11
                                            text: "保存"
                                            visible: currentDevice && currentDevice.status === 1

                                            background: Rectangle {
                                                border.color: "#a6e3a1"; border.width: 1
                                                color: parent.hovered ? (root.isDark ? "#3b3b4f" : "#f0fdf4") : "transparent"
                                                implicitHeight: 24; radius: 4
                                            }

                                            onClicked: {
                                                if (!currentDevice) return;
                                                try {
                                                    var obj = JSON.parse(desiredEditor.text);
                                                    dataManager.httpClient.updateShadow(currentDevice.id, obj);
                                                    root.showToast("保存中...", "#89b4fa");
                                                } catch(e) {
                                                    root.showToast("JSON 格式错误: " + e, "#f38ba8");
                                                }
                                            }
                                        }
                                    }
                                    Rectangle {
                                        Layout.fillHeight: true; Layout.fillWidth: true
                                        border.color: root.isDark ? "#45475a" : "#e0e0e0"
                                        border.width: 1
                                        color: root.isDark ? "#1e1e2e" : "#f8f9fa"
                                        radius: 8

                                        ScrollView {
                                            anchors.fill: parent; anchors.margins: 4; clip: true
                                            TextArea {
                                                id: desiredEditor
                                                color: root.isDark ? "#a6e3a1" : "#2e7d32"
                                                font.family: "Monaco"; font.pixelSize: 12
                                                wrapMode: TextArea.Wrap
                                                text: shadowDesiredText
                                                background: Rectangle { color: "transparent" }
                                                selectByMouse: true
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
                                model: alarmModel

                                header: RowLayout {
                                    width: ListView.view ? ListView.view.width : 0
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
                                    width: ListView.view.width; height: 36
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
                                            width: ListView.view ? ListView.view.width : 0; height: 28; spacing: 8
                                            Label { Layout.fillWidth: true; color: root.isDark ? "#a6adc8" : "#666"; font.pixelSize: 11; font.bold: true; text: "时间" }
                                            Label { Layout.preferredWidth: 120; color: root.isDark ? "#a6adc8" : "#666"; font.pixelSize: 11; font.bold: true; text: "数值" }
                                        }

                                        delegate: Rectangle {
                                            width: ListView.view.width; height: 28
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

    // Shadow 文本
    property string shadowDesiredText: '{\n  "temperature": 25,\n  "fan_speed": "high"\n}'
    property string shadowReportedText: '{\n  "temperature": 23.5,\n  "humidity": 65,\n  "fan_speed": "low",\n  "battery": 85\n}'

    // Toast bar
    Rectangle {
        id: toastBar
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 20
        width: toastLabel.implicitWidth + 32
        height: 36
        radius: 8
        color: root.toastColor
        opacity: root.toastText ? 1.0 : 0.0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: 200 } }

        Label {
            id: toastLabel
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: 12
            font.bold: true
            text: root.toastText
        }

        Timer {
            id: toastTimer
            interval: 2500
            onTriggered: root.toastText = ""
        }
    }

    // 监听 shadow 信号
    Connections {
        target: dataManager ? dataManager.httpClient : null

        function onShadowUpdated(deviceId) {
            if (currentDevice && currentDevice.id === deviceId)
                root.showToast("保存成功", "#a6e3a1");
        }

        function onShadowError(error) {
            root.showToast("保存失败: " + error, "#f38ba8");
        }
    }
}
