import QtQuick 2.15
import QtQuick.Controls 2.15

// 可鼠标选中的只读文本（用于表格单元格）
TextEdit {
    id: root

    property color textColor: "#cdd6f4"

    readOnly: true
    selectByMouse: true
    wrapMode: TextEdit.NoWrap
    color: textColor
    selectedTextColor: "#1e1e2e"
    selectionColor: "#89b4fa"
    font.pixelSize: 12
}
