import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtShadcn
import "../components"

// 设备总览：统计卡 + 设备列表
Rectangle {
    id: root

    property var deviceData: null
    property var alarmData: null

    property string searchText: ""
    // 状态筛选："all" / "online" / "offline" / "alarm"
    property string statusFilter: "all"

    // 筛选 chip 计数：refreshRows 同趟统计（状态含告警覆盖），保证数字与点进去的行一致
    property int countAll: 0
    property int countOnline: 0
    property int countOffline: 0
    property int countAlarm: 0

    property var groupNames: []
    property var groupIds: []

    property var productNames: []
    property var productKeys: []
    property string pendingProductKey: ""

    signal navigateToProducts()
    signal navigateToGuide(string deviceId)
    signal deviceSelected(string deviceId)

    QtShadcnTheme { id: theme }
    color: theme.background

    property bool refreshPending: false
    property string toastText: ""

    function showToast(msg) {
        toastText = msg;
        toastTimer.restart();
    }

    // 相对时间
    function relativeTime(dt) {
        if (!dt) return "-";
        var nowMs = new Date().getTime();
        var thenMs = dt.getTime ? dt.getTime() : (new Date(dt)).getTime();
        if (!thenMs) return "-";
        var diff = (nowMs - thenMs) / 1000;
        if (diff < 60)       return "刚刚";
        if (diff < 3600)     return Math.floor(diff / 60) + " 分钟前";
        if (diff < 86400)    return Math.floor(diff / 3600) + " 小时前";
        if (diff < 86400*7)  return Math.floor(diff / 86400) + " 天前";
        return Qt.formatDateTime(dt, "yyyy-MM-dd");
    }

    ListModel { id: deviceRows }

    function refreshRows() {
        deviceRows.clear();
        countAll = 0; countOnline = 0; countOffline = 0; countAlarm = 0;
        if (!deviceData) return;

        // 活跃告警设备集合：服务端设备状态没有"告警"态，由告警接口交叉覆盖
        var alarmDevs = {};
        if (alarmData) {
            var aids = alarmData.activeAlarmDeviceIds();
            for (var k = 0; k < aids.length; ++k) alarmDevs[aids[k]] = true;
        }

        var q  = searchText.toLowerCase();
        var sf = statusFilter;
        for (var i = 0; i < deviceData.rowCount(); i++) {
            var d = deviceData.deviceAt(i);
            if (!d || !d.id) continue;

            if (q !== ""
                && (d.id   || "").toLowerCase().indexOf(q) < 0
                && (d.name || "").toLowerCase().indexOf(q) < 0)
                continue;

            countAll++;

            var st = d.statusText();
            var sc = d.statusColor().toString();
            // 真实离线保留给"最新数据"灰化；被告警覆盖后筛选按覆盖态走
            var realOffline = (st === "离线");
            if (alarmDevs[d.id]) {
                st = "告警";
                sc = theme.destructive.toString();
            }

            if (st === "在线")      countOnline++;
            else if (st === "告警") countAlarm++;
            else if (st === "离线") countOffline++;

            if (sf === "online"  && st !== "在线") continue;
            if (sf === "offline" && st !== "离线") continue;
            if (sf === "alarm"   && st !== "告警") continue;

            deviceRows.append({
                "deviceId":       d.id,
                "deviceName":     d.name || "",
                "group":          d.group || "",
                "lastSeenText":   d.lastSeen ? root.relativeTime(d.lastSeen) : "-",
                "lastSeenAbs":    d.lastSeen ? Qt.formatDateTime(d.lastSeen, "yyyy-MM-dd hh:mm:ss") : "-",
                "statusText":     st,
                "statusColor":    sc,
                "isOffline":      realOffline,
                "metricSummary":  dataManager ? dataManager.metricSummary(d.id, 2) : "—"
            });
        }
    }

    function refreshProductChoices() {
        var names = [];
        var keys  = [];
        if (dataManager) {
            var list = dataManager.products;
            for (var i = 0; i < list.length; ++i) {
                names.push(list[i].product_name || list[i].product_key);
                keys.push(list[i].product_key);
            }
        }
        productNames = names;
        productKeys = keys;
    }

    function refreshGroupChoices() {
        var names = [];
        var ids   = [];
        if (groupModel) {
            var opts = groupModel.options();
            for (var i = 0; i < opts.length; ++i) {
                names.push(opts[i].name);
                ids.push(opts[i].id);
            }
        }
        groupNames = names;
        groupIds = ids;
    }

    function submitRegister() {
        regError.text = "";
        var id = regDeviceId.text.trim();
        var pk = "";
        if (regProduct.currentIndex >= 0 && regProduct.currentIndex < productKeys.length)
            pk = productKeys[regProduct.currentIndex];
        if (id === "") {
            regError.text = "设备 ID 为必填";
            return;
        }
        if (pk === "") {
            regError.text = "请先选择产品（还没有产品就点「新建」）";
            return;
        }
        root.pendingProductKey = pk;
        var gid = 0;
        if (regGroup.currentIndex >= 0 && regGroup.currentIndex < groupIds.length)
            gid = groupIds[regGroup.currentIndex];
        if (!dataManager) return;
        dataManager.registerDevice(id, regName.text.trim(), pk,
                                   regType.text.trim(), regSecret.text.trim(), gid);
    }

    Component.onCompleted: {
        refreshRows();
        refreshGroupChoices();
        refreshProductChoices();
        if (dataManager) dataManager.refreshProducts();
    }

    Connections {
        target: root.deviceData
        function onCountsChanged() { root.refreshRows(); }
    }

    // 告警确认/解决后：覆盖态与 chip 计数即时刷新
    Connections {
        target: root.alarmData
        function onCountsChanged() { root.refreshRows(); }
    }

    Connections {
        target: dataManager
        function onProductsChanged() { root.refreshProductChoices(); }
        function onDevicesRefreshed(count) {
            root.refreshGroupChoices();
            if (root.refreshPending) {
                root.refreshPending = false;
                root.showToast("设备列表已刷新（" + count + " 台）");
            }
        }
    }

    // 每分钟刷新一次相对时间
    Timer {
        interval: 60000
        running: true
        repeat: true
        onTriggered: root.refreshRows()
    }

    // Toast
    Rectangle {
        id: toastBar
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        color: theme.primary
        height: 36
        width: toastLabel.implicitWidth + 32
        radius: 8
        opacity: root.toastText ? 1.0 : 0.0
        visible: opacity > 0
        z: 100

        Behavior on opacity { NumberAnimation { duration: 200 } }

        ShadcnLabel {
            id: toastLabel
            anchors.centerIn: parent
            text: root.toastText
            color: theme.primaryForeground
        }
        Timer {
            id: toastTimer
            interval: 1800
            onTriggered: root.toastText = ""
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        // ===== 统计卡（纯展示，点击筛选由下方 chip 负责） =====
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            StatCard {
                Layout.fillWidth: true
                title: "设备总数"
                value: root.deviceData ? root.deviceData.totalCount : "0"
            }
            StatCard {
                Layout.fillWidth: true
                title: "在线"
                value: root.deviceData ? root.deviceData.onlineCount : "0"
                valueColor: theme.success
                dotStatus: ShadcnStatusDot.Status.Online
            }
            StatCard {
                Layout.fillWidth: true
                title: "离线"
                value: root.countOffline
                valueColor: theme.mutedForeground
                dotStatus: ShadcnStatusDot.Status.Offline
            }
            StatCard {
                Layout.fillWidth: true
                title: "告警"
                value: root.countAlarm
                valueColor: root.countAlarm > 0 ? theme.destructive : theme.mutedForeground
                dotStatus: root.countAlarm > 0
                           ? ShadcnStatusDot.Status.Danger : ShadcnStatusDot.Status.Offline
            }
        }

        // ===== 主体 =====
        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 16

            Panel {
                Layout.fillHeight: true
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    // 头部：标题 + 搜索 + 操作
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        ShadcnLabel { text: "设备列表" }
                        Item { Layout.fillWidth: true }
                        ShadcnInputGroup {
                            id: searchInput
                            Layout.preferredWidth: 220
                            prefixIcon: "search"
                            placeholderText: "搜索设备 ID / 名称..."
                            onTextChanged: {
                                root.searchText = text;
                                root.refreshRows();
                            }
                        }
                        ShadcnButton {
                            text: "刷新"
                            iconName: "refresh-cw"
                            size: ShadcnButton.Size.Small
                            variant: ShadcnButton.Variant.Outline
                            onClicked: {
                                if (!dataManager) return;
                                root.refreshPending = true;
                                dataManager.refreshDevices();
                                dataManager.refreshGroups();
                            }
                        }
                        ShadcnButton {
                            text: "注册设备"
                            iconName: "plus"
                            size: ShadcnButton.Size.Small
                            onClicked: {
                                regDeviceId.text = "";
                                regName.text = "";
                                regType.text = "sensor";
                                regSecret.text = "";
                                regError.text = "";
                                root.refreshGroupChoices();
                                registerDialog.open();
                            }
                        }
                    }

                    // ===== 筛选 chip 行 =====
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        ShadcnButton {
                            text: "全部 (" + root.countAll + ")"
                            size: ShadcnButton.Size.ExtraSmall
                            variant: root.statusFilter === "all"
                                     ? ShadcnButton.Variant.Primary
                                     : ShadcnButton.Variant.Outline
                            onClicked: { root.statusFilter = "all"; root.refreshRows(); }
                        }
                        ShadcnButton {
                            text: "在线 (" + root.countOnline + ")"
                            size: ShadcnButton.Size.ExtraSmall
                            variant: root.statusFilter === "online"
                                     ? ShadcnButton.Variant.Primary
                                     : ShadcnButton.Variant.Outline
                            onClicked: { root.statusFilter = "online"; root.refreshRows(); }
                        }
                        ShadcnButton {
                            text: "离线 (" + root.countOffline + ")"
                            size: ShadcnButton.Size.ExtraSmall
                            variant: root.statusFilter === "offline"
                                     ? ShadcnButton.Variant.Primary
                                     : ShadcnButton.Variant.Outline
                            onClicked: { root.statusFilter = "offline"; root.refreshRows(); }
                        }
                        ShadcnButton {
                            text: "告警 (" + root.countAlarm + ")"
                            size: ShadcnButton.Size.ExtraSmall
                            variant: root.statusFilter === "alarm"
                                     ? ShadcnButton.Variant.Primary
                                     : ShadcnButton.Variant.Outline
                            onClicked: { root.statusFilter = "alarm"; root.refreshRows(); }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // 表头（浅色底）
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        radius: theme.radius
                        color: Qt.alpha(theme.muted, 0.5)

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10

                            ShadcnLabel { Layout.preferredWidth: 72;  text: "状态";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            ShadcnLabel { Layout.preferredWidth: 110; text: "设备 ID";  size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            ShadcnLabel { Layout.fillWidth: true;    text: "名称";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            ShadcnLabel { Layout.preferredWidth: 200; text: "最新数据"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            ShadcnLabel { Layout.preferredWidth: 110; text: "最后在线"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            ShadcnLabel { Layout.preferredWidth: 72;  text: "操作";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        }
                    }

                    // 列表
                    ListView {
                        id: deviceList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: deviceRows
                        spacing: 0

                        QQC.ScrollBar.vertical: QQC.ScrollBar {
                            active: true
                            policy: QQC.ScrollBar.AsNeeded
                        }

                        delegate: Rectangle {
                            id: delegate

                            required property int index
                            required property string deviceId
                            required property string deviceName
                            required property string group
                            required property string lastSeenText
                            required property string lastSeenAbs
                            required property string statusText
                            required property string statusColor
                            required property bool isOffline
                            required property string metricSummary

                            width: deviceList.width
                            height: 44
                            radius: 6
                            color: deviceHover.containsMouse ? theme.muted : "transparent"

                            MouseArea {
                                id: deviceHover
                                anchors.fill: parent
                                hoverEnabled: true
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 10

                                // 状态：小圆点 + 文字
                                RowLayout {
                                    Layout.preferredWidth: 72
                                    spacing: 6

                                    Rectangle {
                                        Layout.alignment: Qt.AlignVCenter
                                        width: 8; height: 8; radius: 4
                                        color: delegate.statusColor
                                    }
                                    ShadcnLabel {
                                        text: delegate.statusText
                                        size: ShadcnLabel.Size.Small
                                        color: delegate.statusColor
                                    }
                                }

                                // 设备 ID：等宽、弱化
                                ShadcnLabel {
                                    Layout.preferredWidth: 110
                                    text: deviceId
                                    size: ShadcnLabel.Size.Small
                                    variant: ShadcnLabel.Variant.Muted
                                    font.family: "Menlo"
                                    elide: Text.ElideRight
                                }

                                // 名称：主标题
                                ShadcnLabel {
                                    Layout.fillWidth: true
                                    text: deviceName || "—"
                                    size: ShadcnLabel.Size.Small
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                // 最新数据：离线时灰化
                                ShadcnLabel {
                                    Layout.preferredWidth: 200
                                    text: delegate.metricSummary
                                    size: ShadcnLabel.Size.Small
                                    opacity: delegate.isOffline ? 0.45 : 1.0
                                    color: delegate.isOffline ? theme.mutedForeground
                                                              : theme.foreground
                                    elide: Text.ElideRight
                                }

                                // 最后上报：相对时间
                                ShadcnLabel {
                                    Layout.preferredWidth: 110
                                    text: delegate.lastSeenText
                                    size: ShadcnLabel.Size.Small
                                    variant: ShadcnLabel.Variant.Muted
                                }

                                // 操作：图标按钮
                                RowLayout {
                                    Layout.preferredWidth: 72
                                    spacing: 2

                                    ShadcnButton {
                                        iconName: "link"
                                        size: ShadcnButton.Size.ExtraSmall
                                        variant: ShadcnButton.Variant.Ghost
                                        onClicked: root.navigateToGuide(deviceId)
                                    }
                                    ShadcnButton {
                                        iconName: "chevron-right"
                                        size: ShadcnButton.Size.ExtraSmall
                                        variant: ShadcnButton.Variant.Ghost
                                        onClicked: root.deviceSelected(deviceId)
                                    }
                                }
                            }
                        }

                        // 空状态
                        Column {
                            anchors.centerIn: parent
                            spacing: theme.spacingSm
                            visible: deviceList.count === 0

                            ShadcnIcon {
                                anchors.horizontalCenter: parent.horizontalCenter
                                name: "monitor"
                                size: 32
                                color: theme.mutedForeground
                            }
                            ShadcnLabel {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: root.statusFilter === "all"
                                      ? (root.searchText ? "无匹配设备" : "暂无设备")
                                      : "该状态下暂无设备"
                                variant: ShadcnLabel.Variant.Muted
                            }
                        }
                    }
                }
            }
        }
    }

    // ===== 注册设备 =====
    ShadcnDialog {
        id: registerDialog
        modal: true

        ShadcnDialogContent {
            ColumnLayout {
                width: parent.width
                spacing: 10

                ShadcnDialogHeader {
                    Layout.fillWidth: true
                    ShadcnDialogTitle { text: "注册设备" }
                    ShadcnDialogDescription {
                        text: "注册后把密钥填入设备固件即可接入；设备密钥留空由服务端生成"
                    }
                }

                ShadcnLabel { text: "设备 ID *"; size: ShadcnLabel.Size.Small }
                ShadcnInput {
                    id: regDeviceId
                    Layout.fillWidth: true
                    placeholderText: "如 dev_011（建议 dev_ 开头）"
                }

                ShadcnLabel { text: "名称"; size: ShadcnLabel.Size.Small }
                ShadcnInput {
                    id: regName
                    Layout.fillWidth: true
                    placeholderText: "如 工厂A-温湿度-01"
                }

                ShadcnLabel { text: "产品 *"; size: ShadcnLabel.Size.Small }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ShadcnSelect {
                        id: regProduct
                        Layout.fillWidth: true
                        model: root.productNames
                    }
                    ShadcnButton {
                        text: "新建"
                        iconName: "plus"
                        size: ShadcnButton.Size.Small
                        variant: ShadcnButton.Variant.Outline
                        onClicked: {
                            registerDialog.close();
                            root.navigateToProducts();
                        }
                    }
                }
                ShadcnLabel {
                    Layout.fillWidth: true
                    text: "还没有产品，请先点「新建」创建"
                    size: ShadcnLabel.Size.Small
                    variant: ShadcnLabel.Variant.Destructive
                    visible: root.productNames.length === 0
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        ShadcnLabel { text: "设备类型"; size: ShadcnLabel.Size.Small }
                        ShadcnInput {
                            id: regType
                            Layout.fillWidth: true
                            placeholderText: "sensor / meter"
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        ShadcnLabel { text: "分组"; size: ShadcnLabel.Size.Small }
                        ShadcnSelect {
                            id: regGroup
                            Layout.fillWidth: true
                            model: root.groupNames
                        }
                    }
                }

                ShadcnLabel { text: "设备密钥（留空自动生成）"; size: ShadcnLabel.Size.Small }
                ShadcnInput {
                    id: regSecret
                    Layout.fillWidth: true
                    placeholderText: "留空 → 服务端随机生成 32 位密钥"
                }

                ShadcnLabel {
                    id: regError
                    Layout.fillWidth: true
                    size: ShadcnLabel.Size.Small
                    variant: ShadcnLabel.Variant.Destructive
                    wrapMode: Text.Wrap
                }
            }

            footer: ShadcnDialogFooter {
                ShadcnButton {
                    text: "取消"
                    variant: ShadcnButton.Variant.Outline
                    size: ShadcnButton.Size.Small
                    onClicked: registerDialog.close()
                }
                ShadcnButton {
                    text: "注册"
                    size: ShadcnButton.Size.Small
                    onClicked: root.submitRegister()
                }
            }
        }
    }

    // ===== 注册成功：展示密钥 + 固件配置 =====
    ShadcnDialog {
        id: registeredDialog

        property string deviceId: ""
        property string productKey: ""
        property string secret: ""

        modal: true

        ShadcnDialogContent {
            ColumnLayout {
                width: parent.width
                spacing: 10

                ShadcnDialogHeader {
                    Layout.fillWidth: true
                    ShadcnDialogTitle { text: "注册成功" }
                    ShadcnDialogDescription {
                        text: "把下面这段贴进设备固件（esp8266_sensor_relay.ino 顶部）即可接入"
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    ShadcnLabel { text: "设备 ID"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                    Item { Layout.fillWidth: true }
                    ShadcnLabel { text: registeredDialog.deviceId; size: ShadcnLabel.Size.Small }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    ShadcnLabel { text: "密钥"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                    Item { Layout.fillWidth: true }
                    ShadcnLabel {
                        text: registeredDialog.secret
                        size: ShadcnLabel.Size.Small
                        color: theme.primary
                    }
                }

                QQC.TextArea {
                    id: snippetArea
                    Layout.fillWidth: true
                    Layout.preferredHeight: 132
                    readOnly: true
                    selectByMouse: true
                    wrapMode: QQC.TextArea.NoWrap
                    font.family: "Monaco"
                    font.pixelSize: 11
                    text: "const char* PRODUCT_KEY   = \"" + registeredDialog.productKey + "\";\n"
                        + "const char* DEVICE_SECRET = \"" + registeredDialog.secret + "\";\n"
                        + "const char* DEVICE_ID     = \"" + registeredDialog.deviceId + "\";"
                    background: Rectangle {
                        color: theme.input
                        radius: theme.radius
                        border.width: 1
                        border.color: theme.border
                    }
                }

                TextEdit { id: clipboardHelper; visible: false }
            }

            footer: ShadcnDialogFooter {
                ShadcnButton {
                    text: "复制配置"
                    iconName: "copy"
                    variant: ShadcnButton.Variant.Outline
                    size: ShadcnButton.Size.Small
                    onClicked: {
                        clipboardHelper.text = snippetArea.text;
                        clipboardHelper.selectAll();
                        clipboardHelper.copy();
                    }
                }
                ShadcnButton {
                    text: "完成"
                    size: ShadcnButton.Size.Small
                    onClicked: registeredDialog.close()
                }
            }
        }
    }

    // 注册结果接线
    Connections {
        target: dataManager
        function onDeviceRegistered(deviceId, secret) {
            registerDialog.close();
            registeredDialog.deviceId = deviceId;
            registeredDialog.productKey = root.pendingProductKey;
            registeredDialog.secret = secret;
            registeredDialog.open();
            root.refreshRows();
            root.refreshGroupChoices();
        }
        function onErrorOccurred(error) {
            if (registerDialog.opened) regError.text = error;
        }
    }
}