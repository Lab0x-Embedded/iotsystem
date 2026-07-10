#ifndef DEVICEMODEL_H
#define DEVICEMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QString>
#include <QDateTime>
#include <QColor>

enum class DeviceStatus { Offline = 0, Online = 1, Alarm = 2, Maintenance = 3 };

struct DeviceInfo {
    QString id;
    QString name;
    QString productKey;
    QString group;
    DeviceStatus status = DeviceStatus::Offline;
    double temperature = 0.0;
    double humidity = 0.0;
    double battery = 100.0;
    int reportCount = 0;
    QDateTime lastSeen;

    QString statusText() const {
        switch (status) {
            case DeviceStatus::Online: return QStringLiteral("在线");
            case DeviceStatus::Offline: return QStringLiteral("离线");
            case DeviceStatus::Alarm: return QStringLiteral("告警");
            case DeviceStatus::Maintenance: return QStringLiteral("维护");
        }
        return QStringLiteral("未知");
    }
    QColor statusColor() const {
        switch (status) {
            case DeviceStatus::Online: return QColor("#4CAF50");
            case DeviceStatus::Offline: return QColor("#9E9E9E");
            case DeviceStatus::Alarm: return QColor("#FF5722");
            case DeviceStatus::Maintenance: return QColor("#FFC107");
        }
        return QColor("#9E9E9E");
    }
};

class DeviceModel : public QAbstractTableModel {
    Q_OBJECT
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countsChanged)
    Q_PROPERTY(int onlineCount READ onlineCount NOTIFY countsChanged)
    Q_PROPERTY(int alarmCount READ alarmCount NOTIFY countsChanged)

public:
    enum Column {
        ColStatus = 0, ColId, ColName, ColGroup,
        ColTemp, ColHumid, ColBattery, ColLastSeen,
        ColCount
    };

    enum Roles {
        StatusRole = Qt::UserRole + 1,
        IdRole,
        NameRole,
        GroupRole,
        TempRole,
        HumidRole,
        BatteryRole,
        LastSeenRole
    };

    explicit DeviceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE const DeviceInfo &deviceAt(int row) const { return m_devices[row]; }
    void setDevices(const QVector<DeviceInfo> &devices);
    void updateDevice(const DeviceInfo &device);
    void clear();
    int totalCount() const { return m_devices.size(); }
    int onlineCount() const;
    int alarmCount() const;

signals:
    void countsChanged(int total, int online, int alarm);

private:
    QVector<DeviceInfo> m_devices;
};

#endif // DEVICEMODEL_H
