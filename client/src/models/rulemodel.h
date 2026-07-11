#ifndef RULEMODEL_H
#define RULEMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QString>

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

class RuleModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColDevice = 0, ColMetric, ColOp, ColThreshold, ColSeverity, ColEnabled, ColCount
    };
    explicit RuleModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void setRules(const QVector<AlarmRule> &rules);
    void clear();

private:
    QVector<AlarmRule> m_rules;
};

#endif // RULEMODEL_H
