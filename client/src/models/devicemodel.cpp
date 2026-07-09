#include "devicemodel.h"
#include <QDebug>
DeviceModel::DeviceModel(QObject *parent) : QAbstractTableModel(parent) {}
int DeviceModel::rowCount(const QModelIndex &) const { return m_devices.size(); }
int DeviceModel::columnCount(const QModelIndex &) const { return ColCount; }
QHash<int, QByteArray> DeviceModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[StatusRole] = "status";
    roles[IdRole] = "deviceId";
    roles[NameRole] = "deviceName";
    roles[GroupRole] = "group";
    roles[TempRole] = "temperature";
    roles[HumidRole] = "humidity";
    roles[BatteryRole] = "battery";
    roles[LastSeenRole] = "lastSeen";
    return roles;
}
QVariant DeviceModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_devices.size()) return QVariant();
    const DeviceInfo &d = m_devices[index.row()];
    
    // 自定义角色
    switch (role) {
        case StatusRole: return static_cast<int>(d.status);
        case IdRole: return d.id;
        case NameRole: return d.name;
        case GroupRole: return d.group;
        case TempRole: return d.temperature;
        case HumidRole: return d.humidity;
        case BatteryRole: return d.battery;
        case LastSeenRole: return d.lastSeen.isValid() ? d.lastSeen.toString("yyyy-MM-dd hh:mm:ss") : "-";
    }
    
    // 标准DisplayRole (用于TableView)
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ColStatus: return d.statusText();
            case ColId: return d.id;
            case ColName: return d.name;
            case ColGroup: return d.group;
            case ColTemp: return QString::number(d.temperature, 'f', 1) + "°C";
            case ColHumid: return QString::number(d.humidity, 'f', 0) + "%";
            case ColBattery: return QString::number(d.battery, 'f', 0) + "%";
            case ColLastSeen: return d.lastSeen.isValid() ? d.lastSeen.toString("hh:mm:ss") : "-";
            default: return QVariant();
        }
    }
    
    if (role == Qt::ForegroundRole && index.column() == ColStatus) {
        return d.statusColor();
    }
    if (role == Qt::ToolTipRole && index.column() == ColStatus) {
        return d.statusText();
    }
    
    return QVariant();
}
QVariant DeviceModel::headerData(int section, Qt::Orientation o, int role) const {
    if (role != Qt::DisplayRole || o != Qt::Horizontal) return QVariant();
    switch (section) {
        case ColStatus: return QStringLiteral("状态");
        case ColId: return QStringLiteral("设备ID");
        case ColName: return QStringLiteral("名称");
        case ColGroup: return QStringLiteral("分组");
        case ColTemp: return QStringLiteral("温度");
        case ColHumid: return QStringLiteral("湿度");
        case ColBattery: return QStringLiteral("电量");
        case ColLastSeen: return QStringLiteral("最后上报");
    }
    return QVariant();
}
void DeviceModel::setDevices(const QVector<DeviceInfo> &devices) {
    
    beginResetModel();
    m_devices = devices;
    endResetModel();
    emit countsChanged(totalCount(), onlineCount(), alarmCount());
}
void DeviceModel::updateDevice(const DeviceInfo &device) {
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].id == device.id) {
            m_devices[i] = device;
            emit dataChanged(index(i, 0), index(i, ColCount - 1));
            emit countsChanged(totalCount(), onlineCount(), alarmCount());
            return;
        }
    }
    beginInsertRows(QModelIndex(), m_devices.size(), m_devices.size());
    m_devices.push_back(device);
    endInsertRows();
    emit countsChanged(totalCount(), onlineCount(), alarmCount());
}
void DeviceModel::clear() {
    beginResetModel();
    m_devices.clear();
    endResetModel();
    emit countsChanged(0, 0, 0);
}
int DeviceModel::onlineCount() const {
    int n = 0;
    for (const auto &d : m_devices) if (d.status == DeviceStatus::Online) ++n;
    return n;
}
int DeviceModel::alarmCount() const {
    int n = 0;
    for (const auto &d : m_devices) if (d.status == DeviceStatus::Alarm) ++n;
    return n;
}
