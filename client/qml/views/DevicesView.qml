import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtShadcn
import "../components"

// 设备总览：统计卡 + 设备列表
Rectangle {
    id: root

    property var deviceData: null
    signal deviceSelected(string deviceId)

    QtShadcnTheme { id: theme }

    color: theme.background

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
                value: root.deviceData ? root.deviceData.totalCount : "0"
            }
            StatCard {
                Layout.fillWidth: true
                title: "在线"
                value: root.deviceData ? root.deviceData.onlineCount : "0"
                valueColor: theme.success
                dotStatus: ShadcnStatusDot.Status.Online
            }
            StatCard {
                Layout.fillWidth: true
                title: "离线"
                value: root.deviceData
                       ? (root.deviceData.totalCount - root.deviceData.onlineCount - root.deviceData.alarmCount)
                       : "0"
                valueColor: theme.mutedForeground
                dotStatus: ShadcnStatusDot.Status.Offline
            }
            StatCard {
                Layout.fillWidth: true
                title: "告警"
                value: root.deviceData ? root.deviceData.alarmCount : "0"
                valueColor: theme.destructive
                dotStatus: ShadcnStatusDot.Status.Danger
            }
        }

        // ===== 主体 =====
        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 16

            // ---- 设备列表 ----
            Panel {
                Layout.fillHeight: true
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    // 头部：标题 + 搜索
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        ShadcnLabel { text: "设备列表" }
                        Item { Layout.fillWidth: true }
                        ShadcnInputGroup {
                            Layout.preferredWidth: 220
                            prefixIcon: "search"
                            placeholderText: "搜索设备 ID / 名称..."
                        }
                    }

                    ShadcnSeparator { Layout.fillWidth: true }

                    // 表头
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Item { Layout.preferredWidth: 8 }   // 与行的状态点对齐
                        ShadcnLabel { Layout.preferredWidth: 110; text: "设备 ID";  size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        ShadcnLabel { Layout.fillWidth: true;    text: "名称";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        ShadcnLabel { Layout.preferredWidth: 210; text: "最新数据"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        ShadcnLabel { Layout.preferredWidth: 130; text: "最后上报"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        ShadcnLabel { text: "操作"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                    }
                    ShadcnSeparator { Layout.fillWidth: true }

                    // 列表
                    ListView {
                        id: deviceList

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.deviceData
                        spacing: 2

                        QQC.ScrollBar.vertical: QQC.ScrollBar {
                            active: true
                            policy: QQC.ScrollBar.AsNeeded
                        }

                        delegate: Rectangle {
                            required property int index
                            required property string deviceId
                            required property string deviceName
                            required property string group
                            required property string lastSeen

                            width: deviceList.width
                            height: 48
                            radius: theme.radius
                            color: deviceHover.containsMouse ? theme.muted : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 10

                                ShadcnLabel {
                                    Layout.preferredWidth: 110
                                    text: deviceId
                                    size: ShadcnLabel.Size.Small
                                }
                                ShadcnLabel {
                                    Layout.fillWidth: true
                                    text: deviceName || "-"
                                    size: ShadcnLabel.Size.Small
                                    elide: Text.ElideRight
                                }
                                // 该设备真实上报的指标摘要（不再是写死的温湿度）
                                ShadcnLabel {
                                    Layout.preferredWidth: 210
                                    text: dataManager ? dataManager.metricSummary(deviceId, 2) : "—"
                                    size: ShadcnLabel.Size.Small
                                    variant: ShadcnLabel.Variant.Muted
                                    elide: Text.ElideRight
                                }
                                ShadcnLabel {
                                    Layout.preferredWidth: 130
                                    text: lastSeen || ""
                                    size: ShadcnLabel.Size.Small
                                    variant: ShadcnLabel.Variant.Muted
                                }
                                ShadcnButton {
                                    text: "详情"
                                    size: ShadcnButton.Size.ExtraSmall
                                    variant: ShadcnButton.Variant.Ghost
                                    onClicked: root.deviceSelected(deviceId)
                                }
                            }

                            MouseArea {
                                id: deviceHover
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.deviceSelected(deviceId)
                            }
                        }

                        // 空状态
                        Column {
                            anchors.centerIn: parent
                            spacing: theme.spacingSm
                            visible: deviceList.count === 0

                            ShadcnIcon {
                                anchors.horizontalCenter: parent.horizontalCenter
                                name: "monitor"
                                size: 32
                                color: theme.mutedForeground
                            }
                            ShadcnLabel {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "暂无设备"
                                variant: ShadcnLabel.Variant.Muted
                            }
                        }
                    }
                }
            }


        }
    }

}
