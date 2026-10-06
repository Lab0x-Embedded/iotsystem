import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtShadcn
import "views"
import "components"

QQC.ApplicationWindow {
    id: root

    width: 1280
    height: 800
    minimumWidth: 1024
    minimumHeight: 600
    title: "IoT Device Manager"
    visible: true

    QtShadcnTheme { id: theme }
    color: theme.background

    // ===== 视图切换 =====
    // 0: 设备总览  1: 告警中心  2: 分组管理  3: 产品管理  4: 设备详情
    property int currentView: 0

    Component.onCompleted: {
        dataManager.connectToServer("http://127.0.0.1:8080", "admin", "admin@123");
    }

    // ===== 状态栏 =====
    footer: Rectangle {
        height: 32
        color: theme.card
        border.color: theme.border
        border.width: 0

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 8

            ShadcnStatusDot {
                status: dataManager && dataManager.online
                        ? ShadcnStatusDot.Status.Online
                        : ShadcnStatusDot.Status.Danger
            }
            ShadcnLabel {
                text: dataManager && dataManager.online
                      ? "已连接 " + (dataManager.serverUrl || "127.0.0.1")
                      : "未连接 · 离线模式"
                size: ShadcnLabel.Size.Small
                variant: ShadcnLabel.Variant.Muted

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: loginDialog.open()
                }
            }
            Item { Layout.fillWidth: true }
            ShadcnLabel {
                text: "设备 " + (deviceModel ? deviceModel.totalCount : 0)
                      + " · 在线 " + (deviceModel ? deviceModel.onlineCount : 0)
                      + " · 告警 " + (alarmModel ? alarmModel.activeCount : 0)
                size: ShadcnLabel.Size.Small
                variant: ShadcnLabel.Variant.Muted
            }
        }
    }

    // ===== 主体 =====
    RowLayout {
        anchors.fill: parent
        spacing: 0

        AppSidebar {
            Layout.fillHeight: true
            Layout.preferredWidth: 200
            currentIndex: root.currentView === 4 ? 0 : root.currentView
            onPageSelected: function(index) { root.currentView = index; }
        }

        StackLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            currentIndex: root.currentView

            // 0: 设备总览
            DevicesView {
                deviceData: deviceModel
                onDeviceSelected: function(deviceId) {
                    detailView.showDevice(deviceId);
                    root.currentView = 4;
                }
                onNavigateToProducts: root.currentView = 3
            }

            // 1: 告警中心
            AlarmsView {
                alarmListModel: alarmModel
                ruleListModel: ruleModel
            }

            // 2: 分组管理
            GroupsView {
                deviceData: deviceModel
                groupData: groupModel
                groupManager: dataManager
            }

            // 3: 产品管理
            ProductsView {
                id: productsView
            }

            // 4: 设备详情
            DeviceDetailView {
                id: detailView
                onBackRequested: root.currentView = 0
            }
        }
    }

    // ===== 登录对话框 =====
    ShadcnDialog {
        id: loginDialog

        property bool connecting: false
        property string errorMsg: ""

        modal: true
        closePolicy: QQC.Popup.NoAutoClose

        ShadcnDialogContent {
            ShadcnDialogHeader {
                ShadcnDialogTitle { text: "连接服务器" }
                ShadcnDialogDescription { text: "输入服务器地址和凭据" }
            }
            ColumnLayout {
                width: parent.width
                spacing: 12

                ShadcnInput {
                    id: serverUrlField
                    Layout.fillWidth: true
                    text: "http://127.0.0.1:8080"
                    placeholderText: "服务器地址"
                }
                ShadcnInput {
                    id: usernameField
                    Layout.fillWidth: true
                    text: "admin"
                    placeholderText: "用户名"
                }
                ShadcnInput {
                    id: passwordField
                    Layout.fillWidth: true
                    text: "admin@123"
                    echoMode: TextInput.Password
                    placeholderText: "密码"
                }
                ShadcnLabel {
                    text: loginDialog.errorMsg
                    size: ShadcnLabel.Size.Small
                    variant: ShadcnLabel.Variant.Destructive
                    visible: loginDialog.errorMsg !== ""
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
            }
            footer: ShadcnDialogFooter {
                // 靠右 + 垂直居中由组件自身保证：Row 右锚定，footerSlot 高度贴合按钮、上下各留 _pad
                ShadcnButton {
                    text: loginDialog.connecting ? "连接中..." : "连接"
                    loading: loginDialog.connecting
                    enabled: !loginDialog.connecting
                    onClicked: {
                        loginDialog.connecting = true;
                        loginDialog.errorMsg = "";
                        dataManager.connectToServer(serverUrlField.text,
                                                    usernameField.text,
                                                    passwordField.text);
                    }
                }
            }
        }
    }

    // ===== 数据信号接线 =====
    Connections {
        target: dataManager

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
        function onErrorOccurred(error) {
            loginDialog.errorMsg = error;
        }
    }

    Connections {
        target: dataManager ? dataManager.httpClient : null

        function onShadowFetched(deviceId, shadow) {
            detailView.shadowDesiredText = JSON.stringify(shadow.desired || {}, null, 2);
            detailView.shadowReportedText = JSON.stringify(shadow.reported || {}, null, 2);
        }
        function onGroupCreated(groupId) { dataManager.refreshGroups(); }
        function onGroupDeleted(groupId) { dataManager.refreshGroups(); }
        function onGroupUpdated(groupId) { dataManager.refreshGroups(); }
    }
}
