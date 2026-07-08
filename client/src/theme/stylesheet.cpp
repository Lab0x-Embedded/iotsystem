#include "stylesheet.h"
#include "theme.h"
#include <QString>

QString StyleSheet::global(const Theme &t) {
    return QString(R"(
* {
    background-color: %1;
    color: %2;
    font-family: "PingFang SC", "Microsoft YaHei", "Helvetica Neue", Arial, sans-serif;
    selection-background-color: %3;
    selection-color: %4;
}

QWidget#MainWindow { background-color: %1; }

/* ---------- Sidebar ---------- */
QWidget#NavSidebar {
    background-color: %5;
    border-right: 1px solid %6;
}
QWidget#NavSidebar QPushButton {
    background: transparent;
    color: %7;
    text-align: left;
    padding: 0 16px;
    border: none;
    border-left: 3px solid transparent;
    font-size: 14px;
    min-height: 42px;
}
QWidget#NavSidebar QPushButton:hover { background: %8; color: %9; }
QWidget#NavSidebar QPushButton:checked {
    background: %10;
    color: %11;
    border-left: 3px solid %12;
    font-weight: bold;
}
#sidebarTitle { color: %13; font-size: 16px; font-weight: bold; padding: 0 16px 16px; }
#themeToggle {
    background: %14;
    color: %7;
    border: 1px solid %6;
    border-radius: 16px;
    min-height: 32px;
    font-size: 14px;
}
#themeToggle:hover { background: %8; color: %9; }

/* ---------- Generic panels ---------- */
QFrame#card { background-color: %15; border: 1px solid %16; border-radius: 12px; }
#card QLabel[title="true"] { color: %17; font-size: 13px; font-weight: normal; }
#card QLabel[value="true"] { color: %18; font-size: 28px; font-weight: bold; }

QGroupBox {
    background-color: %1;
    border: 1px solid %6;
    border-radius: 10px;
    margin-top: 12px;
    padding-top: 16px;
    font-weight: bold;
    color: %2;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 12px;
    padding: 0 6px;
    color: %2;
}

/* ---------- Inputs ---------- */
QLineEdit, QComboBox, QTextEdit, QPlainTextEdit {
    background: %19;
    color: %2;
    border: 1px solid %6;
    border-radius: 6px;
    padding: 6px 8px;
    selection-background-color: %3;
}
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus { border: 1px solid %3; }
QComboBox:hover { border: 1px solid %3; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox QAbstractItemView {
    background: %1;
    color: %2;
    border: 1px solid %6;
    selection-background-color: %8;
}

/* ---------- Buttons ---------- */
QPushButton {
    background: %20;
    color: %2;
    border: none;
    border-radius: 6px;
    padding: 8px 14px;
    font-weight: 500;
}
QPushButton:hover { background: %21; }
QPushButton:pressed { background: %6; }
QPushButton#primaryButton {
    background: %3;
    color: %4;
    font-weight: bold;
}
QPushButton#primaryButton:hover { background: %22; }
QPushButton#dangerButton { background: %23; color: %4; }
QPushButton#dangerButton:hover { background: %24; }

/* ---------- Tables ---------- */
QTableView, QTreeView {
    background: %1;
    color: %2;
    gridline-color: %25;
    border: 1px solid %6;
    border-radius: 8px;
    alternate-background-color: %26;
    selection-background-color: %27;
    selection-color: %9;
}
QHeaderView::section {
    background: %19;
    color: %17;
    border: none;
    border-bottom: 1px solid %6;
    padding: 8px;
    font-weight: bold;
}
QTableView::item:hover { background: %8; }
QTableView::item:selected { background: %27; color: %9; }

/* ---------- Scrollbars ---------- */
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: %28; border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: %29; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: %28; border-radius: 5px; min-width: 30px; }
QScrollBar::handle:horizontal:hover { background: %29; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

/* ---------- Status bar ---------- */
QStatusBar { background: %30; border-top: 1px solid %31; }
QStatusBar QLabel { color: %32; padding: 4px 12px; }

/* ---------- Tooltip ---------- */
QToolTip { background: %33; color: %34; border: 1px solid %6; border-radius: 6px; padding: 4px 8px; }

/* ---------- Misc ---------- */
QScrollArea, QScrollArea > QWidget { background: %1; }
QSplitter::handle { background: %6; }
QCheckBox { color: %2; }
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px; border: 1px solid %6; background: %19; }
QCheckBox::indicator:checked { background: %3; border: 1px solid %3; }
QSlider::groove:horizontal { background: %6; height: 4px; border-radius: 2px; }
QSlider::handle:horizontal { background: %3; width: 14px; height: 14px; border-radius: 7px; margin: -5px 0; }
QSlider::sub-page:horizontal { background: %3; border-radius: 2px; }
)")
    .arg(t.pageBg)            // 1
    .arg(t.text)              // 2
    .arg(t.accent)            // 3
    .arg(t.accentText)        // 4
    .arg(t.sidebarBg)         // 5
    .arg(t.sidebarBorder)     // 6
    .arg(t.sidebarText)       // 7
    .arg(t.sidebarHover)      // 8
    .arg(t.sidebarTextActive) // 9
    .arg(t.sidebarSelected)   // 10
    .arg(t.sidebarTextActive) // 11
    .arg(t.sidebarSelectedBorder) // 12
    .arg(t.sidebarTitle)      // 13
    .arg(t.sidebarToggleBg)   // 14
    .arg(t.cardBg)            // 15
    .arg(t.cardBorder)        // 16
    .arg(t.textMuted)         // 17
    .arg(t.accent)            // 18 (card value accent overridden per-card below)
    .arg(t.surface)           // 19
    .arg(t.cardBg)            // 20 generic button
    .arg(t.borderStrong)      // 21
    .arg(t.info)              // 22 primary hover
    .arg(t.danger)            // 23
    .arg(t.danger)            // 24
    .arg(t.border)            // 25 gridline
    .arg(t.surfaceAlt)        // 26 alternate row
    .arg(t.accentSoft)        // 27 selection
    .arg(t.borderStrong)      // 28 scroll handle
    .arg(t.textFaint)         // 29
    .arg(t.statusBarBg)       // 30
    .arg(t.statusBarBorder)   // 31
    .arg(t.statusBarText)     // 32
    .arg(t.tooltipBg)         // 33
    .arg(t.tooltipText);      // 34
}
