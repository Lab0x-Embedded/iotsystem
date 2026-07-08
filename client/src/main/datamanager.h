#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <QObject>
#include "models/devicemodel.h"
#include "models/alarmmodel.h"
#include "mock/mockdatasource.h"
#include "network/httpclient.h"

class DataManager : public QObject {
    Q_OBJECT
public:
    explicit DataManager(QObject *parent = nullptr);

    DeviceModel *deviceModel() { return &m_devices; }
    AlarmModel *alarmModel() { return &m_alarms; }
    HttpClient *httpClient() { return &m_http; }

    void start();
    void stop();
    void setOnline(bool online);
    void pushDataPoint(const QString &deviceId, const QString &metric, double value, qint64 ts);

signals:
    void deviceUpdated(const DeviceInfo &d);
    void newAlarm(const AlarmRecord &alarm);
    void dataPointArrived(const QString &deviceId, const QString &metric, double value, qint64 ts);
    void countsChanged(int total, int online, int alarm);

private:
    DeviceModel m_devices;
    AlarmModel m_alarms;
    MockDataSource m_mock;
    HttpClient m_http;
    bool m_online = false;
};
#endif
