import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

Dialog {
    id: dialog

    property string deviceId: ""
    property var editingRule: null
    property bool isDark: true

    closePolicy: Popup.NoAutoClose
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    title: editingRule ? "编辑告警规则" : "添加告警规则"
    width: 460

    onAccepted: {
        var dev = deviceField.text.trim();
        var met = metricField.text.trim();
        if (met === "")
            return;
        if (editingRule) {
            dataManager.editRule(editingRule.id, dev, met, opCombo.currentIndex, parseFloat(thresholdField.text), severityCombo.currentIndex);
        } else {
            dataManager.addAlarmRule(dev, met, opCombo.currentIndex, parseFloat(thresholdField.text), severityCombo.currentIndex);
        }
        editingRule = null;
    }
    onRejected: {
        editingRule = null;
        dialog.close();
    }
    onVisibleChanged: {
        if (visible) {
            if (editingRule) {
                deviceField.text = editingRule.deviceId || "";
                metricField.text = editingRule.metric || "";
                thresholdField.text = String(editingRule.threshold || 0);
                opCombo.currentIndex = editingRule.opIndex || 0;
                severityCombo.currentIndex = editingRule.severityIndex !== undefined ? editingRule.severityIndex : 1;
            } else {
                deviceField.text = "";
                metricField.text = "";
                thresholdField.text = "0";
                opCombo.currentIndex = 0;
                severityCombo.currentIndex = 1;
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            color: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            text: "设备 ID"
        }
        TextField {
            id: deviceField

            Layout.fillWidth: true
            Layout.preferredHeight: 36
            Material.background: dialog.isDark ? "#313244" : "#ffffff"
            Material.foreground: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            placeholderText: "留空表示所有设备"
            selectByMouse: true
        }
        Label {
            color: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            text: "指标 *"
        }
        TextField {
            id: metricField

            Layout.fillWidth: true
            Layout.preferredHeight: 36
            Material.background: dialog.isDark ? "#313244" : "#ffffff"
            Material.foreground: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            placeholderText: "如 temperature"
            selectByMouse: true
        }
        Label {
            color: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            text: "条件"
        }
        ComboBox {
            id: opCombo

            Layout.fillWidth: true
            Layout.preferredHeight: 36
            Material.foreground: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            model: [">", "<", "==", ">=", "<="]
        }
        Label {
            color: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            text: "阈值"
        }
        TextField {
            id: thresholdField

            Layout.fillWidth: true
            Layout.preferredHeight: 36
            Material.background: dialog.isDark ? "#313244" : "#ffffff"
            Material.foreground: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            placeholderText: "如 32.0"
            selectByMouse: true
            text: "0"

            validator: DoubleValidator {
                bottom: -9999
                decimals: 2
                top: 9999
            }
        }
        Label {
            color: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            text: "严重级别"
        }
        ComboBox {
            id: severityCombo

            Layout.fillWidth: true
            Layout.preferredHeight: 36
            Material.foreground: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            model: ["INFO", "WARNING", "CRITICAL"]
        }
    }
}
