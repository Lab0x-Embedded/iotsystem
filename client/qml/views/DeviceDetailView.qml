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

    // 密钥默认打码
    property bool secretVisible: false

    signal backRequested()

    QtShadcnTheme { id: theme }
    color: theme.background

    ListModel { id: historyDataModel }

    // 相对时间
    function relativeTime(dt) {
        if (!dt) return "—";
        var nowMs = new Date().getTime();
        var thenMs = dt.getTime ? dt.getTime() : (new Date(dt)).getTime();
        if (!thenMs) return "—";
        var diff = (nowMs - thenMs) / 1000;
        if (diff < 60)      return "刚刚";
        if (diff < 3600)    return Math.floor(diff / 60) + " 分钟前";
        if (diff < 86400)   return Math.floor(diff / 3600) + " 小时前";
        if (diff < 86400*7) return Math.floor(diff / 86400) + " 天前";
        return Qt.formatDateTime(dt, "yyyy-MM-dd");
    }

    function statusText() {
        if (!currentDevice) return "—";
        return currentDevice.status === 1 ? "在线"
             : currentDevice.status === 2 ? "告警"
             : currentDevice.status === 3 ? "维护" : "离线";
    }

    function statusColorOf() {
        if (!currentDevice) return theme.mutedForeground;
        return currentDevice.status === 1 ? theme.success
             : currentDevice.status === 2 ? theme.destructive
             : theme.mutedForeground;
    }

    function groupNameOf() {
        if (!currentDevice) return "—";
        var g = Number(currentDevice.group);
        if (!g) return "未分组";
        return groupModel ? groupModel.groupName(g) : String(g);
    }

    function showDevice(deviceId) {
        for (var i = 0; i < deviceModel.rowCount(); i++) {
            var device = deviceModel.deviceAt(i);
            if (device.id === deviceId) {
                currentDevice = device;
                secretVisible = false;
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

    // 复制单个字段
    function copyField(text, label) {
        infoClipboard.text = text;
        infoClipboard.selectAll();
        infoClipboard.copy();
        root.showToast((label || "内容") + "已复制");
    }

    function copyDeviceInfo() {
        if (!currentDevice) return;
        infoClipboard.text =
              "设备 ID: " + currentDevice.id + "\n"
            + "名称: "    + (currentDevice.name || "-") + "\n"
            + "产品: "    + (dataManager ? dataManager.productNameOf(currentDevice.productKey)
                                          : currentDevice.productKey)
                          + " (" + currentDevice.productKey + ")\n"
            + "类型: "    + (currentDevice.deviceType || "-") + "\n"
            + "分组: "    + groupNameOf() + "\n"
            + "密钥: "    + (currentDevice.deviceSecret || "-") + "\n"
            + "状态: "    + statusText() + "\n"
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

    Timer {
        interval: 30000
        repeat: true
        running: root.currentDevice !== null && dataManager && dataManager.online
        onTriggered: root.refreshHistory()
    }

    // ── 复制小按钮（内联组件） ─────────────────
    component CopyIcon: Rectangle {
        id: copyIcon

        property string textToCopy: ""
        property string label: "内容"

        Layout.preferredWidth: 22
        Layout.preferredHeight: 22
        radius: 5
        color: copyHover.containsMouse ? Qt.alpha(theme.foreground, 0.08)
                                       : "transparent"

        ShadcnIcon {
            anchors.centerIn: parent
            name: "copy"
            size: 11
            color: theme.mutedForeground
        }
        MouseArea {
            id: copyHover
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.copyField(copyIcon.textToCopy, copyIcon.label)
        }
    }

    // ── 信息字段（内联组件） ────────────────
    component InfoField: ColumnLayout {
        id: infoField

        property string label: ""
        property string value: ""
        property bool copyable: false
        property bool mono: false
        property color valueColor: theme.foreground

        Layout.fillWidth: true
        spacing: 4

        ShadcnLabel {
            text: infoField.label
            size: ShadcnLabel.Size.Small
            variant: ShadcnLabel.Variant.Muted
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            ShadcnLabel {
                Layout.fillWidth: true
                text: infoField.value
                size: ShadcnLabel.Size.Small
                font.bold: true
                font.family: infoField.mono ? "Monaco" : ""
                color: infoField.valueColor
                elide: Text.ElideRight
            }
            CopyIcon {
                visible: infoField.copyable
                textToCopy: infoField.value
                label: infoField.label
            }
        }
    }

    // ── 页面主体 ─────────────────────────────
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
                Layout.bottomMargin: 0
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
                    spacing: 3

                    RowLayout {
                        spacing: 10
                        ShadcnLabel {
                            text: root.currentDevice ? root.currentDevice.name : ""
                            size: ShadcnLabel.Size.Large
                            font.bold: true
                        }
                        // 状态 pill
                        Rectangle {
                            visible: root.currentDevice !== null
                            implicitWidth: statusRow.implicitWidth + 18
                            implicitHeight: 24
                            radius: 12
                            color: Qt.alpha(root.statusColorOf(), 0.12)
                            RowLayout {
                                id: statusRow
                                anchors.centerIn: parent
                                spacing: 6
                                Rectangle {
                                    width: 7; height: 7; radius: 3.5
                                    color: root.statusColorOf()
                                }
                                ShadcnLabel {
                                    text: root.statusText()
                                    size: ShadcnLabel.Size.Small
                                    color: root.statusColorOf()
                                }
                            }
                        }
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

            // ===== 动态指标卡（网格排布，不再每个占整行）=====
            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                columnSpacing: 12
                rowSpacing: 12
                columns: 4
                visible: root.currentDevice !== null && metricRepeater.count > 0

                Repeater {
                    id: metricRepeater
                    model: root.currentDevice && dataManager
                           ? dataManager.metricNamesFor(root.currentDevice.id)
                           : []

                    delegate: Rectangle {
                        required property string modelData

                        Layout.fillWidth: true
                        Layout.preferredHeight: 82
                        radius: 10
                        color: theme.muted
                        border.width: 1
                        border.color: Qt.alpha(theme.foreground, 0.06)

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 4

                            ShadcnLabel {
                                text: dataManager ? dataManager.metricLabel(modelData) : modelData
                                size: ShadcnLabel.Size.Small
                                variant: ShadcnLabel.Variant.Muted
                            }
                            RowLayout {
                                spacing: 4
                                ShadcnLabel {
                                    text: {
                                        if (!dataManager || !root.currentDevice) return "—";
                                        var v = dataManager.latestValue(root.currentDevice.id, modelData);
                                        if (isNaN(v)) return "—";
                                        return v.toFixed(1);
                                    }
                                    size: ShadcnLabel.Size.Large
                                    font.bold: true
                                }
                                ShadcnLabel {
                                    text: dataManager ? dataManager.metricUnit(modelData) : ""
                                    size: ShadcnLabel.Size.Small
                                    variant: ShadcnLabel.Variant.Muted
                                    Layout.alignment: Qt.AlignBottom
                                }
                            }
                        }
                    }
                }
            }

            ShadcnLabel {
                Layout.leftMargin: 20
                text: "该设备尚未上报数据"
                variant: ShadcnLabel.Variant.Muted
                visible: root.currentDevice !== null && metricRepeater.count === 0
            }

            // ===== 基本信息（网格对齐）=====
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                implicitHeight: infoGrid.implicitHeight + 40
                radius: 12
                color: theme.background
                border.width: 1
                border.color: Qt.alpha(theme.foreground, 0.08)
                visible: root.currentDevice !== null

                GridLayout {
                    id: infoGrid
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 20
                    columns: 4
                    columnSpacing: 32
                    rowSpacing: 18

                    InfoField {
                        label: "设备 ID"
                        value: root.currentDevice ? root.currentDevice.id : "—"
                        mono: true
                        copyable: true
                    }
                    InfoField {
                        label: "设备类型"
                        value: root.currentDevice && root.currentDevice.deviceType
                               ? root.currentDevice.deviceType : "—"
                    }
                    InfoField {
                        label: "分组"
                        value: root.groupNameOf()
                    }
                    InfoField {
                        label: "产品"
                        value: root.currentDevice && dataManager
                               ? dataManager.productNameOf(root.currentDevice.productKey) : "—"
                    }

                    InfoField {
                        label: "状态"
                        value: root.statusText()
                        valueColor: root.statusColorOf()
                    }
                    InfoField {
                        label: "上报次数"
                        value: root.currentDevice ? String(root.currentDevice.reportCount) : "—"
                    }
                    InfoField {
                        label: "最后上报"
                        value: root.currentDevice && root.currentDevice.lastSeen
                               ? root.relativeTime(root.currentDevice.lastSeen) : "—"
                    }
                    InfoField {
                        label: "产品 Key"
                        value: root.currentDevice ? root.currentDevice.productKey : "—"
                        mono: true
                        copyable: true
                    }

                    // 密钥（占 3 列，因为旁边有个显示/隐藏按钮）
                    ColumnLayout {
                        Layout.columnSpan: 3
                        Layout.fillWidth: true
                        spacing: 4

                        ShadcnLabel {
                            text: "设备密钥（接入固件用，请妥善保管）"
                            size: ShadcnLabel.Size.Small
                            variant: ShadcnLabel.Variant.Muted
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            ShadcnLabel {
                                Layout.fillWidth: true
                                text: {
                                    if (!root.currentDevice) return "—";
                                    var s = root.currentDevice.deviceSecret;
                                    if (!s || s.length === 0) return "（未设置）";
                                    return root.secretVisible ? s : "••••••••••••••••";
                                }
                                size: ShadcnLabel.Size.Small
                                font.bold: true
                                font.family: "Monaco"
                                elide: Text.ElideRight
                            }
                            CopyIcon {
                                visible: root.currentDevice && root.currentDevice.deviceSecret
                                textToCopy: root.currentDevice ? (root.currentDevice.deviceSecret || "") : ""
                                label: "设备密钥"
                            }
                        }
                    }

                    // 显示/隐藏按钮
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        ShadcnLabel { text: " "; size: ShadcnLabel.Size.Small }
                        ShadcnButton {
                            text: root.secretVisible ? "隐藏" : "显示"
                            size: ShadcnButton.Size.ExtraSmall
                            variant: ShadcnButton.Variant.Ghost
                            onClicked: root.secretVisible = !root.secretVisible
                        }
                    }
                }
            }

            // ===== Tabs =====
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.bottomMargin: 20
                Layout.preferredHeight: 500
                radius: 12
                color: theme.background
                border.width: 1
                border.color: Qt.alpha(theme.foreground, 0.08)

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
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

    // 复制用的隐藏编辑器
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

            detailChart.setPoints(m, points);

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
