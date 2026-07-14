import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

import "pages"
import "components"

ApplicationWindow {
    id: root

    property color accentColor: "#3874F7"
    property color bgColor: isDark ? "#1e1e2e" : "#f5f5f5"
    property color cardBg: isDark ? "#313244" : "#ffffff"

    // Theme properties
    property bool isDark: themeManager ? themeManager.isDark : true
    property color sidebarBg: isDark ? "#181825" : "#ffffff"
    property color textColor: isDark ? "#cdd6f4" : "#1e1e2e"

    Material.accent: accentColor
    Material.theme: isDark ? Material.Dark : Material.Light
    height: 800
    minimumHeight: 600
    minimumWidth: 1024
    title: "IoT Device Manager"
    visible: true
    width: 1280

    // Status bar
    footer: ToolBar {
        Material.background: isDark ? "#11111b" : "#e0e0e0"
        height: 32

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12

            // 连接状态指示器
            Rectangle {
                color: dataManager && dataManager.online ? "#4CAF50" : "#FF5722"
                height: 8
                radius: 4
                width: 8
            }
            Label {
                color: isDark ? "#a6adc8" : "#666666"
                font.pixelSize: 12
                text: dataManager && dataManager.online ? "已连接: " + (dataManager.serverUrl || "127.0.0.1") : "未连接 (离线模式)"

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor

                    onClicked: loginDialog.open()
                }
            }
            Item {
                Layout.fillWidth: true
            }
            Label {
                id: statusLabel

                color: isDark ? "#a6adc8" : "#666666"
                font.pixelSize: 12
                text: "共 " + (deviceModel ? deviceModel.totalCount : 0) + " 设备  ·  在线 " + (deviceModel ? deviceModel.onlineCount : 0) + "  ·  告警 " + (alarmModel ? alarmModel.activeCount : 0)
            }
        }
    }

    // ===== 启动时自动连接服务器 =====
    Component.onCompleted: {
        dataManager.connectToServer("http://127.0.0.1:8080", "admin", "admin@123");
    }

    // ===== 登录对话框 =====
    Dialog {
        id: loginDialog

        property bool connecting: false
        property string errorMsg: ""

        anchors.centerIn: parent
        closePolicy: Popup.NoAutoClose
        modal: true
        title: "连接服务器"
        width: 480

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 16

            TextField {
                id: serverUrlField

                Layout.fillWidth: true
                Layout.preferredHeight: 36
                placeholderText: "服务器地址"
                text: "http://127.0.0.1:8080"
            }
            TextField {
                id: usernameField

                Layout.fillWidth: true
                Layout.preferredHeight: 36
                placeholderText: "用户名"
                text: "admin"
            }
            TextField {
                id: passwordField

                Layout.fillWidth: true
                Layout.preferredHeight: 36
                echoMode: TextInput.Password
                placeholderText: "密码"
                text: "admin@123"
            }
            Label {
                color: "#FF5722"
                font.pixelSize: 12
                text: loginDialog.errorMsg
                visible: loginDialog.errorMsg !== ""
            }
            RowLayout {
                width: parent.width

                Item {
                    Layout.fillWidth: true
                }
                RoundedButton {
                    Material.background: accentColor
                    Material.foreground: "#F3F6FF"
                    enabled: !loginDialog.connecting
                    text: loginDialog.connecting ? "连接中..." : "连接"

                    onClicked: {
                        loginDialog.connecting = true;
                        loginDialog.errorMsg = "";
                        dataManager.connectToServer(serverUrlField.text, usernameField.text, passwordField.text);
                    }
                }
            }
        }
    }

    // Main layout
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Navigation sidebar
        NavSidebar {
            id: sidebar

            Layout.fillHeight: true
            Layout.preferredWidth: 220
            isDark: root.isDark

            onPageSelected: function (index) {
                stackView.currentIndex = index;
            }
            onThemeToggle: {
                themeManager.toggle();
            }
        }

        // Content area
        StackLayout {
            id: stackView

            Layout.fillHeight: true
            Layout.fillWidth: true
            currentIndex: 0

            onCurrentIndexChanged: {
                if (currentIndex === 0 && dataManager) {
                    dataManager.refreshDevices();
                    dataManager.refreshGroups();
                }
            }

            // History data model for detail page
            ListModel {
                id: historyDataModel
            }

            // 0: 设备总览
            OverviewPage {
                id: overviewPage

                deviceData: deviceModel
                isDark: root.isDark
                overPageManager: dataManager

                onDeviceSelected: function (deviceId) {
                    detailPage.showDevice(deviceId);
                    stackView.currentIndex = 2;  // 跳转到设备详情
                    sidebar.currentIndex = 2;
                }
            }

            // 1: 分组管理
            GroupManagePage {
                id: groupManagePage

                deviceData: deviceModel
                groupData: groupModel
                groupPageManager: dataManager
                isDark: root.isDark

                onShowGroupDetail: function (groupId, groupName) {
                    groupDetailPage.showGroup(groupId, groupName);
                    stackView.currentIndex = 5;
                }
            }

            // 2: 设备详情
            DetailPage {
                id: detailPage

                isDark: root.isDark
            }

            // 3: 数据面板
            DashboardPage {
                id: dashboardPage

                isDark: root.isDark
            }

            // 4: 告警中心
            AlarmCenterPage {
                id: alarmCenterPage

                alarmListModel: alarmModel
                isDark: root.isDark
                ruleListModel: ruleModel
            }

            // 5: 分组详情
            GroupDetailPage {
                id: groupDetailPage

                deviceData: deviceModel
                groupDetailManager: dataManager
                isDark: root.isDark
            }
        }
    }

    // Connect signals
    Connections {
        function onConnectionStatusChanged(status) {
            if (status === "connected") {
                loginDialog.close();
                loginDialog.connecting = false;
            } else if (status === "failed") {
                loginDialog.connecting = false;
                loginDialog.errorMsg = "连接失败，请检查服务器地址和密码";
                loginDialog.open();
            }
        }
        function onDataPointArrived(deviceId, metric, value, timestamp) {
            dashboardPage.addDataPoint(metric, value, timestamp);
            detailPage.addDataPoint(deviceId, metric, value, timestamp);
        }
        function onDeviceUpdated(device) {
            deviceModel.updateDevice(device);
        }
        function onErrorOccurred(error) {
            loginDialog.errorMsg = error;
        }
        function onNewAlarm(alarm) {
            alarmModel.addRecord(alarm);
        }

        target: dataManager
    }
    Connections {
        function onDataPointHistoryFetched(points) {
            historyDataModel.clear();
            for (var i = 0; i < points.length; i++) {
                var p = points[i];
                var dt = new Date(p.ts * 1000);
                historyDataModel.append({
                    time: Qt.formatDateTime(dt, "yyyy-MM-dd HH:mm:ss"),
                    value: p.value
                });
            }
        }
        function onShadowFetched(deviceId, shadow) {
            if (detailPage.currentDevice && detailPage.currentDevice.id === deviceId) {
                detailPage.shadowDesiredText = JSON.stringify(shadow.desired || {}, null, 2);
                detailPage.shadowReportedText = JSON.stringify(shadow.reported || {}, null, 2);
            }
        }

        target: dataManager ? dataManager.httpClient : null
    }
    Connections {
        function onGroupCreated(groupId) {
            console.log("[Group] created:", groupId);
            dataManager.refreshGroups();
        }
        function onGroupDeleted(groupId) {
            console.log("[Group] deleted:", groupId);
            dataManager.refreshGroups();
        }
        function onGroupOperationError(error) {
            console.log("[Group] error:", error);
        }
        function onGroupUpdated(groupId) {
            console.log("[Group] updated:", groupId);
            dataManager.refreshGroups();
        }

        target: dataManager ? dataManager.httpClient : null
    }
}
