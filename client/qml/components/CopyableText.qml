import QtQuick
import QtShadcn

// 可复制的文本值：文本本身可鼠标选中，右侧一个轻量复制按钮一键复制
//
// 用法:
//   CopyableText { text: someValue }
//   CopyableText { text: v; color: theme.mutedForeground; fontSize: 12 }
//   CopyableText { text: secret; fontFamily: "Monaco" }
Item {
    id: root

    property string text: ""
    property color color: theme.foreground
    property int fontSize: 12
    property string fontFamily: ""
    property bool showCopyIcon: true

    QtShadcnTheme { id: theme }

    implicitHeight: editor.implicitHeight
    implicitWidth: editor.implicitWidth + (showCopyIcon ? 26 : 0)

    // 用 TextEdit 而非 Text：Text 无法选中复制。
    // 宽度取"内容宽"与"可用宽"的较小值——组件被拉宽时按钮仍紧跟文字，
    // 文本超长时在按钮前截断（clip）。
    TextEdit {
        id: editor

        anchors.top: parent.top
        anchors.left: parent.left
        width: Math.min(implicitWidth,
                        root.width - (root.showCopyIcon ? 26 : 0))
        text: root.text
        readOnly: true
        selectByMouse: true
        selectByKeyboard: true
        color: root.color
        font.pixelSize: root.fontSize
        // 空 family 在 macOS 上会导致布局度量与渲染的字体回退不一致
        // （如 dev_001 被渲染成 dev_0 0 1），空值时显式回退到应用默认字体
        font.family: root.fontFamily.length > 0
                     ? root.fontFamily : Qt.application.font.family
        wrapMode: TextEdit.NoWrap
        clip: true
    }

    // 轻量复制按钮(24x24)，不用整颗 ShadcnButton 以免挤压布局
    Item {
        id: copyBtn

        // 必须显式声明尺寸：Rectangle / MouseArea 都用 anchors.fill 靠父级，
        // ShadcnIcon 用 centerIn 也不贡献隐式尺寸。若这里不写 width/height，
        // Item 会是 0×0，MouseArea 也跟着 0×0，鼠标事件永远命中不到。
        width: 24
        height: 24

        anchors.left: editor.right
        anchors.leftMargin: 8
        anchors.verticalCenter: editor.verticalCenter
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
                // 有选区时复制选区；无选区时全选复制整段
                if (editor.selectedText.length === 0) {
                    editor.selectAll();
                }
                editor.copy();
                editor.deselect();
            }
        }
    }
}