import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15

Rectangle {
    id: root

    property bool isDark: true
    property int maxPoints: 60
    property bool hasData: false

    // 按指标分组的数据 { "temperature": [{value, timestamp}, ...], "humidity": [...] }
    property var metricData: ({})

    // 指标颜色映射
    property var metricColors: ({
        "temperature": "#f38ba8",
        "humidity": "#3874F7",
        "pressure": "#a6e3a1",
        "voltage": "#fab387",
        "current": "#f9e2af",
        "power": "#cba6f7",
        "energy": "#94e2d5"
    })

    // 指标中文名
    property var metricLabels: ({
        "temperature": "温度 °C",
        "humidity": "湿度 %",
        "pressure": "气压 Pa",
        "voltage": "电压 V",
        "current": "电流 A",
        "power": "功率 W",
        "energy": "电能 kWh"
    })

    function addDataPoint(metric, value, timestamp) {
        if (!metricData[metric]) {
            metricData[metric] = [];
        }
        var arr = metricData[metric];
        arr.push({"value": value, "timestamp": timestamp});
        if (arr.length > maxPoints) {
            arr.shift();
        }
        metricData[metric] = arr;  // 触发绑定更新
        hasData = true;
        canvas.requestPaint();
    }

    function clearData() {
        metricData = {};
        hasData = false;
        canvas.requestPaint();
    }

    color: "transparent"

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();

            var keys = Object.keys(metricData);
            if (keys.length === 0) return;

            var width = canvas.width;
            var height = canvas.height;
            var padding = 50;
            var legendH = 20;
            var chartH = height - padding * 2 - legendH;

            // 收集所有指标的 min/max
            var globalMin = Infinity;
            var globalMax = -Infinity;
            for (var k = 0; k < keys.length; k++) {
                var arr = metricData[keys[k]];
                for (var i = 0; i < arr.length; i++) {
                    if (arr[i].value < globalMin) globalMin = arr[i].value;
                    if (arr[i].value > globalMax) globalMax = arr[i].value;
                }
            }
            var range = globalMax - globalMin;
            if (range === 0) range = 1;

            // 绘制网格
            ctx.strokeStyle = root.isDark ? "#45475a" : "#e0e0e0";
            ctx.lineWidth = 1;
            ctx.setLineDash([3, 3]);
            for (var i = 0; i <= 4; i++) {
                var y = padding + chartH * i / 4;
                ctx.beginPath();
                ctx.moveTo(padding, y);
                ctx.lineTo(width - padding, y);
                ctx.stroke();

                // Y 轴标签
                var val = globalMax - (range * i / 4);
                ctx.fillStyle = root.isDark ? "#a6adc8" : "#666666";
                ctx.font = "10px sans-serif";
                ctx.textAlign = "right";
                ctx.fillText(val.toFixed(1), padding - 5, y + 4);
            }
            ctx.setLineDash([]);

            // 绘制每个指标的折线
            for (var k = 0; k < keys.length; k++) {
                var metric = keys[k];
                var arr = metricData[metric];
                if (arr.length < 2) continue;

                var color = metricColors[metric] || "#3874F7";
                ctx.strokeStyle = color;
                ctx.lineWidth = 2;
                ctx.beginPath();

                for (var i = 0; i < arr.length; i++) {
                    var x = padding + (width - 2 * padding) * i / (arr.length - 1);
                    var y = padding + chartH * (1 - (arr[i].value - globalMin) / range);
                    if (i === 0) ctx.moveTo(x, y);
                    else ctx.lineTo(x, y);
                }
                ctx.stroke();

                // 绘制数据点
                ctx.fillStyle = color;
                for (var i = 0; i < arr.length; i++) {
                    var x = padding + (width - 2 * padding) * i / (arr.length - 1);
                    var y = padding + chartH * (1 - (arr[i].value - globalMin) / range);
                    ctx.beginPath();
                    ctx.arc(x, y, 2.5, 0, Math.PI * 2);
                    ctx.fill();
                }
            }

            // 绘制图例
            var legendX = padding;
            var legendY = height - 15;
            ctx.font = "11px sans-serif";
            for (var k = 0; k < keys.length; k++) {
                var metric = keys[k];
                var color = metricColors[metric] || "#3874F7";
                var label = metricLabels[metric] || metric;

                // 色块
                ctx.fillStyle = color;
                ctx.fillRect(legendX, legendY - 8, 12, 3);

                // 文字
                ctx.fillStyle = root.isDark ? "#cdd6f4" : "#1e1e2e";
                ctx.textAlign = "left";
                ctx.fillText(label, legendX + 16, legendY);

                legendX += ctx.measureText(label).width + 36;
            }
        }
    }

    // Empty state
    Label {
        anchors.centerIn: parent
        color: root.isDark ? "#585b70" : "#999999"
        font.pixelSize: 14
        text: "等待数据..."
        visible: !hasData
    }
}
