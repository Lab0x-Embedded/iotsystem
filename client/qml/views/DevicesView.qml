import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtShadcn
import "../components"

// 设备总览: 统计卡 + 搜索 + 设备表格 + 实时图表
Rectangle {
    id: root

    property var deviceData: null   // deviceModel
    property string selectedDeviceId: ""
    signal deviceSelected(string deviceId)

    QtShadcnTheme { id: theme }

    color: theme.background

    // 搜索过滤
    function _matchFilter(deviceId, deviceName) {
        if (searchInput.text === "") return true;
        var q = searchInput.text.toLowerCase();
        return (deviceId || "").toLowerCase().indexOf(q) >= 0
            || (deviceName || "").toLowerCase().indexOf(q) >= 0;
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        // ===== 统计卡 =====
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            StatCard {
                Layout.fillWidth: true
                title: "设备总数"
                value: deviceData ? deviceData.totalCount : "0"
            }
            StatCard {
                Layout.fillWidth: true
                title: "在线"
                value: deviceData ? deviceData.onlineCount : "0"
                valueColor: theme.success
                dotStatus: ShadcnStatusDot.Status.Online
            }
            StatCard {
                Layout.fillWidth: true
                title: "离线"
                value: deviceData ? (deviceData.totalCount - deviceData.onlineCount - deviceData.alarmCount) : "0"
                valueColor: theme.mutedForeground
                dotStatus: ShadcnStatusDot.Status.Offline
            }
            StatCard {
                Layout.fillWidth: true
                title: "告警"
                value: deviceData ? deviceData.alarmCount : "0"
                valueColor: theme.destructive
                dotStatus: ShadcnStatusDot.Status.Danger
            }
        }

        // ===== 主体: 左表格 + 右图表 =====
        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 16

            // 设备表格
            ShadcnCard {
                Layout.fillHeight: true
                Layout.fillWidth: true

                ShadcnCardHeader {
                    RowLayout {
                        width: parent.width

                        ShadcnCardTitle { text: "设备列表" }
                        Item { Layout.fillWidth: true }
                        ShadcnInputGroup {
                            id: searchInput
                            Layout.preferredWidth: 220
                            prefixIcon: "search"
                            placeholderText: "搜索设备 ID / 名称..."
                        }
                    }
                }

                ShadcnCardContent {
                        width: parent.width
                        height: parent.height
                        implicitHeight: 0
                    ListView {
                        id: deviceList
                        width: parent.width
                            height: parent.height
                        clip: true
                        model: root.deviceData
                        spacing: 0

                        QQC.ScrollBar.vertical: QQC.ScrollBar {
                            active: true
                            policy: QQC.ScrollBar.AsNeeded
                        }

                        delegate: Rectangle {
                            width: deviceList.width
                            height: 48
                            radius: theme.radius
                            color: deviceMouse.containsMouse
                                   ? theme.muted
                                   : "transparent"

                            required property int index
                            required property string deviceId
                            required property string deviceName
                            required property int status
                            required property string group
                            required property double temperature
                            required property double humidity
                            required property string lastSeen

                            RowLayout {
                                width: parent.width
                            height: parent.height
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 12

                                ShadcnStatusDot {
                                    status: model.status === 1
                                            ? ShadcnStatusDot.Status.Online
                                            : model.status === 2
                                              ? ShadcnStatusDot.Status.Danger
                                              : ShadcnStatusDot.Status.Offline
                                }
                                ShadcnLabel {
                                    Layout.preferredWidth: 110
                                    text: model.deviceId
                                    size: ShadcnLabel.Size.Small
                                }
                                ShadcnLabel {
                                    Layout.preferredWidth: 160
                                    text: model.deviceName || "-"
                                    size: ShadcnLabel.Size.Small
                                }
                                ShadcnBadge {
                                    text: {
                                        var g = model.group;
                                        return groupModel && g ? groupModel.groupName(Number(g)) : "未分组";
                                    }
                                    variant: ShadcnBadge.Variant.Secondary
                                }
                                ShadcnLabel {
                                    Layout.preferredWidth: 70
                                    text: model.temperature.toFixed(1) + "°C"
                                    size: ShadcnLabel.Size.Small
                                    variant: ShadcnLabel.Variant.Muted
                                }
                                ShadcnLabel {
                                    Layout.preferredWidth: 60
                                    text: model.humidity.toFixed(0) + "%"
                                    size: ShadcnLabel.Size.Small
                                    variant: ShadcnLabel.Variant.Muted
                                }
                                Item { Layout.fillWidth: true }
                                ShadcnLabel {
                                    text: model.lastSeen || ""
                                    size: ShadcnLabel.Size.Small
                                    variant: ShadcnLabel.Variant.Muted
                                }
                                ShadcnButton {
                                    text: "详情"
                                    size: ShadcnButton.Size.ExtraSmall
                                    variant: ShadcnButton.Variant.Ghost
                                    onClicked: root.deviceSelected(model.deviceId || "")
                                }
                            }

                            MouseArea {
                                id: deviceMouse
                                width: parent.width
                            height: parent.height
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.deviceSelected(model.deviceId || "")
                            }
                        }
                    }
                }
            }

            // 实时图表
            ShadcnCard {
                Layout.fillHeight: true
                Layout.preferredWidth: 420

                ShadcnCardHeader {
                    ShadcnCardTitle { text: "实时数据" }
                    ShadcnCardDescription {
                        text: root.selectedDeviceId
                              ? "设备 " + root.selectedDeviceId + " 最近 1 小时"
                              : "选择设备查看"
                    }
                }

                ShadcnCardContent {
                        width: parent.width
                        height: parent.height
                        implicitHeight: 0
                    RealtimeChart {
                        id: overviewChart
                        width: parent.width
                            height: parent.height
                        visible: root.selectedDeviceId !== ""
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: theme.spacingSm
                        visible: root.selectedDeviceId === ""

                        ShadcnIcon {
                            anchors.horizontalCenter: parent.horizontalCenter
                            name: "line-chart"
                            size: 32
                            color: theme.mutedForeground
                        }
                        ShadcnLabel {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "点击设备查看实时曲线"
                            variant: ShadcnLabel.Variant.Muted
                        }
                    }
                }
            }
        }
    }

    // 历史数据回调 (轮询图表数据)
    Connections {
        target: dataManager ? dataManager.httpClient : null
        function onDataPointHistoryFetched(points) {
            overviewChart.clearData();
            var temp = [], hum = [];
            for (var i = 0; i < points.length; i++) {
                var p = points[i];
                if (p.metric === "temperature") temp.push(p);
                else if (p.metric === "humidity") hum.push(p);
            }
            if (temp.length > 0) overviewChart.setPoints("temperature", temp);
            if (hum.length > 0) overviewChart.setPoints("humidity", hum);
        }
    }

    // 选中设备后拉取历史
    onSelectedDeviceIdChanged: {
        if (selectedDeviceId === "" || !dataManager || !dataManager.online) return;
        var now = Math.floor(Date.now() / 1000);
        dataManager.fetchDataPointHistory(selectedDeviceId, "temperature", now - 3600, now, 120);
        dataManager.fetchDataPointHistory(selectedDeviceId, "humidity", now - 3600, now, 120);
    }
}
