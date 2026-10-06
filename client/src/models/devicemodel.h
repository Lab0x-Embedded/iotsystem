#ifndef DEVICEMODEL_H
#define DEVICEMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QString>
#include <QDateTime>
#include <QColor>

enum class DeviceStatus { Offline = 0, Online = 1, Alarm = 2, Maintenance = 3 };

struct DeviceInfo {
    Q_GADGET
public:
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString productKey MEMBER productKey)
    Q_PROPERTY(QString deviceType MEMBER deviceType)
    Q_PROPERTY(QString deviceSecret MEMBER deviceSecret)
    Q_PROPERTY(QString group MEMBER group)
    Q_PROPERTY(DeviceStatus status MEMBER status)
    Q_PROPERTY(double temperature MEMBER temperature)
    Q_PROPERTY(double humidity MEMBER humidity)
    Q_PROPERTY(double battery MEMBER battery)
    Q_PROPERTY(int reportCount MEMBER reportCount)
    Q_PROPERTY(QDateTime lastSeen MEMBER lastSeen)
    QString id;
    QString name;
    QString productKey;
    QString deviceType;
    QString deviceSecret;
    QString group;
    DeviceStatus status = DeviceStatus::Offline;
    double temperature = 0.0;
    double humidity = 0.0;
    double battery = 100.0;
    int reportCount = 0;
    QDateTime lastSeen;

    // Q_INVOKABLE：列表页/详情页 QML 需要直接调用
    Q_INVOKABLE QString statusText() const {
        switch (status) {
            case DeviceStatus::Online: return QStringLiteral("在线");
            case DeviceStatus::Offline: return QStringLiteral("离线");
            case DeviceStatus::Alarm: return QStringLiteral("告警");
            case DeviceStatus::Maintenance: return QStringLiteral("维护");
        }
        return QStringLiteral("未知");
    }
    Q_INVOKABLE QColor statusColor() const {
        switch (status) {
            case DeviceStatus::Online: return QColor("#4CAF50");
            case DeviceStatus::Offline: return QColor("#9E9E9E");
            case DeviceStatus::Alarm: return QColor("#FF5722");
            case DeviceStatus::Maintenance: return QColor("#FFC107");
        }
        return QColor("#9E9E9E");
    }
};

 Q_DECLARE_METATYPE(DeviceInfo)

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

    Q_INVOKABLE QVariant deviceAt(int row) const { if (row < 0 || row >= m_devices.size()) return QVariant(); return QVariant::fromValue(m_devices[row]); }
    Q_INVOKABLE QVariantList devicesByGroup(int groupId) const;
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
