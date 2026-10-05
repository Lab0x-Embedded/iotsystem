#ifndef RULEMODEL_H
#define RULEMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include <QString>
#include <QVariantList>

enum class RuleSeverity { Info = 0, Warning = 1, Critical = 2 };
enum class RuleOp { Gt = 0, Lt, Eq, Gte, Lte };

struct AlarmRule {
    uint64_t id = 0;
    QString deviceId;
    QString metric;
    RuleOp op = RuleOp::Gt;
    double threshold = 0.0;
    RuleSeverity severity = RuleSeverity::Warning;
    bool enabled = true;

    QString opText() const {
        switch (op) {
            case RuleOp::Gt:  return QStringLiteral(">");
            case RuleOp::Lt:  return QStringLiteral("<");
            case RuleOp::Eq:  return QStringLiteral("==");
            case RuleOp::Gte: return QStringLiteral(">=");
            case RuleOp::Lte: return QStringLiteral("<=");
        }
        return QStringLiteral("?");
    }
    QString severityText() const {
        switch (severity) {
            case RuleSeverity::Critical: return QStringLiteral("严重");
            case RuleSeverity::Warning:  return QStringLiteral("警告");
            case RuleSeverity::Info:     return QStringLiteral("信息");
        }
        return QStringLiteral("?");
    }
};

class RuleModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        DeviceRole,
        MetricRole,
        OpRole,
        ThresholdRole,
        SeverityRole,
        EnabledRole
    };

    explicit RuleModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRules(const QVector<AlarmRule> &rules);
    void clear();

    Q_INVOKABLE void setDeviceFilter(const QString &deviceId);

    /** 当前可见行(已按过滤)的规则 id, 顺序与行号一致. 供 QML 做 id 维度选择. */
    Q_INVOKABLE QVariantList ids() const;
    QString deviceFilter() const { return m_deviceFilter; }

private:
    void rebuildFilter();
    QVector<AlarmRule> m_rules;
    QString m_deviceFilter;
    QVector<int> m_filteredIndices;
};

#endif // RULEMODEL_H
