import QtQuick
import QtShadcn

// 实时折线图 (Canvas 自绘)
// 数据源: 轮询 query_history → setPoints(metric, points) 全量灌入
Rectangle {
    id: root

    property int maxPoints: 60
    property bool hasData: false
    property var metricData: ({})

    // 指标颜色映射 (语义色, 随主题不变化以保证可读性)
    property var metricColors: ({
        "temperature": "#f87171",
        "humidity": "#60a5fa",
        "pressure": "#34d399",
        "voltage": "#fbbf24",
        "current": "#f472b6",
        "power": "#a78bfa",
        "battery": "#facc15"
    })

    property var metricLabels: ({
        "temperature": "温度 °C",
        "humidity": "湿度 %",
        "pressure": "气压 Pa",
        "voltage": "电压 V",
        "current": "电流 A",
        "power": "功率 W",
        "battery": "电量 %"
    })

    QtShadcnTheme { id: theme }

    function addDataPoint(metric, value, timestamp) {
        if (!metricData[metric]) metricData[metric] = [];
        var arr = metricData[metric];
        arr.push({ value: value, timestamp: timestamp });
        if (arr.length > maxPoints) arr.shift();
        metricData[metric] = arr;
        hasData = true;
        canvas.requestPaint();
    }

    // 轮询模式: 全量设置某个指标的历史点 [{ts, value}]
    function setPoints(metric, points) {
        var arr = [];
        for (var i = 0; i < points.length; i++) {
            arr.push({ value: points[i].value, timestamp: points[i].ts });
        }
        if (arr.length > maxPoints) {
            arr = arr.slice(arr.length - maxPoints);
        }
        metricData[metric] = arr;
        hasData = Object.keys(metricData).length > 0;
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

            // 网格
            ctx.strokeStyle = theme.border;
            ctx.lineWidth = 1;
            ctx.setLineDash([3, 3]);
            for (var gi = 0; gi <= 4; gi++) {
                var y = padding + chartH * gi / 4;
                ctx.beginPath();
                ctx.moveTo(padding, y);
                ctx.lineTo(width - padding, y);
                ctx.stroke();
                var val = globalMax - (range * gi / 4);
                ctx.fillStyle = theme.mutedForeground;
                ctx.font = "10px sans-serif";
                ctx.textAlign = "right";
                ctx.fillText(val.toFixed(1), padding - 5, y + 4);
            }
            ctx.setLineDash([]);

            // 折线
            for (var m = 0; m < keys.length; m++) {
                var metric = keys[m];
                var pts = metricData[metric];
                if (pts.length < 2) continue;

                var color = root.metricColors[metric] || theme.primary;
                ctx.strokeStyle = color;
                ctx.lineWidth = 2;
                ctx.beginPath();
                for (var p = 0; p < pts.length; p++) {
                    var x = padding + (width - 2 * padding) * p / (pts.length - 1);
                    var py = padding + chartH * (1 - (pts[p].value - globalMin) / range);
                    if (p === 0) ctx.moveTo(x, py);
                    else ctx.lineTo(x, py);
                }
                ctx.stroke();

                ctx.fillStyle = color;
                for (var p2 = 0; p2 < pts.length; p2++) {
                    var x2 = padding + (width - 2 * padding) * p2 / (pts.length - 1);
                    var py2 = padding + chartH * (1 - (pts[p2].value - globalMin) / range);
                    ctx.beginPath();
                    ctx.arc(x2, py2, 2.5, 0, Math.PI * 2);
                    ctx.fill();
                }
            }

            // 图例
            var legendX = padding;
            var legendY = height - 15;
            ctx.font = "11px sans-serif";
            for (var li = 0; li < keys.length; li++) {
                var lm = keys[li];
                var lc = root.metricColors[lm] || theme.primary;
                var label = root.metricLabels[lm] || lm;
                ctx.fillStyle = lc;
                ctx.fillRect(legendX, legendY - 8, 12, 3);
                ctx.fillStyle = theme.foreground;
                ctx.textAlign = "left";
                ctx.fillText(label, legendX + 16, legendY);
                legendX += ctx.measureText(label).width + 36;
            }
        }
    }

    // 空状态
    Column {
        anchors.centerIn: parent
        spacing: theme.spacingSm
        visible: !root.hasData

        ShadcnIcon {
            anchors.horizontalCenter: parent.horizontalCenter
            name: "activity"
            size: 32
            color: theme.mutedForeground
        }
        ShadcnLabel {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "等待数据..."
            variant: ShadcnLabel.Variant.Muted
        }
    }
}
