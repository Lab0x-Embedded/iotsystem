import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import QtShadcn
import "../components"

// MQTT 接入指南
Rectangle {
    id: root

    QtShadcnTheme { id: theme }
    color: theme.background

    // 当前查看接入参数的设备（DeviceInfo gadget），null = 通用占位
    property var currentDevice: null
    property string devId: currentDevice ? currentDevice.id : "{device_id}"
    property string devPk: currentDevice ? (currentDevice.productKey || "{product_key}") : "{product_key}"
    property string devSecret: currentDevice ? currentDevice.deviceSecret : ""

    // 服务器 host：不能直接用绑定（serverUrl 很可能没有 NOTIFY 信号，
    // 或 dataManager 是后注册的 context property），改成显式更新。
    property string serverHost: ""

    function refreshServerHost() {
        var host = "";
        if (dataManager && dataManager.serverUrl) {
            var u = String(dataManager.serverUrl)
                    .replace(/^https?:\/\//, "")   // 去掉协议
                    .replace(/\/.*$/, "");         // 去掉路径
            var idx = u.indexOf(":");              // 去掉端口（IPv6 暂不支持）
            host = idx >= 0 ? u.substring(0, idx) : u;
        }
        if (host !== root.serverHost)
            root.serverHost = host;
    }

    Component.onCompleted: refreshServerHost()

    // 情况 1：serverUrl 有 NOTIFY 信号 —— 走这条通路即可
    Connections {
        target: dataManager
        ignoreUnknownSignals: true     // 若没有 serverUrlChanged 信号，安静忽略
        function onServerUrlChanged() { root.refreshServerHost() }
    }

    // 情况 2/3 兜底：不管有没有 NOTIFY、dataManager 何时就绪，
    // 低频轮询都能把值最终同步上来。开销极小。
    Timer {
        interval: 1500
        running: true
        repeat: true
        onTriggered: root.refreshServerHost()
    }

    readonly property bool hostIsLoopback: serverHost === "" || serverHost === "127.0.0.1" || serverHost === "localhost"
    readonly property string hostText: serverHost !== "" ? serverHost : "<服务器IP>"

    function showDevice(deviceId) {
        currentDevice = null;
        if (!deviceId || !deviceModel) return;
        for (var i = 0; i < deviceModel.rowCount(); i++) {
            var d = deviceModel.deviceAt(i);
            if (d && d.id === deviceId) {
                currentDevice = d;
                return;
            }
        }
    }

    property string toastText: ""
    function showToast(msg) { toastText = msg; toastTimer.restart(); }

    readonly property color hairline: Qt.alpha(theme.foreground, 0.08)
    readonly property color softFill: Qt.alpha(theme.primary, 0.10)

    // ── 内联组件 ──────────────────────────────────────────────

    // 步骤卡片：圆角 + 细描边 + 左上角主色徽章
    component StepCard: Rectangle {
        id: card

        property int step: 1
        property string title: ""
        property string note: ""
        default property alias body: bodyCol.data

        Layout.fillWidth: true
        implicitHeight: cardCol.implicitHeight + 40
        radius: 14
        color: theme.background
        border.width: 1
        border.color: root.hairline

        ColumnLayout {
            id: cardCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    width: 28; height: 28; radius: 9
                    color: root.softFill
                    ShadcnLabel {
                        anchors.centerIn: parent
                        text: card.step
                        color: theme.primary
                        font.bold: true
                        size: ShadcnLabel.Size.Small
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    ShadcnLabel {
                        text: card.title
                        size: ShadcnLabel.Size.Medium
                        font.bold: true
                    }
                    ShadcnLabel {
                        visible: card.note !== ""
                        text: card.note
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                    }
                }
            }

            ColumnLayout {
                id: bodyCol
                Layout.fillWidth: true
                spacing: 12
            }
        }
    }

    // 参数行：名称（muted）+ 值（等宽，可复制）
    component FieldRow: RowLayout {
        id: fieldRow

        property string name: ""
        property string value: ""
        property bool copyable: true

        Layout.fillWidth: true
        spacing: 12

        ShadcnLabel {
            Layout.preferredWidth: 92
            Layout.alignment: Qt.AlignVCenter
            text: fieldRow.name
            size: ShadcnLabel.Size.Small
            variant: ShadcnLabel.Variant.Muted
        }
        CopyableText {
            Layout.fillWidth: true
            text: fieldRow.value
            showCopyIcon: fieldRow.copyable
            fontFamily: "Monaco"
        }
    }

    // 代码块：等宽，右上角常显复制按钮
    component CodeBlock: Rectangle {
        id: codeBlock

        property string code: ""

        Layout.fillWidth: true
        implicitHeight: codeText.implicitHeight + 28
        radius: 10
        color: theme.muted
        border.width: 1
        border.color: root.hairline

        TextEdit {
            id: codeText
            anchors.left: parent.left
            anchors.right: copyBtn.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: 14
            anchors.topMargin: 14
            anchors.bottomMargin: 14
            anchors.rightMargin: 8

            text: codeBlock.code
            readOnly: true
            selectByMouse: true
            selectByKeyboard: true
            color: theme.foreground
            font.family: "Monaco"
            font.pixelSize: 12
            wrapMode: TextEdit.Wrap
            clip: true
            verticalAlignment: TextEdit.AlignVCenter
        }

        Item {
            id: copyBtn
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 8
            width: 28; height: 28

            Rectangle {
                anchors.fill: parent
                radius: 7
                color: copyHover.containsMouse
                       ? Qt.alpha(theme.foreground, 0.10)
                       : "transparent"
            }
            ShadcnIcon {
                anchors.centerIn: parent
                name: "copy"
                size: 13
                color: copyHover.containsMouse ? theme.foreground : theme.mutedForeground
            }
            MouseArea {
                id: copyHover
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    codeText.selectAll();
                    codeText.copy();
                    codeText.deselect();
                    root.showToast("已复制到剪贴板");
                }
            }
        }
    }

    // ── Toast ─────────────────────────────────────────────────

    Rectangle {
        id: toastBar
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
        color: theme.foreground
        height: 38
        width: toastLabel.implicitWidth + 36
        radius: 10
        opacity: root.toastText ? 1.0 : 0.0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: 180 } }

        ShadcnLabel {
            id: toastLabel
            anchors.centerIn: parent
            text: root.toastText
            color: theme.background
        }
        Timer {
            id: toastTimer
            interval: 1800
            onTriggered: root.toastText = ""
        }
    }

    // ── 页面 ──────────────────────────────────────────────────

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        // ===== 头部 =====
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                spacing: 2
                ShadcnLabel {
                    text: "MQTT 接入指南"
                    size: ShadcnLabel.Size.Large
                    font.bold: true
                }
                ShadcnLabel {
                    text: "按以下步骤将设备接入平台"
                    size: ShadcnLabel.Size.Small
                    variant: ShadcnLabel.Variant.Muted
                }
            }
            Item { Layout.fillWidth: true }

            Rectangle {
                visible: root.currentDevice !== null
                implicitWidth: devChip.implicitWidth + 24
                implicitHeight: 30
                radius: 15
                color: root.softFill
                ShadcnLabel {
                    id: devChip
                    anchors.centerIn: parent
                    text: root.currentDevice
                          ? root.currentDevice.id
                            + (root.currentDevice.name ? " · " + root.currentDevice.name : "")
                          : ""
                    size: ShadcnLabel.Size.Small
                    color: theme.primary
                    font.bold: true
                }
            }
        }

        // ===== 正文（可滚动） =====
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: contentCol.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            QQC.ScrollBar.vertical: QQC.ScrollBar {}

            ColumnLayout {
                id: contentCol
                width: parent.width
                spacing: 16

                // ── 1. 连接参数 ──
                StepCard {
                    step: 1
                    title: "连接参数（CONNECT）"
                    note: "MQTT 3.1 / 3.1.1 / 5.0 · Keepalive 30–120s"

                    FieldRow {
                        name: "Broker 地址"
                        value: "tcp://" + root.hostText + ":1883"
                    }

                    ShadcnLabel {
                        visible: root.hostIsLoopback
                        Layout.fillWidth: true
                        text: "当前客户端连的是回环地址（127.0.0.1），局域网设备无法访问 —— 请在左下角「连接服务器」里改用服务器的局域网 IP 登录，上面的地址会自动更新。"
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                        wrapMode: Text.Wrap
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: credCol.implicitHeight + 24
                        radius: 10
                        color: theme.muted

                        ColumnLayout {
                            id: credCol
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            FieldRow {
                                name: "client_id"
                                value: root.currentDevice
                                       ? "esp8266_" + root.devId
                                       : "自定义唯一值（≤128 字节）"
                            }
                            FieldRow {
                                name: "username"
                                value: root.currentDevice ? root.devPk : "product_key"
                            }
                            FieldRow {
                                name: "password"
                                value: root.currentDevice
                                       ? (root.devSecret !== "" ? root.devSecret : "（未设置）")
                                       : "device_secret"
                            }
                        }
                    }

                    ShadcnLabel {
                        Layout.fillWidth: true
                        text: "username = 设备所属产品的 ProductKey，password = 设备密钥，服务端查库校验（状态需为 registered/active；数据库不可用时认证失败）。同 client_id 重复连接会自动踢掉旧连接；1.5 倍 Keepalive 超时未收到报文判定离线。设备密钥可在「设备详情」页查看。"
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                        wrapMode: Text.Wrap
                    }
                }

                // ── 2. 数据上报 ──
                StepCard {
                    step: 2
                    title: "数据上报（PUBLISH）"
                    note: "QoS 0 或 1"

                    FieldRow {
                        name: "topic"
                        value: "devices/" + root.devId + "/data"
                    }

                    ShadcnLabel {
                        Layout.fillWidth: true
                        text: "payload 为 JSON，一次可携带多个数据点（metric 名自定义，ts 为 Unix 秒）："
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                        wrapMode: Text.Wrap
                    }

                    CodeBlock {
                        code: JSON.stringify({
                            device_id: root.devId,
                            datapoints: [
                                { metric: "temperature", value: 25.6, ts: 1759709400 },
                                { metric: "humidity",    value: 61.2, ts: 1759709400 }
                            ]
                        }, null, 2)
                    }
                }

                // ── 3. 指令接收 ──
                StepCard {
                    step: 3
                    title: "指令接收（SUBSCRIBE）"
                    note: "QoS 1"

                    FieldRow {
                        name: "订阅 topic"
                        value: "cmd/" + root.devId + "/exec"
                    }

                    CodeBlock {
                        code: '{ "cmd": "set_relay", "payload": { "relay": "on" } }'
                    }

                    ShadcnLabel {
                        Layout.fillWidth: true
                        text: "平台下发的指令 payload 如上（设备端解析 cmd 与 payload 字段执行）。设备离线期间的指令进入离线队列，重新连接并订阅后自动重放。"
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                        wrapMode: Text.Wrap
                    }
                }

                // ── 4. 快速验证 ──
                StepCard {
                    step: 4
                    title: "快速验证"
                    note: "mosquitto / 端到端模拟脚本"

                    ShadcnLabel {
                        Layout.fillWidth: true
                        text: "用 mosquitto 客户端模拟该设备（需安装 mosquitto-clients），订阅指令："
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                        wrapMode: Text.Wrap
                    }

                    CodeBlock {
                        code: root.currentDevice
                              ? "mosquitto_sub -h " + root.hostText + " -p 1883 -i esp8266_" + root.devId
                                + " -u " + root.devPk + " -P " + (root.devSecret || "<secret>")
                                + " -t 'cmd/" + root.devId + "/exec' -q 1"
                              : "mosquitto_sub -h " + root.hostText + " -p 1883 -i dev_001 -u factory_sensor -P secret_001 -t 'cmd/dev_001/exec' -q 1"
                    }

                    ShadcnLabel {
                        Layout.fillWidth: true
                        text: "上报一条温度数据："
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                        wrapMode: Text.Wrap
                    }

                    CodeBlock {
                        code: root.currentDevice
                              ? "mosquitto_pub -h " + root.hostText + " -p 1883 -i esp8266_" + root.devId
                                + " -u " + root.devPk + " -P " + (root.devSecret || "<secret>")
                                + " -t 'devices/" + root.devId + "/data' -q 1"
                                + " -m '{\"device_id\":\"" + root.devId + "\",\"datapoints\":[{\"metric\":\"temperature\",\"value\":25.6,\"ts\":1759709400}]}'"
                              : "mosquitto_pub -h " + root.hostText + " -p 1883 -i dev_001 -u factory_sensor -P secret_001 -t 'devices/dev_001/data' -q 1 -m '{\"device_id\":\"dev_001\",\"datapoints\":[{\"metric\":\"temperature\",\"value\":25.6,\"ts\":1759709400}]}'"
                    }

                    ShadcnLabel {
                        Layout.fillWidth: true
                        text: "或直接用仓库自带的端到端模拟脚本（无需安装任何客户端）："
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                        wrapMode: Text.Wrap
                    }

                    CodeBlock {
                        code: root.currentDevice
                              ? "python3 deploy/scripts/esp8266_e2e_test.py -d " + root.devId
                                + " --pk " + root.devPk
                                + " --secret " + (root.devSecret || "<secret>")
                              : "python3 deploy/scripts/esp8266_e2e_test.py"
                    }
                }

                // ── 5. ESP8266 AT 指令接入 ──
                StepCard {
                    step: 5
                    title: "ESP8266 AT 指令接入"
                    note: "ESP-AT v2.2+ · 挂 51/STM32 主控"

                    ShadcnLabel {
                        Layout.fillWidth: true
                        text: "ESP-01S 等模组需刷 ESP-AT v2.x 固件（出厂旧版 v1.7 无 MQTT 指令），在串口逐条发送以下指令即可完成联网 + 鉴权 + 订阅："
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                        wrapMode: Text.Wrap
                    }

                    CodeBlock {
                        code: root.currentDevice
                              ? "AT+CWMODE=1\n"
                                + "AT+CWJAP=\"<WiFi名>\",\"<WiFi密码>\"\n"
                                + "AT+MQTTUSERCFG=0,1,\"esp8266_" + root.devId + "\",\""
                                + root.devPk + "\",\""
                                + (root.devSecret || "<device_secret>") + "\",0,0,\"\"\n"
                                + "AT+MQTTCONNCFG=0,120,1,\"\",\"\",0,0\n"
                                + "AT+MQTTCONN=0,\"" + root.hostText + "\",1883,1\n"
                                + "AT+MQTTSUB=0,\"cmd/" + root.devId + "/exec\",1"
                              : "AT+CWMODE=1\n"
                                + "AT+CWJAP=\"<WiFi名>\",\"<WiFi密码>\"\n"
                                + "AT+MQTTUSERCFG=0,1,\"esp8266_dev_001\",\"factory_sensor\",\"secret_001\",0,0,\"\"\n"
                                + "AT+MQTTCONNCFG=0,120,1,\"\",\"\",0,0\n"
                                + "AT+MQTTCONN=0,\"" + root.hostText + "\",1883,1\n"
                                + "AT+MQTTSUB=0,\"cmd/dev_001/exec\",1"
                    }

                    ShadcnLabel {
                        Layout.fillWidth: true
                        text: "连接成功后，用 AT+MQTTPUB 上报一条温度数据："
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                        wrapMode: Text.Wrap
                    }

                    CodeBlock {
                        // ts 用当前 Unix 秒（写死的历史时间戳会让"最后上报"显示成旧时间）
                        code: root.currentDevice
                              ? "AT+MQTTPUB=0,\"devices/" + root.devId + "/data\","
                                + "\"{\\\"device_id\\\":\\\"" + root.devId + "\\\","
                                + "\\\"datapoints\\\":[{\\\"metric\\\":\\\"temperature\\\","
                                + "\\\"value\\\":25.6,\\\"ts\\\":" + Math.floor(Date.now() / 1000) + "}]}\",1,0"
                              : "AT+MQTTPUB=0,\"devices/dev_001/data\","
                                + "\"{\\\"device_id\\\":\\\"dev_001\\\","
                                + "\\\"datapoints\\\":[{\\\"metric\\\":\\\"temperature\\\","
                                + "\\\"value\\\":25.6,\\\"ts\\\":" + Math.floor(Date.now() / 1000) + "}]}\",1,0"
                    }

                    ShadcnLabel {
                        Layout.fillWidth: true
                        text: "若 AT+MQTTPUB 返回 ERROR（data 里的转义引号在部分固件版本上解析失败），改用 MQTTPUBRAW —— 长度不受限、无需转义，等 > 提示符后原样发送 JSON 字节："
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                        wrapMode: Text.Wrap
                    }

                    CodeBlock {
                        // payload 用当前 Unix 秒，AT+MQTTPUBRAW 的长度声明必须是
                        // payload 的真实字节数 —— 声明错了 JSON 会被截断，直接上报失败
                        property string payload: root.currentDevice
                            ? '{"device_id":"' + root.devId + '","datapoints":'
                              + '[{"metric":"temperature","value":25.6,"ts":'
                              + Math.floor(Date.now() / 1000) + '}]}'
                            : '{"device_id":"dev_001","datapoints":'
                              + '[{"metric":"temperature","value":25.6,"ts":'
                              + Math.floor(Date.now() / 1000) + '}]}'

                        code: "AT+MQTTPUBRAW=0,\"devices/"
                              + (root.currentDevice ? root.devId : "dev_001") + "/data\","
                              + payload.length + ",1,0"
                              + "\n（等待 > 提示符后原样发送以下 " + payload.length + " 字节，不转义、不加换行）\n"
                              + payload
                    }

                    ShadcnLabel {
                        Layout.fillWidth: true
                        text: "client_id 规则 esp8266_<设备ID>，username = ProductKey，password = DeviceSecret。AT+MQTTCONN 末位参数 1 表示掉线自动重连；订阅成功后平台下发的指令会以 +MQTTSUBRECV 形式从串口输出。OneNET 风格接入的协议细节见 docs/E2_OneNET兼容接入.md。"
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
}