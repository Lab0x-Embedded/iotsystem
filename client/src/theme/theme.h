#ifndef THEME_H
#define THEME_H

#include <QObject>

class ThemeManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isDark READ isDark NOTIFY themeChanged)

public:
    static ThemeManager *instance();

    bool isDark() const { return m_isDark; }

    Q_INVOKABLE void toggle();
    Q_INVOKABLE void setDark(bool dark);

signals:
    void themeChanged();

public:
    explicit ThemeManager(QObject *parent = nullptr);

private:
    bool m_isDark = true;
};

#endif
