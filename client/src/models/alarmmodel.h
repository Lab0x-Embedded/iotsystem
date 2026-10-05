#ifndef ALARMMODEL_H
#define ALARMMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include <QVariantList>
#include <QString>

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
    QString triggeredAt;
    QString message;
    bool acknowledged = false;
    int acknowledgedBy = 0;
    QString acknowledgedByName;
    QString acknowledgedAt;
    int resolvedBy = 0;
    QString resolvedByName;
    QString resolvedAt;

    QString severityText() const {
        switch (severity) {
            case AlarmSeverity::Critical: return QStringLiteral("严重");
            case AlarmSeverity::Warning:  return QStringLiteral("警告");
            case AlarmSeverity::Info:     return QStringLiteral("信息");
        }
        return QStringLiteral("?");
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

class AlarmModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int activeCount READ activeCount NOTIFY countsChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countsChanged)
    Q_PROPERTY(int unacknowledgedCount READ unacknowledgedCount NOTIFY countsChanged)
    Q_PROPERTY(int unresolvedCount READ unresolvedCount NOTIFY countsChanged)

public:
    enum Role {
        SeverityRole = Qt::UserRole + 1,
        SeverityTextRole,
        DeviceRole,
        MetricRole,
        ValueRole,
        ThresholdRole,
        StatusRole,
        StatusTextRole,
        TimeRole,
        AcknowledgedRole,
        IdRole,
        AcknowledgedByRole,
        AcknowledgedByNameRole,
        AcknowledgedAtRole,
        ResolvedByRole,
        ResolvedByNameRole,
        ResolvedAtRole,
    };

    explicit AlarmModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    const AlarmRecord &at(int row) const { return m_records[row]; }
    void addRecord(const AlarmRecord &rec);
    void setAlarms(const QVector<AlarmRecord> &records);
    void clear();
    int activeCount() const;
    int totalCount() const { return m_records.size(); }
    int unacknowledgedCount() const;
    int unresolvedCount() const;

    Q_INVOKABLE void acknowledge(int row);
    Q_INVOKABLE void resolve(int row);
    Q_INVOKABLE void setDeviceFilter(const QString &deviceId);
    Q_INVOKABLE bool isRowSelectable(int row) const;

    /** 当前可见行(已按过滤)的告警 id, 顺序与行号一致. */
    Q_INVOKABLE QVariantList ids() const;
    QString deviceFilter() const;
    Q_INVOKABLE void setSeverityFilter(int severityIndex);
    int severityFilter() const { return m_severityFilter; }

signals:
    void countsChanged();
    void acknowledgeRequested(ulong alarmId);
    void resolveRequested(ulong alarmId);

private:
    void rebuildFilter();

    QVector<AlarmRecord> m_records;
    QString m_deviceFilter;
    int m_severityFilter = -1; // -1 = 全部, 0 = Info, 1 = Warning, 2 = Critical
    QVector<int> m_filteredIndices;
};

#endif
