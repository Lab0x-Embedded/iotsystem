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

    // Shadow 文本
    property string shadowDesiredText: '{\n  "temperature": 25,\n  "fan_speed": "high"\n}'
    property string shadowReportedText: '{\n  "temperature": 23.5,\n  "humidity": 65,\n  "fan_speed": "low",\n  "battery": 85\n}'
    property color toastColor: "#3874F7"

    // Toast notification
    property string toastText: ""

    function addDataPoint(deviceId, metric, value, timestamp) {
        if (currentDevice && currentDevice.id === deviceId) {
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
                chart.clearData();
                alarmModel.setDeviceFilter(deviceId);
                // 加载影子数据
                if (dataManager && dataManager.online)
                    dataManager.httpClient.getShadow(deviceId);
                return;
            }
        }
    }
    function showToast(msg, color) {
        toastText = msg;
        toastColor = color || "#3874F7";
        toastTimer.restart();
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
                        height: 10
                        status: currentDevice ? currentDevice.status : 0
                        width: 10
                    }
                    Label {
                        color: currentDevice ? (currentDevice.status === 0 ? "#9E9E9E" : currentDevice.status === 1 ? "#4CAF50" : currentDevice.status === 2 ? "#FF5722" : "#FFC107") : "#9E9E9E"
                        font.pixelSize: 13
                        text: currentDevice ? (currentDevice.status === 0 ? "离线" : currentDevice.status === 1 ? "在线" : currentDevice.status === 2 ? "告警" : "维护") : ""
                    }
                }
                Rectangle {
                    Layout.preferredHeight: 20
                    Layout.preferredWidth: 1
                    color: root.isDark ? "#45475a" : "#e0e0e0"
                }
                Label {
                    color: root.isDark ? "#a6adc8" : "#666666"
                    font.pixelSize: 12
                    text: currentDevice ? "上报次数: " + (currentDevice.reportCount + realtimeCount) + " · 最后: " + Qt.formatDateTime(currentDevice.lastSeen, "MM-dd HH:mm:ss") : ""
                }
                Item {
                    Layout.fillWidth: true
                }
                Button {
                    Material.foreground: "#f38ba8"
                    flat: true
                    font.pixelSize: 12
                    text: "⟳ 重启设备"
                    visible: currentDevice && currentDevice.status === 1

                    background: Rectangle {
                        border.color: "#f38ba8"
                        border.width: 1
                        color: parent.hovered ? (root.isDark ? "#3b3b4f" : "#fef2f2") : "transparent"
                        implicitHeight: 30
                        radius: 6
                    }

                    onClicked: {
                        if (currentDevice)
                            dataManager.httpClient.sendCommand(currentDevice.id, "reboot");
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
                    columnSpacing: 16
                    columns: 4
                    rowSpacing: 12

                    MetricCard {
                        Layout.fillWidth: true
                        icon: "🌡️"
                        isDark: root.isDark
                        title: "温度"
                        value: currentDevice ? currentDevice.temperature.toFixed(1) + "°C" : "--"
                    }
                    MetricCard {
                        Layout.fillWidth: true
                        icon: "💧"
                        isDark: root.isDark
                        title: "湿度"
                        value: currentDevice ? currentDevice.humidity.toFixed(0) + "%" : "--"
                    }
                    MetricCard {
                        Layout.fillWidth: true
                        icon: "🔋"
                        isDark: root.isDark
                        title: "电量"
                        value: currentDevice ? currentDevice.battery.toFixed(0) + "%" : "--"
                    }
                    MetricCard {
                        Layout.fillWidth: true
                        icon: "📦"
                        isDark: root.isDark
                        title: "产品密钥"
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
                        font.bold: true
                        font.pixelSize: 15
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
                                anchors.bottom: parent.bottom
                                color: root.isDark ? "#45475a" : "#e0e0e0"
                                height: 1
                                width: parent.width
                            }
                        }

                        TabButton {
                            Material.foreground: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            font.pixelSize: 13
                            text: "设备影子"
                            width: implicitWidth
                        }
                        TabButton {
                            Material.foreground: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            font.pixelSize: 13
                            text: "告警记录"
                            width: implicitWidth
                        }
                        TabButton {
                            Material.foreground: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            font.pixelSize: 13
                            text: "指令下发"
                            width: implicitWidth
                        }
                        TabButton {
                            Material.foreground: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            font.pixelSize: 13
                            text: "数据点历史"
                            width: implicitWidth
                        }
                    }
                    StackLayout {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.margins: 16
                        currentIndex: detailTabBar.currentIndex

                        // ---- Tab 0: 设备影子 ----
                        Item {
                            RowLayout {
                                anchors.fill: parent
                                spacing: 16

                                // ---- Reported (left) ----
                                Rectangle {
                                    Layout.fillHeight: true
                                    Layout.fillWidth: true
                                    border.color: root.isDark ? "#45475a" : "#e0e0e0"
                                    border.width: 1
                                    color: root.isDark ? "#1e1e2e" : "#ffffff"
                                    radius: 10

                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 0

                                        // Header
                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 36
                                            color: root.isDark ? "#313244" : "#f0f0f5"
                                            radius: 10

                                            Rectangle {
                                                anchors.bottom: parent.bottom
                                                color: parent.color
                                                height: 10
                                                width: parent.width
                                            }
                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 12
                                                anchors.rightMargin: 12
                                                spacing: 6

                                                Rectangle {
                                                    color: "#4CAF50"
                                                    height: 8
                                                    radius: 4
                                                    width: 8
                                                }
                                                Label {
                                                    color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                                    font.bold: true
                                                    font.pixelSize: 12
                                                    text: "Reported（上报状态）"
                                                }
                                                Item {
                                                    Layout.fillWidth: true
                                                }
                                                Label {
                                                    color: root.isDark ? "#585b70" : "#aaa"
                                                    font.pixelSize: 10
                                                    text: "只读"
                                                }
                                            }
                                        }

                                        // Content
                                        ScrollView {
                                            Layout.fillHeight: true
                                            Layout.fillWidth: true
                                            Layout.margins: 12
                                            clip: true

                                            Label {
                                                color: root.isDark ? "#a6e3a1" : "#16a34a"
                                                font.family: "Monaco"
                                                font.pixelSize: 12
                                                text: shadowReportedText
                                                width: parent.width
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }
                                }

                                // ---- Desired (right) ----
                                Rectangle {
                                    Layout.fillHeight: true
                                    Layout.fillWidth: true
                                    border.color: root.isDark ? "#45475a" : "#e0e0e0"
                                    border.width: 1
                                    color: root.isDark ? "#1e1e2e" : "#ffffff"
                                    radius: 10

                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 0

                                        // Header
                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 36
                                            color: root.isDark ? "#313244" : "#f0f0f5"
                                            radius: 10

                                            Rectangle {
                                                anchors.bottom: parent.bottom
                                                color: parent.color
                                                height: 10
                                                width: parent.width
                                            }
                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 12
                                                anchors.rightMargin: 12
                                                spacing: 6

                                                Rectangle {
                                                    color: "#89b4fa"
                                                    height: 8
                                                    radius: 4
                                                    width: 8
                                                }
                                                Label {
                                                    color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                                    font.bold: true
                                                    font.pixelSize: 12
                                                    text: "Desired（期望状态）"
                                                }
                                                Item {
                                                    Layout.fillWidth: true
                                                }
                                                Button {
                                                    Material.foreground: "#a6e3a1"
                                                    flat: true
                                                    font.pixelSize: 11
                                                    text: "💾 保存"
                                                    visible: currentDevice && currentDevice.status === 1

                                                    background: Rectangle {
                                                        border.color: "#a6e3a1"
                                                        border.width: 1
                                                        color: parent.hovered ? (root.isDark ? "#3b3b4f" : "#f0fdf4") : "transparent"
                                                        implicitHeight: 26
                                                        radius: 6
                                                    }

                                                    onClicked: {
                                                        if (!currentDevice)
                                                            return;
                                                        try {
                                                            var obj = JSON.parse(desiredEditor.text);
                                                            dataManager.httpClient.updateShadow(currentDevice.id, obj);
                                                            root.showToast("保存中...", "#3874F7");
                                                        } catch (e) {
                                                            root.showToast("JSON 格式错误: " + e, "#f38ba8");
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        // Editor
                                        ScrollView {
                                            Layout.fillHeight: true
                                            Layout.fillWidth: true
                                            Layout.margins: 8
                                            clip: true

                                            TextArea {
                                                id: desiredEditor

                                                color: root.isDark ? "#89b4fa" : "#2563eb"
                                                font.family: "Monaco"
                                                font.pixelSize: 12
                                                selectByMouse: true
                                                text: shadowDesiredText
                                                wrapMode: TextArea.Wrap

                                                background: Rectangle {
                                                    color: "transparent"
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // ---- Tab 1: 告警记录 ----
                        Item {
                            Rectangle {
                                anchors.fill: parent
                                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                                border.width: 1
                                color: root.isDark ? "#1e1e2e" : "#ffffff"
                                radius: 10

                                ListView {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    clip: true
                                    model: alarmModel

                                    delegate: Rectangle {
                                        color: index % 2 === 0 ? (root.isDark ? "#1e1e2e" : "#ffffff") : (root.isDark ? "#252536" : "#f8f8fc")
                                        height: 36
                                        radius: 0
                                        width: ListView.view.width

                                        // 底部分隔线
                                        Rectangle {
                                            anchors.bottom: parent.bottom
                                            anchors.left: parent.left
                                            anchors.leftMargin: 16
                                            anchors.right: parent.right
                                            anchors.rightMargin: 16
                                            color: root.isDark ? "#ffffff0a" : "#00000008"
                                            height: 1
                                        }
                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 16
                                            anchors.rightMargin: 16
                                            spacing: 12

                                            Rectangle {
                                                Layout.preferredHeight: 22
                                                Layout.preferredWidth: 110
                                                border.color: model.severity === 0 ? "#1677ff" : model.severity === 1 ? "#faad14" : "#ff4d4f"
                                                border.width: 1
                                                color: model.severity === 0 ? "#e6f4ff" : model.severity === 1 ? "#fff7e6" : "#fff1f0"
                                                radius: 4

                                                Label {
                                                    anchors.centerIn: parent
                                                    color: model.severity === 0 ? "#1677ff" : model.severity === 1 ? "#faad14" : "#ff4d4f"
                                                    font.bold: true
                                                    font.pixelSize: 10
                                                    text: model.severityText
                                                }
                                            }
                                            SelectableLabel {
                                                Layout.preferredWidth: 100
                                                text: model.metric
                                                textColor: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                            }
                                            SelectableLabel {
                                                Layout.preferredWidth: 80
                                                text: model.value.toFixed(1)
                                                textColor: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                            }
                                            Label {
                                                Layout.preferredWidth: 90
                                                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                                font.pixelSize: 11
                                                text: model.threshold.toFixed(1)
                                            }
                                            Rectangle {
                                                Layout.preferredHeight: 22
                                                Layout.preferredWidth: 80
                                                border.color: model.status === 0 ? "#ff4d4f" : model.status === 1 ? "#1677ff" : "#52c41a"
                                                border.width: 1
                                                color: model.status === 0 ? "#fff1f0" : model.status === 1 ? "#e6f4ff" : "#f6ffed"
                                                radius: 4

                                                Label {
                                                    anchors.centerIn: parent
                                                    color: model.status === 0 ? "#ff4d4f" : model.status === 1 ? "#1677ff" : "#52c41a"
                                                    font.pixelSize: 10
                                                    text: model.statusText
                                                }
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                color: root.isDark ? "#a6adc8" : "#666"
                                                font.pixelSize: 11
                                                text: model.triggeredAt
                                            }
                                        }
                                    }
                                    header: Rectangle {
                                        color: root.isDark ? "#313244" : "#f0f0f5"
                                        height: 28
                                        radius: 10
                                        width: ListView.view ? ListView.view.width : 0

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 16
                                            anchors.rightMargin: 16
                                            spacing: 12

                                            Label {
                                                Layout.preferredWidth: 110
                                                color: root.isDark ? "#a6adc8" : "#666"
                                                font.bold: true
                                                font.pixelSize: 11
                                                text: "级别"
                                            }
                                            Label {
                                                Layout.preferredWidth: 100
                                                color: root.isDark ? "#a6adc8" : "#666"
                                                font.bold: true
                                                font.pixelSize: 11
                                                text: "指标"
                                            }
                                            Label {
                                                Layout.preferredWidth: 80
                                                color: root.isDark ? "#a6adc8" : "#666"
                                                font.bold: true
                                                font.pixelSize: 11
                                                text: "当前值"
                                            }
                                            Label {
                                                Layout.preferredWidth: 90
                                                color: root.isDark ? "#a6adc8" : "#666"
                                                font.bold: true
                                                font.pixelSize: 11
                                                text: "阈值"
                                            }
                                            Label {
                                                Layout.preferredWidth: 80
                                                color: root.isDark ? "#a6adc8" : "#666"
                                                font.bold: true
                                                font.pixelSize: 11
                                                text: "状态"
                                            }
                                            Label {
                                                Layout.fillWidth: true
                                                color: root.isDark ? "#a6adc8" : "#666"
                                                font.bold: true
                                                font.pixelSize: 11
                                                text: "时间"
                                            }
                                        }
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

                                        Layout.preferredHeight: 36
                                        Layout.preferredWidth: 120
                                        font.pixelSize: 12
                                        placeholderText: "指令名"
                                    }
                                    TextField {
                                        id: cmdPayloadField

                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 36
                                        font.pixelSize: 12
                                        placeholderText: 'Payload JSON (如 {"speed":"high"})'
                                    }
                                    RoundedButton {
                                        Material.background: "#3874F7"
                                        Material.foreground: "white"
                                        text: "发送"

                                        onClicked: {
                                            if (cmdNameField.text && currentDevice) {
                                                var payload = {};
                                                try {
                                                    payload = JSON.parse(cmdPayloadField.text || "{}");
                                                } catch (e) {}
                                                dataManager.httpClient.sendCommand(currentDevice.id, cmdNameField.text, payload);
                                            }
                                        }
                                    }
                                }
                                Rectangle {
                                    Layout.fillHeight: true
                                    Layout.fillWidth: true
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
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Label {
                                        color: root.isDark ? "#a6adc8" : "#666"
                                        font.pixelSize: 12
                                        text: "指标:"
                                    }
                                    ComboBox {
                                        id: historyMetric

                                        Layout.preferredHeight: 36
                                        Layout.preferredWidth: 130
                                        model: ["temperature", "humidity", "battery"]
                                    }
                                    Label {
                                        color: root.isDark ? "#a6adc8" : "#666"
                                        font.pixelSize: 12
                                        text: "时间:"
                                    }
                                    ComboBox {
                                        id: historyRange

                                        Layout.preferredHeight: 36
                                        Layout.preferredWidth: 130
                                        model: ["最近1小时", "最近6小时", "最近24小时", "最近7天"]
                                    }
                                    Item {
                                        Layout.fillWidth: true
                                    }
                                    RoundedButton {
                                        Material.background: "#3874F7"
                                        Material.foreground: "#ffffff"
                                        font.bold: true
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
                                    Layout.fillHeight: true
                                    Layout.fillWidth: true
                                    border.color: root.isDark ? "#45475a" : "#e0e0e0"
                                    border.width: 1
                                    color: root.isDark ? "#1e1e2e" : "#ffffff"
                                    radius: 10

                                    ListView {
                                        id: historyList

                                        anchors.fill: parent
                                        anchors.margins: 1
                                        clip: true
                                        model: historyDataModel

                                        delegate: Rectangle {
                                            color: index % 2 === 0 ? (root.isDark ? "#1e1e2e" : "#ffffff") : (root.isDark ? "#252536" : "#f8f8fc")
                                            height: 36
                                            radius: 0
                                            width: ListView.view.width

                                            // 底部分隔线
                                            Rectangle {
                                                anchors.bottom: parent.bottom
                                                anchors.left: parent.left
                                                anchors.leftMargin: 16
                                                anchors.right: parent.right
                                                anchors.rightMargin: 16
                                                color: root.isDark ? "#ffffff0a" : "#00000008"
                                                height: 1
                                            }
                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 16
                                                anchors.rightMargin: 16
                                                spacing: 0

                                                Label {
                                                    Layout.preferredWidth: 40
                                                    color: root.isDark ? "#585b70" : "#aaa"
                                                    font.pixelSize: 11
                                                    text: (index + 1)
                                                }
                                                Label {
                                                    Layout.fillWidth: true
                                                    color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                                                    font.pixelSize: 12
                                                    text: model.time
                                                }
                                                Rectangle {
                                                    Layout.preferredWidth: 120
                                                    color: root.isDark ? "#4CAF5020" : "#16a34a15"
                                                    height: 24
                                                    radius: 4

                                                    Label {
                                                        anchors.centerIn: parent
                                                        color: root.isDark ? "#4CAF50" : "#16a34a"
                                                        font.bold: true
                                                        font.pixelSize: 12
                                                        text: model.value.toFixed(2)
                                                    }
                                                }
                                            }
                                        }
                                        header: Rectangle {
                                            color: root.isDark ? "#313244" : "#f0f0f5"
                                            height: 36
                                            radius: 10
                                            width: ListView.view ? ListView.view.width : 0

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 16
                                                anchors.rightMargin: 16
                                                spacing: 0

                                                Label {
                                                    Layout.preferredWidth: 40
                                                    color: root.isDark ? "#585b70" : "#999"
                                                    font.bold: true
                                                    font.pixelSize: 11
                                                    text: "#"
                                                }
                                                Label {
                                                    Layout.fillWidth: true
                                                    color: root.isDark ? "#a6adc8" : "#666"
                                                    font.bold: true
                                                    font.pixelSize: 11
                                                    text: "时间"
                                                }
                                                Label {
                                                    Layout.preferredWidth: 120
                                                    color: root.isDark ? "#a6adc8" : "#666"
                                                    font.bold: true
                                                    font.pixelSize: 11
                                                    horizontalAlignment: Text.AlignRight
                                                    text: "数值"
                                                }
                                            }
                                        }

                                        // 空状态
                                        Column {
                                            anchors.centerIn: parent
                                            spacing: 8
                                            visible: historyDataModel.count === 0

                                            Label {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                color: root.isDark ? "#585b70" : "#bbb"
                                                font.pixelSize: 28
                                                text: "📊"
                                            }
                                            Label {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                color: root.isDark ? "#585b70" : "#999"
                                                font.pixelSize: 13
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
    }

    // Toast bar
    Rectangle {
        id: toastBar

        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        color: root.toastColor
        height: 36
        opacity: root.toastText ? 1.0 : 0.0
        radius: 8
        visible: opacity > 0
        width: toastLabel.implicitWidth + 32

        Behavior on opacity {
            NumberAnimation {
                duration: 200
            }
        }

        Label {
            id: toastLabel

            anchors.centerIn: parent
            color: "white"
            font.bold: true
            font.pixelSize: 12
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
        function onShadowError(error) {
            root.showToast("保存失败: " + error, "#f38ba8");
        }
        function onShadowUpdated(deviceId) {
            if (currentDevice && currentDevice.id === deviceId)
                root.showToast("保存成功", "#4CAF50");
        }

        target: dataManager ? dataManager.httpClient : null
    }
}
