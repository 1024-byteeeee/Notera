#include "features/library/LibraryEntryModel.h"

#include <utility>

LibraryEntryModel::LibraryEntryModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int LibraryEntryModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

int LibraryEntryModel::count() const
{
    return m_entries.size();
}

QVariant LibraryEntryModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) return {};
    const auto& entry = m_entries.at(index.row());
    switch (role) {
    case ItemTypeRole: return entry.itemType;
    case ItemIdRole: return entry.itemId;
    case TitleRole: return entry.title;
    case CreatedDateRole: return entry.createdAt.isValid()
        ? entry.createdAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd")) : QString {};
    case PageCountRole: return entry.pageCount;
    case ThumbnailPathRole: return entry.thumbnailPath;
    case FavoriteRole: return entry.favorite;
    case FilePathRole: return entry.filePath;
    case FileTypeRole: return entry.fileType;
    case TagsRole: return entry.tags;
    default: return {};
    }
}

QHash<int, QByteArray> LibraryEntryModel::roleNames() const
{
    return {{ItemTypeRole, "itemType"}, {ItemIdRole, "itemId"}, {TitleRole, "title"},
        {CreatedDateRole, "createdDate"}, {PageCountRole, "pageCount"},
        {ThumbnailPathRole, "thumbnailPath"}, {FavoriteRole, "favorite"},
        {FilePathRole, "filePath"}, {FileTypeRole, "fileType"}, {TagsRole, "tags"}};
}

QVariantList LibraryEntryModel::itemIds() const
{
    QVariantList result;
    result.reserve(m_entries.size());
    for (const auto& entry : m_entries) result.append(entry.itemId);
    return result;
}

void LibraryEntryModel::replaceAll(const QVariantList& folders, const QList<Score>& scores)
{
    QList<Entry> entries;
    entries.reserve(folders.size() + scores.size());
    for (const auto& value : folders) {
        const auto folder = value.toMap();
        entries.append({QStringLiteral("folder"), folder.value(QStringLiteral("id")).toString(),
            folder.value(QStringLiteral("name")).toString(),
            QDateTime::fromMSecsSinceEpoch(folder.value(QStringLiteral("createdAt")).toLongLong()),
            0, {}, folder.value(QStringLiteral("favorite")).toBool(), {}, {},
            folder.value(QStringLiteral("tags")).toStringList()});
    }
    for (const auto& score : scores) {
        entries.append({QStringLiteral("score"), score.id, score.title, score.createdAt, score.pageCount,
            score.thumbnailPath, score.favorite, score.filePath, score.fileType, score.tags});
    }

    const auto previousCount = m_entries.size();
    emit resetStarted();
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
    emit resetFinished();
    if (previousCount != m_entries.size()) emit countChanged();
}

bool LibraryEntryModel::updateEntryTags(const QString& itemId, const QStringList& tags)
{
    // folder 与 score 共用同一 model（itemId 为全局唯一 UUID），均需实时更新
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).itemId != itemId) continue;
        if (m_entries.at(row).tags == tags) return true;
        m_entries[row].tags = tags;
        const auto index = createIndex(row, 0);
        emit dataChanged(index, index, {TagsRole});
        return true;
    }
    return false;
}

bool LibraryEntryModel::updateEntryTitle(const QString& itemId, const QString& title)
{
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).itemId != itemId) continue;
        if (m_entries.at(row).title == title) return true;
        m_entries[row].title = title;
        const auto index = createIndex(row, 0);
        emit dataChanged(index, index, {TitleRole});
        return true;
    }
    return false;
}

bool LibraryEntryModel::updateEntryFavorite(const QString& itemId, bool favorite)
{
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).itemId != itemId) continue;
        if (m_entries.at(row).favorite == favorite) return true;
        m_entries[row].favorite = favorite;
        const auto index = createIndex(row, 0);
        emit dataChanged(index, index, {FavoriteRole});
        return true;
    }
    return false;
}

bool LibraryEntryModel::updateEntryThumbnail(const QString& itemId, const QString& thumbnailPath)
{
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).itemId != itemId) continue;
        if (m_entries.at(row).thumbnailPath == thumbnailPath) return true;
        m_entries[row].thumbnailPath = thumbnailPath;
        const auto index = createIndex(row, 0);
        emit dataChanged(index, index, {ThumbnailPathRole});
        return true;
    }
    return false;
}
