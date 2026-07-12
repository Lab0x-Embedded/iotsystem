#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <QObject>
#include <QTimer>
#include "models/devicemodel.h"
#include "models/alarmmodel.h"
#include "models/rulemodel.h"
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
    RuleModel *ruleModel() { return &m_rules; }
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
    Q_INVOKABLE void refreshRules();
    Q_INVOKABLE void toggleRule(ulong ruleId, bool enabled);
    Q_INVOKABLE void editRule(ulong ruleId, const QString &deviceId, const QString &metric, int op, double threshold, int severity);
    Q_INVOKABLE void deleteRule(ulong ruleId);
    Q_INVOKABLE void addAlarmRule(const QString &deviceId, const QString &metric, int op, double threshold, int severity);
    Q_INVOKABLE void acknowledgeAlarm(ulong alarmId);
    Q_INVOKABLE void resolveAlarm(ulong alarmId);
    Q_INVOKABLE void fetchDataPointHistory(const QString &deviceId, const QString &metric, quint64 startTs, quint64 endTs, int limit = 200);
    Q_INVOKABLE void updateDeviceGroup(const QString &deviceId, int groupId);
    Q_INVOKABLE void updateDeviceName(const QString &deviceId, const QString &name);
    Q_INVOKABLE void removeDeviceFromGroup(const QString &deviceId);

signals:
    void onlineChanged();
    void serverUrlChanged();
    void countsChanged();
    void connectionStatusChanged(const QString &status);
    void deviceUpdated(const DeviceInfo &d);
    void newAlarm(const AlarmRecord &alarm);
    void dataPointArrived(const QString &deviceId, const QString &metric, double value, qint64 ts);
    void errorOccurred(const QString &error);
    void alarmRuleAdded();
    void ruleToggled();
    void ruleEdited();
    void ruleDeleted();

private slots:
    void onLoginSucceeded(const QString &token, const QString &role);
    void onLoginFailed(const QString &error);
    void onDevicesFetched(const QJsonArray &devices);
    void onGroupsFetched(const QJsonArray &groups);
    void onAlarmsFetched(const QJsonArray &alarms);
    void onRulesFetched(const QJsonArray &rules);
    void onDeviceOperationError(const QString &error);
    void onDeviceUpdated(const QString &deviceId, int groupId);
    void onGroupOperationError(const QString &error);

private:
    void startAutoRefresh();

    DeviceModel m_devices;
    AlarmModel m_alarms;
    GroupModel m_groups;
    RuleModel m_rules;
    HttpClient m_http;
    QTimer m_refreshTimer;
    bool m_online = false;
};

#endif // DATAMANAGER_H
