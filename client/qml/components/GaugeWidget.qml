import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    width: 120
    height: 120
    
    property string label: ""
    property real value: 0
    property real minValue: 0
    property real maxValue: 100
    property string unit: ""
    property color color: "#89b4fa"
    property bool isDark: true
    
    Canvas {
        id: canvas
        anchors.fill: parent
        
        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            
            var centerX = width / 2;
            var centerY = height / 2;
            var radius = width / 2 - 12;
            
            // Background arc
            ctx.beginPath();
            ctx.arc(centerX, centerY, radius, 0.75 * Math.PI, 0.25 * Math.PI);
            ctx.strokeStyle = root.isDark ? "#45475a" : "#e0e0e0";
            ctx.lineWidth = 12;
            ctx.lineCap = "round";
            ctx.stroke();
            
            // Value arc
            var progress = (root.value - root.minValue) / (root.maxValue - root.minValue);
            var startAngle = 0.75 * Math.PI;
            var endAngle = startAngle + (1.5 * Math.PI) * progress;
            
            ctx.beginPath();
            ctx.arc(centerX, centerY, radius, startAngle, endAngle);
            ctx.strokeStyle = root.color;
            ctx.lineWidth = 12;
            ctx.lineCap = "round";
            ctx.stroke();
        }
        
        Connections {
            target: root
            function onValueChanged() { canvas.requestPaint() }
        }
    }
    
    Column {
        anchors.centerIn: parent
        spacing: 2
        
        Label {
            text: root.value.toFixed(1)
            font.pixelSize: 18
            font.bold: true
            color: root.color
            anchors.horizontalCenter: parent.horizontalCenter
        }
        
        Label {
            text: root.unit
            font.pixelSize: 10
            color: root.isDark ? "#a6adc8" : "#666666"
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }
    
    Label {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.label
        font.pixelSize: 11
        color: root.isDark ? "#cdd6f4" : "#1e1e2e"
    }
}
