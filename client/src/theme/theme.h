#ifndef THEME_H
#define THEME_H

#include <QObject>
#include <QString>
#include <QColor>

struct Theme {
    QString name;
    QString sidebarBg;
    QString sidebarBorder;
    QString sidebarTitle;
    QString sidebarText;
    QString sidebarTextActive;
    QString sidebarHover;
    QString sidebarSelected;
    QString sidebarSelectedBorder;
    QString sidebarToggleBg;
    QString pageBg;
    QString cardBg;
    QString cardBorder;
    QString surface;
    QString surfaceAlt;
    QString border;
    QString borderStrong;
    QString text;
    QString textMuted;
    QString textFaint;
    QString accent;
    QString accentText;
    QString accentSoft;
    QString chipBg;
    QString chipText;
    QString statusBarBg;
    QString statusBarBorder;
    QString statusBarText;
    QString danger;
    QString warning;
    QString success;
    QString info;
    QString tooltipBg;
    QString tooltipText;
};

class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager *instance();

    const Theme &current() const { return m_theme; }
    bool isDark() const { return m_isDark; }

    void load();
    void toggle();
    void setDark(bool dark);

signals:
    void themeChanged();

public:
    explicit ThemeManager(QObject *parent = nullptr);

private:
    Theme buildDark() const;
    Theme buildLight() const;

    Theme m_theme;
    bool m_isDark = true;
};

#endif
