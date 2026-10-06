import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtShadcn
import "../components"

// 设备总览：统计卡 + 设备列表
Rectangle {
    id: root

    property var deviceData: null

    // 搜索关键字（真正用于过滤列表，之前这个搜索框是摆设）
    property string searchText: ""
    // 分组下拉：ShadcnSelect 的 delegate 只可靠支持「字符串数组」，
    // 用对象数组会渲染成 [object Object]，所以拆成「名称数组 + 平行 id 数组」
    property var groupNames: []
    property var groupIds: []

    signal deviceSelected(string deviceId)

    QtShadcnTheme { id: theme }

    color: theme.background

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
                "lastSeen": d.lastSeen ? Qt.formatDateTime(d.lastSeen, "yyyy-MM-dd hh:mm:ss") : "-"
            });
        }
    }

    function refreshGroupChoices() {
        var names = ["未分组"];
        var ids   = [0];
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
        var pk = regProductKey.text.trim();
        if (id === "" || pk === "") {
            regError.text = "设备 ID 与产品 Key 为必填";
            return;
        }
        var gid = 0;
        if (regGroup.currentIndex >= 0 && regGroup.currentIndex < groupIds.length)
            gid = groupIds[regGroup.currentIndex];
        if (!dataManager) return;
        dataManager.registerDevice(id, regName.text.trim(), pk,
                                   regType.text.trim(), regSecret.text.trim(), gid);
    }

    Component.onCompleted: { refreshRows(); refreshGroupChoices(); }

    Connections {
        target: root.deviceData
        function onCountsChanged() { root.refreshRows(); }
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
                value: root.deviceData ? root.deviceData.alarmCount : "0"
                valueColor: theme.destructive
                dotStatus: ShadcnStatusDot.Status.Danger
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
                            text: "注册设备"
                            iconName: "plus"
                            size: ShadcnButton.Size.Small
                            onClicked: {
                                regDeviceId.text = "";
                                regName.text = "";
                                regProductKey.text = "factory_sensor";
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

                        Item { Layout.preferredWidth: 8 }   // 与行的状态点对齐
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
                            required property int index
                            required property string deviceId
                            required property string deviceName
                            required property string group
                            required property string lastSeen

                            width: deviceList.width
                            height: 48
                            radius: theme.radius
                            color: deviceHover.containsMouse ? theme.muted : "transparent"

                            MouseArea {
                                id: deviceHover
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.deviceSelected(deviceId)
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 10

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

                ShadcnLabel { text: "产品 Key *"; size: ShadcnLabel.Size.Small }
                ShadcnInput {
                    id: regProductKey
                    Layout.fillWidth: true
                    placeholderText: "如 factory_sensor / smart_meter"
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
            registeredDialog.productKey = regProductKey.text.trim();
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
