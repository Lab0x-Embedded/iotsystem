import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root

    property color color: "#3b82f6"
    property bool isDark: true
    property string label: ""
    property real maxValue: 100
    property real minValue: 0
    property string unit: ""
    property real value: 0

    height: 180
    width: 180

    Component.onCompleted: {
        canvas.requestPaint();
    }

    Canvas {
        id: canvas

        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d");

            ctx.reset();

            var size = Math.min(width, height);

            var centerX = width / 2;

            var centerY = height / 2;

            var radius = size / 2 - 18;

            /*
             * 背景轨道
             */

            ctx.beginPath();

            ctx.arc(centerX, centerY, radius, Math.PI * 0.75, Math.PI * 2.25, false);

            ctx.strokeStyle = root.isDark ? "#313244" : "#e5e7eb";

            ctx.lineWidth = 14;

            ctx.lineCap = "round";

            ctx.stroke();

            /*
             * 当前值
             */

            var progress = (root.value - root.minValue) / (root.maxValue - root.minValue);

            progress = Math.max(0, Math.min(1, progress));

            var startAngle = Math.PI * 0.75;

            var endAngle = startAngle + Math.PI * 1.5 * progress;

            ctx.beginPath();

            ctx.arc(centerX, centerY, radius, startAngle, endAngle, false);

            ctx.strokeStyle = root.color;

            ctx.lineWidth = 14;

            ctx.lineCap = "round";

            ctx.stroke();
        }

        Connections {
            function onColorChanged() {
                canvas.requestPaint();
            }
            function onIsDarkChanged() {
                canvas.requestPaint();
            }
            function onValueChanged() {
                canvas.requestPaint();
            }

            target: root
        }
    }

    /*
     * 中间数据
     */

    Column {
        anchors.centerIn: parent
        spacing: 3

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            color: root.color
            font.bold: true
            font.pixelSize: 26
            text: root.value.toFixed(1)
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            color: root.isDark ? "#a6adc8" : "#64748b"
            font.pixelSize: 12
            text: root.unit
        }
    }

    /*
     * 底部名称
     */

    Label {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
        color: root.isDark ? "#cdd6f4" : "#334155"
        font.pixelSize: 13
        text: root.label
    }
}
