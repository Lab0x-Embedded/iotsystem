#include "alarmmodel.h"

AlarmModel::AlarmModel(QObject *parent) : QAbstractTableModel(parent) {}

int AlarmModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return m_records.size();
}

int AlarmModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return ColCount;
}

QVariant AlarmModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_records.size())
        return QVariant();

    const AlarmRecord &rec = m_records[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ColSeverity: return rec.severityText();
            case ColDevice: return rec.deviceId;
            case ColMetric: return rec.metric;
            case ColValue: return QString::number(rec.currentValue, 'f', 2);
            case ColThreshold: return QString::number(rec.threshold, 'f', 2);
            case ColStatus: return rec.statusText();
            case ColTime: return rec.createdAt.toString("yyyy-MM-dd hh:mm:ss");
        }
    } else if (role == Qt::ForegroundRole) {
        if (index.column() == ColSeverity)
            return QBrush(rec.severityColor());
        if (index.column() == ColStatus) {
            if (rec.status == AlarmStatus::Active)
                return QBrush(QColor("#FF5722"));
            else if (rec.status == AlarmStatus::Acknowledged)
                return QBrush(QColor("#FFC107"));
            else
                return QBrush(QColor("#4CAF50"));
        }
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColValue || index.column() == ColThreshold)
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
    }

    return QVariant();
}

QVariant AlarmModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QVariant();

    switch (section) {
        case ColSeverity: return QStringLiteral("级别");
        case ColDevice: return QStringLiteral("设备ID");
        case ColMetric: return QStringLiteral("指标");
        case ColValue: return QStringLiteral("当前值");
        case ColThreshold: return QStringLiteral("阈值");
        case ColStatus: return QStringLiteral("状态");
        case ColTime: return QStringLiteral("触发时间");
    }
    return QVariant();
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
