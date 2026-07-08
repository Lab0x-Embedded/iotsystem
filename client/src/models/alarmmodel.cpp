#include "alarmmodel.h"

AlarmModel::AlarmModel(QObject *parent) : QAbstractTableModel(parent) {}

int AlarmModel::rowCount(const QModelIndex &) const { return m_records.size(); }
int AlarmModel::columnCount(const QModelIndex &) const { return ColCount; }

QVariant AlarmModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_records.size()) return {};
    const AlarmRecord &r = m_records[index.row()];
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ColSeverity: return r.severityText();
            case ColDevice:   return r.deviceId;
            case ColMetric:   return r.metric;
            case ColValue:    return QString::number(r.value, 'f', 2);
            case ColMessage:  return r.message;
            case ColTime:     return r.triggeredAt.toString("hh:mm:ss");
            case ColAck:      return r.acknowledged ? QStringLiteral("已确认") : QStringLiteral("-");
        }
    }
    if (role == Qt::ForegroundRole && index.column() == ColSeverity) return r.severityColor();
    if (role == Qt::CheckStateRole && index.column() == ColAck) return r.acknowledged ? Qt::Checked : Qt::Unchecked;
    return {};
}

QVariant AlarmModel::headerData(int section, Qt::Orientation o, int role) const {
    if (role != Qt::DisplayRole || o != Qt::Horizontal) return {};
    switch (section) {
        case ColSeverity: return QStringLiteral("级别");
        case ColDevice:   return QStringLiteral("设备");
        case ColMetric:   return QStringLiteral("指标");
        case ColValue:    return QStringLiteral("值");
        case ColMessage:  return QStringLiteral("描述");
        case ColTime:     return QStringLiteral("时间");
        case ColAck:      return QStringLiteral("确认");
    }
    return {};
}

void AlarmModel::addRecord(const AlarmRecord &rec) {
    beginInsertRows(QModelIndex(), 0, 0);
    m_records.push_front(rec);
    endInsertRows();
}

void AlarmModel::setRecords(const QVector<AlarmRecord> &records) {
    beginResetModel();
    m_records = records;
    endResetModel();
}

void AlarmModel::acknowledge(int row) {
    if (row < 0 || row >= m_records.size()) return;
    m_records[row].acknowledged = true;
    emit dataChanged(index(row, ColAck), index(row, ColAck));
}

int AlarmModel::unacknowledgedCount() const {
    int n = 0; for (const auto &r : m_records) if (!r.acknowledged) ++n; return n;
}
