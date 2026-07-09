#include "theme.h"
#include <QGlobalStatic>
#include <QSettings>

Q_GLOBAL_STATIC(ThemeManager, s_instance)

ThemeManager* ThemeManager::instance() {
    return s_instance;
}

ThemeManager::ThemeManager(QObject *parent) : QObject(parent) {
    QSettings s;
    m_isDark = s.value("theme/dark", true).toBool();
}

void ThemeManager::toggle() {
    setDark(!m_isDark);
}

void ThemeManager::setDark(bool dark) {
    if (dark == m_isDark) return;
    m_isDark = dark;
    QSettings s;
    s.setValue("theme/dark", m_isDark);
    emit themeChanged();
}
