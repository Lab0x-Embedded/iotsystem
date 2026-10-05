import QtQuick
import QtQuick.Layouts
import QtShadcn

// 应用侧边栏: logo + 导航 + 主题切换
Rectangle {
    id: root

    property int currentIndex: 0
    signal pageSelected(int index)

    QtShadcnTheme { id: theme }

    color: theme.card
    border.color: theme.border
    border.width: 1

    // 导航项定义
    property var navItems: [
        { icon: "monitor",  name: "设备" },
        { icon: "bell",     name: "告警" },
        { icon: "folder",   name: "分组" }
    ]

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Logo
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 64

            Row {
                anchors.centerIn: parent
                spacing: 10

                Image {
                    source: "../assets/logo.png"
                    width: 32
                    height: 32
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                }
                Column {
                    spacing: 0
                    anchors.verticalCenter: parent.verticalCenter
                    ShadcnLabel {
                        text: "IoT Platform"
                        size: ShadcnLabel.Size.Medium
                        color: theme.primary
                    }
                    ShadcnLabel {
                        text: "Device Manager"
                        size: ShadcnLabel.Size.Small
                        variant: ShadcnLabel.Variant.Muted
                    }
                }
            }
        }

        ShadcnSeparator { Layout.fillWidth: true }

        // 导航项
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: theme.spacingMd
            Layout.leftMargin: theme.spacingSm
            Layout.rightMargin: theme.spacingSm
            spacing: 4

            Repeater {
                model: root.navItems

                delegate: Item {
                    id: navDelegate

                    required property int index
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.preferredHeight: 40

                    Rectangle {
                        anchors.fill: parent
                        radius: theme.radius
                        color: root.currentIndex === navDelegate.index
                               ? theme.primary
                               : navMouse.containsMouse ? theme.muted : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 12

                            ShadcnIcon {
                                name: navDelegate.modelData.icon
                                size: 18
                                color: root.currentIndex === navDelegate.index
                                       ? theme.primaryForeground
                                       : theme.mutedForeground
                            }
                            ShadcnLabel {
                                Layout.fillWidth: true
                                text: navDelegate.modelData.name
                                size: ShadcnLabel.Size.Medium
                                color: root.currentIndex === navDelegate.index
                                       ? theme.primaryForeground
                                       : theme.foreground
                            }
                        }
                    }

                    MouseArea {
                        id: navMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.currentIndex = navDelegate.index;
                            root.pageSelected(navDelegate.index);
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true; Layout.fillWidth: true }

        ShadcnSeparator { Layout.fillWidth: true }

        // 主题切换
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 48

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 8

                ShadcnIcon {
                    name: theme.mode === "dark" ? "moon" : "sun"
                    size: 16
                    color: theme.mutedForeground
                }
                ShadcnLabel {
                    Layout.fillWidth: true
                    text: theme.mode === "dark" ? "深色模式" : "浅色模式"
                    size: ShadcnLabel.Size.Small
                    variant: ShadcnLabel.Variant.Muted
                }
                ShadcnSwitch {
                    id: themeSwitch

                    size: ShadcnSwitch.Size.Small

                    // 不用 `checked: ...` 绑定：控件交互时会写入 checked，
                    // 破坏声明式绑定导致图标/文字与开关状态脱节。
                    // 改为显式同步：用户操作 → 改 theme.mode；theme.mode 变 → 回写 checked。
                    onToggled: theme.mode = checked ? "dark" : "light"

                    Component.onCompleted: checked = (theme.mode === "dark")

                    Connections {
                        target: theme
                        function onModeChanged() {
                            themeSwitch.checked = (theme.mode === "dark");
                        }
                    }
                }
            }
        }
    }
}
