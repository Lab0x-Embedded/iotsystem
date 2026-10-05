#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class HttpClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString authToken READ authToken NOTIFY authTokenChanged)
    Q_PROPERTY(int userId READ userId NOTIFY userIdChanged)

public:
    explicit HttpClient(QObject *parent = nullptr);

    QString serverUrl() const { return m_baseUrl; }
    bool connected() const { return m_connected; }
    QString authToken() const { return m_token; }
    int userId() const { return m_userId; }

    Q_INVOKABLE void setServerUrl(const QString &url);
    Q_INVOKABLE void setAuthToken(const QString &token);
    Q_INVOKABLE void setUserId(int id);
    Q_INVOKABLE void login(const QString &username, const QString &password);

    // 设备管理
    Q_INVOKABLE void fetchDevices();
    Q_INVOKABLE void fetchLatest();
    Q_INVOKABLE void registerDevice(const QString &deviceId, const QString &name,
                                     const QString &productKey, const QString &groupId);
    Q_INVOKABLE void queryDevice(const QString &deviceId);

    // 更新设备信息 (组/名称)
    Q_INVOKABLE void updateDevice(const QString &deviceId,
                                   const QString &name,
                                   int groupId);

    // 分组管理
    Q_INVOKABLE void fetchGroups();
    Q_INVOKABLE void createGroup(const QString &name, const QString &description);
    Q_INVOKABLE void updateGroup(int groupId, const QString &name, const QString &description);
    Q_INVOKABLE void deleteGroup(int groupId);

    // 按分组查询设备
    Q_INVOKABLE void fetchDevicesByGroup(int groupId);

    // 设备影子
    Q_INVOKABLE void getShadow(const QString &deviceId);
    Q_INVOKABLE void updateShadow(const QString &deviceId, const QJsonObject &desired);

    // 指令下发
    Q_INVOKABLE void sendCommand(const QString &deviceId, const QString &cmd,
                                  const QJsonObject &payload = QJsonObject());

    // 告警
    Q_INVOKABLE void fetchAlarms(const QString &deviceId = QString());
    Q_INVOKABLE void addAlarmRule(const QString &deviceId, const QString &metric, int op, double threshold, int severity);
    Q_INVOKABLE void queryRules();
    Q_INVOKABLE void toggleRule(ulong ruleId, bool enabled);
    Q_INVOKABLE void editRule(ulong ruleId, const QString &deviceId, const QString &metric, int op, double threshold, int severity);
    Q_INVOKABLE void deleteRule(ulong ruleId);
    Q_INVOKABLE void acknowledgeAlarm(ulong alarmId, int userId = 0);
    Q_INVOKABLE void resolveAlarm(ulong alarmId);
    Q_INVOKABLE void fetchDataPointHistory(const QString &deviceId, const QString &metric, quint64 startTs, quint64 endTs, int limit = 200);
signals:
    void serverUrlChanged();
    void connectedChanged();
    void authTokenChanged();
    void connectionError(const QString &error);

    // 认证
    void loginSucceeded(const QString &token, const QString &role);
    void loginFailed(const QString &error);

    // 设备
    void devicesFetched(const QJsonArray &devices);
    void latestFetched(const QJsonArray &datapoints);
    void deviceRegistered(const QString &deviceId);
    void deviceQueryResult(const QJsonObject &device);
    void deviceUpdated(const QString &deviceId, int groupId);
    void deviceOperationError(const QString &error);
    void groupDevicesFetched(int groupId, const QJsonArray &devices);

    // 分组
    void groupsFetched(const QJsonArray &groups);
    void groupCreated(int groupId);
    void groupUpdated(int groupId);
    void groupDeleted(int groupId);
    void groupOperationError(const QString &error);

    // 影子
    void shadowFetched(const QString &deviceId, const QJsonObject &shadow);
    void shadowUpdated(const QString &deviceId);
    void shadowError(const QString &error);

    // 指令
    void commandSent(bool ok, const QString &status, const QString &commandId);
    void commandError(const QString &error);

    // 告警
    void alarmsFetched(const QJsonArray &alarms);
    void alarmError(const QString &error);
    void alarmRuleAdded();
    void rulesFetched(const QJsonArray &rules);
    void ruleToggled();
    void ruleEdited();
    void ruleDeleted();
    void alarmAcknowledged(ulong alarmId);
    void alarmResolved(ulong alarmId);
    void userIdChanged();
    void dataPointHistoryFetched(const QJsonArray &points);

private:
    QNetworkRequest makeRequest(const QString &path);
    void handleReply(QNetworkReply *reply, std::function<void(const QJsonObject &)> onSuccess,
                     std::function<void(const QString &)> onError);

    QNetworkAccessManager m_mgr;
    QString m_baseUrl;
    QString m_token;
    int m_userId = 0;
    bool m_connected = false;
};

#endif // HTTPCLIENT_H
