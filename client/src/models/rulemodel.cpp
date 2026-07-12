#include "rulemodel.h"

RuleModel::RuleModel(QObject *parent) : QAbstractListModel(parent) {}

int RuleModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return m_rules.size();
}

QVariant RuleModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_rules.size())
        return QVariant();

    const AlarmRule &r = m_rules[index.row()];

    switch (role) {
        case IdRole:       return static_cast<qint64>(r.id);
        case DeviceRole:   return r.deviceId.isEmpty() ? QStringLiteral("*") : r.deviceId;
        case MetricRole:   return r.metric;
        case OpRole:       return r.opText();
        case ThresholdRole:return QString::number(r.threshold, 'f', 2);
        case SeverityRole: return r.severityText();
        case EnabledRole:  return r.enabled ? QStringLiteral("启用") : QStringLiteral("禁用");
        default:           return QVariant();
    }
}

QHash<int, QByteArray> RuleModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[IdRole]       = "id";
    roles[DeviceRole]   = "deviceId";
    roles[MetricRole]   = "metric";
    roles[OpRole]       = "op";
    roles[ThresholdRole]= "threshold";
    roles[SeverityRole] = "severity";
    roles[EnabledRole]  = "enabled";
    return roles;
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
