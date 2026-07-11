#ifndef ALARMMODEL_H
#define ALARMMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QString>
#include <QDateTime>
#include <QColor>
#include <QBrush>

enum class AlarmSeverity { Info = 0, Warning = 1, Critical = 2 };
enum class AlarmStatus { Active = 0, Acknowledged = 1, Resolved = 2 };

struct AlarmRecord {
    uint64_t id = 0;
    QString deviceId;
    QString metric;
    double currentValue = 0.0;
    double threshold = 0.0;
    AlarmSeverity severity = AlarmSeverity::Info;
    AlarmStatus status = AlarmStatus::Active;
    QDateTime createdAt;
    QDateTime resolvedAt;
    bool acknowledged = false;

    QString severityText() const {
        switch (severity) {
            case AlarmSeverity::Critical: return QStringLiteral("严重");
            case AlarmSeverity::Warning:  return QStringLiteral("警告");
            case AlarmSeverity::Info:     return QStringLiteral("信息");
        }
        return QStringLiteral("?");
    }
    QColor severityColor() const {
        switch (severity) {
            case AlarmSeverity::Critical: return QColor("#FF5722");
            case AlarmSeverity::Warning:  return QColor("#FFC107");
            case AlarmSeverity::Info:     return QColor("#2196F3");
        }
        return QColor("#9E9E9E");
    }
    QString statusText() const {
        switch (status) {
            case AlarmStatus::Active: return QStringLiteral("活跃");
            case AlarmStatus::Acknowledged: return QStringLiteral("已确认");
            case AlarmStatus::Resolved: return QStringLiteral("已解决");
        }
        return QStringLiteral("?");
    }
};

class AlarmModel : public QAbstractTableModel {
    Q_OBJECT
    Q_PROPERTY(int activeCount READ activeCount NOTIFY countsChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countsChanged)
    Q_PROPERTY(int unacknowledledCount READ unacknowledledCount NOTIFY countsChanged)

public:
    enum Column {
        ColSeverity = 0, ColDevice, ColMetric, ColValue, ColThreshold,
        ColStatus, ColTime, ColCount
    };
    explicit AlarmModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    const AlarmRecord &at(int row) const { return m_records[row]; }
    void addRecord(const AlarmRecord &rec);
    void setAlarms(const QVector<AlarmRecord> &records);
    void clear();
    int activeCount() const;
    int totalCount() const { return m_records.size(); }
    int unacknowledledCount() const;

    Q_INVOKABLE void acknowledge(int row);
    Q_INVOKABLE void setDeviceFilter(const QString &deviceId);
    QString deviceFilter() const;

signals:
    void countsChanged();

private:
    QVector<AlarmRecord> m_records;
    QString m_deviceFilter;
};

#endif // ALARMMODEL_H
