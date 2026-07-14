import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15

Button {
    id: root

    property int radius: 8

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: 40
        radius: root.radius
        color: root.enabled ? root.Material.background : (root.isDark ? "#45475a" : "#e0e0e0")
        opacity: root.enabled ? (root.hovered ? 0.85 : 1.0) : 0.5
        border.width: root.flat ? 0 : 1
        border.color: root.enabled ? Qt.darker(color, 1.1) : "transparent"

        Behavior on opacity { NumberAnimation { duration: 100 } }
        Behavior on color { ColorAnimation { duration: 100 } }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.NoButton
        }
    }
}
