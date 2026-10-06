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
                "deviceCount": p.device_count,
                "propCount": p.prop_count || 0
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
                    ShadcnLabel { Layout.preferredWidth: 90;  text: "物模型";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                    ShadcnLabel { Layout.preferredWidth: 200; text: "描述";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
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
                        required property int propCount

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
                            // 物模型状态一目了然：已定义 N 项 / 自由模式
                            ShadcnBadge {
                                Layout.preferredWidth: 90
                                text: propCount > 0 ? (propCount + " 项属性") : "自由模式"
                                variant: propCount > 0 ? ShadcnBadge.Variant.Secondary
                                                       : ShadcnBadge.Variant.Outline
                            }
                            ShadcnLabel {
                                Layout.preferredWidth: 200
                                text: description || "-"
                                size: ShadcnLabel.Size.Small
                                variant: ShadcnLabel.Variant.Muted
                                elide: Text.ElideRight
                            }
                            RowLayout {
                                spacing: 4

                                ShadcnButton {
                                    text: "物模型"
                                    iconName: "book"
                                    size: ShadcnButton.Size.ExtraSmall
                                    variant: ShadcnButton.Variant.Ghost
                                    onClicked: {
                                        modelDialog.pkey = productKey;
                                        modelDialog.pname = productName || productKey;
                                        modelDialog.open();
                                        modelDialog.refresh();
                                    }
                                }
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

    // ===== 物模型属性管理对话框 =====
    ShadcnDialog {
        id: modelDialog

        property string pkey: ""
        property string pname: ""
        property int propCount: 0

        width: 620

        function refresh() {
            propRows.clear();
            if (!dataManager || !dataManager.httpClient) return;
            dataManager.httpClient.propList(pkey);
        }

        onOpened: refresh()

        ShadcnDialogContent {
            ColumnLayout {
                width: parent.width
                spacing: 12

                ShadcnDialogHeader {
                    ShadcnDialogTitle {
                        text: "物模型 · " + modelDialog.pname
                              + "（" + modelDialog.propCount + " 项）"
                    }
                    ShadcnDialogDescription {
                        text: modelDialog.propCount > 0
                              ? "已启用白名单：仅下列属性允许上报，其它字段将被拒绝入库。"
                              : "自由模式：未定义任何属性，设备上报的所有字段都会入库。添加第一条属性后白名单即生效。"
                        wrapMode: Text.Wrap
                        Layout.preferredWidth: 540
                    }
                }

                // 已定义属性列表
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    ShadcnLabel { text: "已定义属性"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 200
                        radius: theme.radius
                        color: theme.muted

                        // 表头
                        RowLayout {
                            anchors.top: parent.top
                            anchors.leftMargin: 16
                            anchors.rightMargin: 40
                            anchors.topMargin: 6
                            anchors.left: parent.left
                            anchors.right: parent.right
                            spacing: 10
                            ShadcnLabel { Layout.preferredWidth: 150; text: "标识符"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            ShadcnLabel { Layout.preferredWidth: 70;  text: "类型";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            ShadcnLabel { Layout.fillWidth: true;     text: "描述";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                            ShadcnLabel { Layout.preferredWidth: 50;  text: "";     size: ShadcnLabel.Size.Small }
                        }

                    ListView {
                        id: propList
                        anchors.fill: parent
                        anchors.margins: 8
                        clip: true
                        spacing: 2
                        model: propRows

                        delegate: Rectangle {
                            required property int index
                            required property string identifier
                            required property string propType
                            required property string propDesc

                            width: propList.width
                            height: 34
                            radius: theme.radius - 2
                            color: "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 10

                                ShadcnLabel {
                                    Layout.preferredWidth: 150
                                    text: identifier
                                    size: ShadcnLabel.Size.Small
                                }
                                ShadcnBadge {
                                    Layout.preferredWidth: 70
                                    text: propType
                                    variant: propType === "bool"
                                             ? ShadcnBadge.Variant.Secondary
                                             : ShadcnBadge.Variant.Outline
                                }
                                ShadcnLabel {
                                    Layout.fillWidth: true
                                    text: propDesc || "-"
                                    size: ShadcnLabel.Size.Small
                                    variant: ShadcnLabel.Variant.Muted
                                    elide: Text.ElideRight
                                }
                                ShadcnButton {
                                    text: "删除"
                                    iconName: "trash-2"
                                    size: ShadcnButton.Size.ExtraSmall
                                    variant: ShadcnButton.Variant.Ghost
                                    onClicked: {
                                        dataManager.httpClient.propDelete(
                                            modelDialog.pkey, identifier);
                                    }
                                }
                            }
                        }

                        Column {
                            anchors.centerIn: parent
                            visible: propRows.count === 0
                            spacing: 6
                            ShadcnLabel {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "暂无属性定义（自由模式）"
                                size: ShadcnLabel.Size.Small
                                variant: ShadcnLabel.Variant.Muted
                            }
                            ShadcnLabel {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "在下方新增属性，保存后即启用白名单校验"
                                size: ShadcnLabel.Size.Small
                                variant: ShadcnLabel.Variant.Muted
                            }
                        }
                    }
                    }
                }

                // 新增属性（逐条保存，点「保存属性」即写入并生效）
                Rectangle {
                    Layout.fillWidth: true
                    radius: theme.radius
                    color: Qt.alpha(theme.primary, 0.06)
                    border.width: 1
                    border.color: Qt.alpha(theme.primary, 0.25)

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10

                        ShadcnLabel { text: "新增属性"; size: ShadcnLabel.Size.Small; font.bold: true }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            ColumnLayout {
                                Layout.preferredWidth: 180
                                spacing: 4
                                ShadcnLabel { text: "标识符 *"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnInput {
                                    id: propIdent
                                    Layout.fillWidth: true
                                    placeholderText: "如 temperature"
                                }
                            }
                            ColumnLayout {
                                Layout.preferredWidth: 110
                                spacing: 4
                                ShadcnLabel { text: "类型 *"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnSelect {
                                    id: propType
                                    Layout.fillWidth: true
                                    model: ["number", "bool", "string"]
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                ShadcnLabel { text: "描述"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                                ShadcnInput {
                                    id: propDesc
                                    Layout.fillWidth: true
                                    placeholderText: "可选，如 DHT11温度"
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            ShadcnLabel {
                                text: "保存后立即生效（服务端 30s 内全量生效）"
                                size: ShadcnLabel.Size.Small
                                variant: ShadcnLabel.Variant.Muted
                            }
                            Item { Layout.fillWidth: true }
                            ShadcnButton {
                                text: "保存属性"
                                iconName: "check"
                                size: ShadcnButton.Size.Small
                                onClicked: {
                                    var idv = propIdent.text.trim();
                                    if (idv === "") {
                                        root.showToast("标识符不能为空", theme.destructive);
                                        return;
                                    }
                                    dataManager.httpClient.propAdd(
                                        modelDialog.pkey, idv,
                                        propType.model[propType.currentIndex] || "number",
                                        propDesc.text.trim());
                                    propIdent.text = "";
                                    propDesc.text = "";
                                }
                            }
                        }
                    }
                    Layout.preferredHeight: 140
                }
            }

            footer: ShadcnDialogFooter {
                ShadcnButton {
                    text: "关闭"
                    variant: ShadcnButton.Variant.Outline
                    onClicked: modelDialog.close()
                }
            }
        }
    }

    ListModel { id: propRows }

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
