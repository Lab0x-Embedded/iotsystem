#include "datamanager.h"
#include <QDebug>

DataManager::DataManager(QObject *parent) : QObject(parent)
{
    // HttpClient信号
    connect(&m_http, &HttpClient::loginSucceeded, this, &DataManager::onLoginSucceeded);
    connect(&m_http, &HttpClient::loginFailed, this, &DataManager::onLoginFailed);
    connect(&m_http, &HttpClient::devicesFetched, this, &DataManager::onDevicesFetched);
    connect(&m_http, &HttpClient::groupsFetched, this, &DataManager::onGroupsFetched);
    connect(&m_http, &HttpClient::alarmsFetched, this, &DataManager::onAlarmsFetched);
    connect(&m_http, &HttpClient::deviceOperationError, this, &DataManager::onDeviceOperationError);
    connect(&m_http, &HttpClient::deviceUpdated, this, &DataManager::onDeviceUpdated);
    connect(&m_http, &HttpClient::groupOperationError, this, &DataManager::onGroupOperationError);
    connect(&m_http, &HttpClient::groupCreated, this, [this](int id)
            { Q_UNUSED(id); refreshGroups(); });
    connect(&m_http, &HttpClient::groupUpdated, this, [this](int id)
            { Q_UNUSED(id); refreshGroups(); });
    connect(&m_http, &HttpClient::groupDeleted, this, [this](int id)
            { Q_UNUSED(id); refreshGroups(); });

    // 自动刷新
    connect(&m_refreshTimer, &QTimer::timeout, this, [this]()
            {
        if (m_online) {
            refreshDevices();
            refreshGroups();
            refreshAlarms();
        } });
}

void DataManager::start() {}

void DataManager::stop()
{
    m_refreshTimer.stop();
    disconnect();
}

void DataManager::setOnline(bool online)
{
    if (m_online != online)
    {
        m_online = online;
        emit onlineChanged();
    }
}

void DataManager::connectToServer(const QString &url, const QString &username, const QString &password)
{
    qDebug() << "[DataManager] connectToServer:" << url << "user:" << username;
    emit connectionStatusChanged("connecting");
    m_http.setServerUrl(url);
    m_http.login(username, password);
}

void DataManager::disconnect()
{
    m_refreshTimer.stop();
    m_online = false;
    emit onlineChanged();
    emit connectionStatusChanged("disconnected");
}

void DataManager::refreshDevices()
{
    if (m_online)
    {
        m_http.fetchDevices();
    }
}

void DataManager::refreshGroups()
{
    if (m_online)
    {
        m_http.fetchGroups();
    }
}

void DataManager::refreshAlarms()
{
    if (m_online)
    {
        m_http.fetchAlarms();
    }
}

// ==================== 槽函数 ====================

void DataManager::onLoginSucceeded(const QString &token, const QString &role)
{
    Q_UNUSED(token);
    Q_UNUSED(role);
    qDebug() << "[DataManager] onLoginSucceeded! Going online.";
    m_online = true;
    emit onlineChanged();
    emit connectionStatusChanged("connected");

    refreshDevices();
    refreshGroups();
    refreshAlarms();

    startAutoRefresh();
}

void DataManager::onLoginFailed(const QString &error)
{
    qDebug() << "[DataManager] onLoginFailed:" << error;
    m_online = false;
    emit onlineChanged();
    emit connectionStatusChanged("failed");
    emit errorOccurred("登录失败: " + error);
}

