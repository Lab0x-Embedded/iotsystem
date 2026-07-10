#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <QObject>
#include <QTimer>
#include "models/devicemodel.h"
#include "models/alarmmodel.h"
#include "models/groupmodel.h"
#include "network/httpclient.h"

class DataManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool online READ isOnline WRITE setOnline NOTIFY onlineChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY countsChanged)
    Q_PROPERTY(int onlineDeviceCount READ onlineDeviceCount NOTIFY countsChanged)
    Q_PROPERTY(int alarmCount READ alarmCount NOTIFY countsChanged)
    Q_PROPERTY(HttpClient *httpClient READ httpClient CONSTANT)

public:
    explicit DataManager(QObject *parent = nullptr);

    DeviceModel *deviceModel() { return &m_devices; }
    AlarmModel *alarmModel() { return &m_alarms; }
    GroupModel *groupModel() { return &m_groups; }
    HttpClient *httpClient() { return &m_http; }

    bool isOnline() const { return m_online; }
    QString serverUrl() const { return m_http.serverUrl(); }
    int deviceCount() const { return m_devices.totalCount(); }
    int onlineDeviceCount() const { return m_devices.onlineCount(); }
    int alarmCount() const { return m_alarms.activeCount(); }

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void setOnline(bool online);
    Q_INVOKABLE void connectToServer(const QString &url, const QString &username, const QString &password);
    Q_INVOKABLE void disconnect();
    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void refreshGroups();
    Q_INVOKABLE void refreshAlarms();

signals:
    void onlineChanged();
    void serverUrlChanged();
    void countsChanged();
    void connectionStatusChanged(const QString &status);
    void deviceUpdated(const DeviceInfo &d);
    void newAlarm(const AlarmRecord &alarm);
    void dataPointArrived(const QString &deviceId, const QString &metric, double value, qint64 ts);
    void errorOccurred(const QString &error);

private slots:
    void onLoginSucceeded(const QString &token, const QString &role);
    void onLoginFailed(const QString &error);
    void onDevicesFetched(const QJsonArray &devices);
    void onGroupsFetched(const QJsonArray &groups);
    void onAlarmsFetched(const QJsonArray &alarms);
    void onDeviceOperationError(const QString &error);
    void onGroupOperationError(const QString &error);

private:
    void startAutoRefresh();

    DeviceModel m_devices;
    AlarmModel m_alarms;
    GroupModel m_groups;
    HttpClient m_http;
    QTimer m_refreshTimer;
    bool m_online = false;
};

#endif // DATAMANAGER_H
