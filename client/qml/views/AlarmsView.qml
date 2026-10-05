import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtShadcn
import "../components"

// 告警中心: 记录 + 规则 双 Tab
Rectangle {
    id: root

    property var alarmListModel: null   // alarmModel
    property var ruleListModel: null    // ruleModel
    property var selectedRows: ({})

    QtShadcnTheme { id: theme }
    color: theme.background

    function selectedCount() {
        var c = 0;
        for (var k in selectedRows) if (selectedRows[k]) c++;
        return c;
    }
    function clearSelection() { selectedRows = {}; }
    function toggleSelection(index) {
        var rows = selectedRows;
        if (rows[index]) delete rows[index]; else rows[index] = true;
        selectedRows = rows;
    }
    function acknowledgeSelected() {
        for (var i = 0; i < alarmListModel.rowCount(); i++) {
            if (selectedRows[i] && alarmListModel.isRowSelectable(i))
                alarmListModel.acknowledge(i);
        }
        clearSelection();
    }
    function resolveSelected() {
        for (var i = 0; i < alarmListModel.rowCount(); i++) {
            if (selectedRows[i] && alarmListModel.isRowSelectable(i))
                alarmListModel.resolve(i);
        }
        clearSelection();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        // ===== Header =====
        RowLayout {
            Layout.fillWidth: true

            ShadcnLabel { text: "告警中心"; size: ShadcnLabel.Size.Large }
            Item { Layout.fillWidth: true }

            ShadcnBadge {
                text: "未确认 " + (alarmListModel ? alarmListModel.unacknowledgedCount : 0)
                variant: ShadcnBadge.Variant.Destructive
            }
            ShadcnBadge {
                text: "未解决 " + (alarmListModel ? alarmListModel.unresolvedCount : 0)
                variant: ShadcnBadge.Variant.Secondary
            }
        }

        // ===== Tabs =====
        ShadcnTabsList {
            id: alarmTabs
            ShadcnTabsTrigger { text: "告警记录" }
            ShadcnTabsTrigger { text: "告警规则" }
        }

        StackLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            currentIndex: alarmTabs.currentIndex

            // ===== Tab 0: 告警记录 =====
            ColumnLayout {
                spacing: 12

                // Filter row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ShadcnSelect {
                        id: severityFilter
                        width: 130
                        model: ["全部", "INFO", "WARNING", "CRITICAL"]
                        onCurrentIndexChanged: {
                            if (alarmListModel) {
                                alarmListModel.setSeverityFilter(currentIndex - 1);
                                root.clearSelection();
                            }
                        }
                    }
                    ShadcnInput {
                        id: deviceIdFilter
                        Layout.preferredWidth: 180
                        placeholderText: "筛选设备 ID"
                        onTextChanged: {
                            if (alarmListModel) alarmListModel.setDeviceFilter(text.trim());
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
                        enabled: root.selectedCount() > 0
                        text: "确认选中"
                        size: ShadcnButton.Size.Small
                        variant: ShadcnButton.Variant.Outline
                        onClicked: root.acknowledgeSelected()
                    }
                    ShadcnButton {
                        enabled: root.selectedCount() > 0
                        text: "解决选中"
                        size: ShadcnButton.Size.Small
                        onClicked: root.resolveSelected()
                    }
                }

                // Alarm list
                ShadcnCard {
                    Layout.fillHeight: true
                    Layout.fillWidth: true

                    ShadcnCardContent {
                        width: parent.width
                        height: parent.height
                        implicitHeight: 0
                        ListView {
                            id: alarmList
                            width: parent.width
                            height: parent.height
                            clip: true
                            model: alarmListModel
                            spacing: 4

                            QQC.ScrollBar.vertical: QQC.ScrollBar { active: true; policy: QQC.ScrollBar.AsNeeded }

                            delegate: Rectangle {
                                width: alarmList.width
                                height: 52
                                radius: theme.radius
                                color: rowMouse.containsMouse ? theme.muted : "transparent"

                                required property int index
                                required property int severity
                                required property string severityText
                                required property string deviceId
                                required property string metric
                                required property double value
                                required property string triggeredAt
                                required property int status

                                RowLayout {
                                    width: parent.width
                            height: parent.height
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 10

                                    ShadcnCheckbox {
                                        checked: model.status !== 2 && !!root.selectedRows[index]
                                        enabled: model.status !== 2
                                        onToggled: root.toggleSelection(index)
                                    }
                                    ShadcnBadge {
                                        Layout.preferredWidth: 80
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
                                        Layout.preferredWidth: 90
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
                                        Layout.preferredWidth: 130
                                        text: model.triggeredAt
                                        size: ShadcnLabel.Size.Small
                                        variant: ShadcnLabel.Variant.Muted
                                    }
                                    ShadcnBadge {
                                        text: model.status === 2 ? "已解决" : model.status === 1 ? "已确认" : "未确认"
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
                                    id: rowMouse
                                    width: parent.width
                            height: parent.height
                                    hoverEnabled: true
                                }
                            }

                            // 空状态
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

            // ===== Tab 1: 告警规则 =====
            ColumnLayout {
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ShadcnInput {
                        id: ruleDeviceFilter
                        Layout.preferredWidth: 180
                        placeholderText: "筛选设备 ID"
                        onTextChanged: {
                            if (ruleListModel) ruleListModel.setDeviceFilter(text.trim());
                        }
                    }
                    Item { Layout.fillWidth: true }
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

                ShadcnCard {
                    Layout.fillHeight: true
                    Layout.fillWidth: true

                    ShadcnCardContent {
                        width: parent.width
                        height: parent.height
                        implicitHeight: 0
                        ListView {
                            id: ruleList
                            width: parent.width
                            height: parent.height
                            clip: true
                            model: ruleListModel
                            spacing: 4

                            QQC.ScrollBar.vertical: QQC.ScrollBar { active: true; policy: QQC.ScrollBar.AsNeeded }

                            delegate: Rectangle {
                                width: ruleList.width
                                height: 48
                                radius: theme.radius
                                color: ruleMouse.containsMouse ? theme.muted : "transparent"

                                required property int index
                                required property string deviceId
                                required property string metric
                                required property string op
                                required property double threshold
                                required property string severity
                                required property string enabled
                                required property int id

                                RowLayout {
                                    width: parent.width
                            height: parent.height
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 10

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
                                        Layout.preferredWidth: 70
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
                                        onToggled: dataManager.toggleRule(model.id, checked)
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
                                                "severityIndex": model.severity === "INFO" ? 0 : model.severity === "WARNING" ? 1 : 2
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
                                            deleteConfirmDialog.ruleDesc = (model.deviceId === "*" ? "所有设备" : model.deviceId)
                                                + " " + model.metric + " " + model.op + " " + model.threshold;
                                            deleteConfirmDialog.open();
                                        }
                                    }
                                }

                                MouseArea {
                                    id: ruleMouse
                                    width: parent.width
                            height: parent.height
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

    // ===== 添加/编辑规则对话框 =====
    ShadcnDialog {
        id: addRuleDialog

        property var editingRule: null

        modal: true
        width: 420

        ShadcnDialogContent {
            ShadcnDialogHeader {
                ShadcnDialogTitle { text: addRuleDialog.editingRule ? "编辑告警规则" : "添加告警规则" }
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
                    if (met === "") return;
                    var opIdx = ruleOpCombo.currentIndex;
                    var thr = parseFloat(ruleThresholdField.text) || 0;
                    var sevIdx = ruleSeverityCombo.currentIndex;
                    if (addRuleDialog.editingRule) {
                        dataManager.editRule(addRuleDialog.editingRule.id, dev, met, opIdx, thr, sevIdx);
                    } else {
                        dataManager.addAlarmRule(dev, met, opIdx, thr, sevIdx);
                    }
                    addRuleDialog.editingRule = null;
                    addRuleDialog.close();
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
                    dataManager.deleteRule(deleteConfirmDialog.ruleId);
                    deleteConfirmDialog.close();
                }
            }
        }
    }
}
