import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import "../components"

Rectangle {
    id: root

    property var currentDevice: null
    property bool isDark: true

    function addDataPoint(metric, value, timestamp) {
        if (currentDevice) {
            chart.addDataPoint(metric, value, timestamp);
        }
    }
    function showDevice(deviceId) {
        for (var i = 0; i < deviceModel.rowCount(); i++) {
            var device = deviceModel.deviceAt(i);
            if (device.id === deviceId) {
                currentDevice = device;
                return;
            }
        }
    }

    color: isDark ? "#1e1e2e" : "#f5f5f5"

    // Reusable button style component
    Component {
        id: outlineButtonStyle

        Rectangle {
            property bool hovered: false
            property bool pressed: false

            border.color: root.isDark ? "#45475a" : "#cbd5e1"
            border.width: 1
            color: pressed ? (root.isDark ? "#45475a" : "#d1d5db") : hovered ? (root.isDark ? "#3b3b4f" : "#e8edf5") : "transparent"
            radius: 8

            Behavior on color {
                ColorAnimation {
                    duration: 120
                }
            }
        }
    }
    ScrollView {
        id: scrollView

        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        padding: 16

        ScrollBar.vertical: ScrollBar {
            parent: scrollView
            policy: ScrollBar.AsNeeded
            width: 6
            x: scrollView.width - width - 4

            background: Rectangle {
                color: "transparent"
            }
            contentItem: Rectangle {
                color: parent.parent.pressed ? (root.isDark ? "rgba(255,255,255,0.5)" : "rgba(0,0,0,0.4)") : parent.parent.hovered ? (root.isDark ? "rgba(255,255,255,0.35)" : "rgba(0,0,0,0.3)") : (root.isDark ? "rgba(255,255,255,0.15)" : "rgba(0,0,0,0.15)")
                implicitWidth: 6
                radius: 3

                Behavior on color {
                    ColorAnimation {
                        duration: 200
                    }
                }
            }
        }

        ColumnLayout {
            spacing: 16
            width: parent.width

            // ===== Header =====
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    Material.foreground: root.isDark ? "#a6adc8" : "#666666"
                    flat: true
                    font.pixelSize: 13
                    text: "← 返回"

                    onClicked: {
                        currentDevice = null;
                        stackView.currentIndex = 0;
                        sidebar.currentIndex = 0;
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                        font.bold: true
                        font.pixelSize: 18
                        text: currentDevice ? currentDevice.id : ""
                    }
                    Label {
                        color: root.isDark ? "#a6adc8" : "#666666"
                        font.pixelSize: 12
                        text: currentDevice ? currentDevice.name + " · " + currentDevice.group : ""
                    }
                }

                // Status
                RowLayout {
                    spacing: 6

                    StatusIndicator {
                        height: 10
                        status: currentDevice ? currentDevice.status : 0
                        width: 10
                    }
                    Label {
                        color: currentDevice ? (currentDevice.status === 0 ? "#9E9E9E" : currentDevice.status === 1 ? "#4CAF50" : currentDevice.status === 2 ? "#FF5722" : "#FFC107") : "#9E9E9E"
                        font.pixelSize: 13
                        text: currentDevice ? (currentDevice.status === 0 ? "离线" : currentDevice.status === 1 ? "在线" : currentDevice.status === 2 ? "告警" : "维护") : ""
                    }
                }

                // Separator
                Rectangle {
                    Layout.preferredHeight: 20
                    Layout.preferredWidth: 1
                    color: root.isDark ? "#45475a" : "#e0e0e0"
                }

                // Action buttons - simple outlined
                Repeater {
                    model: ["编辑", "删除", "重启"]

                    Button {
                        Material.foreground: root.isDark ? "#a6adc8" : "#666666"
                        flat: true
                        font.pixelSize: 12
                        text: modelData

                        background: Rectangle {
                            border.color: root.isDark ? "#45475a" : "#cbd5e1"
                            border.width: 1
                            color: parent.hovered ? (root.isDark ? "#3b3b4f" : "#e8edf5") : "transparent"
                            implicitHeight: 32
                            radius: 6

                            Behavior on color {
                                ColorAnimation {
                                    duration: 120
                                }
                            }
                        }

                        onClicked: console.log(modelData)
                    }
                }
            }

            // ===== Metrics =====
            Rectangle {
                Layout.fillWidth: true
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1
                color: root.isDark ? "#313244" : "#ffffff"
                radius: 12

                GridLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    columnSpacing: 16
                    columns: 4
                    rowSpacing: 12

                    MetricCard {
                        Layout.fillWidth: true
                        icon: "🌡️"
                        isDark: root.isDark
                        title: "温度"
                        value: currentDevice ? currentDevice.temperature.toFixed(1) + "°C" : "--"
                    }
                    MetricCard {
                        Layout.fillWidth: true
                        icon: "💧"
                        isDark: root.isDark
                        title: "湿度"
                        value: currentDevice ? currentDevice.humidity.toFixed(0) + "%" : "--"
                    }
                    MetricCard {
                        Layout.fillWidth: true
                        icon: "🔋"
                        isDark: root.isDark
                        title: "电量"
                        value: currentDevice ? currentDevice.battery.toFixed(0) + "%" : "--"
                    }
                    MetricCard {
                        Layout.fillWidth: true
                        icon: "📊"
                        isDark: root.isDark
                        title: "上报"
                        value: currentDevice ? currentDevice.reportCount.toString() : "--"
                    }
                }
            }

            // ===== Device Shadow =====
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1
                color: root.isDark ? "#313244" : "#ffffff"
                radius: 12

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                            font.bold: true
                            font.pixelSize: 15
                            text: "Device Shadow"
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Button {
                            Material.foreground: "#89b4fa"
                            flat: true
                            font.pixelSize: 12
                            text: "修改期望值"

                            background: Rectangle {
                                color: parent.hovered ? (root.isDark ? "#3b3b4f" : "#dbeafe") : "transparent"
                                implicitHeight: 28
                                radius: 6

                                Behavior on color {
                                    ColorAnimation {
                                        duration: 120
                                    }
                                }
                            }

                            onClicked: desiredDialog.open()
                        }
                    }
                    RowLayout {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        spacing: 12

                        ColumnLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            spacing: 6

                            Label {
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.bold: true
                                font.pixelSize: 11
                                text: "Desired"
                            }
                            Rectangle {
                                Layout.fillHeight: true
                                Layout.fillWidth: true
                                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                                border.width: 1
                                color: root.isDark ? "#1e1e2e" : "#f8f9fa"
                                radius: 8

                                ScrollView {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    clip: true

                                    Label {
                                        color: root.isDark ? "#a6e3a1" : "#2e7d32"
                                        font.family: "Monaco"
                                        font.pixelSize: 12
                                        text: '{\n  "temperature": 25,\n  "fan_speed": "high"\n}'
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            spacing: 6

                            Label {
                                color: root.isDark ? "#a6adc8" : "#666666"
                                font.bold: true
                                font.pixelSize: 11
                                text: "Reported"
                            }
                            Rectangle {
                                Layout.fillHeight: true
                                Layout.fillWidth: true
                                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                                border.width: 1
                                color: root.isDark ? "#1e1e2e" : "#f8f9fa"
                                radius: 8

                                ScrollView {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    clip: true

                                    Label {
                                        color: root.isDark ? "#89b4fa" : "#1565c0"
                                        font.family: "Monaco"
                                        font.pixelSize: 12
                                        text: '{\n  "temperature": 23.5,\n  "humidity": 65,\n  "fan_speed": "low",\n  "battery": 85,\n  "online": true\n}'
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ===== Chart =====
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 280
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1
                color: root.isDark ? "#313244" : "#ffffff"
                radius: 12

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Label {
                        color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                        font.bold: true
                        font.pixelSize: 15
                        text: "实时监控"
                    }
                    RealtimeChart {
                        id: chart

                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        isDark: root.isDark
                    }
                }
            }

            // ===== Remote control =====
            Rectangle {
                Layout.fillWidth: true
                border.color: root.isDark ? "#45475a" : "#e0e0e0"
                border.width: 1
                color: root.isDark ? "#313244" : "#ffffff"
                radius: 12

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    Label {
                        color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                        font.bold: true
                        font.pixelSize: 15
                        text: "远程控制"
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Repeater {
                            model: ListModel {
                                ListElement {
                                    cmd: "fan_on"
                                    danger: false
                                    name: "开启风扇"
                                }
                                ListElement {
                                    cmd: "fan_off"
                                    danger: false
                                    name: "关闭风扇"
                                }
                                ListElement {
                                    cmd: "set_temp"
                                    danger: false
                                    name: "设置温度"
                                }
                                ListElement {
                                    cmd: "reboot"
                                    danger: true
                                    name: "重启设备"
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                text: model.name

                                background: Rectangle {
                                    border.color: model.danger ? "#f38ba8" : (root.isDark ? "#45475a" : "#cbd5e1")
                                    border.width: 1
                                    color: parent.pressed ? (root.isDark ? "#45475a" : "#d1d5db") : parent.hovered ? (root.isDark ? "#3b3b4f" : "#e8edf5") : (root.isDark ? "#1e1e2e" : "#f8f9fa")
                                    implicitHeight: 40
                                    radius: 8

                                    Behavior on color {
                                        ColorAnimation {
                                            duration: 120
                                        }
                                    }
                                }
                                contentItem: Label {
                                    color: model.danger ? "#f38ba8" : (root.isDark ? "#cdd6f4" : "#1e1e2e")
                                    font.bold: model.danger
                                    font.pixelSize: 13
                                    horizontalAlignment: Text.AlignHCenter
                                    text: parent.text
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: console.log("Command:", model.cmd)
                            }
                        }
                    }
                }
            }
        }
    }

    // Edit desired state dialog
    Dialog {
        id: desiredDialog
        title: "修改期望值"
        anchors.centerIn: parent
        width: 400
        modal: true
        closePolicy: Dialog.CloseOnEscape

        contentItem: ColumnLayout {
            spacing: 12

            Label {
                text: "Desired JSON:"
                font.pixelSize: 12
                color: root.isDark ? "#a6adc8" : "#666666"
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                radius: 8
                color: root.isDark ? "#1e1e2e" : "#f8f9fa"
                border.color: root.isDark ? "#45475a" : "#cbd5e1"
                border.width: 1

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 4
                    clip: true

                    TextArea {
                        id: desiredEditor
                        font.family: "Monaco"
                        font.pixelSize: 12
                        color: root.isDark ? "#a6e3a1" : "#2e7d32"
                        wrapMode: TextArea.Wrap
                        text: '{\n  "temperature": 25,\n  "fan_speed": "high"\n}'
                        background: Rectangle { color: "transparent" }
                    }
                }
            }
        }

        footer: RowLayout {
            spacing: 8
            Item { Layout.fillWidth: true }
            Button {
                text: "取消"
                flat: true
                Material.foreground: root.isDark ? "#a6adc8" : "#666666"
                onClicked: desiredDialog.close()
            }
            Button {
                text: "保存"
                flat: true
                Material.foreground: "#89b4fa"
                onClicked: {
                    console.log("Save desired:", desiredEditor.text)
                    desiredDialog.close()
                }
            }
        }
    }

}