#include "features/library/ScoreListModel.h"

ScoreListModel::ScoreListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int ScoreListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_scores.size();
}

int ScoreListModel::count() const
{
    return m_scores.size();
}

QVariant ScoreListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_scores.size()) {
        return {};
    }

    const auto& score = m_scores.at(index.row());
    switch (role) {
    case IdRole: return score.id;
    case TitleRole: return score.title;
    case ComposerRole: return score.composer;
    case CreatedDateRole: return score.createdAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd"));
    case PageCountRole: return score.pageCount;
    case ThumbnailPathRole: return score.thumbnailPath;
    case FavoriteRole: return score.favorite;
    case FilePathRole: return score.filePath;
    case FileTypeRole: return score.fileType;
    default: return {};
    }
}

QHash<int, QByteArray> ScoreListModel::roleNames() const
{
    return {
        {IdRole, "scoreId"}, {TitleRole, "title"}, {ComposerRole, "composer"}, {CreatedDateRole, "createdDate"},
        {PageCountRole, "pageCount"}, {ThumbnailPathRole, "thumbnailPath"},
        {FavoriteRole, "favorite"}, {FilePathRole, "filePath"}, {FileTypeRole, "fileType"}
    };
}

void ScoreListModel::replaceAll(QList<Score> scores)
{
    const auto previousCount = m_scores.size();
    beginResetModel();
    m_scores = std::move(scores);
    endResetModel();
    if (m_scores.size() != previousCount) {
        emit countChanged();
    }
}
