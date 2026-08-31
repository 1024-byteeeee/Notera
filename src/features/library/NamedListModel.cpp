#include "features/library/NamedListModel.h"

#include <utility>

NamedListModel::NamedListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int NamedListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

int NamedListModel::count() const
{
    return m_items.size();
}

QVariant NamedListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const auto& item = m_items.at(index.row());
    switch (role) {
    case ItemIdRole: return item.id;
    case NameRole: return item.name;
    case ParentIdRole: return item.parentId;
    default: return {};
    }
}

QHash<int, QByteArray> NamedListModel::roleNames() const
{
    return {{ItemIdRole, "itemId"}, {NameRole, "name"}, {ParentIdRole, "parentId"}};
}

QVariant NamedListModel::get(const int index) const
{
    if (index < 0 || index >= m_items.size()) {
        return {};
    }
    const auto& item = m_items.at(index);
    return QVariantMap{
        {"itemId", item.id},
        {"name", item.name},
        {"parentId", item.parentId}
    };
}

void NamedListModel::replaceAll(const QVariantList& values)
{
    QList<Item> items;
    items.reserve(values.size());
    for (const auto& value : values) {
        const auto map = value.toMap();
        const auto id = map.value(QStringLiteral("id")).toString();
        const auto name = map.value(QStringLiteral("name")).toString();
        const auto parentId = map.value(QStringLiteral("parentId")).toString();
        if (!id.isEmpty() && !name.isEmpty()) {
            items.append({id, name, parentId});
        }
    }

    const auto previousCount = m_items.size();
    beginResetModel();
    m_items = std::move(items);
    endResetModel();
    if (m_items.size() != previousCount) {
        emit countChanged();
    }
}
