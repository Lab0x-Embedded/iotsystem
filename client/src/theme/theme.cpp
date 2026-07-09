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
    t.name = "latte";
    t.sidebarBg        = "#e6e9ef";
    t.sidebarBorder    = "#ccd0da";
    t.sidebarTitle     = "#1e66f5";
    t.sidebarText      = "#5c5f77";
    t.sidebarTextActive= "#4c4f69";
    t.sidebarHover     = "#ccd0da";
    t.sidebarSelected  = "#bcc0cc";
    t.sidebarSelectedBorder = "#1e66f5";
    t.sidebarToggleBg  = "#dce0e8";
    t.pageBg           = "#eff1f5";
    t.cardBg           = "#dce0e8";
    t.cardBorder       = "#ccd0da";
    t.surface          = "#ccd0da";
    t.surfaceAlt       = "#e6e9ef";
    t.border           = "#ccd0da";
    t.borderStrong     = "#bcc0cc";
    t.text             = "#4c4f69";
    t.textMuted        = "#5c5f77";
    t.textFaint        = "#9ca0b0";
    t.accent           = "#1e66f5";
    t.accentText       = "#eff1f5";
    t.accentSoft       = "rgba(30,102,245,0.14)";
    t.chipBg           = "rgba(30,102,245,0.12)";
    t.chipText         = "#1e66f5";
    t.statusBarBg      = "#dce0e8";
    t.statusBarBorder  = "#ccd0da";
    t.statusBarText    = "#5c5f77";
    t.danger           = "#d20f39";
    t.warning          = "#df8e1d";
    t.success          = "#40a02b";
    t.info             = "#1e66f5";
    t.tooltipBg        = "#bcc0cc";
    t.tooltipText      = "#4c4f69";
    return t;
}

void ThemeManager::load() {
    QSettings s;
    m_isDark = s.value("theme/dark", true).toBool();
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
