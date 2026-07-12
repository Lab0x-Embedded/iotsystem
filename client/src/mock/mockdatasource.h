#ifndef MOCKDATASOURCE_H
#define MOCKDATASOURCE_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include "models/devicemodel.h"
#include "models/alarmmodel.h"

class MockDataSource : public QObject {
    Q_OBJECT
public:
    explicit MockDataSource(QObject *parent = nullptr);
    void buildDevices();

public slots:
    void start();
    void stop();

signals:
    void deviceUpdated(const DeviceInfo &d);
    void newAlarm(const AlarmRecord &r);
    void dataPoint(const QString &deviceId, const QString &metric, double value, qint64 timestamp);

private slots:
    void onTick();

private:
    QTimer *m_timer;
    QVector<DeviceInfo> m_devices;
    uint64_t m_alarmSeq = 0;
};
#endif
