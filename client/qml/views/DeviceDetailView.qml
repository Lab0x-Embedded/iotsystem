import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtShadcn
import "../components"

// 设备详情: 头部信息 + 指标卡 + Tabs(实时/影子/指令/历史)
Rectangle {
    id: root

    property var currentDevice: null
    property string shadowDesiredText: '{\n  "temperature": 25\n}'
    property string shadowReportedText: '{\n  "temperature": 23.5\n}'
    property string toastText: ""
    property color toastColor: theme.primary
    property int realtimeCount: 0

    function showDevice(deviceId) {
        for (var i = 0; i < deviceModel.rowCount(); i++) {
            var device = deviceModel.deviceAt(i);
            if (device.id === deviceId) {
                currentDevice = device;
                realtimeCount = 0;
                detailChart.clearData();
                alarmModel.setDeviceFilter(deviceId);
                if (dataManager && dataManager.online)
                    dataManager.httpClient.getShadow(deviceId);
                return;
            }
        }
    }

    function showToast(msg, color) {
        toastText = msg;
        toastColor = color || theme.primary;
        toastTimer.restart();
    }

    function refreshHistory() {
        if (!currentDevice || !dataManager || !dataManager.online) return;
        var now = Math.floor(Date.now() / 1000);
        dataManager.fetchDataPointHistory(currentDevice.id, "temperature", now - 3600, now, 120);
        dataManager.fetchDataPointHistory(currentDevice.id, "humidity", now - 3600, now, 120);
    }

    signal backRequested()

    QtShadcnTheme { id: theme }
    color: theme.background

    // 历史数据模型 (Tab 3)
    ListModel { id: historyDataModel }

    // 历史轮询定时器 (30s)
    Timer {
        interval: 30000
        running: root.currentDevice !== null && dataManager && dataManager.online
        repeat: true
        onTriggered: root.refreshHistory()
    }

    QQC.ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: root.width
            spacing: 16

            // ===== 头部 =====
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 20
                spacing: 12

                ShadcnButton {
                    text: "返回"
                    variant: ShadcnButton.Variant.Ghost
                    size: ShadcnButton.Size.Small
                    iconName: "arrow-left"
                    onClicked: {
                        root.currentDevice = null;
                        alarmModel.setDeviceFilter("");
                        root.backRequested();
                    }
                }
                ColumnLayout {
                    spacing: 0
                    ShadcnLabel {
                        text: root.currentDevice ? root.currentDevice.name : ""
                        size: ShadcnLabel.Size.Large
                    }
                    ShadcnLabel {
                        text: root.currentDevice
                              ? root.currentDevice.id + " · " + root.currentDevice.productKey
                              : ""
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                    }
                }
                Item { Layout.fillWidth: true }

                ShadcnBadge {
                    visible: root.currentDevice !== null
                    text: root.currentDevice
                          ? (root.currentDevice.status === 1 ? "在线"
                             : root.currentDevice.status === 2 ? "告警"
                             : root.currentDevice.status === 3 ? "维护" : "离线")
                          : ""
                    variant: root.currentDevice && root.currentDevice.status === 1
                             ? ShadcnBadge.Variant.Default
                             : root.currentDevice && root.currentDevice.status === 2
                               ? ShadcnBadge.Variant.Destructive
                               : ShadcnBadge.Variant.Secondary
                }
                ShadcnButton {
                    visible: root.currentDevice && root.currentDevice.status === 1
                    text: "重启设备"
                    variant: ShadcnButton.Variant.Outline
                    size: ShadcnButton.Size.Small
                    iconName: "refresh-cw"
                    onClicked: {
                        if (root.currentDevice)
                            dataManager.httpClient.sendCommand(root.currentDevice.id, "reboot");
                    }
                }
            }

            // ===== 指标卡 =====
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                spacing: 12
                visible: root.currentDevice !== null

                StatCard {
                    Layout.fillWidth: true
                    title: "温度"
                    value: root.currentDevice ? root.currentDevice.temperature.toFixed(1) + " °C" : "--"
                    valueColor: theme.destructive
                    dotStatus: ShadcnStatusDot.Status.Warning
                }
                StatCard {
                    Layout.fillWidth: true
                    title: "湿度"
                    value: root.currentDevice ? root.currentDevice.humidity.toFixed(0) + " %" : "--"
                    valueColor: "#60a5fa"
                }
                StatCard {
                    Layout.fillWidth: true
                    title: "电量"
                    value: root.currentDevice ? root.currentDevice.battery.toFixed(0) + " %" : "--"
                    valueColor: theme.success
                }
                StatCard {
                    Layout.fillWidth: true
                    title: "上报次数"
                    value: root.currentDevice ? (root.currentDevice.reportCount + root.realtimeCount) : "--"
                }
            }

            // ===== Tabs =====
            ShadcnCard {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.bottomMargin: 20
                Layout.preferredHeight: 480

                ShadcnCardContent {
                        width: parent.width
                        height: parent.height
                        implicitHeight: 0

                    ShadcnTabsList {
                        id: detailTabs
                        variant: "line"
                        ShadcnTabsTrigger { text: "实时数据" }
                        ShadcnTabsTrigger { text: "设备影子" }
                        ShadcnTabsTrigger { text: "指令下发" }
                        ShadcnTabsTrigger { text: "历史查询" }
                    }

                    StackLayout {
                        width: parent.width
                        height: 380
                        currentIndex: detailTabs.currentIndex

                        // ---- Tab 0: 实时数据 ----
                        Item {
                            RealtimeChart {
                                id: detailChart
                                width: parent.width
                            height: parent.height
                            }
                        }

                        // ---- Tab 1: 设备影子 ----
                        RowLayout {
                            spacing: 12

                            // Reported
                            ShadcnCard {
                                Layout.fillHeight: true
                                Layout.fillWidth: true
                                size: ShadcnCard.Size.Small

                                ShadcnCardHeader {
                                    RowLayout {
                                        width: parent.width
                                        ShadcnStatusDot { status: ShadcnStatusDot.Status.Success }
                                        ShadcnCardTitle { text: "Reported (上报状态)" }
                                        Item { Layout.fillWidth: true }
                                        ShadcnBadge { text: "只读"; variant: ShadcnBadge.Variant.Secondary }
                                    }
                                }
                                ShadcnCardContent {
                                    QQC.ScrollView {
                                        width: parent.width
                            height: parent.height
                                        clip: true
                                        ShadcnLabel {
                                            width: parent.width
                                            text: root.shadowReportedText
                                            size: ShadcnLabel.Size.Small
                                            color: theme.success
                                        }
                                    }
                                }
                            }

                            // Desired
                            ShadcnCard {
                                Layout.fillHeight: true
                                Layout.fillWidth: true
                                size: ShadcnCard.Size.Small

                                ShadcnCardHeader {
                                    RowLayout {
                                        width: parent.width
                                        ShadcnStatusDot { status: ShadcnStatusDot.Status.Online }
                                        ShadcnCardTitle { text: "Desired (期望状态)" }
                                        Item { Layout.fillWidth: true }
                                        ShadcnButton {
                                            visible: root.currentDevice && root.currentDevice.status === 1
                                            text: "保存"
                                            variant: ShadcnButton.Variant.Outline
                                            size: ShadcnButton.Size.ExtraSmall
                                            iconName: "check"
                                            onClicked: {
                                                if (!root.currentDevice) return;
                                                try {
                                                    var obj = JSON.parse(desiredEditor.text);
                                                    dataManager.httpClient.updateShadow(root.currentDevice.id, obj);
                                                    root.showToast("保存中...");
                                                } catch (e) {
                                                    root.showToast("JSON 格式错误: " + e, theme.destructive);
                                                }
                                            }
                                        }
                                    }
                                }
                                ShadcnCardContent {
                                    QQC.TextArea {
                                        id: desiredEditor
                                        width: parent.width
                            height: parent.height
                                        text: root.shadowDesiredText
                                        color: theme.foreground
                                        selectByMouse: true
                                        wrapMode: QQC.TextArea.Wrap
                                        font.family: "Monaco"
                                        font.pixelSize: 12
                                        background: Rectangle {
                                            color: theme.input
                                            radius: theme.radius
                                            border.color: theme.border
                                        }
                                    }
                                }
                            }
                        }

                        // ---- Tab 2: 指令下发 ----
                        ColumnLayout {
                            spacing: 12

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                ShadcnInput {
                                    id: cmdNameField
                                    Layout.preferredWidth: 120
                                    placeholderText: "指令名"
                                }
                                ShadcnInput {
                                    id: cmdPayloadField
                                    Layout.fillWidth: true
                                    placeholderText: 'Payload JSON (如 {"speed":"high"})'
                                }
                                ShadcnButton {
                                    text: "发送"
                                    iconName: "send"
                                    onClicked: {
                                        if (cmdNameField.text && root.currentDevice) {
                                            var payload = {};
                                            try { payload = JSON.parse(cmdPayloadField.text || "{}"); } catch (e) {}
                                            dataManager.httpClient.sendCommand(root.currentDevice.id, cmdNameField.text, payload);
                                            root.showToast("指令已发送");
                                        }
                                    }
                                }
                            }

                            ShadcnAlert {
                                width: parent.width
                                title: "指令通过 MQTT QoS 1 下发"
                                description: "设备离线时指令进入队列，重连后自动重放。"
                                variant: ShadcnAlert.Variant.Default
                            }
                        }

                        // ---- Tab 3: 历史查询 ----
                        ColumnLayout {
                            spacing: 12

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                ShadcnLabel { text: "指标:"; variant: ShadcnLabel.Variant.Muted }
                                ShadcnSelect {
                                    id: historyMetric
                                    width: 140
                                    model: ["temperature", "humidity", "battery"]
                                }
                                ShadcnLabel { text: "时间:"; variant: ShadcnLabel.Variant.Muted }
                                ShadcnSelect {
                                    id: historyRange
                                    width: 140
                                    model: ["最近1小时", "最近6小时", "最近24小时", "最近7天"]
                                }
                                Item { Layout.fillWidth: true }
                                ShadcnButton {
                                    text: "查询"
                                    iconName: "search"
                                    onClicked: {
                                        if (!root.currentDevice) return;
                                        var now = Math.floor(Date.now() / 1000);
                                        var spans = [3600, 21600, 86400, 604800];
                                        var span = spans[historyRange.currentIndex] || 3600;
                                        dataManager.fetchDataPointHistory(
                                            root.currentDevice.id,
                                            historyMetric.model[historyMetric.currentIndex] || "temperature",
                                            now - span, now, 200);
                                    }
                                }
                            }

                            // 历史数据列表
                            ShadcnCard {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                size: ShadcnCard.Size.Small

                                ShadcnCardContent {
                                    width: parent.width
                                    height: parent.height
                                    implicitHeight: 0
                                    ListView {
                                        id: historyList
                                        width: parent.width
                                        height: parent.height
                                        clip: true
                                        model: historyDataModel
                                        spacing: 2

                                        delegate: RowLayout {
                                            width: historyList.width
                                            height: 32

                                            required property int index
                                            required property string time
                                            required property double value

                                            ShadcnLabel {
                                                Layout.preferredWidth: 36
                                                text: index + 1
                                                size: ShadcnLabel.Size.Small
                                                variant: ShadcnLabel.Variant.Muted
                                            }
                                            ShadcnLabel {
                                                Layout.fillWidth: true
                                                text: time
                                                size: ShadcnLabel.Size.Small
                                            }
                                            ShadcnLabel {
                                                Layout.preferredWidth: 100
                                                text: Number(value).toFixed(2)
                                                size: ShadcnLabel.Size.Small
                                                horizontalAlignment: Text.AlignRight
                                            }
                                        }
                                    }

                                    Column {
                                        anchors.centerIn: parent
                                        spacing: theme.spacingSm
                                        visible: historyDataModel.count === 0

                                        ShadcnIcon {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            name: "clock"
                                            size: 28
                                            color: theme.mutedForeground
                                        }
                                        ShadcnLabel {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            text: "点击查询获取历史数据"
                                            variant: ShadcnLabel.Variant.Muted
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

    // Toast
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

        Behavior on opacity { NumberAnimation { duration: 200 } }

        ShadcnLabel {
            id: toastLabel
            anchors.centerIn: parent
            text: root.toastText
            color: theme.background
        }
        Timer {
            id: toastTimer
            interval: 2500
            onTriggered: root.toastText = ""
        }
    }

    // Shadow 信号
    Connections {
        target: dataManager ? dataManager.httpClient : null
        function onShadowError(error) { root.showToast("保存失败: " + error, theme.destructive); }
        function onShadowUpdated(deviceId) {
            if (root.currentDevice && root.currentDevice.id === deviceId)
                root.showToast("保存成功", theme.success);
        }
        function onDataPointHistoryFetched(points) {
            // 填充历史列表 (Tab 3)
            if (root.currentDevice) {
                historyDataModel.clear();
                for (var i = 0; i < points.length; i++) {
                    var p = points[i];
                    var dt = new Date(p.ts * 1000);
                    historyDataModel.append({
                        "time": Qt.formatDateTime(dt, "yyyy-MM-dd HH:mm:ss"),
                        "value": p.value
                    });
                }
            }
            // 实时曲线 (Tab 0)
            detailChart.clearData();
            var temp = [], hum = [];
            for (var i = 0; i < points.length; i++) {
                var p = points[i];
                if (p.metric === "temperature") temp.push(p);
                else if (p.metric === "humidity") hum.push(p);
            }
            if (temp.length > 0) detailChart.setPoints("temperature", temp);
            if (hum.length > 0) detailChart.setPoints("humidity", hum);
        }
    }
}
