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
    default: return {};
    }
}

QHash<int, QByteArray> NamedListModel::roleNames() const
{
    return {{ItemIdRole, "itemId"}, {NameRole, "name"}};
}

void NamedListModel::replaceAll(const QVariantList& values)
{
    QList<Item> items;
    items.reserve(values.size());
    for (const auto& value : values) {
        const auto map = value.toMap();
        const auto id = map.value(QStringLiteral("id")).toString();
        const auto name = map.value(QStringLiteral("name")).toString();
        if (!id.isEmpty() && !name.isEmpty()) {
            items.append({id, name});
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
