#include "groupmodel.h"

GroupModel::GroupModel(QObject *parent) : QAbstractListModel(parent) {}

int GroupModel::rowCount(const QModelIndex &) const {
    return m_groups.size();
}

QVariant GroupModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_groups.size())
        return QVariant();

    const GroupInfo &g = m_groups[index.row()];

    switch (role) {
        case GroupIdRole: return g.groupId;
        case ParentIdRole: return g.parentId;
        case NameRole: return g.name;
        case DescriptionRole: return g.description;
        case DeviceCountRole: return g.deviceCount;
        case SortOrderRole: return g.sortOrder;
        case Qt::DisplayRole: return g.name;
    }

    return QVariant();
}

QHash<int, QByteArray> GroupModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[GroupIdRole] = "groupId";
    roles[ParentIdRole] = "parentId";
    roles[NameRole] = "groupName";
    roles[DescriptionRole] = "description";
    roles[DeviceCountRole] = "deviceCount";
    roles[SortOrderRole] = "sortOrder";
    return roles;
}

void GroupModel::setGroups(const QVector<GroupInfo> &groups) {
    beginResetModel();
    m_groups = groups;
    m_groupNameCache.clear();
    for (const auto &g : groups)
        m_groupNameCache[g.groupId] = g.name;
    endResetModel();
    emit countsChanged();
}

void GroupModel::addGroup(const GroupInfo &group) {
    beginInsertRows(QModelIndex(), m_groups.size(), m_groups.size());
    m_groups.append(group);
    m_groupNameCache[group.groupId] = group.name;
    endInsertRows();
    emit countsChanged();
}

void GroupModel::updateGroup(const GroupInfo &group) {
    for (int i = 0; i < m_groups.size(); ++i) {
        if (m_groups[i].groupId == group.groupId) {
            m_groups[i] = group;
            m_groupNameCache[group.groupId] = group.name;
            emit dataChanged(index(i), index(i));
            return;
        }
    }
}

void GroupModel::removeGroup(int groupId) {
    for (int i = 0; i < m_groups.size(); ++i) {
        if (m_groups[i].groupId == groupId) {
            beginRemoveRows(QModelIndex(), i, i);
            m_groups.removeAt(i);
            m_groupNameCache.remove(groupId);
            endRemoveRows();
            emit countsChanged();
            return;
        }
    }
}

void GroupModel::clear() {
    beginResetModel();
    m_groups.clear();
    m_groupNameCache.clear();
    endResetModel();
    emit countsChanged();
}

int GroupModel::deviceCountForGroup(int groupId) const {
    for (const auto &g : m_groups) {
        if (g.groupId == groupId)
            return g.deviceCount;
    }
    return 0;
}

QString GroupModel::groupName(int groupId) const {
    return m_groupNameCache.value(groupId);
}
