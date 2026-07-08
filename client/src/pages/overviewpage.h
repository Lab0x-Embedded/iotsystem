#ifndef OVERVIEWPAGE_H
#define OVERVIEWPAGE_H

#include <QWidget>
#include "models/devicemodel.h"

class QTableView;
class QGroupBox;
class QLabel;
class QLineEdit;
class QComboBox;

class OverviewPage : public QWidget {
    Q_OBJECT
public:
    explicit OverviewPage(DeviceModel *model, QWidget *parent = nullptr);

signals:
    void deviceSelected(const QString &deviceId);

public slots:
    void updateCounts(int total, int online, int alarm);

private:
    DeviceModel *m_model;
    QTableView *m_table;
    QLabel *m_statTotal;
    QLabel *m_statOnline;
    QLabel *m_statOffline;
    QLabel *m_statAlarm;
    QLineEdit *m_search;
    QComboBox *m_filterStatus;
};
#endif
