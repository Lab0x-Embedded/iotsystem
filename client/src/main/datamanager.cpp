#include "datamanager.h"

DataManager::DataManager(QObject *parent) : QObject(parent) {
    connect(&m_mock, &MockDataSource::deviceUpdated, this, &DataManager::deviceUpdated);
    connect(&m_mock, &MockDataSource::newAlarm, this, &DataManager::newAlarm);
    connect(&m_mock, &MockDataSource::dataPoint, this, &DataManager::pushDataPoint);
    connect(&m_devices, &DeviceModel::countsChanged, this, &DataManager::countsChanged);
}

void DataManager::start() {
    m_devices.setDevices({});
    m_mock.start();
}

void DataManager::stop() {
    m_mock.stop();
}

void DataManager::setOnline(bool online) {
    m_online = online;
}

void DataManager::pushDataPoint(const QString &deviceId, const QString &metric, double value, qint64 ts) {
    emit dataPointArrived(deviceId, metric, value, ts);
}
