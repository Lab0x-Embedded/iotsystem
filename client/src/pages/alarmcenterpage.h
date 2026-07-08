#ifndef ALARMCENTERPAGE_H
#define ALARMCENTERPAGE_H

#include <QWidget>
#include "models/alarmmodel.h"

class QTableView;
class QComboBox;
class QPushButton;

class AlarmCenterPage : public QWidget {
    Q_OBJECT
public:
    explicit AlarmCenterPage(AlarmModel *model, QWidget *parent = nullptr);

private slots:
    void filterChanged();
    void ackSelected();

private:
    AlarmModel *m_model;
    QTableView *m_table;
    QComboBox *m_filterSeverity;
    QPushButton *m_ackButton;
};
#endif
