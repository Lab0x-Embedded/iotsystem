import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtShadcn
import "../components"

// 分组管理: 左侧分组列表 + 右侧组内设备 (master-detail 合一页)
Rectangle {
    id: root

    property var deviceData: null    // deviceModel
    property var groupData: null     // groupModel
    property var groupManager: null  // dataManager

    property int selectedGroupId: -1
    property string selectedGroupName: ""

    signal showGroupDetail(int groupId, string groupName)

    // 左侧选中组的设备列表
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

    // deviceModel counts 变化时刷新
    Connections {
        target: root.deviceData
        function onCountsChanged() { root.refreshGroupDevices(); }
    }

    QtShadcnTheme { id: theme }
    color: theme.background

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        // ===== Header =====
        RowLayout {
            Layout.fillWidth: true

            ShadcnLabel { text: "分组管理"; size: ShadcnLabel.Size.Large }
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

            // 左: 分组列表
            ShadcnCard {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.maximumWidth: 380

                ShadcnCardHeader {
                    ShadcnCardTitle { text: "所有分组" }
                    ShadcnCardDescription {
                        text: groupData ? "共 " + groupData.totalCount + " 个分组" : ""
                    }
                }

                ShadcnCardContent {
                    ListView {
                        id: groupList
                        anchors.fill: parent
                        clip: true
                        model: groupData
                        spacing: 4

                        ScrollBar.vertical: QQC.ScrollBar { active: true; policy: QQC.ScrollBar.AsNeeded }

                        delegate: Rectangle {
                            width: groupList.width
                            height: 60
                            radius: theme.radius
                            color: root.selectedGroupId === model.groupId
                                   ? theme.accent
                                   : groupMouse.containsMouse ? theme.muted : "transparent"

                            required property int index
                            required property int groupId
                            required property string groupName
                            required property string description
                            required property int deviceCount

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 10

                                ShadcnIcon {
                                    name: "folder"
                                    size: 18
                                    color: root.selectedGroupId === model.groupId
                                           ? theme.primaryForeground : theme.mutedForeground
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    ShadcnLabel {
                                        text: model.groupName
                                        size: ShadcnLabel.Size.Medium
                                        color: root.selectedGroupId === model.groupId
                                               ? theme.primaryForeground : theme.foreground
                                    }
                                    ShadcnLabel {
                                        text: model.description || "-"
                                        size: ShadcnLabel.Size.Small
                                        variant: ShadcnLabel.Variant.Muted
                                        elide: Text.ElideRight
                                        Layout.maximumWidth: 200
                                    }
                                }
                                ShadcnBadge {
                                    text: model.deviceCount + " 台"
                                    variant: ShadcnBadge.Variant.Secondary
                                }
                            }

                            MouseArea {
                                id: groupMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.selectedGroupId = model.groupId;
                                    root.selectedGroupName = model.groupName;
                                }
                            }
                        }
                    }
                }
            }

            // 右: 选中组的设备
            ShadcnCard {
                Layout.fillHeight: true
                Layout.fillWidth: true

                ShadcnCardHeader {
                    RowLayout {
                        width: parent.width

                        ColumnLayout {
                            spacing: 0
                            ShadcnCardTitle {
                                text: root.selectedGroupId >= 0
                                      ? root.selectedGroupName
                                      : "选择一个分组"
                            }
                            ShadcnCardDescription {
                                text: root.selectedGroupId >= 0
                                      ? "共 " + groupDeviceModel.count + " 台设备"
                                      : "点击左侧分组查看设备"
                            }
                        }
                        Item { Layout.fillWidth: true }

                        ShadcnButton {
                            visible: root.selectedGroupId >= 0 && root.selectedGroupId !== 1
                            text: "编辑"
                            size: ShadcnButton.Size.ExtraSmall
                            variant: ShadcnButton.Variant.Outline
                            iconName: "pencil"
                            onClicked: {
                                // 从 groupData 找到当前组
                                if (!groupData) return;
                                for (var i = 0; i < groupData.rowCount(); i++) {
                                    var g = groupData.groupAt ? groupData.groupAt(i) : null;
                                    if (g && (g.groupId === root.selectedGroupId)) {
                                        editGroupDialog.currentGroupId = g.groupId;
                                        editGroupNameField.text = g.groupName || "";
                                        editGroupDescField.text = g.description || "";
                                        break;
                                    }
                                }
                                editGroupDialog.open();
                            }
                        }
                        ShadcnButton {
                            visible: root.selectedGroupId >= 1
                            text: "添加设备"
                            size: ShadcnButton.Size.ExtraSmall
                            iconName: "plus"
                            onClicked: addDeviceDialog.open()
                        }
                        ShadcnButton {
                            visible: root.selectedGroupId >= 1
                            text: "删除分组"
                            size: ShadcnButton.Size.ExtraSmall
                            variant: ShadcnButton.Variant.Destructive
                            iconName: "trash-2"
                            onClicked: deleteConfirmDialog.open()
                        }
                    }
                }

                ShadcnCardContent {
                    ListView {
                        id: groupDeviceList
                        anchors.fill: parent
                        clip: true
                        model: groupDeviceModel
                        spacing: 4

                        ScrollBar.vertical: QQC.ScrollBar { active: true; policy: QQC.ScrollBar.AsNeeded }

                        delegate: Rectangle {
                            width: groupDeviceList.width
                            height: 48
                            radius: theme.radius
                            color: deviceMouse.containsMouse ? theme.muted : "transparent"

                            required property int index
                            required property string deviceId
                            required property string deviceName
                            required property string productKey
                            required property int statusValue
                            required property string statusText

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 10

                                ShadcnStatusDot {
                                    status: model.statusValue === 1
                                            ? ShadcnStatusDot.Status.Online
                                            : model.statusValue === 2
                                              ? ShadcnStatusDot.Status.Danger
                                              : ShadcnStatusDot.Status.Offline
                                }
                                ShadcnLabel {
                                    Layout.preferredWidth: 120
                                    text: model.deviceId
                                    size: ShadcnLabel.Size.Small
                                }
                                ShadcnLabel {
                                    Layout.fillWidth: true
                                    text: model.deviceName || "-"
                                    size: ShadcnLabel.Size.Small
                                }
                                ShadcnLabel {
                                    Layout.preferredWidth: 120
                                    text: model.productKey || "-"
                                    size: ShadcnLabel.Size.Small
                                    variant: ShadcnLabel.Variant.Muted
                                }
                                ShadcnButton {
                                    visible: root.selectedGroupId >= 1
                                    text: "移除"
                                    size: ShadcnButton.Size.ExtraSmall
                                    variant: ShadcnButton.Variant.Ghost
                                    color: theme.destructive
                                    onClicked: {
                                        removeConfirmDialog.targetDeviceId = model.deviceId;
                                        removeConfirmDialog.targetDeviceName = model.deviceName;
                                        removeConfirmDialog.open();
                                    }
                                }
                            }

                            MouseArea {
                                id: deviceMouse
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                        }
                    }

                    // 空状态
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
                            text: root.selectedGroupId >= 0 ? "该分组暂无设备" : "选择左侧分组查看设备"
                            variant: ShadcnLabel.Variant.Muted
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
            ShadcnDialogHeader {
                ShadcnDialogTitle { text: "新增分组" }
            }
            ColumnLayout {
                width: parent.width
                spacing: 12
                ShadcnLabel { text: "分组名称"; size: ShadcnLabel.Size.Small }
                ShadcnInput { id: groupNameField; Layout.fillWidth: true; placeholderText: "请输入分组名称" }
                ShadcnLabel { text: "描述"; size: ShadcnLabel.Size.Small }
                ShadcnInput { id: groupDescField; Layout.fillWidth: true; placeholderText: "请输入描述" }
            }
        }
        footer: ShadcnDialogFooter {
            ShadcnButton {
                text: "取消"; variant: ShadcnButton.Variant.Outline; size: ShadcnButton.Size.Small
                onClicked: addGroupDialog.close()
            }
            ShadcnButton {
                text: "创建"; size: ShadcnButton.Size.Small
                onClicked: {
                    if (groupNameField.text && groupManager) {
                        groupManager.httpClient.createGroup(groupNameField.text, groupDescField.text);
                        groupNameField.text = ""; groupDescField.text = "";
                    }
                    addGroupDialog.close();
                }
            }
        }
    }

    // ===== 编辑分组 =====
    ShadcnDialog {
        id: editGroupDialog
        property int currentGroupId: 0
        modal: true

        ShadcnDialogContent {
            ShadcnDialogHeader {
                ShadcnDialogTitle { text: "编辑分组" }
            }
            ColumnLayout {
                width: parent.width
                spacing: 12
                ShadcnLabel { text: "分组名称"; size: ShadcnLabel.Size.Small }
                ShadcnInput { id: editGroupNameField; Layout.fillWidth: true }
                ShadcnLabel { text: "描述"; size: ShadcnLabel.Size.Small }
                ShadcnInput { id: editGroupDescField; Layout.fillWidth: true }
            }
        }
        footer: ShadcnDialogFooter {
            ShadcnButton {
                text: "取消"; variant: ShadcnButton.Variant.Outline; size: ShadcnButton.Size.Small
                onClicked: editGroupDialog.close()
            }
            ShadcnButton {
                text: "保存"; size: ShadcnButton.Size.Small
                onClicked: {
                    if (editGroupNameField.text && groupManager) {
                        groupManager.httpClient.updateGroup(editGroupDialog.currentGroupId,
                                                            editGroupNameField.text, editGroupDescField.text);
                    }
                    editGroupDialog.close();
                }
            }
        }
    }

    // ===== 删除分组确认 =====
    ShadcnDialog {
        id: deleteConfirmDialog
        modal: true

        ShadcnDialogContent {
            ShadcnDialogHeader {
                ShadcnDialogTitle { text: "确认删除" }
                ShadcnDialogDescription {
                    text: "确定要删除分组 \"" + root.selectedGroupName + "\" 吗？删除后无法恢复。"
                }
            }
        }
        footer: ShadcnDialogFooter {
            ShadcnButton {
                text: "取消"; variant: ShadcnButton.Variant.Outline; size: ShadcnButton.Size.Small
                onClicked: deleteConfirmDialog.close()
            }
            ShadcnButton {
                text: "删除"; size: ShadcnButton.Size.Small
                variant: ShadcnButton.Variant.Destructive
                onClicked: {
                    if (groupManager && root.selectedGroupId > 0)
                        groupManager.httpClient.deleteGroup(root.selectedGroupId);
                    root.selectedGroupId = -1;
                    deleteConfirmDialog.close();
                }
            }
        }
    }

    // ===== 添加设备到分组 =====
    ShadcnDialog {
        id: addDeviceDialog
        property var selectedIds: []
        modal: true

        onOpened: {
            selectedIds = [];
        }

        ShadcnDialogContent {
            ShadcnDialogHeader {
                ShadcnDialogTitle { text: "添加设备到分组" }
                ShadcnDialogDescription { text: "选择要添加到 \"" + root.selectedGroupName + "\" 的设备" }
            }

            ListView {
                id: devicePicker
                width: parent.width
                height: 280
                clip: true
                model: root.deviceData
                spacing: 4

                delegate: Rectangle {
                    width: devicePicker.width
                    height: 44
                    radius: theme.radius
                    color: "transparent"

                    required property int index
                    required property string deviceId
                    required property string deviceName
                    required property string group

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        spacing: 10

                        ShadcnCheckbox {
                            checked: addDeviceDialog.selectedIds.indexOf(model.deviceId) >= 0
                            onCheckedChanged: {
                                var ids = addDeviceDialog.selectedIds.slice();
                                var idx = ids.indexOf(model.deviceId);
                                if (checked && idx < 0) ids.push(model.deviceId);
                                else if (!checked && idx >= 0) ids.splice(idx, 1);
                                addDeviceDialog.selectedIds = ids;
                            }
                        }
                        ShadcnLabel {
                            Layout.fillWidth: true
                            text: model.deviceName || model.deviceId
                            size: ShadcnLabel.Size.Small
                        }
                        ShadcnLabel {
                            text: model.group || "未分组"
                            size: ShadcnLabel.Size.Small
                            variant: ShadcnLabel.Variant.Muted
                        }
                    }
                }
            }
        }
        footer: ShadcnDialogFooter {
            ShadcnButton {
                text: "取消"; variant: ShadcnButton.Variant.Outline; size: ShadcnButton.Size.Small
                onClicked: addDeviceDialog.close()
            }
            ShadcnButton {
                text: "添加"; size: ShadcnButton.Size.Small
                onClicked: {
                    if (groupManager) {
                        for (var i = 0; i < addDeviceDialog.selectedIds.length; i++) {
                            groupManager.updateDeviceGroup(addDeviceDialog.selectedIds[i], root.selectedGroupId);
                        }
                    }
                    addDeviceDialog.selectedIds = [];
                    addDeviceDialog.close();
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
            ShadcnDialogHeader {
                ShadcnDialogTitle { text: "确认移除" }
                ShadcnDialogDescription {
                    text: "确定要将 \"" + (removeConfirmDialog.targetDeviceName || removeConfirmDialog.targetDeviceId)
                          + "\" 从分组中移除吗？移除后设备将变为未分组状态。"
                }
            }
        }
        footer: ShadcnDialogFooter {
            ShadcnButton {
                text: "取消"; variant: ShadcnButton.Variant.Outline; size: ShadcnButton.Size.Small
                onClicked: removeConfirmDialog.close()
            }
            ShadcnButton {
                text: "移除"; size: ShadcnButton.Size.Small
                variant: ShadcnButton.Variant.Destructive
                onClicked: {
                    if (groupManager)
                        groupManager.removeDeviceFromGroup(removeConfirmDialog.targetDeviceId);
                    removeConfirmDialog.close();
                }
            }
        }
    }
}
