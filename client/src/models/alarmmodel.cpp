#include "alarmmodel.h"

AlarmModel::AlarmModel(QObject *parent) : QAbstractListModel(parent) {}

int AlarmModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return m_records.size();
}

QVariant AlarmModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_records.size())
        return QVariant();

    const AlarmRecord &rec = m_records[index.row()];

    switch (role) {
        case SeverityRole:      return static_cast<int>(rec.severity);
        case SeverityTextRole:  return rec.severityText();
        case DeviceRole:        return rec.deviceId;
        case MetricRole:        return rec.metric;
        case ValueRole:         return rec.currentValue;
        case ThresholdRole:     return rec.threshold;
        case StatusRole:        return static_cast<int>(rec.status);
        case StatusTextRole:    return rec.statusText();
        case TimeRole:          return rec.triggeredAt;
        case AcknowledgedRole:  return rec.acknowledged;
        case MessageRole:       return rec.message;
        case IdRole:            return static_cast<qint64>(rec.id);
        default:                return QVariant();
    }
}

QHash<int, QByteArray> AlarmModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[SeverityRole]     = "severity";
    roles[SeverityTextRole] = "severityText";
    roles[DeviceRole]       = "deviceId";
    roles[MetricRole]       = "metric";
    roles[ValueRole]        = "value";
    roles[ThresholdRole]    = "threshold";
    roles[StatusRole]       = "status";
    roles[StatusTextRole]   = "statusText";
    roles[TimeRole]         = "triggeredAt";
    roles[AcknowledgedRole] = "acknowledged";
    roles[MessageRole]      = "message";
    roles[IdRole]           = "id";
    return roles;
}

void AlarmModel::addRecord(const AlarmRecord &rec) {
    beginInsertRows(QModelIndex(), m_records.size(), m_records.size());
    m_records.append(rec);
    endInsertRows();
    emit countsChanged();
}

void AlarmModel::setAlarms(const QVector<AlarmRecord> &records) {
    beginResetModel();
    m_records = records;
    endResetModel();
    emit countsChanged();
}

void AlarmModel::clear() {
    beginResetModel();
    m_records.clear();
    endResetModel();
    emit countsChanged();
}

int AlarmModel::activeCount() const {
    int count = 0;
    for (const auto &rec : m_records) {
        if (rec.status == AlarmStatus::Active)
            count++;
    }
    return count;
}

int AlarmModel::unacknowledgedCount() const {
    int count = 0;
    for (const auto &rec : m_records) {
        if (rec.status == AlarmStatus::Active && !rec.acknowledged)
            count++;
    }
    return count;
}

void AlarmModel::acknowledge(int row) {
    if (row < 0 || row >= m_records.size()) return;
    if (m_records[row].status == AlarmStatus::Active) {
        m_records[row].status = AlarmStatus::Acknowledged;
        m_records[row].acknowledged = true;
        QModelIndex idx = index(row, 0);
        emit dataChanged(idx, idx, {StatusRole, StatusTextRole, AcknowledgedRole});
        emit countsChanged();
    }
}

void AlarmModel::setDeviceFilter(const QString &deviceId) {
    m_deviceFilter = deviceId;
    emit layoutChanged();
}

QString AlarmModel::deviceFilter() const {
    return m_deviceFilter;
}
