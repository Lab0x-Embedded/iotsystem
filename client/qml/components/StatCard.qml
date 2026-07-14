import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property color accentColor: "#3874F7"
    property bool isDark: true
    property string title: ""
    property int value: 0

    border.color: isDark ? "#45475a" : "#e0e0e0"
    border.width: 1
    color: isDark ? "#313244" : "#ffffff"
    height: 100
    radius: 12

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 8

        Label {
            color: root.isDark ? "#a6adc8" : "#666666"
            font.pixelSize: 13
            text: root.title
        }
        Label {
            color: root.accentColor
            font.bold: true
            font.pixelSize: 28
            text: root.value.toString()
        }
    }
}
