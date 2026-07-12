#include "alarmmodel.h"

AlarmModel::AlarmModel(QObject *parent) : QAbstractListModel(parent) {}

void AlarmModel::rebuildFilter() {
    m_filteredIndices.clear();
    for (int i = 0; i < m_records.size(); ++i) {
        const auto &rec = m_records[i];
        if (m_severityFilter >= 0 && static_cast<int>(rec.severity) != m_severityFilter)
            continue;
        if (!m_deviceFilter.isEmpty() && rec.deviceId != m_deviceFilter)
            continue;
        m_filteredIndices.append(i);
    }
}

int AlarmModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return m_filteredIndices.size();
}

QVariant AlarmModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filteredIndices.size())
        return QVariant();

    const AlarmRecord &rec = m_records[m_filteredIndices[index.row()]];

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
    roles[IdRole]           = "id";
    return roles;
}

void AlarmModel::addRecord(const AlarmRecord &rec) {
    beginInsertRows(QModelIndex(), m_records.size(), m_records.size());
    m_records.append(rec);
    endInsertRows();
    rebuildFilter();
    emit countsChanged();
}

void AlarmModel::setAlarms(const QVector<AlarmRecord> &records) {
    beginResetModel();
    m_records = records;
    rebuildFilter();
    endResetModel();
    emit countsChanged();
}

void AlarmModel::clear() {
    beginResetModel();
    m_records.clear();
    m_filteredIndices.clear();
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
    if (row < 0 || row >= m_filteredIndices.size()) return;
    int realRow = m_filteredIndices[row];
    if (m_records[realRow].status == AlarmStatus::Active) {
        m_records[realRow].status = AlarmStatus::Acknowledged;
        m_records[realRow].acknowledged = true;
        QModelIndex idx = index(row, 0);
        emit dataChanged(idx, idx, {StatusRole, StatusTextRole, AcknowledgedRole});
        emit countsChanged();
    }
}

void AlarmModel::setDeviceFilter(const QString &deviceId) {
    m_deviceFilter = deviceId;
    beginResetModel();
    rebuildFilter();
    endResetModel();
    emit layoutChanged();
}

QString AlarmModel::deviceFilter() const {
    return m_deviceFilter;
}

void AlarmModel::setSeverityFilter(int severityIndex) {
    if (m_severityFilter == severityIndex) return;
    m_severityFilter = severityIndex;
    beginResetModel();
    rebuildFilter();
    endResetModel();
}
