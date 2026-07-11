#include "rulemodel.h"

RuleModel::RuleModel(QObject *parent) : QAbstractTableModel(parent) {}

int RuleModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return m_rules.size();
}

int RuleModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return ColCount;
}

QVariant RuleModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_rules.size())
        return QVariant();

    const AlarmRule &r = m_rules[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ColDevice:    return r.deviceId.isEmpty() ? QStringLiteral("*") : r.deviceId;
            case ColMetric:    return r.metric;
            case ColOp:        return r.opText();
            case ColThreshold: return QString::number(r.threshold, 'f', 2);
            case ColSeverity:  return r.severityText();
            case ColEnabled:   return r.enabled ? QStringLiteral("启用") : QStringLiteral("禁用");
        }
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColThreshold)
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        return static_cast<int>(Qt::AlignCenter);
    }

    return QVariant();
}

QVariant RuleModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QVariant();

    switch (section) {
        case ColDevice:    return QStringLiteral("设备");
        case ColMetric:    return QStringLiteral("指标");
        case ColOp:        return QStringLiteral("条件");
        case ColThreshold: return QStringLiteral("阈值");
        case ColSeverity:  return QStringLiteral("级别");
        case ColEnabled:   return QStringLiteral("状态");
    }
    return QVariant();
}

void RuleModel::setRules(const QVector<AlarmRule> &rules) {
    beginResetModel();
    m_rules = rules;
    endResetModel();
}

void RuleModel::clear() {
    beginResetModel();
    m_rules.clear();
    endResetModel();
}
