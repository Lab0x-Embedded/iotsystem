#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include <QString>

struct GroupInfo {
    int groupId;
    int parentId;
    QString name;
    QString description;
    int deviceCount;
    int sortOrder;
};

class GroupModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countsChanged)

public:
    enum Roles {
        GroupIdRole = Qt::UserRole + 1,
        ParentIdRole,
        NameRole,
        DescriptionRole,
        DeviceCountRole,
        SortOrderRole
    };

    explicit GroupModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setGroups(const QVector<GroupInfo> &groups);
    void addGroup(const GroupInfo &group);
    void updateGroup(const GroupInfo &group);
    void removeGroup(int groupId);
    void clear();
    
    int totalCount() const { return m_groups.size(); }
    Q_INVOKABLE int deviceCountForGroup(int groupId) const;
    Q_INVOKABLE QString groupName(int groupId) const;

signals:
    void countsChanged();

private:
    QVector<GroupInfo> m_groups;
};

#endif // GROUPMODEL_H