void DataManager::onDevicesFetched(const QJsonArray &devices)
{
    QVector<DeviceInfo> deviceList;

    for (const auto &item : devices)
    {
        QJsonObject obj = item.toObject();
        DeviceInfo info;

        info.id = obj["device_id"].toString();
        info.name = obj["name"].toString();
        info.productKey = obj["product_key"].toString();

        /* group_id 是整数, 转字符串给界面 */
        int gid = obj["group_id"].toInt();
        info.group = QString::number(gid);

        int state = obj["state"].toInt();
        bool online = obj["online"].toBool();

        /*
         * 状态映射 (对齐服务端 device_state_t):
         *   online=true                       -> Online
         *   state=3 (MAINTENANCE)             -> Maintenance
         *   state=4 (DISABLED)                -> Offline
         *   其他                              -> Offline
         */
        if (online)
        {
            info.status = DeviceStatus::Online;
        }
        else if (state == 3)
        {
            info.status = DeviceStatus::Maintenance;
        }
        else
        {
            info.status = DeviceStatus::Offline;
        }

        info.temperature = 20.0 + (rand() % 150) / 10.0;
        info.humidity = 40.0 + (rand() % 400) / 10.0;
        info.battery = 80.0 + (rand() % 200) / 10.0;

        qint64 ts = 0;
        if (obj.contains("last_online") && obj["last_online"].toDouble() > 0)
            ts = obj["last_online"].toDouble();
        else if (obj.contains("last_active") && obj["last_active"].toDouble() > 0)
            ts = obj["last_active"].toDouble();
        info.lastSeen = (ts > 0) ? QDateTime::fromSecsSinceEpoch(ts) : QDateTime::currentDateTime();

        info.reportCount = obj["report_count"].toInt();
        deviceList.append(info);
    }

    m_devices.setDevices(deviceList);
}

void DataManager::onGroupsFetched(const QJsonArray &groups)
{
    QVector<GroupInfo> groupList;

    for (const auto &item : groups)
    {
        QJsonObject obj = item.toObject();

        int parentId = obj["parent_id"].toInt();
        GroupInfo info;
        info.groupId = obj["group_id"].toInt();
        info.parentId = parentId;
        info.name = obj["group_name"].toString();
        info.description = obj["description"].toString();
        info.deviceCount = obj["device_count"].toInt();
        info.sortOrder = obj["sort_order"].toInt();

        groupList.append(info);
    }

    m_groups.setGroups(groupList);
}

void DataManager::onAlarmsFetched(const QJsonArray &alarms)
{
    QVector<AlarmRecord> alarmList;

    for (const auto &item : alarms)
    {
        QJsonObject obj = item.toObject();
        AlarmRecord record;

        record.id = obj["id"].toInt();
        record.deviceId = obj["device_id"].toString();
        record.metric = obj["metric"].toString();
        record.currentValue = obj["current_value"].toDouble();
        record.threshold = obj["threshold"].toDouble();

        QString severity = obj["severity"].toString();
        if (severity == "critical")
            record.severity = AlarmSeverity::Critical;
        else if (severity == "warning")
            record.severity = AlarmSeverity::Warning;
        else
            record.severity = AlarmSeverity::Info;

        QString status = obj["status"].toString();
        if (status == "active")
            record.status = AlarmStatus::Active;
        else if (status == "acknowledged")
            record.status = AlarmStatus::Acknowledged;
        else
            record.status = AlarmStatus::Resolved;

        record.createdAt = QDateTime::fromString(obj["created_at"].toString(), Qt::ISODate);
        alarmList.append(record);
    }

    m_alarms.setAlarms(alarmList);
}

void DataManager::onDeviceOperationError(const QString &error)
{
    emit errorOccurred("设备操作失败: " + error);
}

void DataManager::updateDeviceGroup(const QString &deviceId, int groupId)
{
    if (m_online) {
        m_http.updateDevice(deviceId, QString(), groupId);
    }
}

void DataManager::updateDeviceName(const QString &deviceId, const QString &name)
{
    if (m_online) {
        m_http.updateDevice(deviceId, name, -1);
    }
}

void DataManager::removeDeviceFromGroup(const QString &deviceId)
{
    if (m_online) {
        m_http.updateDevice(deviceId, QString(), 0);
    }
}

void DataManager::onDeviceUpdated(const QString &deviceId, int groupId)
{
    Q_UNUSED(deviceId);
    Q_UNUSED(groupId);
    refreshDevices();
    refreshGroups();
}

void DataManager::onGroupOperationError(const QString &error)
{
    emit errorOccurred("分组操作失败: " + error);
}

void DataManager::startAutoRefresh()
{
    m_refreshTimer.start(30000);
}
