import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtShadcn
import "../components"

// 告警中心：告警记录 + 告警规则
Rectangle {
    id: root

    property var alarmListModel: null
    property var ruleListModel: null
    property var selectedAlarmIds: ({})
    property var selectedRuleIds: ({})

    // 当前级别筛选（-1 全部 / 0 INFO / 1 WARNING / 2 CRITICAL）
    property int severityFilter: -1

    QtShadcnTheme { id: theme }
    color: theme.background

    // ═══════════════ 工具函数 ═══════════════

    function countOf(map) {
        var c = 0;
        for (var k in map) if (map[k]) c++;
        return c;
    }
    function alarmSelectedCount() { return countOf(selectedAlarmIds); }
    function ruleSelectedCount()  { return countOf(selectedRuleIds); }

    function relativeTime(str) {
        if (!str) return "—";
        var t = new Date(String(str).replace(" ", "T")).getTime();
        if (!t) return str;
        var diff = (Date.now() - t) / 1000;
        if (diff < 60)      return "刚刚";
        if (diff < 3600)    return Math.floor(diff / 60) + " 分钟前";
        if (diff < 86400)   return Math.floor(diff / 3600) + " 小时前";
        if (diff < 86400*7) return Math.floor(diff / 86400) + " 天前";
        return String(str).substring(0, 10);
    }

    function metricLabel(m) {
        if (dataManager && dataManager.metricLabel) return dataManager.metricLabel(m);
        var map = { "temperature":"温度", "humidity":"湿度", "voltage":"电压",
                    "current":"电流", "power":"功率", "pressure":"气压",
                    "co2":"CO₂", "pm25":"PM2.5" };
        return map[m] || m;
    }
    function metricUnit(m) {
        if (dataManager && dataManager.metricUnit) return dataManager.metricUnit(m);
        var map = { "temperature":"°C", "humidity":"%", "voltage":"V",
                    "current":"A", "power":"W", "pressure":"Pa" };
        return map[m] || "";
    }

    function severityColor(sev) {
        if (sev === 2) return theme.destructive;
        if (sev === 1) return "#F59E0B";
        return theme.primary;
    }
    function statusColorOf(s) {
        if (s === 0) return theme.destructive;   // 未确认
        if (s === 1) return "#F59E0B";           // 已确认
        return theme.mutedForeground;            // 已解决
    }
    function statusBgOf(s) {
        if (s === 0) return Qt.alpha(theme.destructive, 0.12);
        if (s === 1) return Qt.alpha("#F59E0B", 0.15);
        return theme.muted;
    }

    // ═══════════════ 选中集合管理（保持原逻辑） ═══════════════

    function syncAlarmSelectAll() {
        if (!alarmSelectAll) return;
        var target = allAlarmsSelected();
        if (alarmSelectAll.checked !== target) alarmSelectAll.checked = target;
    }
    function syncRuleSelectAll() {
        if (!ruleSelectAll) return;
        var target = allRulesSelected();
        if (ruleSelectAll.checked !== target) ruleSelectAll.checked = target;
    }

    function toggleAlarm(id) {
        var m = Object.assign({}, selectedAlarmIds);
        if (m[id]) delete m[id];
        else m[id] = true;
        selectedAlarmIds = m;
        syncAlarmSelectAll();
    }
    function clearAlarmSelection() {
        selectedAlarmIds = {};
        syncAlarmSelectAll();
    }
    function toggleSelectAllAlarms(checked) {
        if (!alarmListModel) return;
        if (!checked) { clearAlarmSelection(); return; }
        var m = {};
        var ids = alarmListModel.ids();
        for (var i = 0; i < ids.length; i++)
            if (alarmListModel.isRowSelectable(i)) m[ids[i]] = true;
        selectedAlarmIds = m;
        syncAlarmSelectAll();
    }
    function allAlarmsSelected() {
        if (!alarmListModel) return false;
        var ids = alarmListModel.ids();
        var any = false;
        for (var i = 0; i < ids.length; i++) {
            if (!alarmListModel.isRowSelectable(i)) continue;
            any = true;
            if (!selectedAlarmIds[ids[i]]) return false;
        }
        return any;
    }
    function acknowledgeSelected() {
        if (!alarmListModel) return;
        var ids = alarmListModel.ids();
        for (var i = 0; i < ids.length; i++)
            if (selectedAlarmIds[ids[i]] && alarmListModel.isRowSelectable(i))
                alarmListModel.acknowledge(i);
        clearAlarmSelection();
    }
    function resolveSelected() {
        if (!alarmListModel) return;
        var ids = alarmListModel.ids();
        for (var i = 0; i < ids.length; i++)
            if (selectedAlarmIds[ids[i]] && alarmListModel.isRowSelectable(i))
                alarmListModel.resolve(i);
        clearAlarmSelection();
    }

    function toggleRuleSelection(id) {
        var m = Object.assign({}, selectedRuleIds);
        if (m[id]) delete m[id];
        else m[id] = true;
        selectedRuleIds = m;
        syncRuleSelectAll();
    }
    function clearRuleSelection() {
        selectedRuleIds = {};
        syncRuleSelectAll();
    }
    function toggleSelectAllRules(checked) {
        if (!ruleListModel) return;
        if (!checked) { clearRuleSelection(); return; }
        var m = {};
        var ids = ruleListModel.ids();
        for (var i = 0; i < ids.length; i++) m[ids[i]] = true;
        selectedRuleIds = m;
        syncRuleSelectAll();
    }
    function allRulesSelected() {
        if (!ruleListModel) return false;
        var ids = ruleListModel.ids();
        if (ids.length === 0) return false;
        for (var i = 0; i < ids.length; i++)
            if (!selectedRuleIds[ids[i]]) return false;
        return true;
    }
    function setSelectedRulesEnabled(enabled) {
        if (!ruleListModel || !dataManager) return;
        var ids = ruleListModel.ids();
        for (var i = 0; i < ids.length; i++)
            if (selectedRuleIds[ids[i]]) dataManager.toggleRule(ids[i], enabled);
        clearRuleSelection();
    }
    function deleteSelectedRules() {
        if (!ruleListModel || !dataManager) return;
        var ids = ruleListModel.ids();
        for (var i = 0; i < ids.length; i++)
            if (selectedRuleIds[ids[i]]) dataManager.deleteRule(ids[i]);
        clearRuleSelection();
    }

    // ═══════════════ 内联组件 ═══════════════

    // 行选中复选框
    component RowCheckbox: Item {
        id: cbRoot

        property bool checked: false
        signal toggled(bool checked)

        implicitWidth: 18
        implicitHeight: 18
        opacity: cbRoot.enabled ? 1.0 : 0.4

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: cbRoot.checked ? theme.primary : theme.background
            border.width: 1
            border.color: cbRoot.checked ? theme.primary
                                         : Qt.alpha(theme.foreground, 0.35)
        }
        ShadcnIcon {
            anchors.centerIn: parent
            visible: cbRoot.checked
            name: "check"
            size: 12
            color: theme.primaryForeground
        }
        MouseArea {
            anchors.fill: parent
            enabled: cbRoot.enabled
            hoverEnabled: true
            cursorShape: cbRoot.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: cbRoot.toggled(!cbRoot.checked)
        }
    }

    // 级别小圆点 + 文字
    component SeverityCell: RowLayout {
        property int sev: 0
        property string label: ""

        spacing: 6
        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            width: 8; height: 8; radius: 4
            color: root.severityColor(parent.parent.sev)
        }
        ShadcnLabel {
            text: parent.parent.label
            size: ShadcnLabel.Size.Small
            color: root.severityColor(parent.parent.sev)
            font.bold: parent.parent.sev === 2
        }
    }

    // 状态 chip
    component StatusChip: Rectangle {
        property int st: 0
        property string label: ""

        implicitWidth: chipLabel.implicitWidth + 20
        implicitHeight: 22
        radius: 11
        color: root.statusBgOf(st)

        ShadcnLabel {
            id: chipLabel
            anchors.centerIn: parent
            text: parent.label
            size: ShadcnLabel.Size.Small
            color: root.statusColorOf(parent.st)
            font.bold: parent.st === 0
        }
    }

    // ═══════════════ 布局 ═══════════════

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        // ── 头部 ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ShadcnLabel {
                text: "告警中心"
                size: ShadcnLabel.Size.Large
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            // 未确认 chip（红底）
            Rectangle {
                implicitWidth: unackRow.implicitWidth + 22
                implicitHeight: 30
                radius: 15
                color: (root.alarmListModel && root.alarmListModel.unacknowledgedCount > 0)
                       ? Qt.alpha(theme.destructive, 0.12)
                       : theme.muted

                RowLayout {
                    id: unackRow
                    anchors.centerIn: parent
                    spacing: 6
                    Rectangle {
                        width: 7; height: 7; radius: 3.5
                        color: theme.destructive
                        visible: root.alarmListModel && root.alarmListModel.unacknowledgedCount > 0
                    }
                    ShadcnLabel {
                        text: "未确认 " + (root.alarmListModel ? root.alarmListModel.unacknowledgedCount : 0)
                        size: ShadcnLabel.Size.Small
                        color: (root.alarmListModel && root.alarmListModel.unacknowledgedCount > 0)
                               ? theme.destructive : theme.mutedForeground
                        font.bold: root.alarmListModel && root.alarmListModel.unacknowledgedCount > 0
                    }
                }
            }

            // 未解决 chip（灰底）
            Rectangle {
                implicitWidth: unresRow.implicitWidth + 22
                implicitHeight: 30
                radius: 15
                color: theme.muted

                RowLayout {
                    id: unresRow
                    anchors.centerIn: parent
                    spacing: 6
                    ShadcnLabel {
                        text: "未解决 " + (root.alarmListModel ? root.alarmListModel.unresolvedCount : 0)
                        size: ShadcnLabel.Size.Small
                        color: theme.foreground
                        font.bold: true
                    }
                }
            }
        }

        // ── Tabs ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            component TabBtn: ShadcnButton {
                property int tabIdx: 0
                property string label: ""
                text: label
                size: ShadcnButton.Size.Small
                variant: (alarmTabs.currentIndex ?? 0) === tabIdx
                         ? ShadcnButton.Variant.Default
                         : ShadcnButton.Variant.Ghost
                onClicked: alarmTabs.currentIndex = tabIdx
            }

            TabBtn { tabIdx: 0; label: "告警记录" }
            TabBtn { tabIdx: 1; label: "告警规则" }
            Item { Layout.fillWidth: true }

            // ShadcnTabsList 保留但不可见，仅用于承载 currentIndex 状态
            ShadcnTabsList {
                id: alarmTabs
                visible: false
                ShadcnTabsTrigger { text: "告警记录" }
                ShadcnTabsTrigger { text: "告警规则" }
            }
        }

        ShadcnSeparator { Layout.fillWidth: true }

        StackLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            currentIndex: alarmTabs.currentIndex

            // ═══════════ Tab 0：告警记录 ═══════════
            ColumnLayout {
                spacing: 12

                // ── 筛选行 ──
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ShadcnLabel {
                        text: "级别"
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                    }

                    component SevChip: ShadcnButton {
                        property int sevId: -1
                        property string label: ""
                        text: label
                        size: ShadcnButton.Size.ExtraSmall
                        variant: (root.severityFilter ?? -1) === sevId
                                 ? ShadcnButton.Variant.Default
                                 : ShadcnButton.Variant.Outline
                        onClicked: {
                            root.severityFilter = sevId;
                            if (root.alarmListModel) {
                                root.alarmListModel.setSeverityFilter(sevId);
                                root.clearAlarmSelection();
                            }
                        }
                    }

                    SevChip { sevId: -1; label: "全部" }
                    SevChip { sevId: 2;  label: "严重" }
                    SevChip { sevId: 1;  label: "警告" }
                    SevChip { sevId: 0;  label: "提示" }

                    Item { Layout.fillWidth: true }

                    ShadcnInput {
                        Layout.preferredWidth: 180
                        placeholderText: "筛选设备 ID"
                        onTextChanged: {
                            if (root.alarmListModel)
                                root.alarmListModel.setDeviceFilter(text.trim());
                        }
                    }
                }

                // ── 批量操作栏（选中时浮出） ──
                Rectangle {
                    Layout.fillWidth: true
                    visible: root.alarmSelectedCount() > 0
                    implicitHeight: 44
                    radius: 10
                    color: Qt.alpha(theme.primary, 0.08)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 10

                        ShadcnLabel {
                            text: "已选 " + root.alarmSelectedCount() + " 条"
                            size: ShadcnLabel.Size.Small
                            font.bold: true
                            color: theme.primary
                        }
                        Item { Layout.fillWidth: true }

                        ShadcnButton {
                            text: "确认选中"
                            iconName: "check"
                            size: ShadcnButton.Size.Small
                            variant: ShadcnButton.Variant.Outline
                            onClicked: root.acknowledgeSelected()
                        }
                        ShadcnButton {
                            text: "解决选中"
                            iconName: "check-check"
                            size: ShadcnButton.Size.Small
                            onClicked: root.resolveSelected()
                        }
                        ShadcnButton {
                            text: "取消"
                            size: ShadcnButton.Size.Small
                            variant: ShadcnButton.Variant.Ghost
                            onClicked: root.clearAlarmSelection()
                        }
                    }
                }

                // ── 表格 ──
                Panel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        // 表头
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            color: Qt.alpha(theme.muted, 0.5)
                            radius: theme.radius

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 10

                                Item {
                                    Layout.preferredWidth: 28
                                    RowCheckbox {
                                        id: alarmSelectAll
                                        anchors.centerIn: parent
                                        checked: false
                                        onToggled: function(checked) {
                                            root.toggleSelectAllAlarms(checked);
                                        }
                                    }
                                }
                                ShadcnLabel { Layout.preferredWidth: 76;  text: "级别";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.preferredWidth: 130; text: "设备";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.preferredWidth: 100; text: "指标";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.preferredWidth: 110; text: "当前值";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.fillWidth:    true;  text: "触发时间"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.preferredWidth: 90;  text: "状态";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.preferredWidth: 100; text: "确认人";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.preferredWidth: 120; text: "操作";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            }
                        }

                        ShadcnSeparator { Layout.fillWidth: true }

                        ListView {
                            id: alarmList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: root.alarmListModel
                            spacing: 0

                            QQC.ScrollBar.vertical: QQC.ScrollBar {
                                active: true
                                policy: QQC.ScrollBar.AsNeeded
                            }

                            delegate: Rectangle {
                                id: alarmDelegate

                                width: alarmList.width
                                height: 52
                                color: alarmHover.hovered
                                       ? Qt.alpha(theme.accent, 0.30)
                                       : "transparent"

                                // 未确认行左侧红条
                                Rectangle {
                                    visible: (model.status ?? 0) === 0
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: 3
                                    color: theme.destructive
                                }

                                HoverHandler { id: alarmHover }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 10

                                    Item {
                                        Layout.preferredWidth: 28
                                        RowCheckbox {
                                            anchors.centerIn: parent
                                            checked: !!root.selectedAlarmIds[model.id]
                                            enabled: (model.status ?? 0) !== 2
                                            onToggled: root.toggleAlarm(model.id)
                                        }
                                    }

                                    SeverityCell {
                                        Layout.preferredWidth: 76
                                        sev: model.severity ?? 0
                                        label: model.severityText ?? ""
                                    }

                                    ShadcnLabel {
                                        Layout.preferredWidth: 130
                                        text: model.deviceId
                                        size: ShadcnLabel.Size.Small
                                        font.family: "Monaco"
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }

                                    ShadcnLabel {
                                        Layout.preferredWidth: 100
                                        text: root.metricLabel(model.metric)
                                        size: ShadcnLabel.Size.Small
                                    }

                                    ShadcnLabel {
                                        Layout.preferredWidth: 110
                                        text: Number(model.value).toFixed(1) + " " + root.metricUnit(model.metric)
                                        size: ShadcnLabel.Size.Small
                                        font.bold: true
                                        color: root.severityColor(model.severity)
                                    }

                                    ShadcnLabel {
                                        Layout.fillWidth: true
                                        text: root.relativeTime(model.triggeredAt)
                                        size: ShadcnLabel.Size.Small
                                        variant: ShadcnLabel.Variant.Muted
                                    }

                                    Item {
                                        Layout.preferredWidth: 90
                                        StatusChip {
                                            anchors.left: parent.left
                                            st: model.status ?? 0
                                            label: (model.status ?? 0) === 2 ? "已解决"
                                                 : (model.status ?? 0) === 1 ? "已确认" : "未确认"
                                        }
                                    }

                                    ShadcnLabel {
                                        Layout.preferredWidth: 100
                                        text: (model.status === 2
                                               ? (model.resolvedByName || "—")
                                               : (model.acknowledgedByName || "—"))
                                        size: ShadcnLabel.Size.Small
                                        variant: ShadcnLabel.Variant.Muted
                                        elide: Text.ElideRight
                                    }

                                    RowLayout {
                                        Layout.preferredWidth: 120
                                        spacing: 4

                                        ShadcnButton {
                                            text: "确认"
                                            size: ShadcnButton.Size.ExtraSmall
                                            variant: ShadcnButton.Variant.Ghost
                                            visible: (model.status ?? 0) === 0
                                            onClicked: root.alarmListModel.acknowledge(index)
                                        }
                                        ShadcnButton {
                                            text: "解决"
                                            size: ShadcnButton.Size.ExtraSmall
                                            variant: ShadcnButton.Variant.Ghost
                                            visible: (model.status ?? 0) !== 2
                                            onClicked: root.alarmListModel.resolve(index)
                                        }
                                    }
                                }
                            }

                            Column {
                                anchors.centerIn: parent
                                spacing: theme.spacingSm
                                visible: alarmList.count === 0

                                ShadcnIcon {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    name: "check-circle"
                                    size: 32
                                    color: theme.success
                                }
                                ShadcnLabel {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: root.severityFilter === -1 ? "暂无告警记录"
                                                                     : "当前筛选级别下无告警"
                                    variant: ShadcnLabel.Variant.Muted
                                }
                            }
                        }
                    }
                }
            }

            // ═══════════ Tab 1：告警规则 ═══════════
            ColumnLayout {
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ShadcnInput {
                        Layout.preferredWidth: 180
                        placeholderText: "筛选设备 ID"
                        onTextChanged: {
                            if (root.ruleListModel)
                                root.ruleListModel.setDeviceFilter(text.trim());
                        }
                    }
                    Item { Layout.fillWidth: true }

                    ShadcnLabel {
                        text: "已选 " + root.ruleSelectedCount() + " 条"
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                        visible: root.ruleSelectedCount() > 0
                    }
                    ShadcnButton {
                        text: "批量启用"
                        size: ShadcnButton.Size.Small
                        variant: ShadcnButton.Variant.Outline
                        visible: root.ruleSelectedCount() > 0
                        onClicked: root.setSelectedRulesEnabled(true)
                    }
                    ShadcnButton {
                        text: "批量禁用"
                        size: ShadcnButton.Size.Small
                        variant: ShadcnButton.Variant.Outline
                        visible: root.ruleSelectedCount() > 0
                        onClicked: root.setSelectedRulesEnabled(false)
                    }
                    ShadcnButton {
                        text: "批量删除"
                        size: ShadcnButton.Size.Small
                        variant: ShadcnButton.Variant.Destructive
                        visible: root.ruleSelectedCount() > 0
                        onClicked: root.deleteSelectedRules()
                    }

                    ShadcnButton {
                        text: "添加规则"
                        iconName: "plus"
                        size: ShadcnButton.Size.Small
                        onClicked: {
                            addRuleDialog.editingRule = null;
                            addRuleDialog.open();
                        }
                    }
                }

                Panel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            RowCheckbox {
                                id: ruleSelectAll
                                checked: false
                                onToggled: function(checked) {
                                    root.toggleSelectAllRules(checked);
                                }
                            }
                            ShadcnLabel { Layout.preferredWidth: 130; text: "设备";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            ShadcnLabel { Layout.preferredWidth: 100; text: "指标";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            ShadcnLabel { Layout.preferredWidth: 80;  text: "条件";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            ShadcnLabel { text: "级别"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            ShadcnLabel { text: "启用"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            Item { Layout.fillWidth: true }
                            ShadcnLabel { text: "操作"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        }
                        ShadcnSeparator { Layout.fillWidth: true }

                        ListView {
                            id: ruleList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: root.ruleListModel
                            spacing: 2

                            QQC.ScrollBar.vertical: QQC.ScrollBar {
                                active: true
                                policy: QQC.ScrollBar.AsNeeded
                            }

                            delegate: Rectangle {
                                width: ruleList.width
                                height: 48
                                radius: theme.radius
                                color: ruleHover.containsMouse ? theme.muted : "transparent"

                                MouseArea {
                                    id: ruleHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 10

                                    RowCheckbox {
                                        checked: !!root.selectedRuleIds[model.id]
                                        onToggled: root.toggleRuleSelection(model.id)
                                    }
                                    ShadcnLabel {
                                        Layout.preferredWidth: 130
                                        text: model.deviceId === "*" ? "所有设备" : model.deviceId
                                        size: ShadcnLabel.Size.Small
                                        font.bold: true
                                    }
                                    ShadcnLabel {
                                        Layout.preferredWidth: 100
                                        text: root.metricLabel(model.metric)
                                        size: ShadcnLabel.Size.Small
                                        variant: ShadcnLabel.Variant.Muted
                                    }
                                    ShadcnLabel {
                                        Layout.preferredWidth: 80
                                        text: model.op + " " + model.threshold
                                        size: ShadcnLabel.Size.Small
                                    }
                                    ShadcnBadge {
                                        text: model.severity
                                        variant: model.severity === "严重"
                                                 ? ShadcnBadge.Variant.Destructive
                                                 : model.severity === "警告"
                                                   ? ShadcnBadge.Variant.Secondary
                                                   : ShadcnBadge.Variant.Outline
                                    }
                                    ShadcnSwitch {
                                        size: ShadcnSwitch.Size.Small
                                        checked: model.enabled === "启用"
                                        onToggled: {
                                            if (dataManager) dataManager.toggleRule(model.id, checked);
                                        }
                                    }
                                    Item { Layout.fillWidth: true }
                                    ShadcnButton {
                                        text: "编辑"
                                        size: ShadcnButton.Size.ExtraSmall
                                        variant: ShadcnButton.Variant.Ghost
                                        iconName: "pencil"
                                        onClicked: {
                                            addRuleDialog.editingRule = {
                                                "id": model.id,
                                                "deviceId": model.deviceId === "*" ? "" : model.deviceId,
                                                "metric": model.metric,
                                                "threshold": model.threshold,
                                                "opIndex": Math.max(0, [">", "<", "==", ">=", "<="].indexOf(model.op)),
                                                "severityIndex": model.severity === "信息" ? 0
                                                               : model.severity === "警告" ? 1 : 2
                                            };
                                            addRuleDialog.open();
                                        }
                                    }
                                    ShadcnButton {
                                        text: "删除"
                                        size: ShadcnButton.Size.ExtraSmall
                                        variant: ShadcnButton.Variant.Ghost
                                        iconName: "trash-2"
                                        onClicked: {
                                            deleteConfirmDialog.ruleId = model.id;
                                            deleteConfirmDialog.ruleDesc =
                                                (model.deviceId === "*" ? "所有设备" : model.deviceId)
                                                + " " + model.metric + " " + model.op + " " + model.threshold;
                                            deleteConfirmDialog.open();
                                        }
                                    }
                                }
                            }

                            Column {
                                anchors.centerIn: parent
                                spacing: theme.spacingSm
                                visible: ruleList.count === 0

                                ShadcnIcon {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    name: "bell"
                                    size: 32
                                    color: theme.mutedForeground
                                }
                                ShadcnLabel {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "暂无告警规则"
                                    variant: ShadcnLabel.Variant.Muted
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ═══════════ 添加/编辑规则 ═══════════
    ShadcnDialog {
        id: addRuleDialog

        property var editingRule: null

        modal: true

        onOpened: {
            if (editingRule) {
                ruleDeviceField.text    = editingRule.deviceId || "";
                ruleMetricField.text    = editingRule.metric || "";
                ruleThresholdField.text = String(editingRule.threshold !== undefined ? editingRule.threshold : 0);
                ruleOpCombo.currentIndex = (editingRule.opIndex >= 0) ? editingRule.opIndex : 0;
                ruleSeverityCombo.currentIndex = (editingRule.severityIndex >= 0)
                                                 ? editingRule.severityIndex : 1;
            } else {
                ruleDeviceField.text    = "";
                ruleMetricField.text    = "";
                ruleThresholdField.text = "0";
                ruleOpCombo.currentIndex = 0;
                ruleSeverityCombo.currentIndex = 1;
            }
        }
        onClosed: editingRule = null

        ShadcnDialogContent {
            ShadcnDialogHeader {
                ShadcnDialogTitle {
                    text: addRuleDialog.editingRule ? "编辑告警规则" : "添加告警规则"
                }
                ShadcnDialogDescription { text: "当指标满足条件时触发告警" }
            }

            ColumnLayout {
                width: parent.width
                spacing: 12

                ShadcnLabel { text: "设备 ID"; size: ShadcnLabel.Size.Small }
                ShadcnInput {
                    id: ruleDeviceField
                    Layout.fillWidth: true
                    placeholderText: "留空表示所有设备"
                }
                ShadcnLabel { text: "指标 *"; size: ShadcnLabel.Size.Small }
                ShadcnInput {
                    id: ruleMetricField
                    Layout.fillWidth: true
                    placeholderText: "如 temperature"
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        ShadcnLabel { text: "条件"; size: ShadcnLabel.Size.Small }
                        ShadcnSelect {
                            id: ruleOpCombo
                            Layout.fillWidth: true
                            model: [">", "<", "==", ">=", "<="]
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        ShadcnLabel { text: "阈值"; size: ShadcnLabel.Size.Small }
                        ShadcnInput {
                            id: ruleThresholdField
                            Layout.fillWidth: true
                            text: "0"
                            placeholderText: "如 32.0"
                        }
                    }
                }
                ShadcnLabel { text: "严重级别"; size: ShadcnLabel.Size.Small }
                ShadcnSelect {
                    id: ruleSeverityCombo
                    Layout.fillWidth: true
                    model: ["INFO", "WARNING", "CRITICAL"]
                    currentIndex: 1
                }
            }

            footer: ShadcnDialogFooter {
                ShadcnButton {
                    text: "取消"
                    variant: ShadcnButton.Variant.Outline
                    size: ShadcnButton.Size.Small
                    onClicked: addRuleDialog.close()
                }
                ShadcnButton {
                    text: addRuleDialog.editingRule ? "保存" : "添加"
                    size: ShadcnButton.Size.Small
                    onClicked: {
                        var dev = ruleDeviceField.text.trim();
                        var met = ruleMetricField.text.trim();
                        if (met === "" || !dataManager) return;
                        var opIdx = ruleOpCombo.currentIndex;
                        var thr = parseFloat(ruleThresholdField.text) || 0;
                        var sevIdx = ruleSeverityCombo.currentIndex;
                        if (addRuleDialog.editingRule)
                            dataManager.editRule(addRuleDialog.editingRule.id, dev, met, opIdx, thr, sevIdx);
                        else
                            dataManager.addAlarmRule(dev, met, opIdx, thr, sevIdx);
                        addRuleDialog.editingRule = null;
                        addRuleDialog.close();
                    }
                }
            }
        }
    }

    // ═══════════ 删除确认 ═══════════
    ShadcnDialog {
        id: deleteConfirmDialog

        property string ruleDesc: ""
        property int ruleId: 0

        modal: true

        ShadcnDialogContent {
            ShadcnDialogHeader {
                ShadcnDialogTitle { text: "确认删除" }
                ShadcnDialogDescription {
                    text: "确定要删除规则【" + deleteConfirmDialog.ruleDesc + "】吗？此操作不可撤销。"
                }
            }
            footer: ShadcnDialogFooter {
                ShadcnButton {
                    text: "取消"
                    variant: ShadcnButton.Variant.Outline
                    size: ShadcnButton.Size.Small
                    onClicked: deleteConfirmDialog.close()
                }
                ShadcnButton {
                    text: "删除"
                    size: ShadcnButton.Size.Small
                    variant: ShadcnButton.Variant.Destructive
                    onClicked: {
                        if (dataManager) dataManager.deleteRule(deleteConfirmDialog.ruleId);
                        deleteConfirmDialog.close();
                    }
                }
            }
        }
    }
}
