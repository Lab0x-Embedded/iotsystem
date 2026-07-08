#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include <QWidget>
#include "models/devicemodel.h"

class GaugeWidget;
class RealtimeChart;
class TopDevicesTable;
class QLabel;

class DashboardPage : public QWidget {
    Q_OBJECT
public:
    explicit DashboardPage(QWidget *parent = nullptr);

public slots:
    void setDevices(const QVector<DeviceInfo> &devices);
    void addDataPoint(const QString &metric, double value, qint64 timestamp);

private:
    GaugeWidget *m_gaugeTemp;
    GaugeWidget *m_gaugeHumid;
    GaugeWidget *m_gaugeBattery;
    GaugeWidget *m_gaugeOnline;
    RealtimeChart *m_chart;
    TopDevicesTable *m_topTable;
    QLabel *m_lblTotal;
    QLabel *m_lblOnline;
    QLabel *m_lblAlarm;
};
#endif
