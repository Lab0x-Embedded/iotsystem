import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property color accentColor: "#3874F7"
    property bool isDark: true
    property real maxValue: 100
    property real minValue: 0
    property string title: ""
    property string unit: ""
    property real value: 0

    border.color: isDark ? "#45475a" : "#e0e0e0"
    border.width: 1
    color: isDark ? "#313244" : "#ffffff"
    height: 160
    radius: 12

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            Layout.alignment: Qt.AlignHCenter
            color: root.isDark ? "#a6adc8" : "#666666"
            font.pixelSize: 13
            text: root.title
        }

        // Gauge circle
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 80

            Canvas {
                id: gauge

                property real progress: (root.value - root.minValue) / (root.maxValue - root.minValue)

                anchors.centerIn: parent
                height: 80
                width: 80

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();

                    var centerX = width / 2;
                    var centerY = height / 2;
                    var radius = width / 2 - 8;

                    // Background circle
                    ctx.beginPath();
                    ctx.arc(centerX, centerY, radius, 0, Math.PI * 2);
                    ctx.strokeStyle = root.isDark ? "#45475a" : "#e0e0e0";
                    ctx.lineWidth = 8;
                    ctx.stroke();

                    // Progress arc
                    ctx.beginPath();
                    ctx.arc(centerX, centerY, radius, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * progress);
                    ctx.strokeStyle = root.accentColor;
                    ctx.lineWidth = 8;
                    ctx.lineCap = "round";
                    ctx.stroke();
                }

                Connections {
                    function onValueChanged() {
                        gauge.requestPaint();
                    }

                    target: root
                }
            }
            Column {
                anchors.centerIn: parent
                spacing: 2

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: root.accentColor
                    font.bold: true
                    font.pixelSize: 20
                    text: root.value.toFixed(1)
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: root.isDark ? "#a6adc8" : "#666666"
                    font.pixelSize: 10
                    text: root.unit
                }
            }
        }
    }
}
