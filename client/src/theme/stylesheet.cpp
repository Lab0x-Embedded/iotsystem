#include "stylesheet.h"
#include "theme.h"
#include <QString>

QString StyleSheet::global(const Theme &t) {
    return QString(R"(
/* ============================================
 * IoT Device Manager - Dynamic Theme Stylesheet
 * Following Qt UI Design Guidelines
 * ============================================ */

/* ---------- Global Reset ---------- */
* {
    background-color: %1;
    color: %2;
    font-family: "PingFang SC", "Microsoft YaHei", "Helvetica Neue", Arial, sans-serif;
    font-size: 14px;
    selection-background-color: %3;
    selection-color: %4;
}

/* ---------- Main Window ---------- */
QWidget#MainWindow { background-color: %1; }
QMainWindow, QDialog { background-color: %1; color: %2; }
QWidget#centralWidget { background-color: %1; }

/* ============================================
 * Sidebar Navigation
 * Proximity + Similarity: Group navigation items
 * Wayfinding: Clear active state indicator
 * ============================================ */
QWidget#NavSidebar {
    background-color: %5;
    border-right: 1px solid %6;
}
#sidebarTitle {
    color: %13;
    font-size: 20px;
    font-weight: bold;
    padding: 4px 16px 20px;
    letter-spacing: 0.5px;
}
QWidget#NavSidebar QPushButton {
    background: transparent;
    color: %7;
    text-align: left;
    padding: 0 16px;
    border: none;
    border-left: 3px solid transparent;
    font-size: 14px;
    min-height: 44px;  /* Touch target: 44px minimum */
}
QWidget#NavSidebar QPushButton:hover {
    background: %8;
    color: %9;
}
QWidget#NavSidebar QPushButton:checked {
    background: %10;
    color: %11;
    border-left: 3px solid %12;
    font-weight: bold;
}
#themeToggle {
    background: %14;
    color: %7;
    border: 1px solid %6;
    border-radius: 16px;
    min-height: 32px;
    font-size: 14px;
}
#themeToggle:hover {
    background: %8;
    color: %9;
}

/* ============================================
 * Cards - Dashboard Stats
 * Aesthetic-Usability Effect: Polished design
 * Uniform Connectedness: Visual grouping
 * ============================================ */
QFrame#card {
    background-color: %15;
    border: 1px solid %16;
    border-radius: 12px;
    padding: 16px;
}
QFrame#card:hover {
    border-color: %29;
}
#card QLabel[title="true"] {
    color: %17;
    font-size: 13px;
    font-weight: normal;
}
#card QLabel[value="true"] {
    color: %18;
    font-size: 28px;
    font-weight: bold;
}

/* ============================================
 * GroupBox - Content Sections
 * Proximity + Similarity: Visual grouping
 * Layer-cake: Clear section headers
 * ============================================ */
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
    color: %13;
    font-size: 14px;
}

/* ============================================
 * Input Fields
 * Affordance: Clear input boundaries
 * Doherty Threshold: Visual feedback on focus
 * ============================================ */
QLineEdit, QComboBox, QTextEdit, QPlainTextEdit {
    background: %19;
    color: %2;
    border: 1px solid %6;
    border-radius: 6px;
    padding: 8px 12px;
    font-size: 14px;
    min-height: 28px;
    selection-background-color: %3;
    selection-color: %4;
}
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
    border: 1px solid %3;
}
QLineEdit:hover, QTextEdit:hover, QPlainTextEdit:hover {
    border: 1px solid %29;
}
QLineEdit:disabled, QTextEdit:disabled, QPlainTextEdit:disabled {
    background: %5;
    color: %35;
}
QComboBox {
    padding-right: 24px;
}
QComboBox::drop-down {
    border: none;
    width: 24px;
}
QComboBox::down-arrow {
    image: none;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 6px solid %17;
    margin-right: 8px;
}
QComboBox QAbstractItemView {
    background: %1;
    color: %2;
    border: 1px solid %6;
    selection-background-color: %8;
    selection-color: %3;
    outline: none;
}

/* ============================================
 * Buttons
 * Affordance: Clear clickable elements
 * Hick's Law: Limit choices, clear hierarchy
 * ============================================ */
