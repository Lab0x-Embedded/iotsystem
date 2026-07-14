#include "rulemodel.h"

RuleModel::RuleModel(QObject *parent) : QAbstractListModel(parent) {}

void RuleModel::rebuildFilter() {
    m_filteredIndices.clear();
    for (int i = 0; i < m_rules.size(); ++i) {
        const auto &r = m_rules[i];
        if (!m_deviceFilter.isEmpty()) {
            QString did = r.deviceId.isEmpty() ? QStringLiteral("*") : r.deviceId;
            if (did != m_deviceFilter)
                continue;
        }
        m_filteredIndices.append(i);
    }
}

int RuleModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return m_filteredIndices.size();
}

QVariant RuleModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filteredIndices.size())
        return QVariant();

    const AlarmRule &r = m_rules[m_filteredIndices[index.row()]];

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
    rebuildFilter();
    endResetModel();
}

void RuleModel::clear() {
    beginResetModel();
    m_rules.clear();
    m_filteredIndices.clear();
    endResetModel();
}

void RuleModel::setDeviceFilter(const QString &deviceId) {
    m_deviceFilter = deviceId;
    beginResetModel();
    rebuildFilter();
    endResetModel();
}
