#include "features/library/LibrarySelectionModel.h"

#include <utility>

LibrarySelectionModel::LibrarySelectionModel(QObject* parent)
    : QObject(parent)
{
}

int LibrarySelectionModel::count() const
{
    return m_selectedIds.size();
}

QVariantList LibrarySelectionModel::selectedIds() const
{
    QVariantList result;
    result.reserve(m_selectedIds.size());
    for (const auto& id : m_selectedIds) result.append(id);
    return result;
}

bool LibrarySelectionModel::contains(const QString& itemId) const
{
    return m_selectedIds.contains(itemId);
}

void LibrarySelectionModel::toggle(const QString& itemId)
{
    if (itemId.isEmpty()) return;
    if (m_selectedIds.contains(itemId)) m_selectedIds.remove(itemId);
    else m_selectedIds.insert(itemId);
    emit selectionChanged();
}

void LibrarySelectionModel::replace(const QVariantList& itemIds)
{
    QSet<QString> replacement;
    for (const auto& value : itemIds) {
        const auto id = value.toString();
        if (!id.isEmpty()) replacement.insert(id);
    }
    if (replacement == m_selectedIds) return;
    m_selectedIds = std::move(replacement);
    emit selectionChanged();
}

void LibrarySelectionModel::clear()
{
    if (m_selectedIds.isEmpty()) return;
    m_selectedIds.clear();
    emit selectionChanged();
}