QPushButton {
    background: %20;
    color: %2;
    border: none;
    border-radius: 6px;
    padding: 8px 16px;
    font-weight: 500;
    font-size: 14px;
    min-height: 28px;
}
QPushButton:hover {
    background: %21;
    color: %2;
}
QPushButton:pressed {
    background: %29;
}
QPushButton:disabled {
    background: %5;
    color: %35;
}
QPushButton#primaryButton {
    background: %3;
    color: %4;
    font-weight: bold;
}
QPushButton#primaryButton:hover {
    background: %22;
}
QPushButton#primaryButton:pressed {
    background: %36;
}
QPushButton#primaryButton:disabled {
    background: %29;
    color: %35;
}
QPushButton#dangerButton {
    background: %23;
    color: %4;
    font-weight: bold;
}
QPushButton#dangerButton:hover {
    background: %24;
}
QPushButton#dangerButton:pressed {
    background: %37;
}

/* ============================================
 * Tables
 * F-shaped reading pattern: Clear headers
 * Recognition Over Recall: Visible selection
 * ============================================ */
QTableView, QTreeView {
    background: %1;
    color: %2;
    gridline-color: %25;
    border: 1px solid %6;
    border-radius: 8px;
    alternate-background-color: %26;
    selection-background-color: %27;
    selection-color: %9;
    font-size: 14px;
}
QHeaderView::section {
    background: %19;
    color: %17;
    border: none;
    border-bottom: 1px solid %6;
    padding: 10px 8px;
    font-weight: bold;
    font-size: 13px;
}
QTableView::item {
    padding: 6px 8px;
}
QTableView::item:hover {
    background: %8;
}
QTableView::item:selected {
    background: %27;
    color: %3;
}

/* ============================================
 * ScrollBars
 * Performance Load: Minimal visual noise
 * ============================================ */
QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: %28;
    min-height: 30px;
    border-radius: 5px;
}
QScrollBar::handle:vertical:hover {
    background: %29;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}
QScrollBar:horizontal {
    background: transparent;
    height: 10px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background: %28;
    min-width: 30px;
    border-radius: 5px;
}
QScrollBar::handle:horizontal:hover {
    background: %29;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
}

/* ============================================
 * Status Bar
 * State visibility: Always visible system status
 * ============================================ */
QStatusBar {
    background: %30;
    border-top: 1px solid %31;
}
QStatusBar QLabel {
    color: %32;
    padding: 4px 12px;
    font-size: 13px;
}

/* ============================================
 * Tooltips
 * Progressive Disclosure: Information on demand
 * ============================================ */
QToolTip {
    background: %33;
    color: %34;
    border: 1px solid %29;
    border-radius: 6px;
    padding: 6px 10px;
    font-size: 13px;
}

/* ============================================
 * Scroll Areas
 * ============================================ */
QScrollArea, QScrollArea > QWidget {
    background: %1;
}

/* ============================================
 * Splitter
 * ============================================ */
QSplitter::handle {
    background: %6;
    width: 2px;
}

/* ============================================
 * Checkboxes
 * Affordance: Clear state indication
 * ============================================ */
QCheckBox {
    color: %2;
    spacing: 8px;
    font-size: 14px;
}
QCheckBox::indicator {
    width: 18px;
    height: 18px;
    border-radius: 4px;
    border: 1px solid %29;
    background: %19;
}
QCheckBox::indicator:hover {
    border-color: %3;
}
QCheckBox::indicator:checked {
    background: %3;
    border: 1px solid %3;
}

/* ============================================
 * Sliders
 * Affordance: Clear draggable element
 * ============================================ */
QSlider::groove:horizontal {
    background: %6;
    height: 4px;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    background: %3;
    width: 16px;
    height: 16px;
    border-radius: 8px;
    margin: -6px 0;
}
QSlider::handle:horizontal:hover {
    background: %22;
}
QSlider::sub-page:horizontal {
    background: %3;
    border-radius: 2px;
}

/* ============================================
 * Progress Bars
 * Doherty Threshold: Visual progress feedback
 * ============================================ */
QProgressBar {
    background: %6;
    border: none;
    border-radius: 4px;
    height: 8px;
    text-align: center;
    font-size: 11px;
    color: %2;
}
QProgressBar::chunk {
    background: %3;
    border-radius: 4px;
}

/* ============================================
 * Tab Widget
 * Wayfinding: Clear navigation structure
 * ============================================ */
QTabWidget::pane {
    border: 1px solid %6;
    border-radius: 8px;
    background: %1;
}
QTabBar::tab {
    background: %5;
    color: %17;
    border: 1px solid %6;
    padding: 8px 16px;
    font-size: 14px;
    min-height: 28px;
}
QTabBar::tab:selected {
    background: %1;
    color: %3;
    border-bottom-color: %1;
}
QTabBar::tab:hover:!selected {
    background: %8;
    color: %9;
}

