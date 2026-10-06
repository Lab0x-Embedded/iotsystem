import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtShadcn
import "../components"

// 产品管理：products 表 CRUD
// 产品与设备是一对多（devices.product_key 外键指向 products.product_key）
Rectangle {
    id: root

    QtShadcnTheme { id: theme }
    color: theme.background

    property string toastText: ""
    property color toastColor: theme.primary

    function showToast(msg, color) {
        toastText = msg;
        toastColor = color || theme.primary;
        toastTimer.restart();
    }

    ListModel { id: productRows }

    function refreshRows() {
        productRows.clear();
        if (!dataManager) return;
        var list = dataManager.products;
        for (var i = 0; i < list.length; ++i) {
            var p = list[i];
            productRows.append({
                "pid": p.id,
                "productKey": p.product_key,
                "productName": p.product_name,
                "description": p.description,
                "deviceCount": p.device_count
            });
        }
    }

    Component.onCompleted: {
        refreshRows();
        if (dataManager) dataManager.refreshProducts();
    }

    Connections {
        target: dataManager
        function onProductsChanged() { root.refreshRows(); }
        function onProductCreated(key) { root.showToast("产品已创建: " + key, theme.success); }
        function onProductUpdated(id)   { Q_UNUSED(id); root.showToast("产品已更新", theme.success); }
        function onProductDeleted(id)   { Q_UNUSED(id); root.showToast("产品已删除", theme.success); }
        function onErrorOccurred(error) { root.showToast(error, theme.destructive); }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        // ===== 头部 =====
        RowLayout {
            Layout.fillWidth: true

            ShadcnLabel { text: "产品管理"; size: ShadcnLabel.Size.Large }
            Item { Layout.fillWidth: true }
            ShadcnButton {
                text: "刷新"
                iconName: "refresh-cw"
                size: ShadcnButton.Size.Small
                variant: ShadcnButton.Variant.Outline
                onClicked: if (dataManager) dataManager.refreshProducts()
            }
            ShadcnButton {
                text: "新建产品"
                iconName: "plus"
                size: ShadcnButton.Size.Small
                onClicked: {
                    productDialog.editing = null;
                    productDialog.open();
                }
            }
        }

        // ===== 列表 =====
        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                // 表头
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ShadcnLabel { Layout.fillWidth: true;    text: "产品名称"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                    ShadcnLabel { Layout.preferredWidth: 160; text: "产品 Key"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                    ShadcnLabel { Layout.preferredWidth: 80;  text: "设备数";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                    ShadcnLabel { Layout.preferredWidth: 260; text: "描述";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                    ShadcnLabel { text: "操作"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                }
                ShadcnSeparator { Layout.fillWidth: true }

                ListView {
                    id: productList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: productRows
                    spacing: 2

                    QQC.ScrollBar.vertical: QQC.ScrollBar {
                        active: true
                        policy: QQC.ScrollBar.AsNeeded
                    }

                    delegate: Rectangle {
                        required property int index
                        required property int pid
                        required property string productKey
                        required property string productName
                        required property string description
                        required property int deviceCount

                        width: productList.width
                        height: 48
                        radius: theme.radius
                        color: "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 10

                            ShadcnLabel {
                                Layout.fillWidth: true
                                text: productName || productKey
                                size: ShadcnLabel.Size.Small
                            }
                            ShadcnLabel {
                                Layout.preferredWidth: 160
                                text: productKey
                                size: ShadcnLabel.Size.Small
                                variant: ShadcnLabel.Variant.Muted
                            }
                            ShadcnBadge {
                                Layout.preferredWidth: 80
                                text: deviceCount + " 台"
                                variant: ShadcnBadge.Variant.Secondary
                            }
                            ShadcnLabel {
                                Layout.preferredWidth: 260
                                text: description || "-"
                                size: ShadcnLabel.Size.Small
                                variant: ShadcnLabel.Variant.Muted
                                elide: Text.ElideRight
                            }
                            RowLayout {
                                spacing: 4

                                ShadcnButton {
                                    text: "编辑"
                                    iconName: "pencil"
                                    size: ShadcnButton.Size.ExtraSmall
                                    variant: ShadcnButton.Variant.Ghost
                                    onClicked: {
                                        productDialog.editing = {
                                            "id": pid,
                                            "product_key": productKey,
                                            "product_name": productName,
                                            "description": description
                                        };
                                        productDialog.open();
                                    }
                                }
                                ShadcnButton {
                                    text: "删除"
                                    iconName: "trash-2"
                                    size: ShadcnButton.Size.ExtraSmall
                                    variant: ShadcnButton.Variant.Ghost
                                    onClicked: {
                                        deleteDialog.pid = pid;
                                        deleteDialog.pkey = productKey;
                                        deleteDialog.devices = deviceCount;
                                        deleteDialog.open();
                                    }
                                }
                            }
                        }
                    }

                    // 空状态
                    Column {
                        anchors.centerIn: parent
                        spacing: theme.spacingSm
                        visible: productList.count === 0

                        ShadcnIcon {
                            anchors.horizontalCenter: parent.horizontalCenter
                            name: "tag"
                            size: 32
                            color: theme.mutedForeground
                        }
                        ShadcnLabel {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "暂无产品"
                            variant: ShadcnLabel.Variant.Muted
                        }
                    }
                }
            }
        }
    }

    // ===== 新建 / 编辑产品 =====
    ShadcnDialog {
        id: productDialog

        property var editing: null

        modal: true

        onOpened: {
            if (editing) {
                fieldKey.text  = editing.product_key;
                fieldName.text = editing.product_name;
                fieldDesc.text = editing.description;
            } else {
                fieldKey.text  = "";
                fieldName.text = "";
                fieldDesc.text = "";
            }
            fieldError.text = "";
        }
        onClosed: editing = null

        ShadcnDialogContent {
            ColumnLayout {
                width: parent.width
                spacing: 10

                ShadcnDialogHeader {
                    Layout.fillWidth: true
                    ShadcnDialogTitle {
                        text: productDialog.editing ? "编辑产品" : "新建产品"
                    }
                    ShadcnDialogDescription {
                        text: productDialog.editing
                              ? "产品 Key 不可修改（设备的认证凭据依赖它）"
                              : "产品 Key 是设备认证的一部分，创建后不可修改"
                    }
                }

                ShadcnLabel { text: "产品 Key *"; size: ShadcnLabel.Size.Small }
                ShadcnInput {
                    id: fieldKey
                    Layout.fillWidth: true
                    placeholderText: "如 factory_sensor"
                    readOnly: productDialog.editing !== null
                }

                ShadcnLabel { text: "产品名称"; size: ShadcnLabel.Size.Small }
                ShadcnInput {
                    id: fieldName
                    Layout.fillWidth: true
                    placeholderText: "如 工厂传感器"
                }

                ShadcnLabel { text: "描述"; size: ShadcnLabel.Size.Small }
                ShadcnInput {
                    id: fieldDesc
                    Layout.fillWidth: true
                    placeholderText: "可选"
                }

                ShadcnLabel {
                    id: fieldError
                    Layout.fillWidth: true
                    size: ShadcnLabel.Size.Small
                    variant: ShadcnLabel.Variant.Destructive
                    wrapMode: Text.Wrap
                }
            }

            footer: ShadcnDialogFooter {
                ShadcnButton {
                    text: "取消"
                    variant: ShadcnButton.Variant.Outline
                    size: ShadcnButton.Size.Small
                    onClicked: productDialog.close()
                }
                ShadcnButton {
                    text: productDialog.editing ? "保存" : "创建"
                    size: ShadcnButton.Size.Small
                    onClicked: {
                        if (!dataManager) return;
                        if (fieldKey.text.trim() === "") {
                            fieldError.text = "产品 Key 必填";
                            return;
                        }
                        if (productDialog.editing)
                            dataManager.updateProduct(productDialog.editing.id,
                                                      fieldName.text.trim(), fieldDesc.text.trim());
                        else
                            dataManager.createProduct(fieldKey.text.trim(),
                                                      fieldName.text.trim(), fieldDesc.text.trim());
                        productDialog.close();
                    }
                }
            }
        }
    }

    // ===== 删除确认 =====
    ShadcnDialog {
        id: deleteDialog

        property int pid: 0
        property string pkey: ""
        property int devices: 0

        modal: true

        ShadcnDialogContent {
            ColumnLayout {
                width: parent.width
                spacing: 10

                ShadcnDialogHeader {
                    Layout.fillWidth: true
                    ShadcnDialogTitle { text: "删除产品" }
                    ShadcnDialogDescription {
                        text: deleteDialog.devices > 0
                              ? "该产品下还有 " + deleteDialog.devices + " 台设备，无法删除。请先迁移或删除这些设备。"
                              : "确定要删除产品 \"" + deleteDialog.pkey + "\" 吗？此操作不可撤销。"
                    }
                }
            }

            footer: ShadcnDialogFooter {
                ShadcnButton {
                    text: "关闭"
                    variant: ShadcnButton.Variant.Outline
                    size: ShadcnButton.Size.Small
                    onClicked: deleteDialog.close()
                }
                ShadcnButton {
                    text: "删除"
                    variant: ShadcnButton.Variant.Destructive
                    size: ShadcnButton.Size.Small
                    enabled: deleteDialog.devices === 0
                    onClicked: {
                        if (dataManager) dataManager.deleteProduct(deleteDialog.pid);
                        deleteDialog.close();
                    }
                }
            }
        }
    }

    // Toast
    Rectangle {
        id: toastBar
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        width: toastLabel.implicitWidth + 32
        height: 36
        radius: 8
        color: root.toastColor
        opacity: root.toastText ? 1.0 : 0.0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: 200 } }

        ShadcnLabel {
            id: toastLabel
            anchors.centerIn: parent
            text: root.toastText
            color: theme.primaryForeground
        }
        Timer {
            id: toastTimer
            interval: 2500
            onTriggered: root.toastText = ""
        }
    }
}
