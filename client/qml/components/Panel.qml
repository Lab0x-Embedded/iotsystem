import QtQuick
import QtShadcn

// 内容面板：主题化容器（卡片外观），专用于需要填满高度的内容区（列表 / 图表）。
//
// 为什么不用 ShadcnCard：
//   ShadcnCard.implicitHeight = 内部 Column.implicitHeight + padding，
//   而 Column 是 anchors.fill；子项若用 anchors.fill / height: parent.height
//   会与父级隐式尺寸互相依赖，形成尺寸环（表现为持续重排、CPU 100%）。
//   本组件是纯 Rectangle + Item，高度完全由外部 Layout 决定，无隐式尺寸耦合。
//
// 用法：
//   Panel {
//       Layout.fillWidth: true
//       Layout.fillHeight: true
//       ListView { anchors.fill: parent; ... }     // 或 ColumnLayout 自行排布
//   }
Rectangle {
    id: root

    default property alias contentData: body.data

    QtShadcnTheme { id: theme }

    color: theme.card
    radius: theme.radius
    border.width: 1
    border.color: theme.border
    clip: true

    Item {
        id: body

        anchors.fill: parent
        anchors.margins: 16
    }
}
