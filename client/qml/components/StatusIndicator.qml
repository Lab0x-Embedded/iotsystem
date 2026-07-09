import QtQuick 2.15

Rectangle {
    id: root

    property int status: 0 // 0=Offline, 1=Online, 2=Alarm, 3=Maintenance

    color: status === 0 ? "#9E9E9E" : status === 1 ? "#4CAF50" : status === 2 ? "#FF5722" : "#FFC107"
    height: 12
    radius: 6
    width: 12

    // Pulse animation for alarm status
    SequentialAnimation on opacity {
        loops: Animation.Infinite
        running: root.status === 2

        NumberAnimation {
            duration: 1000
            easing.type: Easing.InOutQuad
            from: 1.0
            to: 0.3
        }
        NumberAnimation {
            duration: 1000
            easing.type: Easing.InOutQuad
            from: 0.3
            to: 1.0
        }
    }
}
