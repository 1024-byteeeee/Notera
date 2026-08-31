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
    default: return {};
    }
}

QHash<int, QByteArray> LibraryEntryModel::roleNames() const
{
    return {{ItemTypeRole, "itemType"}, {ItemIdRole, "itemId"}, {TitleRole, "title"},
        {CreatedDateRole, "createdDate"}, {PageCountRole, "pageCount"},
        {ThumbnailPathRole, "thumbnailPath"}, {FavoriteRole, "favorite"},
        {FilePathRole, "filePath"}, {FileTypeRole, "fileType"}};
}

void LibraryEntryModel::replaceAll(const QVariantList& folders, const QList<Score>& scores)
{
    QList<Entry> entries;
    entries.reserve(folders.size() + scores.size());
    for (const auto& value : folders) {
        const auto folder = value.toMap();
        entries.append({QStringLiteral("folder"), folder.value(QStringLiteral("id")).toString(),
            folder.value(QStringLiteral("name")).toString(),
            QDateTime::fromMSecsSinceEpoch(folder.value(QStringLiteral("createdAt")).toLongLong())});
    }
    for (const auto& score : scores) {
        entries.append({QStringLiteral("score"), score.id, score.title, score.createdAt, score.pageCount,
            score.thumbnailPath, score.favorite, score.filePath, score.fileType});
    }

    const auto previousCount = m_entries.size();
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
    if (previousCount != m_entries.size()) emit countChanged();
}
