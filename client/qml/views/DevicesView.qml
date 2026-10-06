import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtShadcn
import "../components"

// 设备总览：统计卡 + 设备列表
Rectangle {
    id: root

    property var deviceData: null
    // 活跃告警记录模型（与底部状态栏同源），用于顶部"告警"卡片
    property var alarmData: null

    // 搜索关键字（真正用于过滤列表，之前这个搜索框是摆设）
    property string searchText: ""
    // 分组下拉：ShadcnSelect 的 delegate 只可靠支持「字符串数组」，
    // 用对象数组会渲染成 [object Object]，所以拆成「名称数组 + 平行 id 数组」
    property var groupNames: []
    property var groupIds: []

    // 产品下拉(同样用平行数组)
    property var productNames: []
    property var productKeys: []
    property string pendingProductKey: ""

    // 去产品管理页新建产品
    signal navigateToProducts()

    // 请求打开该设备的 MQTT 接入指南（参数自动填入）
    signal navigateToGuide(string deviceId)

    signal deviceSelected(string deviceId)

    QtShadcnTheme { id: theme }

    color: theme.background

    // 用户点击了"刷新"按钮：列表拉取完成后弹一次成功提示
    // （登录/轮询触发的拉取不弹，避免无关打扰）
    property bool refreshPending: false
    property string toastText: ""

    function showToast(msg) {
        toastText = msg;
        toastTimer.restart();
    }

    // 列表数据（过滤后的副本；deviceModel 是 QAbstractListModel，不能直接在 QML 里过滤）
    ListModel { id: deviceRows }

    function refreshRows() {
        deviceRows.clear();
        if (!deviceData) return;
        var q = searchText.toLowerCase();
        for (var i = 0; i < deviceData.rowCount(); i++) {
            var d = deviceData.deviceAt(i);
            if (!d || !d.id) continue;
            if (q !== ""
                && (d.id || "").toLowerCase().indexOf(q) < 0
                && (d.name || "").toLowerCase().indexOf(q) < 0)
                continue;
            deviceRows.append({
                "deviceId": d.id,
                "deviceName": d.name || "",
                "group": d.group || "",
                "lastSeen": d.lastSeen ? Qt.formatDateTime(d.lastSeen, "yyyy-MM-dd hh:mm:ss") : "-",
                "statusText": d.statusText(),
                "statusColor": d.statusColor().toString()
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
        // 分组选项以服务端列表为准：init SQL 预置了「未分组」(group_id=1)，
        // 这里不再硬编码合成项，否则会出现两个"未分组"。
        // 注册选「未分组」→ group_id=1；group_id<=0 服务端会写 NULL。
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

    // 注册设备
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

    Connections {
        target: dataManager
        function onProductsChanged() { root.refreshProductChoices(); }
        function onDevicesRefreshed(count) {
            // 分组下拉也顺带重取一次（refreshGroups 的结果通常已先/同时到达）
            root.refreshGroupChoices();
            if (root.refreshPending) {
                root.refreshPending = false;
                root.showToast("设备列表已刷新（" + count + " 台）");
            }
        }
    }

    // Toast（与 DeviceDetailView 同款）
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

        // ===== 统计卡 =====
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
                value: root.deviceData
                       ? (root.deviceData.totalCount - root.deviceData.onlineCount - root.deviceData.alarmCount)
                       : "0"
                valueColor: theme.mutedForeground
                dotStatus: ShadcnStatusDot.Status.Offline
            }
            StatCard {
                Layout.fillWidth: true
                title: "告警"
                // 与底部状态栏/告警中心同源：活跃告警记录数。
                // 服务端设备状态机没有"告警态"，deviceData.alarmCount 恒为 0，勿用。
                value: root.alarmData ? root.alarmData.activeCount : "0"
                valueColor: root.alarmData && root.alarmData.activeCount > 0
                            ? theme.destructive : theme.mutedForeground
                dotStatus: root.alarmData && root.alarmData.activeCount > 0
                           ? ShadcnStatusDot.Status.Danger : ShadcnStatusDot.Status.Offline
            }
        }

        // ===== 主体 =====
        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 16

            // ---- 设备列表 ----
            Panel {
                Layout.fillHeight: true
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    // 头部：标题 + 搜索
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

                    ShadcnSeparator { Layout.fillWidth: true }

                    // 表头
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Item { Layout.preferredWidth: 8 }   // 与行的状态徽章对齐
                        ShadcnLabel { Layout.preferredWidth: 72;  text: "状态";    size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        ShadcnLabel { Layout.preferredWidth: 110; text: "设备 ID";  size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        ShadcnLabel { Layout.fillWidth: true;    text: "名称";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        ShadcnLabel { Layout.preferredWidth: 210; text: "最新数据"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        ShadcnLabel { Layout.preferredWidth: 130; text: "最后上报"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        ShadcnLabel { text: "操作"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                    }
                    ShadcnSeparator { Layout.fillWidth: true }

                    // 列表
                    ListView {
                        id: deviceList

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: deviceRows
                        spacing: 2

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
                            required property string lastSeen
                            required property string statusText
                            required property string statusColor

                            width: deviceList.width
                            height: 48
                            radius: theme.radius
                            color: deviceHover.containsMouse ? theme.muted : "transparent"

                            MouseArea {
                                id: deviceHover
                                anchors.fill: parent
                                hoverEnabled: true
                                // 整行不响应点击：跳详情只走「详情」按钮，避免误触
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 10

                                // 状态徽章：彩点 + 文字，一眼可辨在线/离线/告警/维护
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

                                ShadcnLabel {
                                    Layout.preferredWidth: 110
                                    text: deviceId
                                    size: ShadcnLabel.Size.Small
                                }
                                ShadcnLabel {
                                    Layout.fillWidth: true
                                    text: deviceName || "-"
                                    size: ShadcnLabel.Size.Small
                                    elide: Text.ElideRight
                                }
                                // 该设备真实上报的指标摘要（不再是写死的温湿度）
                                ShadcnLabel {
                                    Layout.preferredWidth: 210
                                    text: dataManager ? dataManager.metricSummary(deviceId, 2) : "—"
                                    size: ShadcnLabel.Size.Small
                                    variant: ShadcnLabel.Variant.Muted
                                    elide: Text.ElideRight
                                }
                                ShadcnLabel {
                                    Layout.preferredWidth: 130
                                    text: lastSeen || ""
                                    size: ShadcnLabel.Size.Small
                                    variant: ShadcnLabel.Variant.Muted
                                }
                                ShadcnButton {
                                    text: "接入"
                                    size: ShadcnButton.Size.ExtraSmall
                                    variant: ShadcnButton.Variant.Ghost
                                    onClicked: root.navigateToGuide(deviceId)
                                }
                                ShadcnButton {
                                    text: "详情"
                                    size: ShadcnButton.Size.ExtraSmall
                                    variant: ShadcnButton.Variant.Ghost
                                    onClicked: root.deviceSelected(deviceId)
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
                                text: "暂无设备"
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

                // 可直接复制到固件的配置片段（可选中的只读文本）
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

                // 用于“复制”的隐藏编辑器
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
