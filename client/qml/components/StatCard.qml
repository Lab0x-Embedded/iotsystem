import QtQuick
import QtQuick.Layouts
import QtShadcn

// 统计卡片: 标题 + 大字数值 + 可选状态点
ShadcnCard {
    id: root

    property string title: ""
    property string value: "0"
    property color valueColor: theme.foreground
    property int dotStatus: ShadcnStatusDot.Status.None  // ShadcnStatusDot.Status.*

    QtShadcnTheme { id: theme }

    ShadcnCardContent {
        ColumnLayout {
            width: parent.width
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                ShadcnLabel {
                    text: root.title
                    size: ShadcnLabel.Size.Small
                    variant: ShadcnLabel.Variant.Muted
                }
                Item { Layout.fillWidth: true }
                ShadcnStatusDot {
                    status: root.dotStatus
                    size: 8
                    visible: root.dotStatus !== ShadcnStatusDot.Status.None
                }
            }

            ShadcnLabel {
                text: root.value
                size: ShadcnLabel.Size.Large
                color: root.valueColor
            }
        }
    }
}
