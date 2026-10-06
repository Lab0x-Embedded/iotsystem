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

    property int severityFilter: -1
    property int activeTab: 0

    QtShadcnTheme { id: theme }
    color: theme.background

    // ═══════════════ 时间处理 ═══════════════
    function parseLocalTime(s) {
        var m = String(s).match(/^(\d{4})-(\d{2})-(\d{2})[\sT](\d{2}):(\d{2}):(\d{2})/);
        if (!m) return NaN;
        return new Date(Number(m[1]), Number(m[2]) - 1, Number(m[3]),
                        Number(m[4]), Number(m[5]), Number(m[6])).getTime();
    }

    function relativeTime(str) {
        if (!str) return "—";
        var s = String(str);
        var t = NaN;

        if (/^\d{10}$/.test(s)) {
            t = Number(s) * 1000;
        } else if (/^\d{13}$/.test(s)) {
            t = Number(s);
        } else {
            t = parseLocalTime(s);
            if (isNaN(t)) t = new Date(s).getTime();
        }

        if (isNaN(t)) return s;

        var diff = (Date.now() - t) / 1000;
        if (diff < 0)        return "刚刚";
        if (diff < 60)       return "刚刚";
        if (diff < 3600)     return Math.floor(diff / 60) + " 分钟前";
        if (diff < 86400)    return Math.floor(diff / 3600) + " 小时前";
        if (diff < 86400*7)  return Math.floor(diff / 86400) + " 天前";
        return s.substring(0, 10);
    }

    function countOf(map) {
        var c = 0;
        for (var k in map) if (map[k]) c++;
        return c;
    }
    function alarmSelectedCount() { return countOf(selectedAlarmIds); }
    function ruleSelectedCount()  { return countOf(selectedRuleIds); }

    function metricLabel(m) {
        if (dataManager && dataManager.metricLabel) return dataManager.metricLabel(m);
        var map = { "temperature":"温度", "humidity":"湿度", "voltage":"电压",
                    "current":"电流", "power":"功率", "pressure":"气压" };
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
        if (sev === 1) return "#D97706";
        return theme.primary;
    }
    function severityBg(sev) {
        if (sev === 2) return Qt.alpha(theme.destructive, 0.10);
        if (sev === 1) return Qt.alpha("#D97706", 0.12);
        return Qt.alpha(theme.primary, 0.10);
    }
    function statusText(s) {
        if (s === 2) return "已解决";
        if (s === 1) return "已确认";
        return "未确认";
    }
    function statusColorOf(s) {
        if (s === 0) return theme.destructive;
        if (s === 1) return "#D97706";
        return theme.mutedForeground;
    }
    function statusBgOf(s) {
        if (s === 0) return Qt.alpha(theme.destructive, 0.10);
        if (s === 1) return Qt.alpha("#D97706", 0.12);
        return Qt.alpha(theme.foreground, 0.06);
    }

    // ═══════════════ 选择管理 ═══════════════
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
        if (m[id]) delete m[id]; else m[id] = true;
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
        if (m[id]) delete m[id]; else m[id] = true;
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

    // 复选框
    component RowCheckbox: Item {
        id: cbRoot
        property bool checked: false
        signal toggled(bool checked)
        implicitWidth: 16
        implicitHeight: 16
        opacity: cbRoot.enabled ? 1.0 : 0.4

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: cbRoot.checked ? theme.primary : "transparent"
            border.width: 1
            border.color: cbRoot.checked ? theme.primary
                                         : Qt.alpha(theme.foreground, 0.30)
        }
        ShadcnIcon {
            anchors.centerIn: parent
            visible: cbRoot.checked
            name: "check"
            size: 11
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

    // 级别单元格（行内）：圆点 + 文字
    component SeverityCell: RowLayout {
        id: sevRoot
        property int sev: 0
        property string label: ""

        spacing: 6
        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            width: 7; height: 7; radius: 3.5
            color: root.severityColor(sevRoot.sev)
        }
        ShadcnLabel {
            Layout.alignment: Qt.AlignVCenter
            text: sevRoot.label
            size: ShadcnLabel.Size.Small
            color: root.severityColor(sevRoot.sev)
            font.bold: sevRoot.sev >= 1
        }
    }

    // 状态 chip —— 用原生 Text 保证垂直居中
    component StatusChip: Rectangle {
        id: chipRoot
        property int st: 0

        implicitWidth: chipText.implicitWidth + 20
        implicitHeight: 22
        radius: 11
        color: root.statusBgOf(chipRoot.st)

        Text {
            id: chipText
            anchors.centerIn: parent
            text: root.statusText(chipRoot.st)
            color: root.statusColorOf(chipRoot.st)
            font.pixelSize: 12
            font.bold: chipRoot.st === 0
        }
    }

    // 下划线 Tab
    component TabBtn: Item {
        id: tabRoot
        property int tabIdx: 0
        property string label: ""

        implicitWidth: tabLabel.implicitWidth + 24
        implicitHeight: 40

        Text {
            id: tabLabel
            anchors.centerIn: parent
            text: tabRoot.label
            color: root.activeTab === tabRoot.tabIdx
                   ? theme.foreground : theme.mutedForeground
            font.pixelSize: 13
            font.bold: root.activeTab === tabRoot.tabIdx
        }
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            height: 2
            radius: 1
            color: root.activeTab === tabRoot.tabIdx
                   ? theme.primary : "transparent"
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.activeTab = tabRoot.tabIdx
        }
    }

    // 级别筛选 chip —— 自定义选中态
    component SevChip: Rectangle {
        id: sevChipRoot
        property int sevId: -1
        property string label: ""

        readonly property bool selected: root.severityFilter === sevChipRoot.sevId

        implicitWidth: chipLabel.implicitWidth + 28
        implicitHeight: 28
        radius: 14
        color: selected ? theme.primary : "transparent"
        border.width: 1
        border.color: selected ? theme.primary
                               : Qt.alpha(theme.foreground, 0.18)

        Text {
            id: chipLabel
            anchors.centerIn: parent
            text: sevChipRoot.label
            color: sevChipRoot.selected ? theme.primaryForeground
                                        : theme.foreground
            font.pixelSize: 12
            font.bold: sevChipRoot.selected
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                root.severityFilter = sevChipRoot.sevId;
                if (root.alarmListModel) {
                    root.alarmListModel.setSeverityFilter(sevChipRoot.sevId);
                    root.clearAlarmSelection();
                }
            }
        }
    }

    // ═══════════════ 布局 ═══════════════
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 0

        // ── 头部 ──
        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 16
            spacing: 12

            ShadcnLabel {
                text: "告警中心"
                size: ShadcnLabel.Size.Large
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                implicitWidth: unackRow.implicitWidth + 20
                implicitHeight: 28
                radius: 14
                color: (root.alarmListModel && root.alarmListModel.unacknowledgedCount > 0)
                       ? Qt.alpha(theme.destructive, 0.10)
                       : Qt.alpha(theme.foreground, 0.06)

                RowLayout {
                    id: unackRow
                    anchors.centerIn: parent
                    spacing: 6
                    Rectangle {
                        width: 7; height: 7; radius: 3.5
                        color: theme.destructive
                        visible: root.alarmListModel && root.alarmListModel.unacknowledgedCount > 0
                    }
                    Text {
                        text: "未确认 " + (root.alarmListModel ? root.alarmListModel.unacknowledgedCount : 0)
                        color: (root.alarmListModel && root.alarmListModel.unacknowledgedCount > 0)
                               ? theme.destructive : theme.mutedForeground
                        font.pixelSize: 12
                        font.bold: root.alarmListModel && root.alarmListModel.unacknowledgedCount > 0
                    }
                }
            }

            Rectangle {
                implicitWidth: unresRow.implicitWidth + 20
                implicitHeight: 28
                radius: 14
                color: Qt.alpha(theme.foreground, 0.06)

                RowLayout {
                    id: unresRow
                    anchors.centerIn: parent
                    spacing: 6
                    Text {
                        text: "未解决 " + (root.alarmListModel ? root.alarmListModel.unresolvedCount : 0)
                        color: theme.foreground
                        font.pixelSize: 12
                    }
                }
            }
        }

        // ── Tabs ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            TabBtn { tabIdx: 0; label: "告警记录" }
            TabBtn { tabIdx: 1; label: "告警规则" }
            Item { Layout.fillWidth: true }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: -1
            implicitHeight: 1
            color: Qt.alpha(theme.foreground, 0.08)
        }

        StackLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.topMargin: 16
            currentIndex: root.activeTab

            // ═══════════ Tab 0：告警记录 ═══════════
            ColumnLayout {
                spacing: 12

                // 筛选行
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ShadcnLabel {
                        text: "级别"
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                    }

                    SevChip { sevId: -1; label: "全部" }
                    SevChip { sevId: 2;  label: "严重" }
                    SevChip { sevId: 1;  label: "警告" }
                    SevChip { sevId: 0;  label: "提示" }

                    Item { Layout.fillWidth: true }

                    ShadcnInput {
                        Layout.preferredWidth: 200
                        placeholderText: "筛选设备 ID"
                        onTextChanged: {
                            if (root.alarmListModel)
                                root.alarmListModel.setDeviceFilter(text.trim());
                        }
                    }
                }

                // 批量操作栏
                Rectangle {
                    Layout.fillWidth: true
                    visible: root.alarmSelectedCount() > 0
                    implicitHeight: 40
                    radius: 8
                    color: Qt.alpha(theme.primary, 0.06)
                    border.width: 1
                    border.color: Qt.alpha(theme.primary, 0.15)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 8
                        spacing: 8

                        ShadcnLabel {
                            text: "已选 " + root.alarmSelectedCount() + " 条"
                            size: ShadcnLabel.Size.Small
                            font.bold: true
                            color: theme.primary
                        }
                        Item { Layout.fillWidth: true }

                        ShadcnButton {
                            text: "确认"
                            iconName: "check"
                            size: ShadcnButton.Size.ExtraSmall
                            variant: ShadcnButton.Variant.Ghost
                            onClicked: root.acknowledgeSelected()
                        }
                        ShadcnButton {
                            text: "解决"
                            iconName: "check-check"
                            size: ShadcnButton.Size.ExtraSmall
                            variant: ShadcnButton.Variant.Ghost
                            onClicked: root.resolveSelected()
                        }
                        ShadcnButton {
                            text: "取消"
                            size: ShadcnButton.Size.ExtraSmall
                            variant: ShadcnButton.Variant.Ghost
                            onClicked: root.clearAlarmSelection()
                        }
                    }
                }

                // 表格
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 10
                    color: theme.background
                    border.width: 1
                    border.color: Qt.alpha(theme.foreground, 0.08)

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        // 表头
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            color: Qt.alpha(theme.foreground, 0.03)
                            topLeftRadius: 10
                            topRightRadius: 10

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                Item {
                                    Layout.preferredWidth: 20
                                    RowCheckbox {
                                        id: alarmSelectAll
                                        anchors.verticalCenter: parent.verticalCenter
                                        checked: false
                                        onToggled: function(checked) {
                                            root.toggleSelectAllAlarms(checked);
                                        }
                                    }
                                }
                                ShadcnLabel { Layout.preferredWidth: 72;  text: "级别";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.preferredWidth: 120; text: "设备";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.preferredWidth: 90;  text: "指标";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.preferredWidth: 110; text: "当前值";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.fillWidth:    true;  text: "触发时间"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.preferredWidth: 80;  text: "状态";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.preferredWidth: 90;  text: "处理人";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.preferredWidth: 96;  text: "操作";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 1
                            color: Qt.alpha(theme.foreground, 0.08)
                        }

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
                                height: 48
                                color: alarmHover.hovered
                                       ? Qt.alpha(theme.foreground, 0.03)
                                       : "transparent"
                                opacity: model.status === 2 ? 0.62 : 1.0

                                Rectangle {
                                    visible: model.status === 0
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: 3
                                    color: theme.destructive
                                }

                                HoverHandler { id: alarmHover }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 16
                                    anchors.rightMargin: 16
                                    spacing: 12

                                    Item {
                                        Layout.preferredWidth: 20
                                        Layout.fillHeight: true
                                        RowCheckbox {
                                            anchors.verticalCenter: parent.verticalCenter
                                            checked: !!root.selectedAlarmIds[model.id]
                                            enabled: model.status !== 2
                                            onToggled: root.toggleAlarm(model.id)
                                        }
                                    }

                                    SeverityCell {
                                        Layout.preferredWidth: 72
                                        Layout.fillHeight: true
                                        Layout.alignment: Qt.AlignVCenter
                                        sev: model.severity
                                        label: model.severityText
                                    }

                                    ShadcnLabel {
                                        Layout.preferredWidth: 120
                                        Layout.alignment: Qt.AlignVCenter
                                        text: model.deviceId
                                        size: ShadcnLabel.Size.Small
                                        font.family: "Monaco"
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }

                                    ShadcnLabel {
                                        Layout.preferredWidth: 90
                                        Layout.alignment: Qt.AlignVCenter
                                        text: root.metricLabel(model.metric)
                                        size: ShadcnLabel.Size.Small
                                        color: theme.mutedForeground
                                    }

                                    ShadcnLabel {
                                        Layout.preferredWidth: 110
                                        Layout.alignment: Qt.AlignVCenter
                                        text: Number(model.value).toFixed(1)
                                              + " " + root.metricUnit(model.metric)
                                        size: ShadcnLabel.Size.Small
                                        font.bold: model.status !== 2
                                        color: model.status === 2
                                               ? theme.foreground
                                               : root.severityColor(model.severity)
                                    }

                                    ShadcnLabel {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignVCenter
                                        text: root.relativeTime(model.triggeredAt)
                                        size: ShadcnLabel.Size.Small
                                        color: theme.mutedForeground
                                    }

                                    Item {
                                        Layout.preferredWidth: 80
                                        Layout.fillHeight: true
                                        StatusChip {
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.left: parent.left
                                            st: model.status
                                        }
                                    }

                                    ShadcnLabel {
                                        Layout.preferredWidth: 90
                                        Layout.alignment: Qt.AlignVCenter
                                        text: model.status === 2
                                              ? (model.resolvedByName || "—")
                                              : (model.acknowledgedByName || "—")
                                        size: ShadcnLabel.Size.Small
                                        color: theme.mutedForeground
                                        elide: Text.ElideRight
                                    }

                                    Item {
                                        Layout.preferredWidth: 96
                                        Layout.fillHeight: true
                                        RowLayout {
                                            anchors.verticalCenter: parent.verticalCenter
                                            spacing: 2

                                            ShadcnButton {
                                                text: "确认"
                                                size: ShadcnButton.Size.ExtraSmall
                                                variant: ShadcnButton.Variant.Ghost
                                                visible: model.status === 0
                                                onClicked: root.alarmListModel.acknowledge(index)
                                            }
                                            ShadcnButton {
                                                text: "解决"
                                                size: ShadcnButton.Size.ExtraSmall
                                                variant: ShadcnButton.Variant.Ghost
                                                visible: model.status === 1
                                                onClicked: root.alarmListModel.resolve(index)
                                            }
                                        }
                                    }
                                }
                            }

                            Column {
                                anchors.centerIn: parent
                                spacing: 8
                                visible: alarmList.count === 0

                                ShadcnIcon {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    name: "check-circle"
                                    size: 32
                                    color: theme.success
                                }
                                ShadcnLabel {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: root.severityFilter === -1
                                          ? "暂无告警记录"
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

                // 筛选行
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ShadcnInput {
                        Layout.preferredWidth: 200
                        Layout.alignment: Qt.AlignVCenter
                        placeholderText: "筛选设备 ID"
                        onTextChanged: {
                            if (root.ruleListModel)
                                root.ruleListModel.setDeviceFilter(text.trim());
                        }
                    }
                    Item { Layout.fillWidth: true }

                    // 批量操作栏
                    Rectangle {
                        visible: root.ruleSelectedCount() > 0
                        implicitWidth: ruleBatchRow.implicitWidth + 20
                        implicitHeight: 32
                        radius: 8
                        color: Qt.alpha(theme.primary, 0.06)
                        border.width: 1
                        border.color: Qt.alpha(theme.primary, 0.15)

                        RowLayout {
                            id: ruleBatchRow
                            anchors.centerIn: parent
                            spacing: 2

                            Text {
                                text: "已选 " + root.ruleSelectedCount() + " 条"
                                color: theme.primary
                                font.pixelSize: 12
                                font.bold: true
                                leftPadding: 6
                                rightPadding: 6
                            }
                            ShadcnButton {
                                text: "启用"
                                size: ShadcnButton.Size.ExtraSmall
                                variant: ShadcnButton.Variant.Ghost
                                onClicked: root.setSelectedRulesEnabled(true)
                            }
                            ShadcnButton {
                                text: "禁用"
                                size: ShadcnButton.Size.ExtraSmall
                                variant: ShadcnButton.Variant.Ghost
                                onClicked: root.setSelectedRulesEnabled(false)
                            }
                            ShadcnButton {
                                text: "删除"
                                size: ShadcnButton.Size.ExtraSmall
                                variant: ShadcnButton.Variant.Ghost
                                onClicked: root.deleteSelectedRules()
                            }
                            ShadcnButton {
                                text: "取消"
                                size: ShadcnButton.Size.ExtraSmall
                                variant: ShadcnButton.Variant.Ghost
                                onClicked: root.clearRuleSelection()
                            }
                        }
                    }

                    ShadcnButton {
                        text: "添加规则"
                        iconName: "plus"
                        size: ShadcnButton.Size.Small
                        variant: ShadcnButton.Variant.Outline
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: {
                            addRuleDialog.editingRule = null;
                            addRuleDialog.open();
                        }
                    }
                }

                // 表格
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 10
                    color: theme.background
                    border.width: 1
                    border.color: Qt.alpha(theme.foreground, 0.08)

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        // 表头
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            color: Qt.alpha(theme.foreground, 0.03)
                            topLeftRadius: 10
                            topRightRadius: 10

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                spacing: 12

                                Item {
                                    Layout.preferredWidth: 20
                                    RowCheckbox {
                                        id: ruleSelectAll
                                        anchors.verticalCenter: parent.verticalCenter
                                        checked: false
                                        onToggled: function(checked) {
                                            root.toggleSelectAllRules(checked);
                                        }
                                    }
                                }
                                ShadcnLabel { Layout.preferredWidth: 130; text: "设备";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.preferredWidth: 90;  text: "指标";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.preferredWidth: 120; text: "条件";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.preferredWidth: 90;  text: "级别";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnLabel { Layout.preferredWidth: 70;  text: "启用";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                Item { Layout.fillWidth: true }
                                ShadcnLabel { Layout.preferredWidth: 130; text: "操作";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 1
                            color: Qt.alpha(theme.foreground, 0.08)
                        }

                        ListView {
                            id: ruleList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: root.ruleListModel
                            spacing: 0

                            QQC.ScrollBar.vertical: QQC.ScrollBar {
                                active: true
                                policy: QQC.ScrollBar.AsNeeded
                            }

                            delegate: Rectangle {
                                id: ruleDelegate

                                width: ruleList.width
                                height: 48
                                color: ruleHover.hovered
                                       ? Qt.alpha(theme.foreground, 0.03)
                                       : "transparent"

                                HoverHandler { id: ruleHover }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 16
                                    anchors.rightMargin: 16
                                    spacing: 12

                                    Item {
                                        Layout.preferredWidth: 20
                                        Layout.fillHeight: true
                                        RowCheckbox {
                                            anchors.verticalCenter: parent.verticalCenter
                                            checked: !!root.selectedRuleIds[model.id]
                                            onToggled: root.toggleRuleSelection(model.id)
                                        }
                                    }

                                    ShadcnLabel {
                                        Layout.preferredWidth: 130
                                        Layout.alignment: Qt.AlignVCenter
                                        text: model.deviceId === "*" ? "所有设备" : model.deviceId
                                        size: ShadcnLabel.Size.Small
                                        font.family: model.deviceId === "*" ? "" : "Monaco"
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }

                                    ShadcnLabel {
                                        Layout.preferredWidth: 90
                                        Layout.alignment: Qt.AlignVCenter
                                        text: root.metricLabel(model.metric)
                                        size: ShadcnLabel.Size.Small
                                        color: theme.mutedForeground
                                    }

                                    ShadcnLabel {
                                        Layout.preferredWidth: 120
                                        Layout.alignment: Qt.AlignVCenter
                                        text: model.op + " " + Number(model.threshold).toFixed(2)
                                        size: ShadcnLabel.Size.Small
                                        font.family: "Monaco"
                                    }

                                    Item {
                                        Layout.preferredWidth: 90
                                        Layout.fillHeight: true
                                        Rectangle {
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.left: parent.left
                                            implicitWidth: sevLabel.implicitWidth + 16
                                            implicitHeight: 22
                                            radius: 11
                                            color: model.severity === "严重"
                                                   ? root.severityBg(2)
                                                   : model.severity === "警告"
                                                     ? root.severityBg(1)
                                                     : root.severityBg(0)
                                            Text {
                                                id: sevLabel
                                                anchors.centerIn: parent
                                                text: model.severity
                                                font.pixelSize: 12
                                                color: model.severity === "严重"
                                                       ? root.severityColor(2)
                                                       : model.severity === "警告"
                                                         ? root.severityColor(1)
                                                         : root.severityColor(0)
                                            }
                                        }
                                    }

                                    Item {
                                        Layout.preferredWidth: 70
                                        Layout.fillHeight: true
                                        ShadcnSwitch {
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.left: parent.left
                                            size: ShadcnSwitch.Size.Small
                                            checked: model.enabled === "启用"
                                            onToggled: {
                                                if (dataManager) dataManager.toggleRule(model.id, checked);
                                            }
                                        }
                                    }

                                    Item { Layout.fillWidth: true }

                                    RowLayout {
                                        Layout.preferredWidth: 130
                                        spacing: 2
                                        ShadcnButton {
                                            text: "编辑"
                                            size: ShadcnButton.Size.ExtraSmall
                                            variant: ShadcnButton.Variant.Ghost
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
                            }

                            Column {
                                anchors.centerIn: parent
                                spacing: 8
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
