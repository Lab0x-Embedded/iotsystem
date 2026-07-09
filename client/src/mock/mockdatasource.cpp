#include "mockdatasource.h"
#include <QRandomGenerator>
#include <QDateTime>
#include <QtMath>

MockDataSource::MockDataSource(QObject *parent)
    : QObject(parent), m_timer(new QTimer(this)) {
    connect(m_timer, &QTimer::timeout, this, &MockDataSource::onTick);
}

void MockDataSource::buildDevices() {
    struct Seed { QString id; QString name; QString group; };
    QVector<Seed> seeds = {
        {"dev_001", "车间1温湿度", "工厂A"},
        {"dev_002", "车间1电表", "工厂A"},
        {"dev_003", "车间2温湿度", "工厂A"},
        {"dev_004", "仓库温湿度", "工厂B"},
        {"dev_005", "仓库烟感", "工厂B"},
        {"dev_006", "机房温度", "工厂B"},
        {"dev_007", "宿舍空调", "生活区"},
        {"dev_008", "宿舍电表", "生活区"},
        {"dev_009", "门禁主机", "安保"},
        {"dev_010", "摄像头NVR", "安保"},
    };
    for (const auto &s : seeds) {
        DeviceInfo d;
        d.id = s.id; d.name = s.name; d.group = s.group;
        d.productKey = "pk_demo";
        d.status = (QRandomGenerator::global()->bounded(100) < 85)
                       ? DeviceStatus::Online : DeviceStatus::Offline;
        d.temperature = 20.0 + QRandomGenerator::global()->bounded(150) / 10.0;
        d.humidity = 40.0 + QRandomGenerator::global()->bounded(400) / 10.0;
        d.battery = 30.0 + QRandomGenerator::global()->bounded(700) / 10.0;
        d.reportCount = QRandomGenerator::global()->bounded(10000);
        d.lastSeen = QDateTime::currentDateTime().addSecs(-QRandomGenerator::global()->bounded(300));
        m_devices.push_back(d);
    }
}

void MockDataSource::start() {
    buildDevices();
    for (const auto &d : m_devices) emit deviceUpdated(d);
    m_timer->start(2000);
}

void MockDataSource::stop() { m_timer->stop(); }

void MockDataSource::onTick() {
    int i = QRandomGenerator::global()->bounded(m_devices.size());
    DeviceInfo &d = m_devices[i];
    d.temperature += (QRandomGenerator::global()->bounded(20) - 10) / 10.0;
    d.humidity = qBound(20.0, d.humidity + (QRandomGenerator::global()->bounded(20) - 10) / 10.0, 95.0);
    d.lastSeen = QDateTime::currentDateTime();
    d.reportCount++;

    qint64 ts = d.lastSeen.toMSecsSinceEpoch();
    emit dataPoint(d.id, "temperature", d.temperature, ts);
    emit dataPoint(d.id, "humidity", d.humidity, ts);

    double r = QRandomGenerator::global()->bounded(1000) / 10.0;
    if (d.temperature > 32.0 && r < 40.0) {
        d.status = DeviceStatus::Alarm;
        AlarmRecord a;
        a.id = ++m_alarmSeq;
        a.deviceId = d.id;
        a.metric = "temperature";
        a.currentValue = d.temperature;
        a.threshold = 32.0;
        a.severity = d.temperature > 35.0 ? AlarmSeverity::Critical : AlarmSeverity::Warning;
        a.status = AlarmStatus::Active;
        a.createdAt = QDateTime::currentDateTime();
        emit newAlarm(a);
    } else if (d.temperature < 30.0 && d.status == DeviceStatus::Alarm) {
        d.status = DeviceStatus::Online;
    } else if (d.status != DeviceStatus::Maintenance && QRandomGenerator::global()->bounded(100) < 5) {
        d.status = DeviceStatus::Online;
    }
    emit deviceUpdated(d);
}
