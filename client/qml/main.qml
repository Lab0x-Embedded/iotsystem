import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

import "pages"
import "components"

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 800
    minimumWidth: 1024
    minimumHeight: 600
    title: "IoT Device Manager"
    
    // Theme properties
    property bool isDark: themeManager ? themeManager.isDark : true
    property color bgColor: isDark ? "#1e1e2e" : "#f5f5f5"
    property color sidebarBg: isDark ? "#181825" : "#ffffff"
    property color cardBg: isDark ? "#313244" : "#ffffff"
    property color textColor: isDark ? "#cdd6f4" : "#1e1e2e"
    property color accentColor: "#89b4fa"
    
    Material.theme: isDark ? Material.Dark : Material.Light
    Material.accent: accentColor
    
    // ===== 启动时自动连接服务器 =====
    Component.onCompleted: {
        console.log("应用启动，正在连接服务器...")
        dataManager.connectToServer("http://127.0.0.1:18080", "admin", "admin@123")
    }
    
    // ===== 登录对话框 =====
    Dialog {
        id: loginDialog
        title: "连接服务器"
        anchors.centerIn: parent
        modal: true
        closePolicy: Popup.NoAutoClose
        
        property bool connecting: false
        property string errorMsg: ""
        
        ColumnLayout {
            spacing: 16
            implicitWidth: 600
            
            Label {
                text: "请输入服务器信息"
                font.pixelSize: 14
                color: textColor
            }
            
            TextField {
                id: serverUrlField
                placeholderText: "服务器地址"
                text: "http://127.0.0.1:18080"
                Layout.fillWidth: true
            }
            
            TextField {
                id: usernameField
                placeholderText: "用户名"
                text: "admin"
                Layout.fillWidth: true
            }
            
            TextField {
                id: passwordField
                placeholderText: "密码"
                echoMode: TextInput.Password
                text: "admin@123"
                Layout.fillWidth: true
            }
            
            Label {
                text: loginDialog.errorMsg
                color: "#FF5722"
                visible: loginDialog.errorMsg !== ""
                font.pixelSize: 12
            }
            
            RowLayout {
                Layout.fillWidth: true
                
                Item { Layout.fillWidth: true }
                
                Button {
                    text: loginDialog.connecting ? "连接中..." : "连接"
                    enabled: !loginDialog.connecting
                    Material.background: accentColor
                    onClicked: {
                        loginDialog.connecting = true
                        loginDialog.errorMsg = ""
                        dataManager.connectToServer(
                            serverUrlField.text,
                            usernameField.text,
                            passwordField.text
                        )
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
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            isDark: root.isDark
            
            // 显示连接状态
            property bool isConnected: dataManager ? dataManager.online : false
            
            onPageSelected: function(index) {
                stackView.currentIndex = index
            }
            
            onThemeToggle: {
                themeManager.toggle()
            }
        }
        
        // Content area
        StackLayout {
            id: stackView
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: 0
            
            OverviewPage {
                id: overviewPage
                isDark: root.isDark
                deviceModel: deviceModel
                
                onDeviceSelected: function(deviceId) {
                    detailPage.showDevice(deviceId)
                    stackView.currentIndex = 1
                    sidebar.currentIndex = 1
                }
            }
            
            DetailPage {
                id: detailPage
                isDark: root.isDark
            }
            
            DashboardPage {
                id: dashboardPage
                isDark: root.isDark
            }
            
            AlarmCenterPage {
                id: alarmCenterPage
                isDark: root.isDark
                alarmModel: alarmModel
            }
        }
    }
    
    // Status bar
    footer: ToolBar {
        height: 32
        Material.background: isDark ? "#11111b" : "#e0e0e0"
        
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            
            // 连接状态指示器
            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: dataManager && dataManager.online ? "#4CAF50" : "#FF5722"
            }
            
            Label {
                text: dataManager && dataManager.online ? "已连接: " + (dataManager.serverUrl || "127.0.0.1") : "未连接 (离线模式)"
                font.pixelSize: 12
                color: isDark ? "#a6adc8" : "#666666"
                
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: loginDialog.open()
                }
            }
            
            Item { Layout.fillWidth: true }
            
            Label {
                id: statusLabel
                text: "共 " + (deviceModel ? deviceModel.totalCount : 0) + " 设备  ·  在线 " + (deviceModel ? deviceModel.onlineCount : 0) + "  ·  告警 " + (alarmModel ? alarmModel.activeCount : 0)
                font.pixelSize: 12
                color: isDark ? "#a6adc8" : "#666666"
            }
        }
    }
    
    // Connect signals
    Connections {
        target: dataManager
        
        function onConnectionStatusChanged(status) {
            console.log("连接状态:", status)
            if (status === "connected") {
                loginDialog.close()
                loginDialog.connecting = false
            } else if (status === "failed") {
                loginDialog.connecting = false
                loginDialog.errorMsg = "连接失败，请检查服务器地址和密码"
                loginDialog.open()
            }
        }
        
        function onErrorOccurred(error) {
            console.log("错误:", error)
            loginDialog.errorMsg = error
        }
        
        function onDeviceUpdated(device) {
            deviceModel.updateDevice(device)
        }
        
        function onNewAlarm(alarm) {
            alarmModel.addRecord(alarm)
        }
        
        function onDataPointArrived(deviceId, metric, value, timestamp) {
            dashboardPage.addDataPoint(metric, value, timestamp)
            detailPage.addDataPoint(metric, value, timestamp)
        }
    }
}