/* ============================================
 * Menu
 * ============================================ */
QMenuBar {
    background: %5;
    color: %17;
    border-bottom: 1px solid %6;
}
QMenuBar::item:selected {
    background: %8;
    color: %9;
}
QMenu {
    background: %19;
    color: %2;
    border: 1px solid %6;
    border-radius: 8px;
    padding: 4px;
}
QMenu::item {
    padding: 8px 24px;
    min-height: 28px;
}
QMenu::item:selected {
    background: %8;
    color: %3;
}
QMenu::separator {
    height: 1px;
    background: %6;
}

/* ============================================
 * Industrial IoT Dashboard Styles
 * Gray-Blue Industrial Theme
 * ============================================ */

#detailHost {
    background-color: #eef2f7;
}

QFrame#statusCard {
    background-color: #f8fafc;
    border: 1px solid #cbd5e1;
    border-radius: 14px;
    padding: 16px;
}

QFrame#statusCard:hover {
    border-color: #2563eb;
}

#statusCardTitle {
    font-size: 22px;
    font-weight: bold;
    color: #334155;
    background: transparent;
}

#statusCardSubtitle {
    font-size: 15px;
    color: #64748b;
    background: transparent;
}

#statusDotOnline {
    color: #22c55e;
    font-size: 16px;
    background: transparent;
}

#statusDotOffline {
    color: #9ca0b0;
    font-size: 16px;
    background: transparent;
}

QPlainTextEdit#codeEditor,
QPlainTextEdit#codeEditorReadonly {
    background: #e8edf5;
    color: #334155;
    border: 1px solid #cbd5e1;
    border-radius: 8px;
    padding: 10px 12px;
    font-family: "JetBrains Mono", "SF Mono", "Cascadia Code", "Consolas", monospace;
    font-size: 13px;
}

QPlainTextEdit#codeEditor:focus {
    border: 1px solid #2563eb;
}

QPlainTextEdit#codeEditorReadonly {
    color: #64748b;
    border: 1px solid #cbd5e1;
}

QGroupBox#iotSection {
    background-color: #f8fafc;
    border: 1px solid #cbd5e1;
    border-radius: 12px;
    margin-top: 12px;
    padding-top: 16px;
    font-weight: bold;
    color: #334155;
}

QGroupBox#iotSection::title {
    subcontrol-origin: margin;
    left: 15px;
    padding: 0 8px;
    color: #2563eb;
    font-size: 14px;
    font-weight: bold;
}

QPushButton#primaryButton {
    background: #2563eb;
    color: white;
    border-radius: 8px;
    padding: 10px 20px;
    font-weight: bold;
}

QPushButton#primaryButton:hover {
    background: #1d4ed8;
}

QPushButton#controlButton {
    background: #f8fafc;
    color: #334155;
    border: 1px solid #cbd5e1;
    border-radius: 8px;
    padding: 10px 16px;
}

QPushButton#controlButton:hover {
    background: #e2e8f0;
    border-color: #2563eb;
}

QPushButton#chipButton {
    background: rgba(37,99,235,0.08);
    color: #2563eb;
    border: 1px solid rgba(37,99,235,0.2);
    border-radius: 16px;
    padding: 6px 14px;
    font-size: 12px;
    font-family: "JetBrains Mono", monospace;
}

QPushButton#chipButton:hover {
    background: rgba(37,99,235,0.15);
    border-color: #2563eb;
}

#syncBadge {
    color: #22c55e;
    font-size: 12px;
    background: transparent;
    padding: 2px 8px;
}

QFrame#iotSeparator {
    background: #cbd5e1;
    max-height: 1px;
    margin: 4px 0;
}

#formLabel {
    color: #64748b;
    font-size: 13px;
    font-weight: 600;
    background: transparent;
}

QLineEdit {
    background: #e8edf5;
    color: #334155;
    border: 1px solid #cbd5e1;
    border-radius: 8px;
    padding: 10px 12px;
}

QLineEdit:focus {
    border: 1px solid #2563eb;
}

QComboBox {
    background: #e8edf5;
    color: #334155;
    border: 1px solid #cbd5e1;
    border-radius: 8px;
    padding: 10px 12px;
}

QComboBox:hover {
    border-color: #2563eb;
}
    margin: 4px 8px;
}
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
    .arg(t.accent)            // 18 (card value accent overridden per-card)
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
    .arg(t.tooltipText)       // 34
    .arg(t.textFaint)         // 35 disabled text
    .arg(t.info)              // 36 primary pressed
    .arg(t.danger);           // 37 danger pressed
}
