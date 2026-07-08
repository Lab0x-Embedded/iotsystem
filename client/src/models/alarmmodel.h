#ifndef ALARMMODEL_H
#define ALARMMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QString>
#include <QDateTime>
#include <QColor>

enum class AlarmSeverity { Info = 0, Warning = 1, Critical = 2 };

struct AlarmRecord {
    uint64_t id = 0;
    QString deviceId;
    QString metric;
    double value = 0.0;
    AlarmSeverity severity = AlarmSeverity::Info;
    QString message;
    QDateTime triggeredAt;
    bool acknowledged = false;

    QString severityText() const {
        switch (severity) {
            case AlarmSeverity::Critical: return QStringLiteral("CRITICAL");
            case AlarmSeverity::Warning:  return QStringLiteral("WARNING");
            case AlarmSeverity::Info:     return QStringLiteral("INFO");
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
};

class AlarmModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColSeverity = 0, ColDevice, ColMetric, ColValue,
        ColMessage, ColTime, ColAck, ColCount
    };
    explicit AlarmModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    const AlarmRecord &at(int row) const { return m_records[row]; }
    void addRecord(const AlarmRecord &rec);
    void acknowledge(int row);
    void setRecords(const QVector<AlarmRecord> &records);
    int unacknowledgedCount() const;

private:
    QVector<AlarmRecord> m_records;
};
#endif
