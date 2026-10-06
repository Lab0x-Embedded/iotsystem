#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <QObject>
#include <QStringList>
#include <QTimer>
#include "models/devicemodel.h"
#include "models/alarmmodel.h"
#include "models/rulemodel.h"
#include "models/groupmodel.h"
#include "network/httpclient.h"
#include "mock/mockdatasource.h"

class DataManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool online READ isOnline WRITE setOnline NOTIFY onlineChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY countsChanged)
    Q_PROPERTY(int onlineDeviceCount READ onlineDeviceCount NOTIFY countsChanged)
    Q_PROPERTY(int alarmCount READ alarmCount NOTIFY countsChanged)
    Q_PROPERTY(HttpClient *httpClient READ httpClient CONSTANT)
    /** 产品列表 [{id, product_id, product_key, product_name, description, device_count}] */
    Q_PROPERTY(QVariantList products READ products NOTIFY productsChanged)

public:
    explicit DataManager(QObject *parent = nullptr);

    DeviceModel *deviceModel() { return &m_devices; }
    AlarmModel *alarmModel() { return &m_alarms; }
    GroupModel *groupModel() { return &m_groups; }
    RuleModel *ruleModel() { return &m_rules; }
    HttpClient *httpClient() { return &m_http; }
    QVariantList products() const { return m_products; }

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

    /** 按 product_key 查产品名；找不到时返回原 key（用于详情页展示） */
    Q_INVOKABLE QString productNameOf(const QString &productKey) const;

    // 产品管理
    Q_INVOKABLE void refreshProducts();
    Q_INVOKABLE void createProduct(const QString &productKey, const QString &productName,
                                   const QString &description);
    Q_INVOKABLE void updateProduct(int id, const QString &productName, const QString &description);
    Q_INVOKABLE void deleteProduct(int id);

    /** 注册设备(deviceSecret 留空则服务端自动生成)；成功后发 registered 信号 */
    Q_INVOKABLE void registerDevice(const QString &deviceId, const QString &name,
                                    const QString &productKey, const QString &deviceType,
                                    const QString &deviceSecret, int groupId);

    /* ---- 动态指标查询 (不再写死 temperature/humidity) ---- */
    /** 某设备当前已上报的指标名列表 (已排序) */
    Q_INVOKABLE QStringList metricNamesFor(const QString &deviceId) const;
    /** 某指标最新值; 无数据返回 NaN */
    Q_INVOKABLE double latestValue(const QString &deviceId, const QString &metric) const;
    /** 指标中文名 (如 temperature -> 温度) */
    Q_INVOKABLE QString metricLabel(const QString &metric) const;
    /** 指标单位 (如 temperature -> °C) */
    Q_INVOKABLE QString metricUnit(const QString &metric) const;
    /** 一行的紧凑摘要, 如 "温度 23.5°C · 湿度 50%" */
    Q_INVOKABLE QString metricSummary(const QString &deviceId, int maxItems = 2) const;

signals:
    void onlineChanged();
    void serverUrlChanged();
    void countsChanged();
    void connectionStatusChanged(const QString &status);
    void deviceUpdated(const DeviceInfo &d);
    void deviceRegistered(const QString &deviceId, const QString &secret);

    // 产品
    void productsChanged();
    void productCreated(const QString &productKey);
    void productUpdated(int id);
    void productDeleted(int id);
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
    MockDataSource m_mock;
    QTimer m_refreshTimer;
    QMap<QString, QMap<QString, double>> m_deviceMetrics;  // deviceId -> metric -> value
    QVariantList m_products;                                // 产品列表
    bool m_online = false;
};

#endif // DATAMANAGER_H
