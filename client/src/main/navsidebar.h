#ifndef NAVSIDEBAR_H
#define NAVSIDEBAR_H

#include <QWidget>
#include <QPushButton>
#include <QButtonGroup>
#include <QVector>

class NavSidebar : public QWidget {
    Q_OBJECT
public:
    explicit NavSidebar(QWidget *parent = nullptr);

public slots:
    void updateToggleLabel();

signals:
    void pageSelected(int index);

private:
    QButtonGroup *m_group;
    QPushButton *m_themeToggle;
    void addEntry(const QString &text, int index);
};
#endif
