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
            
            Label {
                text: "数据源: Mock (离线演示模式)"
                font.pixelSize: 12
                color: isDark ? "#a6adc8" : "#666666"
            }
            
            Item { Layout.fillWidth: true }
            
            Label {
                id: statusLabel
                text: "共 " + (deviceModel ? deviceModel.totalCount : 0) + " 设备  ·  在线 " + (deviceModel ? deviceModel.onlineCount : 0) + "  ·  告警 " + (deviceModel ? deviceModel.alarmCount : 0)
                font.pixelSize: 12
                color: isDark ? "#a6adc8" : "#666666"
            }
        }
    }
    
    // Connect signals
    Connections {
        target: dataManager
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
