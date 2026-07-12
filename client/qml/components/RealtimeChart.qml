import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property var dataPoints: []
    property bool isDark: true
    property int maxPoints: 60
    property bool hasData: false

    function addDataPoint(metric, value, timestamp) {
        var point = {
            "metric": metric,
            "value": value,
            "timestamp": timestamp
        };
        dataPoints.push(point);
        if (dataPoints.length > maxPoints) {
            dataPoints.shift();
        }
        hasData = true;
        canvas.requestPaint();
    }

    color: "transparent"

    Canvas {
        id: canvas

        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();

            if (dataPoints.length < 2)
                return;

            var width = canvas.width;
            var height = canvas.height;
            var padding = 40;

            // Find min/max values
            var minVal = Infinity;
            var maxVal = -Infinity;
            for (var i = 0; i < dataPoints.length; i++) {
                if (dataPoints[i].value < minVal)
                    minVal = dataPoints[i].value;
                if (dataPoints[i].value > maxVal)
                    maxVal = dataPoints[i].value;
            }

            var range = maxVal - minVal;
            if (range === 0)
                range = 1;

            // Draw grid
            ctx.strokeStyle = root.isDark ? "#45475a" : "#e0e0e0";
            ctx.lineWidth = 1;
            ctx.setLineDash([5, 5]);

            for (var i = 0; i < 5; i++) {
                var y = padding + (height - 2 * padding) * i / 4;
                ctx.beginPath();
                ctx.moveTo(padding, y);
                ctx.lineTo(width - padding, y);
                ctx.stroke();
            }

            ctx.setLineDash([]);

            // Draw line
            ctx.beginPath();
            ctx.strokeStyle = "#89b4fa";
            ctx.lineWidth = 2;

            for (var i = 0; i < dataPoints.length; i++) {
                var x = padding + (width - 2 * padding) * i / (dataPoints.length - 1);
                var y = height - padding - (height - 2 * padding) * (dataPoints[i].value - minVal) / range;

                if (i === 0) {
                    ctx.moveTo(x, y);
                } else {
                    ctx.lineTo(x, y);
                }
            }

            ctx.stroke();

            // Draw points
            for (var i = 0; i < dataPoints.length; i++) {
                var x = padding + (width - 2 * padding) * i / (dataPoints.length - 1);
                var y = height - padding - (height - 2 * padding) * (dataPoints[i].value - minVal) / range;

                ctx.beginPath();
                ctx.arc(x, y, 3, 0, Math.PI * 2);
                ctx.fillStyle = "#89b4fa";
                ctx.fill();
            }

            // Draw labels
            ctx.fillStyle = root.isDark ? "#a6adc8" : "#666666";
            ctx.font = "10px sans-serif";
            ctx.textAlign = "right";

            for (var i = 0; i < 5; i++) {
                var val = maxVal - (range * i / 4);
                var y = padding + (height - 2 * padding) * i / 4;
                ctx.fillText(val.toFixed(1), padding - 8, y + 4);
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
