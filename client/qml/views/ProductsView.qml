import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtShadcn
import "../components"

// 产品管理：products 表 CRUD
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
    ListModel { id: propRows }

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

    // ═══════════════ 内联组件 ═══════════════

    component Pill: Rectangle {
        id: pillRoot
        property string text: ""
        property color fg: theme.foreground
        property color bg: theme.muted

        implicitWidth: pillText.implicitWidth + 20
        implicitHeight: 24
        radius: 12
        color: pillRoot.bg

        Text {
            id: pillText
            anchors.centerIn: parent
            text: pillRoot.text
            color: pillRoot.fg
            font.pixelSize: 12
        }
    }

    component IconBtn: Rectangle {
        id: iconBtnRoot
        property string iconName: ""
        property color iconColor: theme.mutedForeground
        property color iconHoverColor: theme.foreground
        property color hoverBg: Qt.alpha(theme.foreground, 0.06)
        signal clicked()

        implicitWidth: 30
        implicitHeight: 30
        radius: 6
        color: btnHover.hovered ? iconBtnRoot.hoverBg : "transparent"

        ShadcnIcon {
            anchors.centerIn: parent
            name: iconBtnRoot.iconName
            size: 14
            color: btnHover.hovered ? iconBtnRoot.iconHoverColor
                                    : iconBtnRoot.iconColor
        }
        MouseArea {
            id: btnHover
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: iconBtnRoot.clicked()
        }
    }

    // ═══════════════ 布局 ═══════════════

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ShadcnLabel {
                text: "产品管理"
                size: ShadcnLabel.Size.Large
                font.bold: true
            }
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

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 10
            color: theme.background
            border.width: 1
            border.color: Qt.alpha(theme.foreground, 0.08)

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    color: Qt.alpha(theme.foreground, 0.03)
                    topLeftRadius: 10
                    topRightRadius: 10

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        spacing: 16

                        ShadcnLabel { Layout.fillWidth: true; Layout.minimumWidth: 200; text: "产品名称"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        ShadcnLabel { Layout.preferredWidth: 170; text: "产品 Key"; size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        ShadcnLabel { Layout.preferredWidth: 90;  text: "设备数";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        ShadcnLabel { Layout.preferredWidth: 110; text: "物模型";   size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        ShadcnLabel { Layout.preferredWidth: 260; text: "描述";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                        ShadcnLabel { Layout.preferredWidth: 108; text: "操作";     size: ShadcnLabel.Size.Small; variant: ShadcnLabel.Variant.Muted }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: Qt.alpha(theme.foreground, 0.08)
                }

                ListView {
                    id: productList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: productRows
                    spacing: 0

                    QQC.ScrollBar.vertical: QQC.ScrollBar {
                        active: true
                        policy: QQC.ScrollBar.AsNeeded
                    }

                    delegate: Rectangle {
                        id: rowDelegate

                        required property int index
                        required property int pid
                        required property string productKey
                        required property string productName
                        required property string description
                        required property int deviceCount
                        required property int propCount

                        width: productList.width
                        height: 56
                        color: rowHover.hovered
                               ? Qt.alpha(theme.foreground, 0.025)
                               : "transparent"

                        HoverHandler { id: rowHover }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 20
                            anchors.rightMargin: 20
                            height: 1
                            color: Qt.alpha(theme.foreground, 0.05)
                            visible: rowDelegate.index !== productList.count - 1
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 20
                            anchors.rightMargin: 20
                            spacing: 16

                            ShadcnLabel {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 200
                                text: productName || productKey
                                size: ShadcnLabel.Size.Small
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            ShadcnLabel {
                                Layout.preferredWidth: 170
                                text: productKey
                                size: ShadcnLabel.Size.Small
                                color: theme.mutedForeground
                                font.family: "Monaco"
                                elide: Text.ElideRight
                            }

                            Item {
                                Layout.preferredWidth: 90
                                Pill {
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: deviceCount + " 台"
                                    fg: theme.foreground
                                    bg: Qt.alpha(theme.foreground, 0.06)
                                }
                            }

                            Item {
                                Layout.preferredWidth: 110
                                Pill {
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: propCount > 0 ? (propCount + " 项属性") : "自由模式"
                                    fg: propCount > 0 ? theme.primary : theme.mutedForeground
                                    bg: propCount > 0
                                        ? Qt.alpha(theme.primary, 0.10)
                                        : Qt.alpha(theme.foreground, 0.05)
                                }
                            }

                            ShadcnLabel {
                                Layout.preferredWidth: 260
                                text: description || "—"
                                size: ShadcnLabel.Size.Small
                                color: theme.mutedForeground
                                elide: Text.ElideRight
                            }

                            RowLayout {
                                Layout.preferredWidth: 108
                                spacing: 2

                                IconBtn {
                                    iconName: "book"
                                    onClicked: {
                                        modelDialog.pkey = productKey;
                                        modelDialog.pname = productName || productKey;
                                        modelDialog.open();
                                    }
                                }
                                IconBtn {
                                    iconName: "pencil"
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
                                IconBtn {
                                    iconName: "trash-2"
                                    iconHoverColor: theme.destructive
                                    hoverBg: Qt.alpha(theme.destructive, 0.08)
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

                    Column {
                        anchors.centerIn: parent
                        spacing: 10
                        visible: productList.count === 0

                        ShadcnIcon {
                            anchors.horizontalCenter: parent.horizontalCenter
                            name: "tag"
                            size: 36
                            color: theme.mutedForeground
                        }
                        ShadcnLabel {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "暂无产品"
                            size: ShadcnLabel.Size.Small
                            variant: ShadcnLabel.Variant.Muted
                        }
                        ShadcnButton {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "新建第一个产品"
                            iconName: "plus"
                            size: ShadcnButton.Size.Small
                            variant: ShadcnButton.Variant.Outline
                            onClicked: {
                                productDialog.editing = null;
                                productDialog.open();
                            }
                        }
                    }
                }
            }
        }
    }

    // ═══════════════ 新建 / 编辑产品 ═══════════════
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

    // ═══════════════ 删除确认 ═══════════════
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

    // ═══════════════ 物模型属性管理 ═══════════════
    ShadcnDialog {
        id: modelDialog

        property string pkey: ""
        property string pname: ""

        width: 620
        // ⚠️ 不要在这里设 height：ShadcnDialogContent 自带 implicitHeight
        // （body 超高由 maxHeight=0.85×窗口封顶并滚动）。显式撑高会让 footer
        // 条（锚定在 contentItem 底部）与弹窗底边脱离，底下露出背景白边。

        modal: true

        readonly property int  propCount:     propRows.count
        readonly property bool whitelistMode: propRows.count > 0

        function refresh() {
            propRows.clear();
            if (!dataManager || !dataManager.httpClient) return;
            dataManager.httpClient.propList(pkey);
        }

        Connections {
            target: (typeof dataManager !== "undefined") ? dataManager : null

            function onPropListFetched(productKey, list) {
                if (productKey !== modelDialog.pkey) return;
                propRows.clear();
                if (!list) return;
                for (var i = 0; i < list.length; ++i) {
                    var p = list[i];
                    propRows.append({
                        "identifier": p.identifier  || "",
                        "propType":   p.prop_type   || "unknown",
                        "propDesc":   p.description || ""
                    });
                }
            }

            function onPropChanged(productKey, success) {
                if (productKey !== modelDialog.pkey) return;
                if (!success) {
                    root.showToast("物模型操作失败", theme.destructive);
                    return;
                }
                modelDialog.refresh();
                if (dataManager) dataManager.refreshProducts();
            }
        }

        onOpened: refresh()
        onClosed: propRows.clear()

        ShadcnDialogContent {
            ColumnLayout {
                width: parent.width
                spacing: 10

                // ── 标题 + badge
                RowLayout {
                    Layout.fillWidth: true
                    Layout.rightMargin: 32
                    spacing: 10

                    Text {
                        Layout.fillWidth: true
                        text: "物模型 · " + (modelDialog.pname || modelDialog.pkey)
                        color: theme.foreground
                        font.pixelSize: 16
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        implicitWidth: badgeText.implicitWidth + 18
                        implicitHeight: 24
                        radius: 12
                        color: modelDialog.whitelistMode
                               ? Qt.alpha(theme.primary, 0.10)
                               : Qt.alpha(theme.foreground, 0.05)
                        Text {
                            id: badgeText
                            anchors.centerIn: parent
                            text: modelDialog.whitelistMode
                                  ? modelDialog.propCount + " 项属性"
                                  : "自由模式"
                            color: modelDialog.whitelistMode
                                   ? theme.primary : theme.mutedForeground
                            font.pixelSize: 12
                        }
                    }
                }

                // ── 描述行
                Text {
                    Layout.fillWidth: true
                    Layout.rightMargin: 32
                    wrapMode: Text.Wrap
                    text: modelDialog.whitelistMode
                          ? "白名单已启用：仅下列属性允许上报，其它字段将被拒绝入库。"
                          : "自由模式：未定义属性时，设备上报的所有字段都会入库。添加第一条属性后白名单即生效。"
                    color: theme.mutedForeground
                    font.pixelSize: 12
                    lineHeight: 1.4
                }

                // ── 已定义属性 header
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: "已定义属性"
                        color: theme.foreground
                        font.pixelSize: 13
                        font.bold: true
                    }
                    Rectangle {
                        implicitWidth: countText.implicitWidth + 14
                        implicitHeight: 20
                        radius: 10
                        color: Qt.alpha(theme.foreground, 0.06)
                        Text {
                            id: countText
                            anchors.centerIn: parent
                            text: modelDialog.propCount
                            color: theme.mutedForeground
                            font.pixelSize: 11
                        }
                    }
                    Item { Layout.fillWidth: true }
                }

                // ── 已定义属性列表
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 220   /* 列表区固定高，超出滚动 */
                    radius: 8
                    color: theme.background
                    border.width: 1
                    border.color: Qt.alpha(theme.foreground, 0.08)

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            color: Qt.alpha(theme.foreground, 0.03)
                            topLeftRadius: 8
                            topRightRadius: 8

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                spacing: 10

                                Text { Layout.preferredWidth: 150; text: "标识符"; color: theme.mutedForeground; font.pixelSize: 12 }
                                Text { Layout.preferredWidth: 90;  text: "类型";   color: theme.mutedForeground; font.pixelSize: 12 }
                                Text { Layout.fillWidth: true;     text: "描述";   color: theme.mutedForeground; font.pixelSize: 12 }
                                Item { Layout.preferredWidth: 40 }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 1
                            color: Qt.alpha(theme.foreground, 0.08)
                        }

                        ListView {
                            id: propList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 0
                            model: propRows

                            QQC.ScrollBar.vertical: QQC.ScrollBar {
                                active: true
                                policy: QQC.ScrollBar.AsNeeded
                            }

                            delegate: Rectangle {
                                required property int index
                                required property string identifier
                                required property string propType
                                required property string propDesc

                                width: propList.width
                                height: 40
                                color: propHover.hovered
                                       ? Qt.alpha(theme.foreground, 0.025)
                                       : "transparent"

                                HoverHandler { id: propHover }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 14
                                    spacing: 10

                                    Text {
                                        Layout.preferredWidth: 150
                                        text: identifier
                                        color: theme.foreground
                                        font.pixelSize: 13
                                        font.family: "Monaco"
                                        elide: Text.ElideRight
                                    }
                                    Rectangle {
                                        Layout.preferredWidth: 90
                                        implicitHeight: 20
                                        radius: 10
                                        color: Qt.alpha(theme.foreground, 0.06)
                                        Text {
                                            anchors.centerIn: parent
                                            text: propType
                                            color: theme.mutedForeground
                                            font.pixelSize: 11
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: propDesc || "—"
                                        color: theme.mutedForeground
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                    Item {
                                        Layout.preferredWidth: 40
                                        Layout.fillHeight: true
                                        IconBtn {
                                            anchors.centerIn: parent
                                            iconName: "trash-2"
                                            iconHoverColor: theme.destructive
                                            hoverBg: Qt.alpha(theme.destructive, 0.08)
                                            onClicked: {
                                                dataManager.httpClient.propDelete(
                                                    modelDialog.pkey, identifier);
                                            }
                                        }
                                    }
                                }
                            }

                            Column {
                                anchors.centerIn: parent
                                visible: propList.count === 0
                                spacing: 4

                                ShadcnIcon {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    name: "book"
                                    size: 22
                                    color: theme.mutedForeground
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "暂无属性定义（自由模式）"
                                    color: theme.mutedForeground
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }
                }

                // ── 新增属性区
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 128
                    radius: 8
                    color: Qt.alpha(theme.primary, 0.04)
                    border.width: 1
                    border.color: Qt.alpha(theme.primary, 0.15)

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            ShadcnIcon {
                                name: "plus-circle"
                                size: 13
                                color: theme.primary
                            }
                            Text {
                                text: "新增属性"
                                color: theme.primary
                                font.pixelSize: 12
                                font.bold: true
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: "保存后立即生效"
                                color: theme.mutedForeground
                                font.pixelSize: 11
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            ShadcnInput {
                                id: propIdent
                                Layout.fillWidth: true
                                placeholderText: "标识符，如 temperature"
                            }
                            ShadcnSelect {
                                id: propTypeSelect
                                Layout.preferredWidth: 120
                                model: ["number", "bool", "string"]
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            ShadcnInput {
                                id: propDesc
                                Layout.fillWidth: true
                                placeholderText: "描述（可选），如 DHT11 温度"
                            }
                            ShadcnButton {
                                text: "保存"
                                iconName: "check"
                                size: ShadcnButton.Size.Small
                                Layout.preferredWidth: 96
                                onClicked: {
                                    var idv = propIdent.text.trim();
                                    if (idv === "") {
                                        root.showToast("标识符不能为空", theme.destructive);
                                        return;
                                    }
                                    dataManager.httpClient.propAdd(
                                        modelDialog.pkey, idv,
                                        propTypeSelect.model[propTypeSelect.currentIndex] || "number",
                                        propDesc.text.trim());
                                    propIdent.text = "";
                                    propDesc.text = "";
                                }
                            }
                        }
                    }
                }
            }

            footer: ShadcnDialogFooter {
                ShadcnButton {
                    text: "关闭"
                    iconName: "x"
                    variant: ShadcnButton.Variant.Outline
                    onClicked: modelDialog.close()
                }
            }
        }
    }

    // Toast
    Rectangle {
        id: toastBar
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
        width: toastLabel.implicitWidth + 32
        height: 36
        radius: 8
        color: root.toastColor
        opacity: root.toastText ? 1.0 : 0.0
        visible: opacity > 0
        z: 100

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