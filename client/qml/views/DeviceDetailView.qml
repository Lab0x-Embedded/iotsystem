import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtShadcn
import "../components"

// 设备详情：头部 + 动态指标卡 + Tabs(实时 / 影子 / 指令 / 历史)
Rectangle {
    id: root

    property var currentDevice: null
    property string shadowDesiredText: '{\n  "temperature": 25\n}'
    property string shadowReportedText: '{\n  "temperature": 23.5\n}'
    property string toastText: ""
    property color toastColor: theme.primary

    signal backRequested()

    QtShadcnTheme { id: theme }

    color: theme.background

    ListModel { id: historyDataModel }

    function showDevice(deviceId) {
        for (var i = 0; i < deviceModel.rowCount(); i++) {
            var device = deviceModel.deviceAt(i);
            if (device.id === deviceId) {
                currentDevice = device;
                detailChart.clearData();
                historyDataModel.clear();
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

    // 把当前设备的完整信息拼成文本复制到剪贴板
    function copyDeviceInfo() {
        if (!currentDevice) return;
        var g = Number(currentDevice.group);
        var groupText = !g ? "未分组" : (groupModel ? groupModel.groupName(g) : String(g));
        var statusText = currentDevice.status === 1 ? "在线"
                       : currentDevice.status === 2 ? "告警"
                       : currentDevice.status === 3 ? "维护" : "离线";

        infoClipboard.text =
              "设备 ID: " + currentDevice.id + "\n"
            + "名称: "    + (currentDevice.name || "-") + "\n"
            + "产品: "    + (dataManager ? dataManager.productNameOf(currentDevice.productKey)
                                          : currentDevice.productKey)
                          + " (" + currentDevice.productKey + ")\n"
            + "类型: "    + (currentDevice.deviceType || "-") + "\n"
            + "分组: "    + groupText + "\n"
            + "状态: "    + statusText + "\n"
            + "上报次数: " + currentDevice.reportCount + "\n"
            + "最后上报: "
            + (currentDevice.lastSeen ? Qt.formatDateTime(currentDevice.lastSeen, "yyyy-MM-dd hh:mm:ss") : "-");
        infoClipboard.selectAll();
        infoClipboard.copy();
        root.showToast("设备信息已复制");
    }

    function refreshHistory() {
        if (!currentDevice || !dataManager || !dataManager.online) return;
        var now = Math.floor(Date.now() / 1000);
        detailChart.clearData();
        var names = dataManager.metricNamesFor(currentDevice.id);
        if (names.length === 0) names = ["temperature", "humidity"];
        for (var i = 0; i < Math.min(names.length, 2); ++i)
            dataManager.fetchDataPointHistory(currentDevice.id, names[i], now - 3600, now, 120);
    }

    // 30s 轮询历史
    Timer {
        interval: 30000
        repeat: true
        running: root.currentDevice !== null && dataManager && dataManager.online
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
                    iconName: "arrow-left"
                    size: ShadcnButton.Size.Small
                    variant: ShadcnButton.Variant.Ghost
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
                              ? root.currentDevice.id + " · "
                                + (dataManager ? dataManager.productNameOf(root.currentDevice.productKey)
                                               : root.currentDevice.productKey)
                              : ""
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                    }
                }
                Item { Layout.fillWidth: true }

                ShadcnBadge {
                    visible: root.currentDevice !== null
                    text: !root.currentDevice ? ""
                          : root.currentDevice.status === 1 ? "在线"
                          : root.currentDevice.status === 2 ? "告警"
                          : root.currentDevice.status === 3 ? "维护" : "离线"
                    variant: root.currentDevice && root.currentDevice.status === 1
                             ? ShadcnBadge.Variant.Default
                             : root.currentDevice && root.currentDevice.status === 2
                               ? ShadcnBadge.Variant.Destructive
                               : ShadcnBadge.Variant.Secondary
                }
                ShadcnButton {
                    text: "复制信息"
                    iconName: "copy"
                    size: ShadcnButton.Size.Small
                    variant: ShadcnButton.Variant.Outline
                    visible: root.currentDevice !== null
                    onClicked: root.copyDeviceInfo()
                }
                ShadcnButton {
                    text: "重启设备"
                    iconName: "refresh-cw"
                    size: ShadcnButton.Size.Small
                    variant: ShadcnButton.Variant.Outline
                    visible: root.currentDevice && root.currentDevice.status === 1
                    onClicked: {
                        if (root.currentDevice && dataManager)
                            dataManager.httpClient.sendCommand(root.currentDevice.id, "reboot");
                    }
                }
            }

            // ===== 动态指标卡（按设备实际上报的指标生成）=====
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                spacing: 12
                visible: root.currentDevice !== null && metricRepeater.count > 0

                Repeater {
                    id: metricRepeater
                    model: root.currentDevice && dataManager
                           ? dataManager.metricNamesFor(root.currentDevice.id)
                           : []

                    StatCard {
                        required property string modelData

                        Layout.fillWidth: true
                        title: dataManager ? dataManager.metricLabel(modelData) : modelData
                        value: {
                            if (!dataManager || !root.currentDevice) return "—";
                            var v = dataManager.latestValue(root.currentDevice.id, modelData);
                            if (isNaN(v)) return "—";
                            return v.toFixed(1) + " " + dataManager.metricUnit(modelData);
                        }
                    }
                }
            }

            // 无指标数据提示
            ShadcnLabel {
                Layout.leftMargin: 20
                text: "该设备尚未上报数据"
                variant: ShadcnLabel.Variant.Muted
                visible: root.currentDevice !== null && metricRepeater.count === 0
            }


            // ===== 基本信息 =====
            // 服务端 query_all 早就返回了 device_type（之前客户端 DeviceInfo 没接，白丢了）
            Panel {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.preferredHeight: 92
                visible: root.currentDevice !== null

                RowLayout {
                    anchors.fill: parent
                    spacing: 24

                    ColumnLayout {
                        Layout.preferredWidth: 150
                        spacing: 4
                        ShadcnLabel { text: "设备 ID"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        CopyableText {
                            Layout.fillWidth: true
                            text: root.currentDevice ? root.currentDevice.id : "-"
                        }
                    }
                    ColumnLayout {
                        Layout.preferredWidth: 160
                        spacing: 4
                        ShadcnLabel { text: "产品"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        CopyableText {
                            Layout.fillWidth: true
                            text: root.currentDevice && dataManager
                                  ? dataManager.productNameOf(root.currentDevice.productKey) : "-"
                        }
                        CopyableText {
                            Layout.fillWidth: true
                            text: root.currentDevice ? root.currentDevice.productKey : ""
                            color: theme.mutedForeground
                        }
                    }
                    ColumnLayout {
                        Layout.preferredWidth: 110
                        spacing: 4
                        ShadcnLabel { text: "设备类型"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        CopyableText {
                            Layout.fillWidth: true
                            text: root.currentDevice && root.currentDevice.deviceType
                                  ? root.currentDevice.deviceType : "-"
                        }
                    }
                    ColumnLayout {
                        Layout.preferredWidth: 130
                        spacing: 4
                        ShadcnLabel { text: "分组"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        CopyableText {
                            Layout.fillWidth: true
                            text: {
                                if (!root.currentDevice) return "-";
                                var g = Number(root.currentDevice.group);
                                if (!g) return "未分组";
                                return groupModel ? groupModel.groupName(g) : String(g);
                            }
                        }
                    }
                    ColumnLayout {
                        Layout.preferredWidth: 80
                        spacing: 4
                        ShadcnLabel { text: "上报次数"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        ShadcnLabel {
                            text: root.currentDevice ? root.currentDevice.reportCount : "-"
                            size: ShadcnLabel.Size.Small
                        }
                    }
                    ColumnLayout {
                        Layout.preferredWidth: 170
                        spacing: 4
                        ShadcnLabel { text: "最后上报"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        CopyableText {
                            Layout.fillWidth: true
                            text: root.currentDevice && root.currentDevice.lastSeen
                                  ? Qt.formatDateTime(root.currentDevice.lastSeen, "yyyy-MM-dd hh:mm:ss") : "-"
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }

            // ===== Tabs =====
            Panel {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.bottomMargin: 20
                Layout.preferredHeight: 480

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    ShadcnTabsList {
                        id: detailTabs
                        variant: "line"
                        ShadcnTabsTrigger { text: "实时数据" }
                        ShadcnTabsTrigger { text: "设备影子" }
                        ShadcnTabsTrigger { text: "指令下发" }
                        ShadcnTabsTrigger { text: "历史查询" }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: detailTabs.currentIndex

                        // ---- Tab 0: 实时数据 ----
                        RealtimeChart {
                            id: detailChart
                        }

                        // ---- Tab 1: 设备影子 ----
                        RowLayout {
                            spacing: 12

                            Panel {
                                Layout.fillHeight: true
                                Layout.fillWidth: true

                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 8

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        ShadcnStatusDot { status: ShadcnStatusDot.Status.Success }
                                        ShadcnLabel { text: "Reported（上报状态）" }
                                        Item { Layout.fillWidth: true }
                                        ShadcnBadge {
                                            text: "只读"
                                            variant: ShadcnBadge.Variant.Secondary
                                        }
                                    }

                                    QQC.ScrollView {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        clip: true

                                        ShadcnLabel {
                                            width: parent.width
                                            text: root.shadowReportedText
                                            color: theme.success
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                }
                            }

                            Panel {
                                Layout.fillHeight: true
                                Layout.fillWidth: true

                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 8

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        ShadcnStatusDot { status: ShadcnStatusDot.Status.Online }
                                        ShadcnLabel { text: "Desired（期望状态）" }
                                        Item { Layout.fillWidth: true }
                                        ShadcnButton {
                                            text: "保存"
                                            iconName: "check"
                                            size: ShadcnButton.Size.ExtraSmall
                                            variant: ShadcnButton.Variant.Outline
                                            visible: root.currentDevice && root.currentDevice.status === 1
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

                                    QQC.TextArea {
                                        id: desiredEditor
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        text: root.shadowDesiredText
                                        color: theme.foreground
                                        selectByMouse: true
                                        wrapMode: QQC.TextArea.Wrap
                                        font.family: "Monaco"
                                        font.pixelSize: 12

                                        background: Rectangle {
                                            color: theme.input
                                            radius: theme.radius
                                            border.width: 1
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
                                    Layout.preferredWidth: 140
                                    placeholderText: "指令名"
                                }
                                ShadcnInput {
                                    id: cmdPayloadField
                                    Layout.fillWidth: true
                                    placeholderText: 'Payload JSON，如 {"relay":"on"}'
                                }
                                ShadcnButton {
                                    text: "发送"
                                    iconName: "send"
                                    onClicked: {
                                        if (cmdNameField.text && root.currentDevice) {
                                            var payload = {};
                                            try {
                                                payload = JSON.parse(cmdPayloadField.text || "{}");
                                            } catch (e) {
                                                root.showToast("Payload 不是合法 JSON", theme.destructive);
                                                return;
                                            }
                                            dataManager.httpClient.sendCommand(root.currentDevice.id,
                                                                               cmdNameField.text, payload);
                                            root.showToast("指令已发送");
                                        }
                                    }
                                }
                            }

                            ShadcnAlert {
                                Layout.fillWidth: true
                                title: "指令通过 MQTT QoS 1 下发"
                                description: "设备离线时指令进入队列，重连后自动重放；在线时等待 PUBACK 确认。"
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
                                    width: 150
                                    model: root.currentDevice && dataManager
                                           ? dataManager.metricNamesFor(root.currentDevice.id)
                                           : []
                                }
                                ShadcnLabel { text: "时间:"; variant: ShadcnLabel.Variant.Muted }
                                ShadcnSelect {
                                    id: historyRange
                                    width: 130
                                    model: ["最近1小时", "最近6小时", "最近24小时", "最近7天"]
                                }
                                Item { Layout.fillWidth: true }
                                ShadcnButton {
                                    text: "查询"
                                    iconName: "search"
                                    onClicked: {
                                        if (!root.currentDevice || !dataManager) return;
                                        var metric = historyMetric.model[historyMetric.currentIndex];
                                        if (!metric) metric = "temperature";
                                        var spans = [3600, 21600, 86400, 604800];
                                        var span = spans[historyRange.currentIndex] || 3600;
                                        var now = Math.floor(Date.now() / 1000);
                                        dataManager.fetchDataPointHistory(root.currentDevice.id, metric,
                                                                          now - span, now, 200);
                                    }
                                }
                            }

                            Panel {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                ListView {
                                    id: historyList
                                    anchors.fill: parent
                                    clip: true
                                    model: historyDataModel
                                    spacing: 2

                                    delegate: RowLayout {
                                        required property int index
                                        required property string time
                                        required property double value

                                        width: historyList.width
                                        height: 32

                                        ShadcnLabel {
                                            Layout.preferredWidth: 40
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

    // 复制“设备信息”用的隐藏编辑器
    TextEdit { id: infoClipboard; visible: false }

    // Toast
    Rectangle {
        id: toastBar
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        color: root.toastColor
        height: 36
        width: toastLabel.implicitWidth + 32
        radius: 8
        opacity: root.toastText ? 1.0 : 0.0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: 200 } }

        ShadcnLabel {
            id: toastLabel
            anchors.centerIn: parent
            text: root.toastText
            color: theme.primaryForeground
        }
        Timer {
            id: toastTimer
            interval: 2500
            onTriggered: root.toastText = ""
        }
    }

    Connections {
        target: dataManager ? dataManager.httpClient : null

        function onShadowError(error) {
            root.showToast("保存失败: " + error, theme.destructive);
        }
        function onShadowUpdated(deviceId) {
            if (root.currentDevice && root.currentDevice.id === deviceId)
                root.showToast("保存成功", theme.success);
        }
        function onDataPointHistoryFetched(points) {
            if (points.length === 0) return;
            var m = points[0].metric || "value";

            // 实时曲线：按指标分别灌入
            detailChart.setPoints(m, points);

            // 历史列表：只显示当前选中的指标，避免多指标混在一起
            var selected = historyMetric.model[historyMetric.currentIndex];
            if (selected && selected === m) {
                historyDataModel.clear();
                for (var i = 0; i < points.length; i++) {
                    var dt = new Date(points[i].ts * 1000);
                    historyDataModel.append({
                        "time": Qt.formatDateTime(dt, "yyyy-MM-dd HH:mm:ss"),
                        "value": points[i].value
                    });
                }
            }
        }
    }
}
