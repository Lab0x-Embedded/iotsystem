import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

Dialog {
    id: dialog

    property string deviceId: ""
    property bool isDark: true

    closePolicy: Popup.NoAutoClose
    modal: true
    padding: 24
    standardButtons: Dialog.Ok | Dialog.Cancel
    title: "添加告警规则"
    width: 560

    onAccepted: {
        var dev = deviceField.text.trim();
        var met = metricField.text.trim();
        if (dev !== "" && met !== "") {
            dataManager.addAlarmRule(dev, met, opCombo.currentIndex, parseFloat(thresholdField.text), severityCombo.currentIndex);
        }
    }
    onRejected: dialog.close()

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // 设备 ID
        Label {
            color: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            text: "设备 ID"
        }
        TextField {
            id: deviceField

            Layout.fillWidth: true
            Material.background: dialog.isDark ? "#313244" : "#ffffff"
            Material.foreground: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            placeholderText: "留空表示所有设备"
            selectByMouse: true
            text: dialog.deviceId
        }

        // 指标
        Label {
            color: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            text: "指标 *"
        }
        TextField {
            id: metricField

            Layout.fillWidth: true
            Material.background: dialog.isDark ? "#313244" : "#ffffff"
            Material.foreground: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            placeholderText: "如 temperature"
            selectByMouse: true
        }

        // 比较符
        Label {
            color: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            text: "条件"
        }
        ComboBox {
            id: opCombo

            Layout.fillWidth: true
            Material.foreground: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            model: [">", ">=", "<", "<=", "=="]
        }

        // 阈值
        Label {
            color: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            text: "阈值"
        }
        TextField {
            id: thresholdField

            Layout.fillWidth: true
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

        // 严重级别
        Label {
            color: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            text: "严重级别"
        }
        ComboBox {
            id: severityCombo

            Layout.fillWidth: true
            Material.foreground: dialog.isDark ? "#cdd6f4" : "#1e1e2e"
            currentIndex: 1
            model: ["INFO", "WARNING", "CRITICAL"]
        }
    }
}
