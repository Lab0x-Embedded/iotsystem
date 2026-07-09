#include "datamanager.h"
#include <QDebug>
#include <QRandomGenerator>

DataManager::DataManager(QObject *parent) : QObject(parent) {
    // 连接HttpClient信号
    connect(&m_http, &HttpClient::loginSucceeded, this, &DataManager::onLoginSucceeded);
    connect(&m_http, &HttpClient::loginFailed, this, &DataManager::onLoginFailed);
    connect(&m_http, &HttpClient::devicesFetched, this, &DataManager::onDevicesFetched);
    connect(&m_http, &HttpClient::alarmsFetched, this, &DataManager::onAlarmsFetched);
    connect(&m_http, &HttpClient::deviceOperationError, this, &DataManager::onDeviceOperationError);
    connect(&m_http, &HttpClient::connectionError, this, &DataManager::errorOccurred);
    
    // 设备模型信号
    connect(&m_devices, &DeviceModel::countsChanged, this, &DataManager::countsChanged);
    
    // 自动刷新定时器
    connect(&m_refreshTimer, &QTimer::timeout, this, [this]() {
        if (m_online) {
            refreshDevices();
            refreshAlarms();
        }
    });
}

void DataManager::start() {
    qDebug() << "DataManager started";
    // 启动时不自动连接，等待用户调用connectToServer
}

void DataManager::stop() {
    m_refreshTimer.stop();
    disconnect();
    qDebug() << "DataManager stopped";
}

void DataManager::setOnline(bool online) {
    if (m_online != online) {
        m_online = online;
        emit onlineChanged();
    }
}

void DataManager::connectToServer(const QString &url, const QString &username, const QString &password) {
    emit connectionStatusChanged("connecting");
    m_http.setServerUrl(url);
    m_http.login(username, password);
}

void DataManager::disconnect() {
    m_refreshTimer.stop();
    m_online = false;
    emit onlineChanged();
    emit connectionStatusChanged("disconnected");
}

void DataManager::refreshDevices() {
    if (m_online) {
        m_http.fetchDevices();
    }
}

void DataManager::refreshAlarms() {
    if (m_online) {
        m_http.fetchAlarms();
    }
}

void DataManager::pushDataPoint(const QString &deviceId, const QString &metric, double value, qint64 ts) {
    emit dataPointArrived(deviceId, metric, value, ts);
}

// ==================== 私有槽函数 ====================

void DataManager::onLoginSucceeded(const QString &token, const QString &role) {
    Q_UNUSED(token);
    Q_UNUSED(role);
    m_online = true;
    emit onlineChanged();
    emit connectionStatusChanged("connected");
    
    // 登录成功后立即获取数据
    refreshDevices();
    refreshAlarms();
    
    // 启动自动刷新（每30秒）
    startAutoRefresh();
}

void DataManager::onLoginFailed(const QString &error) {
    m_online = false;
    emit onlineChanged();
    emit connectionStatusChanged("failed");
    emit errorOccurred("登录失败: " + error);
}

void DataManager::onDevicesFetched(const QJsonArray &devices) {
    QVector<DeviceInfo> deviceList;
    
    for (const auto &item : devices) {
        QJsonObject obj = item.toObject();
        DeviceInfo info;
        
        info.id = obj["device_id"].toString();
        info.name = obj["name"].toString();
        info.productKey = obj["product_key"].toString();
        info.group = obj["group"].toString();
        
        // 解析状态 (服务端: 0=REGISTERED, 1=ACTIVE, 2=ONLINE, 3=OFFLINE, 4=DISABLED)
        int state = obj["state"].toInt();
        bool online = obj["online"].toBool();
        
        if (online) {
            info.status = DeviceStatus::Online;
        } else {
            switch (state) {
                case 0: info.status = DeviceStatus::Offline; break;  // REGISTERED
                case 1: info.status = DeviceStatus::Offline; break;  // ACTIVE
                case 2: info.status = DeviceStatus::Online; break;   // ONLINE
                case 3: info.status = DeviceStatus::Offline; break;  // OFFLINE
                case 4: info.status = DeviceStatus::Maintenance; break; // DISABLED
                default: info.status = DeviceStatus::Offline;
            }
        }
        
        // 传感器数据（服务端暂未返回，使用默认值）
        info.temperature = 20.0 + (QRandomGenerator::global()->bounded(1000) % 150) / 10.0;
        info.humidity = 40.0 + (QRandomGenerator::global()->bounded(1000) % 400) / 10.0;
        info.battery = 80.0 + (QRandomGenerator::global()->bounded(1000) % 200) / 10.0;
        
        // 解析时间
        if (obj.contains("last_active") && obj["last_active"].toDouble() > 0) {
            info.lastSeen = QDateTime::fromSecsSinceEpoch(obj["last_active"].toDouble());
        } else {
            info.lastSeen = QDateTime::currentDateTime();
        }
        
        info.reportCount = obj["report_count"].toInt();
        
        deviceList.append(info);
    }
    
    m_devices.setDevices(deviceList);
    qDebug() << "Fetched" << deviceList.size() << "devices from server";
}

void DataManager::onAlarmsFetched(const QJsonArray &alarms) {
    QVector<AlarmRecord> alarmList;
    
    for (const auto &item : alarms) {
        QJsonObject obj = item.toObject();
        AlarmRecord record;
        
        record.id = obj["id"].toInt();
        record.deviceId = obj["device_id"].toString();
        record.metric = obj["metric"].toString();
        record.currentValue = obj["current_value"].toDouble();
        record.threshold = obj["threshold"].toDouble();
        
        QString severity = obj["severity"].toString();
        if (severity == "critical") record.severity = AlarmSeverity::Critical;
        else if (severity == "warning") record.severity = AlarmSeverity::Warning;
        else record.severity = AlarmSeverity::Info;
        
        QString status = obj["status"].toString();
        if (status == "active") record.status = AlarmStatus::Active;
        else if (status == "acknowledged") record.status = AlarmStatus::Acknowledged;
        else record.status = AlarmStatus::Resolved;
        
        record.createdAt = QDateTime::fromString(obj["created_at"].toString(), Qt::ISODate);
        
        alarmList.append(record);
    }
    
    m_alarms.setAlarms(alarmList);
    qDebug() << "Fetched" << alarmList.size() << "alarms from server";
}

void DataManager::onDeviceOperationError(const QString &error) {
    emit errorOccurred("设备操作失败: " + error);
}

void DataManager::startAutoRefresh() {
    m_refreshTimer.start(30000); // 30秒刷新一次
}
