import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtShadcn
import "../components"

// 分组管理：左分组列表 + 右组内设备（单页 master-detail）
Rectangle {
    id: root

    property var deviceData: null
    property var groupData: null
    property var groupManager: null

    property int selectedGroupId: -1
    property string selectedGroupName: ""

    QtShadcnTheme { id: theme }

    color: theme.background

    // 选中分组的设备列表
    ListModel { id: groupDeviceModel }

    function refreshGroupDevices() {
        groupDeviceModel.clear();
        if (selectedGroupId < 0 || !deviceData) return;
        var devices = deviceData.devicesByGroup(selectedGroupId);
        for (var i = 0; i < devices.length; i++) {
            var d = devices[i];
            groupDeviceModel.append({
                "deviceId": d.id || "",
                "deviceName": d.name || "",
                "productKey": d.productKey || "",
                "statusValue": d.status,
                "statusText": d.statusText || "离线"
            });
        }
    }

    onSelectedGroupIdChanged: refreshGroupDevices()

    Connections {
        target: root.deviceData
        function onCountsChanged() { root.refreshGroupDevices(); }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        // ===== 头部 =====
        RowLayout {
            Layout.fillWidth: true

            ShadcnLabel {
                text: "分组管理"
                size: ShadcnLabel.Size.Large
            }
            Item { Layout.fillWidth: true }
            ShadcnButton {
                text: "新增分组"
                iconName: "plus"
                size: ShadcnButton.Size.Small
                onClicked: addGroupDialog.open()
            }
        }

        // ===== 主体 =====
        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 16

            // ---- 分组列表 ----
            Panel {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.maximumWidth: 380

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    ShadcnLabel { text: "所有分组" }
                    ShadcnLabel {
                        text: root.groupData ? "共 " + root.groupData.totalCount + " 个分组" : ""
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                        Layout.fillWidth: true
                    }
                    ShadcnSeparator { Layout.fillWidth: true }

                    ListView {
                        id: groupList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.groupData
                        spacing: 2

                        QQC.ScrollBar.vertical: QQC.ScrollBar {
                            active: true
                            policy: QQC.ScrollBar.AsNeeded
                        }

                        delegate: Rectangle {
                            required property int index
                            required property int groupId
                            required property string groupName
                            required property string description
                            required property int deviceCount

                            width: groupList.width
                            height: 58
                            radius: theme.radius
                            color: root.selectedGroupId === groupId
                                   ? theme.primary
                                   : groupHover.containsMouse ? theme.muted : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 10

                                ShadcnIcon {
                                    name: "folder"
                                    size: 18
                                    color: root.selectedGroupId === groupId
                                           ? theme.primaryForeground
                                           : theme.mutedForeground
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0

                                    ShadcnLabel {
                                        text: groupName
                                        color: root.selectedGroupId === groupId
                                               ? theme.primaryForeground
                                               : theme.foreground
                                    }
                                    ShadcnLabel {
                                        Layout.fillWidth: true
                                        text: description || "-"
                                        size: ShadcnLabel.Size.Small
                                        elide: Text.ElideRight
                                        color: root.selectedGroupId === groupId
                                               ? theme.primaryForeground
                                               : theme.mutedForeground
                                    }
                                }
                                ShadcnBadge {
                                    text: deviceCount + " 台"
                                    variant: ShadcnBadge.Variant.Secondary
                                }
                            }

                            MouseArea {
                                id: groupHover
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.selectedGroupId = groupId;
                                    root.selectedGroupName = groupName;
                                }
                            }
                        }
                    }
                }
            }

            // ---- 组内设备 ----
            Panel {
                Layout.fillHeight: true
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        ColumnLayout {
                            spacing: 0
                            ShadcnLabel {
                                text: root.selectedGroupId >= 0 ? root.selectedGroupName : "选择一个分组"
                            }
                            ShadcnLabel {
                                text: root.selectedGroupId >= 0
                                      ? "共 " + groupDeviceModel.count + " 台设备"
                                      : "点击左侧分组查看设备"
                                size: ShadcnLabel.Size.Small
                                variant: ShadcnLabel.Variant.Muted
                            }
                        }
                        Item { Layout.fillWidth: true }

                        ShadcnButton {
                            text: "添加设备"
                            iconName: "plus"
                            size: ShadcnButton.Size.ExtraSmall
                            visible: root.selectedGroupId >= 0
                            onClicked: addDeviceDialog.open()
                        }
                        ShadcnButton {
                            text: "删除分组"
                            iconName: "trash-2"
                            size: ShadcnButton.Size.ExtraSmall
                            variant: ShadcnButton.Variant.Destructive
                            visible: root.selectedGroupId > 0
                            onClicked: deleteConfirmDialog.open()
                        }
                    }

                    ShadcnSeparator { Layout.fillWidth: true }

                    ListView {
                        id: groupDeviceList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: groupDeviceModel
                        spacing: 2

                        QQC.ScrollBar.vertical: QQC.ScrollBar {
                            active: true
                            policy: QQC.ScrollBar.AsNeeded
                        }

                        delegate: Rectangle {
                            required property int index
                            required property string deviceId
                            required property string deviceName
                            required property string productKey
                            required property int statusValue
                            required property string statusText

                            width: groupDeviceList.width
                            height: 48
                            radius: theme.radius
                            color: devHover.containsMouse ? theme.muted : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 10

                                ShadcnStatusDot {
                                    status: statusValue === 1
                                            ? ShadcnStatusDot.Status.Online
                                            : statusValue === 2
                                              ? ShadcnStatusDot.Status.Danger
                                              : ShadcnStatusDot.Status.Offline
                                }
                                ShadcnLabel {
                                    Layout.preferredWidth: 120
                                    text: deviceId
                                    size: ShadcnLabel.Size.Small
                                }
                                ShadcnLabel {
                                    Layout.fillWidth: true
                                    text: deviceName || "-"
                                    size: ShadcnLabel.Size.Small
                                }
                                ShadcnLabel {
                                    Layout.preferredWidth: 120
                                    text: productKey || "-"
                                    size: ShadcnLabel.Size.Small
                                    variant: ShadcnLabel.Variant.Muted
                                }
                                ShadcnButton {
                                    text: "移除"
                                    size: ShadcnButton.Size.ExtraSmall
                                    variant: ShadcnButton.Variant.Ghost
                                    visible: root.selectedGroupId > 0
                                    onClicked: {
                                        removeConfirmDialog.targetDeviceId = deviceId;
                                        removeConfirmDialog.targetDeviceName = deviceName;
                                        removeConfirmDialog.open();
                                    }
                                }
                            }

                            MouseArea {
                                id: devHover
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                        }

                        Column {
                            anchors.centerIn: parent
                            spacing: theme.spacingSm
                            visible: groupDeviceModel.count === 0

                            ShadcnIcon {
                                anchors.horizontalCenter: parent.horizontalCenter
                                name: root.selectedGroupId >= 0 ? "file" : "folder"
                                size: 32
                                color: theme.mutedForeground
                            }
                            ShadcnLabel {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: root.selectedGroupId >= 0
                                      ? "该分组暂无设备"
                                      : "选择左侧分组查看设备"
                                variant: ShadcnLabel.Variant.Muted
                            }
                        }
                    }
                }
            }
        }
    }

    // ===== 新增分组 =====
    ShadcnDialog {
        id: addGroupDialog
        modal: true

        ShadcnDialogContent {
            ShadcnDialogHeader { ShadcnDialogTitle { text: "新增分组" } }
            ColumnLayout {
                width: parent.width
                spacing: 12
                ShadcnLabel { text: "分组名称"; size: ShadcnLabel.Size.Small }
                ShadcnInput { id: addGroupName; Layout.fillWidth: true; placeholderText: "请输入分组名称" }
                ShadcnLabel { text: "描述"; size: ShadcnLabel.Size.Small }
                ShadcnInput { id: addGroupDesc; Layout.fillWidth: true; placeholderText: "请输入描述" }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    spacing: 8

                    Item { Layout.fillWidth: true }
                    ShadcnButton {
                        text: "取消"
                        variant: ShadcnButton.Variant.Outline
                        size: ShadcnButton.Size.Small
                        onClicked: addGroupDialog.close()
                    }
                    ShadcnButton {
                        text: "创建"
                        size: ShadcnButton.Size.Small
                        onClicked: {
                            if (addGroupName.text && root.groupManager) {
                                root.groupManager.httpClient.createGroup(addGroupName.text, addGroupDesc.text);
                                addGroupName.text = "";
                                addGroupDesc.text = "";
                            }
                            addGroupDialog.close();
                        }
                    }
                }
            }
        }
    }

    // ===== 删除分组确认 =====
    ShadcnDialog {
        id: deleteConfirmDialog
        modal: true

        ShadcnDialogContent {
            ColumnLayout {
                width: parent.width
                spacing: 16

                ShadcnDialogHeader {
                    Layout.fillWidth: true
                    ShadcnDialogTitle { text: "确认删除" }
                    ShadcnDialogDescription {
                        text: "确定要删除分组 \"" + root.selectedGroupName + "\" 吗？删除后无法恢复。"
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Item { Layout.fillWidth: true }
                    ShadcnButton {
                        text: "取消"
                        variant: ShadcnButton.Variant.Outline
                        size: ShadcnButton.Size.Small
                        onClicked: deleteConfirmDialog.close()
                    }
                    ShadcnButton {
                        text: "删除"
                        size: ShadcnButton.Size.Small
                        variant: ShadcnButton.Variant.Destructive
                        onClicked: {
                            if (root.groupManager && root.selectedGroupId > 0)
                                root.groupManager.httpClient.deleteGroup(root.selectedGroupId);
                            root.selectedGroupId = -1;
                            deleteConfirmDialog.close();
                        }
                    }
                }
            }
        }
    }

    // ===== 添加设备到分组 =====
    ShadcnDialog {
        id: addDeviceDialog

        property var selectedIds: []
        modal: true

        onOpened: selectedIds = []

        ShadcnDialogContent {
            ShadcnDialogHeader {
                ShadcnDialogTitle { text: "添加设备到分组" }
                ShadcnDialogDescription {
                    text: "选择要添加到 \"" + root.selectedGroupName + "\" 的设备"
                }
            }

            ListView {
                id: devicePicker
                width: parent.width
                height: 280
                clip: true
                model: root.deviceData
                spacing: 2

                delegate: Rectangle {
                    required property int index
                    required property string deviceId
                    required property string deviceName
                    required property string group

                    width: devicePicker.width
                    height: 44
                    radius: theme.radius
                    color: "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        spacing: 10

                        ShadcnCheckbox {
                            checked: addDeviceDialog.selectedIds.indexOf(deviceId) >= 0
                            onCheckedChanged: {
                                var ids = addDeviceDialog.selectedIds.slice();
                                var idx = ids.indexOf(deviceId);
                                if (checked && idx < 0) ids.push(deviceId);
                                else if (!checked && idx >= 0) ids.splice(idx, 1);
                                addDeviceDialog.selectedIds = ids;
                            }
                        }
                        ShadcnLabel {
                            Layout.fillWidth: true
                            text: deviceName || deviceId
                            size: ShadcnLabel.Size.Small
                        }
                        ShadcnLabel {
                            text: group || "未分组"
                            size: ShadcnLabel.Size.Small
                            variant: ShadcnLabel.Variant.Muted
                        }
                    }
                }

                RowLayout {
                    width: parent.width
                    spacing: 8

                    Item { Layout.fillWidth: true }
                    ShadcnButton {
                        text: "取消"
                        variant: ShadcnButton.Variant.Outline
                        size: ShadcnButton.Size.Small
                        onClicked: addDeviceDialog.close()
                    }
                    ShadcnButton {
                        text: "添加"
                        size: ShadcnButton.Size.Small
                        onClicked: {
                            if (root.groupManager) {
                                for (var i = 0; i < addDeviceDialog.selectedIds.length; i++)
                                    root.groupManager.updateDeviceGroup(addDeviceDialog.selectedIds[i], root.selectedGroupId);
                            }
                            addDeviceDialog.selectedIds = [];
                            addDeviceDialog.close();
                        }
                    }
                }
            }
        }
    }

    // ===== 移除设备确认 =====
    ShadcnDialog {
        id: removeConfirmDialog

        property string targetDeviceId: ""
        property string targetDeviceName: ""
        modal: true

        ShadcnDialogContent {
            ColumnLayout {
                width: parent.width
                spacing: 16

                ShadcnDialogHeader {
                    Layout.fillWidth: true
                    ShadcnDialogTitle { text: "确认移除" }
                    ShadcnDialogDescription {
                        text: "确定要将 \""
                              + (removeConfirmDialog.targetDeviceName || removeConfirmDialog.targetDeviceId)
                              + "\" 从分组中移除吗？移除后设备将变为未分组状态。"
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Item { Layout.fillWidth: true }
                    ShadcnButton {
                        text: "取消"
                        variant: ShadcnButton.Variant.Outline
                        size: ShadcnButton.Size.Small
                        onClicked: removeConfirmDialog.close()
                    }
                    ShadcnButton {
                        text: "移除"
                        size: ShadcnButton.Size.Small
                        variant: ShadcnButton.Variant.Destructive
                        onClicked: {
                            if (root.groupManager)
                                root.groupManager.removeDeviceFromGroup(removeConfirmDialog.targetDeviceId);
                            removeConfirmDialog.close();
                        }
                    }
                }
            }
        }
    }
}
