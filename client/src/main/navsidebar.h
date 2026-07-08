#ifndef NAVSIDEBAR_H
#define NAVSIDEBAR_H

#include <QWidget>
#include <QButtonGroup>
#include <QVector>

class NavSidebar : public QWidget {
    Q_OBJECT
public:
    explicit NavSidebar(QWidget *parent = nullptr);

signals:
    void pageSelected(int index);

private:
    QButtonGroup *m_group;
    void addEntry(const QString &text, int index);
};
#endif
