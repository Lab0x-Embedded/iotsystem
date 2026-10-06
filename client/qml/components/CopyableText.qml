import QtQuick
import QtQuick.Layouts
import QtShadcn

// 可复制的文本值：文本本身可鼠标选中，右侧一个轻量复制按钮一键复制
//
// 用法:
//   CopyableText { text: someValue }
//   CopyableText { text: v; color: theme.mutedForeground; fontSize: 12 }
Item {
    id: root

    property string text: ""
    property color color: theme.foreground
    property int fontSize: 12
    property bool showCopyIcon: true

    QtShadcnTheme { id: theme }

    implicitHeight: editor.implicitHeight
    implicitWidth: editor.implicitWidth + (showCopyIcon ? 24 : 0)

    RowLayout {
        anchors.fill: parent
        spacing: 2

        // 用 TextEdit 而非 Text：Text 无法选中复制
        TextEdit {
            id: editor

            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: root.text
            readOnly: true
            selectByMouse: true
            selectByKeyboard: true
            color: root.color
            font.pixelSize: root.fontSize
            wrapMode: TextEdit.NoWrap
            clip: true
        }

        // 轻量复制按钮(24x24)，不用整颗 ShadcnButton 以免挤压布局
        Item {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            Layout.alignment: Qt.AlignVCenter
            visible: root.showCopyIcon

            Rectangle {
                anchors.fill: parent
                radius: theme.radius - 2
                color: copyMouse.containsMouse ? theme.muted : "transparent"
            }
            ShadcnIcon {
                anchors.centerIn: parent
                name: "copy"
                size: 13
                color: copyMouse.containsMouse ? theme.foreground : theme.mutedForeground
            }
            MouseArea {
                id: copyMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    editor.selectAll();
                    editor.copy();
                    editor.deselect();
                }
            }
        }
    }
}
