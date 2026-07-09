import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property string icon: ""
    property bool isDark: true
    property string title: ""
    property string value: ""

    border.color: isDark ? "#45475a" : "#e0e0e0"
    border.width: 1
    color: isDark ? "#1e1e2e" : "#f8f9fa"
    height: 80
    radius: 8

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        Label {
            font.pixelSize: 24
            text: root.icon
        }
        ColumnLayout {
            spacing: 4

            Label {
                color: root.isDark ? "#a6adc8" : "#666666"
                font.pixelSize: 11
                text: root.title
            }
            Label {
                color: root.isDark ? "#cdd6f4" : "#1e1e2e"
                font.bold: true
                font.pixelSize: 18
                text: root.value
            }
        }
    }
}
