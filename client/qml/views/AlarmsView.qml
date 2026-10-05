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
    property var selectedIds: ({})

    QtShadcnTheme { id: theme }

    color: theme.background

    function selectedCount() {
        var c = 0;
        for (var k in selectedIds)
            if (selectedIds[k]) c++;
        return c;
    }
    function clearSelection() {
        selectedIds = {};
    }
    function toggleSelection(id) {
        var m = selectedIds;
        if (m[id]) delete m[id];
        else m[id] = true;
        selectedIds = m;
    }

    // ---- 告警记录: 批量确认 / 解决 ----
    function toggleSelectAllAlarms(checked) {
        if (!alarmListModel) return;
        if (!checked) { clearSelection(); return; }
        var m = {};
        var ids = alarmListModel.ids();
        for (var i = 0; i < ids.length; i++)
            if (alarmListModel.isRowSelectable(i)) m[ids[i]] = true;
        selectedIds = m;
    }
    function allAlarmsSelected() {
        if (!alarmListModel) return false;
        var ids = alarmListModel.ids();
        var any = false;
        for (var i = 0; i < ids.length; i++) {
            if (!alarmListModel.isRowSelectable(i)) continue;
            any = true;
            if (!selectedIds[ids[i]]) return false;
        }
        return any;
    }
    function acknowledgeSelected() {
        if (!alarmListModel) return;
        var ids = alarmListModel.ids();
        for (var i = 0; i < ids.length; i++)
            if (selectedIds[ids[i]] && alarmListModel.isRowSelectable(i))
                alarmListModel.acknowledge(i);
        clearSelection();
    }
    function resolveSelected() {
        if (!alarmListModel) return;
        var ids = alarmListModel.ids();
        for (var i = 0; i < ids.length; i++)
            if (selectedIds[ids[i]] && alarmListModel.isRowSelectable(i))
                alarmListModel.resolve(i);
        clearSelection();
    }

    // ---- 告警规则: 批量启用 / 禁用 / 删除 ----
    function toggleSelectAllRules(checked) {
        if (!ruleListModel) return;
        if (!checked) { clearSelection(); return; }
        var m = {};
        var ids = ruleListModel.ids();
        for (var i = 0; i < ids.length; i++) m[ids[i]] = true;
        selectedIds = m;
    }
    function allRulesSelected() {
        if (!ruleListModel) return false;
        var ids = ruleListModel.ids();
        if (ids.length === 0) return false;
        for (var i = 0; i < ids.length; i++)
            if (!selectedIds[ids[i]]) return false;
        return true;
    }
    function setSelectedRulesEnabled(enabled) {
        if (!ruleListModel || !dataManager) return;
        var ids = ruleListModel.ids();
        for (var i = 0; i < ids.length; i++)
            if (selectedIds[ids[i]]) dataManager.toggleRule(ids[i], enabled);
        clearSelection();
    }
    function deleteSelectedRules() {
        if (!ruleListModel || !dataManager) return;
        var ids = ruleListModel.ids();
        for (var i = 0; i < ids.length; i++)
            if (selectedIds[ids[i]]) dataManager.deleteRule(ids[i]);
        clearSelection();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        // ===== 头部 =====
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ShadcnLabel {
                text: "告警中心"
                size: ShadcnLabel.Size.Large
            }
            Item { Layout.fillWidth: true }
            ShadcnBadge {
                text: "未确认 " + (root.alarmListModel ? root.alarmListModel.unacknowledgedCount : 0)
                variant: ShadcnBadge.Variant.Destructive
            }
            ShadcnBadge {
                text: "未解决 " + (root.alarmListModel ? root.alarmListModel.unresolvedCount : 0)
                variant: ShadcnBadge.Variant.Secondary
            }
        }

        // ===== Tabs =====
        ShadcnTabsList {
            id: alarmTabs
            Layout.fillWidth: true
            ShadcnTabsTrigger { text: "告警记录" }
            ShadcnTabsTrigger { text: "告警规则" }
        }

        StackLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            currentIndex: alarmTabs.currentIndex

            // ================= Tab 0: 告警记录 =================
            ColumnLayout {
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ShadcnSelect {
                        id: severityFilter
                        width: 130
                        model: ["全部", "INFO", "WARNING", "CRITICAL"]
                        onCurrentIndexChanged: {
                            if (root.alarmListModel) {
                                root.alarmListModel.setSeverityFilter(currentIndex - 1);
                                root.clearSelection();
                            }
                        }
                    }
                    ShadcnInput {
                        Layout.preferredWidth: 180
                        placeholderText: "筛选设备 ID"
                        onTextChanged: {
                            if (root.alarmListModel)
                                root.alarmListModel.setDeviceFilter(text.trim());
                        }
                    }
                    Item { Layout.fillWidth: true }
                    ShadcnLabel {
                        text: "已选 " + root.selectedCount() + " 条"
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                        visible: root.selectedCount() > 0
                    }
                    ShadcnButton {
                        text: "确认选中"
                        size: ShadcnButton.Size.Small
                        variant: ShadcnButton.Variant.Outline
                        enabled: root.selectedCount() > 0
                        onClicked: root.acknowledgeSelected()
                    }
                    ShadcnButton {
                        text: "解决选中"
                        size: ShadcnButton.Size.Small
                        enabled: root.selectedCount() > 0
                        onClicked: root.resolveSelected()
                    }
                }

                Panel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        // ===== 表头 =====
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            ShadcnCheckbox {
                                checked: root.allAlarmsSelected()
                                onToggled: root.toggleSelectAllAlarms(checked)
                            }
                            ShadcnLabel { Layout.preferredWidth: 88;  text: "级别";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            ShadcnLabel { Layout.preferredWidth: 120; text: "设备";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            ShadcnLabel { Layout.preferredWidth: 100; text: "指标";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            ShadcnLabel { Layout.preferredWidth: 70;  text: "当前值";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            ShadcnLabel { Layout.preferredWidth: 140; text: "触发时间"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            ShadcnLabel { text: "状态"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            Item { Layout.fillWidth: true }
                            ShadcnLabel { text: "确认人"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        }
                        ShadcnSeparator { Layout.fillWidth: true }

                        ListView {
                        id: alarmList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.alarmListModel
                        spacing: 2

                        QQC.ScrollBar.vertical: QQC.ScrollBar {
                            active: true
                            policy: QQC.ScrollBar.AsNeeded
                        }

                        delegate: Rectangle {

                            width: alarmList.width
                            height: 52
                            radius: theme.radius
                            color: alarmHover.containsMouse ? theme.muted : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 10

                                ShadcnCheckbox {
                                    checked: !!root.selectedIds[model.id]
                                    enabled: model.status !== 2
                                    onToggled: root.toggleSelection(model.id)
                                }
                                ShadcnBadge {
                                    Layout.preferredWidth: 88
                                    text: model.severityText
                                    variant: model.severity === 0
                                             ? ShadcnBadge.Variant.Secondary
                                             : model.severity === 1
                                               ? ShadcnBadge.Variant.Outline
                                               : ShadcnBadge.Variant.Destructive
                                }
                                ShadcnLabel {
                                    Layout.preferredWidth: 120
                                    text: model.deviceId
                                    size: ShadcnLabel.Size.Small
                                }
                                ShadcnLabel {
                                    Layout.preferredWidth: 100
                                    text: model.metric
                                    size: ShadcnLabel.Size.Small
                                    variant: ShadcnLabel.Variant.Muted
                                }
                                ShadcnLabel {
                                    Layout.preferredWidth: 70
                                    text: Number(model.value).toFixed(1)
                                    size: ShadcnLabel.Size.Small
                                }
                                ShadcnLabel {
                                    Layout.preferredWidth: 140
                                    text: model.triggeredAt
                                    size: ShadcnLabel.Size.Small
                                    variant: ShadcnLabel.Variant.Muted
                                }
                                ShadcnBadge {
                                    text: model.status === 2 ? "已解决"
                                          : model.status === 1 ? "已确认" : "未确认"
                                    variant: model.status === 2
                                             ? ShadcnBadge.Variant.Default
                                             : model.status === 1
                                               ? ShadcnBadge.Variant.Secondary
                                               : ShadcnBadge.Variant.Destructive
                                }
                                Item { Layout.fillWidth: true }
                                ShadcnLabel {
                                    text: model.acknowledgedByName || "-"
                                    size: ShadcnLabel.Size.Small
                                    variant: ShadcnLabel.Variant.Muted
                                }
                            }

                            MouseArea {
                                id: alarmHover
                                anchors.fill: parent
                                hoverEnabled: true
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
                                text: "暂无告警记录"
                                variant: ShadcnLabel.Variant.Muted
                            }
                        }
                        }
                    }
                }
            }

            // ================= Tab 1: 告警规则 =================
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

                    // 批量操作（选中后出现）
                    ShadcnLabel {
                        text: "已选 " + root.selectedCount() + " 条"
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                        visible: root.selectedCount() > 0
                    }
                    ShadcnButton {
                        text: "批量启用"
                        size: ShadcnButton.Size.Small
                        variant: ShadcnButton.Variant.Outline
                        visible: root.selectedCount() > 0
                        onClicked: root.setSelectedRulesEnabled(true)
                    }
                    ShadcnButton {
                        text: "批量禁用"
                        size: ShadcnButton.Size.Small
                        variant: ShadcnButton.Variant.Outline
                        visible: root.selectedCount() > 0
                        onClicked: root.setSelectedRulesEnabled(false)
                    }
                    ShadcnButton {
                        text: "批量删除"
                        size: ShadcnButton.Size.Small
                        variant: ShadcnButton.Variant.Destructive
                        visible: root.selectedCount() > 0
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

                        // ===== 表头（支持全选 + 批量启用/禁用/删除）=====
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            ShadcnCheckbox {
                                checked: root.allRulesSelected()
                                onToggled: root.toggleSelectAllRules(checked)
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

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 10

                                ShadcnCheckbox {
                                    checked: !!root.selectedIds[model.id]
                                    onToggled: root.toggleSelection(model.id)
                                }
                                ShadcnLabel {
                                    Layout.preferredWidth: 130
                                    text: model.deviceId === "*" ? "所有设备" : model.deviceId
                                    size: ShadcnLabel.Size.Small
                                }
                                ShadcnLabel {
                                    Layout.preferredWidth: 100
                                    text: model.metric
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
                                            "deviceId": model.deviceId,
                                            "metric": model.metric,
                                            "threshold": model.threshold,
                                            "opIndex": [">", "<", "==", ">=", "<="].indexOf(model.op),
                                            "severityIndex": model.severity === "INFO" ? 0
                                                           : model.severity === "WARNING" ? 1 : 2
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

                            MouseArea {
                                id: ruleHover
                                anchors.fill: parent
                                hoverEnabled: true
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

    // ===== 添加 / 编辑规则 =====
    ShadcnDialog {
        id: addRuleDialog

        property var editingRule: null

        modal: true

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
                // 靠右 + 垂直居中由组件自身保证：Row 右锚定，footerSlot 高度贴合按钮、上下各留 _pad
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

    // ===== 删除确认 =====
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
