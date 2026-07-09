#include "theme.h"
#include <QGlobalStatic>
#include <QSettings>
#include <QApplication>
#include <QFile>
#include <QTextStream>

Q_GLOBAL_STATIC(ThemeManager, s_instance)

ThemeManager* ThemeManager::instance() {
    return s_instance;
}

ThemeManager::ThemeManager(QObject *parent) : QObject(parent) {}

Theme ThemeManager::buildDark() const {
    Theme t;
    t.name = "mocha";
    t.sidebarBg        = "#181825";
    t.sidebarBorder    = "#313244";
    t.sidebarTitle     = "#89b4fa";
    t.sidebarText      = "#a6adc8";
    t.sidebarTextActive= "#cdd6f4";
    t.sidebarHover     = "#313244";
    t.sidebarSelected  = "#45475a";
    t.sidebarSelectedBorder = "#89b4fa";
    t.sidebarToggleBg  = "#11111b";
    t.pageBg           = "#1e1e2e";
    t.cardBg           = "#313244";
    t.cardBorder       = "#45475a";
    t.surface          = "#11111b";
    t.surfaceAlt       = "#1e1e2e";
    t.border           = "#313244";
    t.borderStrong     = "#45475a";
    t.text             = "#cdd6f4";
    t.textMuted        = "#a6adc8";
    t.textFaint        = "#6c7086";
    t.accent           = "#89b4fa";
    t.accentText       = "#1e1e2e";
    t.accentSoft       = "rgba(137,180,250,0.18)";
    t.chipBg           = "rgba(137,180,250,0.15)";
    t.chipText         = "#89b4fa";
    t.statusBarBg      = "#11111b";
    t.statusBarBorder  = "#313244";
    t.statusBarText    = "#a6adc8";
    t.danger           = "#f38ba8";
    t.warning          = "#f9e2af";
    t.success          = "#a6e3a1";
    t.info             = "#89b4fa";
    t.tooltipBg        = "#45475a";
    t.tooltipText      = "#cdd6f4";
    return t;
}

Theme ThemeManager::buildLight() const {
    Theme t;
    t.name = "industrial";
    // Industrial IoT Gray-Blue Theme
    t.sidebarBg        = "#e2e8f0";
    t.sidebarBorder    = "#cbd5e1";
    t.sidebarTitle     = "#2563eb";
    t.sidebarText      = "#64748b";
    t.sidebarTextActive= "#334155";
    t.sidebarHover     = "#d1d5db";
    t.sidebarSelected  = "#dbeafe";
    t.sidebarSelectedBorder = "#2563eb";
    t.sidebarToggleBg  = "#d1d5db";
    
    // Page Background - Industrial Gray-Blue
    t.pageBg           = "#eef2f7";
    t.cardBg           = "#f8fafc";  // User requested color
    t.cardBorder       = "#b0b5bf";
    t.surface          = "#e8edf5";
    t.surfaceAlt       = "#d1d5db";
    t.border           = "#cbd5e1";
    t.borderStrong     = "#9ca0b0";
    
    // Text Colors
    t.text             = "#334155";
    t.textMuted        = "#64748b";
    t.textFaint        = "#9ca0b0";
    
    // Accent - Industrial Blue
    t.accent           = "#2563eb";
    t.accentText       = "#ffffff";
    t.accentSoft       = "rgba(37,99,235,0.1)";
    t.chipBg           = "rgba(37,99,235,0.08)";
    t.chipText         = "#2563eb";
    
    // Status Bar
    t.statusBarBg      = "#d1d5db";
    t.statusBarBorder  = "#cbd5e1";
    t.statusBarText    = "#64748b";
    
    // Status Colors - Industrial IoT
    t.danger           = "#dc2626";
    t.warning          = "#f59e0b";
    t.success          = "#22c55e";
    t.info             = "#2563eb";
    
    // Tooltip
    t.tooltipBg        = "#334155";
    t.tooltipText      = "#f8fafc";
    return t;
}

void ThemeManager::load() {
    QSettings s;
    // Force light industrial theme on first launch
    if (!s.contains("theme/dark")) {
        s.setValue("theme/dark", false);
    }
    m_isDark = s.value("theme/dark", false).toBool();
    m_theme = m_isDark ? buildDark() : buildLight();
    applyStyleSheet();
}

void ThemeManager::toggle() {
    setDark(!m_isDark);
}

void ThemeManager::setDark(bool dark) {
    if (dark == m_isDark) return;
    m_isDark = dark;
    m_theme = m_isDark ? buildDark() : buildLight();
    QSettings s;
    s.setValue("theme/dark", m_isDark);
    applyStyleSheet();
    emit themeChanged();
}

void ThemeManager::applyStyleSheet() {
    QString styleFile = m_isDark ? ":/styles/dark.css" : ":/styles/light.css";
    QFile file(styleFile);
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream in(&file);
        qApp->setStyleSheet(in.readAll());
        file.close();
    }
}
