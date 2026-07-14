import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15
import QtQuick.Controls.Material 2.15

import "../components"

Rectangle {
    id: root

    property var alarmListModel: null
    property bool isDark: true
    property var ruleListModel: null
    property var selectedRows: ({})

    function acknowledgeSelected() {
        var indices = Object.keys(selectedRows).map(Number).sort(function (a, b) { return b - a; });
        for (var i = 0; i < indices.length; i++) alarmListModel.acknowledge(indices[i]);
        clearSelection();
    }
    function clearSelection() { selectedRows = ({}); }
    function resolveSelected() {
        var indices = Object.keys(selectedRows).map(Number).sort(function (a, b) { return b - a; });
        for (var i = 0; i < indices.length; i++) alarmListModel.resolve(indices[i]);
        clearSelection();
    }
    function selectedCount() { return Object.keys(selectedRows).length; }
    function toggleSelection(idx) {
        var rows = Object.assign({}, selectedRows);
        if (rows[idx]) delete rows[idx]; else rows[idx] = true;
        selectedRows = rows;
    }

    color: isDark ? "#1e1e2e" : "#f5f5f5"

    onVisibleChanged: {
        if (visible && alarmListModel) alarmListModel.setDeviceFilter("");
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // ====== Header ======
        RowLayout {
            Layout.fillWidth: true

            Label {
                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                font.bold: true
                font.pixelSize: 20
                text: "告警中心"
            }
            Item { Layout.fillWidth: true }
            Label { color: "#f38ba8"; font.pixelSize: 13; text: "未确认: " + (alarmListModel ? alarmListModel.unacknowledgedCount : 0) }
            Label { color: "#fab387"; font.pixelSize: 13; text: "未解决: " + (alarmListModel ? alarmListModel.unresolvedCount : 0) }
        }

        // ====== Tab bar ======
        TabBar {
            id: tabBar
            Layout.fillWidth: true
            background: Rectangle { color: "transparent" }

            TabButton { Material.foreground: root.isDark ? "#cdd6f4" : "#1e1e2e"; text: "告警记录" }
            TabButton { Material.foreground: root.isDark ? "#cdd6f4" : "#1e1e2e"; text: "告警规则" }
        }

        // ====== Content area ======
        StackLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            currentIndex: tabBar.currentIndex

            // ====== Tab 1: 告警记录 ======
            Rectangle {
                Layout.fillHeight: true
                Layout.fillWidth: true
                color: "transparent"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    // Filter row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Label { color: root.isDark ? "#cdd6f4" : "#1e1e2e"; font.pixelSize: 13; text: "严重级别:" }
                        ComboBox {
                            id: severityFilter
                            Layout.preferredHeight: 36
                            Layout.preferredWidth: 140
                            model: ["全部", "INFO", "WARNING", "CRITICAL"]
                            onCurrentIndexChanged: {
                                if (alarmListModel) { alarmListModel.setSeverityFilter(currentIndex - 1); root.clearSelection(); }
                            }
                        }
                        Label { color: root.isDark ? "#cdd6f4" : "#1e1e2e"; font.pixelSize: 13; text: "设备ID:" }
                        TextField {
                            id: deviceIdFilter
                            Layout.preferredHeight: 36
                            Layout.preferredWidth: 160
                            placeholderText: "输入设备ID筛选"
                            onTextChanged: { if (alarmListModel) alarmListModel.setDeviceFilter(text.trim()); }
                        }
                        Item { Layout.fillWidth: true }
                        Label { color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 12; text: "已选 " + root.selectedCount() + " 条"; visible: root.selectedCount() > 0 }
                        RoundedButton { Material.background: "#3874F7"; Material.foreground: "white"; enabled: root.selectedCount() > 0; text: "确认选中"; onClicked: root.acknowledgeSelected() }
                        RoundedButton { Material.background: "#4CAF50"; Material.foreground: "white"; enabled: root.selectedCount() > 0; text: "解决选中"; onClicked: root.resolveSelected() }
                    }

                    // Alarm table
                    Rectangle {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        border.color: root.isDark ? "#45475a" : "#e0e0e0"
                        border.width: 1
                        color: root.isDark ? "#313244" : "#ffffff"
                        radius: 12

                        ListView {
                            id: alarmList
                            anchors.fill: parent
                            anchors.margins: 12
                            clip: true
                            model: alarmListModel

                            header: Rectangle {
                                width: ListView.view ? ListView.view.width : 0
                                height: 36
                                color: root.isDark ? "#1e1e2e" : "#f8f9fa"
                                radius: 6

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 16; anchors.rightMargin: 16
                                    spacing: 12

                                    CheckBox {
                                        Layout.preferredWidth: 32
                                        Layout.preferredHeight: 32
                                        onToggled: {
                                            if (checked) {
                                                var rows = {};
                                                for (var i = 0; i < alarmListModel.rowCount(); i++) {
                                                    if (alarmListModel.isRowSelectable(i))
                                                        rows[i] = true;
                                                }
                                                selectedRows = rows;
                                            } else {
                                                root.clearSelection();
                                            }
                                        }
                                    }
                                    Label { Layout.preferredWidth: 88; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 11; text: "级别" }
                                    Label { Layout.preferredWidth: 140; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 11; text: "设备ID" }
                                    Label { Layout.preferredWidth: 110; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 11; text: "指标" }
                                    Label { Layout.preferredWidth: 90; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 11; text: "当前值" }
                                    Label { Layout.preferredWidth: 150; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 11; text: "触发时间" }
                                    Label { Layout.preferredWidth: 60; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 11; text: "状态" }
                                }
                            }

                            delegate: Rectangle {
                                color: index % 2 === 0 ? (root.isDark ? "#313244" : "#ffffff") : (root.isDark ? "#2a2a3c" : "#f8f9fa")
                                height: 48
                                width: alarmList.width

                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16; spacing: 12

                                    CheckBox {
                                        Layout.preferredWidth: 32; Layout.preferredHeight: 32
                                        checked: model.status !== 2 && !!selectedRows[index]
                                        enabled: model.status !== 2; opacity: enabled ? 1.0 : 0.3
                                        onToggled: root.toggleSelection(index)
                                    }
                                    Rectangle {
                                        Layout.preferredWidth: 88; Layout.preferredHeight: 24
                                        color: model.severity === 0 ? "#2196F3" : model.severity === 1 ? "#FFC107" : "#FF5722"; radius: 4
                                        Label { anchors.centerIn: parent; color: "white"; font.bold: true; font.pixelSize: 10; text: model.severityText }
                                    }
                                    SelectableLabel { Layout.preferredWidth: 140; text: model.deviceId; textColor: root.isDark ? "#cdd6f4" : "#1e1e2e" }
                                    SelectableLabel { Layout.preferredWidth: 110; text: model.metric; textColor: root.isDark ? "#cdd6f4" : "#1e1e2e" }
                                    Label { Layout.preferredWidth: 90; color: root.isDark ? "#cdd6f4" : "#1e1e2e"; font.pixelSize: 12; text: Number(model.value).toFixed(1) }
                                    Label { Layout.preferredWidth: 150; color: root.isDark ? "#a6adc8" : "#666666"; font.pixelSize: 12; text: model.triggeredAt }
                                    Label { Layout.preferredWidth: 60; color: model.acknowledged ? "#4CAF50" : "#f38ba8"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; text: model.acknowledged ? "✓" : "●" }
                                }
                            }
                        }
                    }
                }
            }

            // ====== Tab 2: 告警规则 ======
            Rectangle {
                Layout.fillHeight: true
                Layout.fillWidth: true
                color: "transparent"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Item { Layout.fillWidth: true }
                        RoundedButton {
                            Material.background: "#4CAF50"; Material.foreground: "#1e1e2e"; text: "添加规则"
                            onClicked: { addRuleDialog.editingRule = null; addRuleDialog.open(); }
                        }
                    }

                    Rectangle {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        border.color: root.isDark ? "#45475a" : "#e0e0e0"
                        border.width: 1
                        color: root.isDark ? "#313244" : "#ffffff"
                        radius: 12

                        ListView {
                            id: ruleList
                            anchors.fill: parent
                            anchors.margins: 12
                            clip: true
                            model: ruleListModel

                            header: Rectangle {
                                width: ListView.view ? ListView.view.width : 0
                                height: 36
                                color: root.isDark ? "#1e1e2e" : "#f8f9fa"
                                radius: 6

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 16; anchors.rightMargin: 16
                                    spacing: 12

                                    Label { Layout.preferredWidth: 180; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 11; text: "设备" }
                                    Label { Layout.preferredWidth: 120; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 11; text: "指标" }
                                    Label { Layout.preferredWidth: 120; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 11; text: "条件" }
                                    Label { Layout.preferredWidth: 80; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 11; text: "阈值" }
                                    Label { Layout.preferredWidth: 120; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 11; text: "级别" }
                                    Label { Layout.preferredWidth: 120; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 11; text: "启用" }
                                    Label { Layout.preferredWidth: 80; color: root.isDark ? "#a6adc8" : "#666666"; font.bold: true; font.pixelSize: 11; text: "操作" }
                                }
                            }

                            delegate: Rectangle {
                                color: index % 2 === 0 ? (root.isDark ? "#313244" : "#ffffff") : (root.isDark ? "#2a2a3c" : "#f8f9fa")
                                height: 48
                                width: ruleList.width

                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16; spacing: 12

                                    Label { Layout.preferredWidth: 180; color: root.isDark ? "#cdd6f4" : "#1e1e2e"; elide: Text.ElideRight; font.pixelSize: 12; text: model.deviceId }
                                    Label { Layout.preferredWidth: 120; color: root.isDark ? "#cdd6f4" : "#1e1e2e"; font.pixelSize: 12; text: model.metric }
                                    Label { Layout.preferredWidth: 120; color: root.isDark ? "#3874F7" : "#4a6fa5"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; text: model.op }
                                    Label { Layout.preferredWidth: 80; color: root.isDark ? "#cdd6f4" : "#1e1e2e"; font.pixelSize: 12; horizontalAlignment: Text.AlignRight; text: model.threshold }
                                    Label { Layout.preferredWidth: 120; color: model.severity === "严重" ? "#f38ba8" : model.severity === "警告" ? "#fab387" : "#3874F7"; font.bold: true; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; text: model.severity }
                                    Switch { Layout.preferredWidth: 120; checked: model.enabled === "启用"; scale: 0.5; onToggled: dataManager.toggleRule(model.id, checked) }
                                    RowLayout {
                                        spacing: 4
                                        Button {
                                            Layout.preferredWidth: 50; flat: true; font.pixelSize: 12; text: "编辑"
                                            Material.foreground: root.isDark ? "#3874F7" : "#4a6fa5"
                                            onClicked: {
                                                var opMap = {">": 0, "<": 1, "==": 2, ">=": 3, "<=": 4};
                                                var sevMap = {"信息": 0, "警告": 1, "严重": 2};
                                                addRuleDialog.editingRule = { id: model.id, deviceId: model.deviceId === "*" ? "" : model.deviceId, metric: model.metric, opIndex: opMap[model.op] !== undefined ? opMap[model.op] : 0, threshold: parseFloat(model.threshold), severityIndex: sevMap[model.severity] };
                                                addRuleDialog.open();
                                            }
                                        }
                                        Button {
                                            Layout.preferredWidth: 50; flat: true; font.pixelSize: 12; text: "删除"
                                            Material.foreground: "#f38ba8"
                                            onClicked: {
                                                deleteConfirmDialog.ruleId = model.id;
                                                deleteConfirmDialog.ruleDesc = (model.deviceId === "*" ? "所有设备" : model.deviceId) + " " + model.metric + " " + model.op + " " + model.threshold;
                                                deleteConfirmDialog.open();
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
    }

    // ====== Dialogs (outside StackLayout) ======
    AddAlarmRuleDialog {
        id: addRuleDialog
        isDark: root.isDark
        width: 460
        x: (root.width - width) / 2
        y: (root.height - height) / 2
    }

    Dialog {
        id: deleteConfirmDialog
        property string ruleDesc: ""
        property int ruleId: 0

        closePolicy: Popup.NoAutoClose
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        title: "确认删除"
        x: (root.width - width) / 2
        y: (root.height - height) / 2

        onAccepted: dataManager.deleteRule(deleteConfirmDialog.ruleId)
        onRejected: close()

        Label {
            color: root.isDark ? "#cdd6f4" : "#1e1e2e"
            text: "确定要删除规则【" + deleteConfirmDialog.ruleDesc + "】吗？此操作不可撤销。"
            width: 300
            wrapMode: Text.Wrap
        }
    }
}
