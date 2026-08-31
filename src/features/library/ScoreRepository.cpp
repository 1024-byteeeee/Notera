#include "features/library/ScoreRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariantMap>

namespace {

qint64 milliseconds(const QDateTime& dateTime)
{
    return dateTime.isValid() ? dateTime.toMSecsSinceEpoch() : 0;
}

} // namespace

ScoreRepository::ScoreRepository(QSqlDatabase database)
    : m_database(std::move(database))
{
}

QList<Score> ScoreRepository::list(const QString& searchQuery, QString* error) const
{
    QSqlQuery query(m_database);
    const auto needle = QStringLiteral("%%1%").arg(searchQuery);
    query.prepare(QStringLiteral(R"(
        SELECT id, title, composer, file_name, file_path, file_type, page_count, thumbnail_path,
               favorite, last_page, created_at, updated_at, last_opened_at
        FROM scores
        WHERE title LIKE ? OR composer LIKE ? OR EXISTS (
            SELECT 1 FROM score_tags JOIN tags ON tags.id = score_tags.tag_id
            WHERE score_tags.score_id = scores.id AND tags.name LIKE ?
        )
        ORDER BY favorite DESC, last_opened_at DESC, created_at DESC
    )"));
    query.addBindValue(needle);
    query.addBindValue(needle);
    query.addBindValue(needle);
    if (!query.exec()) {
        *error = query.lastError().text();
        return {};
    }

    QList<Score> scores;
    while (query.next()) {
        Score score;
        score.id = query.value(0).toString();
        score.title = query.value(1).toString();
        score.composer = query.value(2).toString();
        score.fileName = query.value(3).toString();
        score.filePath = query.value(4).toString();
        score.fileType = query.value(5).toString();
        score.pageCount = query.value(6).toInt();
        score.thumbnailPath = query.value(7).toString();
        score.favorite = query.value(8).toBool();
        score.lastPage = query.value(9).toInt();
        score.createdAt = QDateTime::fromMSecsSinceEpoch(query.value(10).toLongLong());
        score.updatedAt = QDateTime::fromMSecsSinceEpoch(query.value(11).toLongLong());
        score.lastOpenedAt = QDateTime::fromMSecsSinceEpoch(query.value(12).toLongLong());
        scores.append(std::move(score));
    }
    return scores;
}

QList<Score> ScoreRepository::listFavorites(const QString& searchQuery, QString* error) const
{
    QSqlQuery query(m_database);
    const auto needle = QStringLiteral("%%1%").arg(searchQuery);
    query.prepare(QStringLiteral(R"(
        SELECT id, title, composer, file_name, file_path, file_type, page_count, thumbnail_path,
               favorite, last_page, created_at, updated_at, last_opened_at
        FROM scores
        WHERE favorite = 1
          AND (title LIKE ? OR composer LIKE ? OR EXISTS (
            SELECT 1 FROM score_tags JOIN tags ON tags.id = score_tags.tag_id
            WHERE score_tags.score_id = scores.id AND tags.name LIKE ?
          ))
        ORDER BY last_opened_at DESC, created_at DESC
    )"));
    query.addBindValue(needle);
    query.addBindValue(needle);
    query.addBindValue(needle);
    if (!query.exec()) {
        *error = query.lastError().text();
        return {};
    }

    QList<Score> scores;
    while (query.next()) {
        Score score;
        score.id = query.value(0).toString();
        score.title = query.value(1).toString();
        score.composer = query.value(2).toString();
        score.fileName = query.value(3).toString();
        score.filePath = query.value(4).toString();
        score.fileType = query.value(5).toString();
        score.pageCount = query.value(6).toInt();
        score.thumbnailPath = query.value(7).toString();
        score.favorite = query.value(8).toBool();
        score.lastPage = query.value(9).toInt();
        score.createdAt = QDateTime::fromMSecsSinceEpoch(query.value(10).toLongLong());
        score.updatedAt = QDateTime::fromMSecsSinceEpoch(query.value(11).toLongLong());
        score.lastOpenedAt = QDateTime::fromMSecsSinceEpoch(query.value(12).toLongLong());
        scores.append(std::move(score));
    }
    return scores;
}

QList<Score> ScoreRepository::listRecent(const QString& searchQuery, QString* error) const
{
    QSqlQuery query(m_database);
    const auto needle = QStringLiteral("%%1%").arg(searchQuery);
    query.prepare(QStringLiteral(R"(
        SELECT id, title, composer, file_name, file_path, file_type, page_count, thumbnail_path,
               favorite, last_page, created_at, updated_at, last_opened_at
        FROM scores
        WHERE title LIKE ? OR composer LIKE ? OR EXISTS (
            SELECT 1 FROM score_tags JOIN tags ON tags.id = score_tags.tag_id
            WHERE score_tags.score_id = scores.id AND tags.name LIKE ?
        )
        ORDER BY last_opened_at IS NULL, last_opened_at DESC, created_at DESC
    )"));
    query.addBindValue(needle);
    query.addBindValue(needle);
    query.addBindValue(needle);
    if (!query.exec()) {
        *error = query.lastError().text();
        return {};
    }

    QList<Score> scores;
    while (query.next()) {
        Score score;
        score.id = query.value(0).toString();
        score.title = query.value(1).toString();
        score.composer = query.value(2).toString();
        score.fileName = query.value(3).toString();
        score.filePath = query.value(4).toString();
        score.fileType = query.value(5).toString();
        score.pageCount = query.value(6).toInt();
        score.thumbnailPath = query.value(7).toString();
        score.favorite = query.value(8).toBool();
        score.lastPage = query.value(9).toInt();
        score.createdAt = QDateTime::fromMSecsSinceEpoch(query.value(10).toLongLong());
        score.updatedAt = QDateTime::fromMSecsSinceEpoch(query.value(11).toLongLong());
        score.lastOpenedAt = QDateTime::fromMSecsSinceEpoch(query.value(12).toLongLong());
        scores.append(std::move(score));
    }
    return scores;
}

bool ScoreRepository::insert(const Score& score, const QString& folderId, QString* error) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"(
        INSERT INTO scores (id, title, composer, file_name, file_path, file_type, page_count, thumbnail_path,
                            favorite, last_page, created_at, updated_at, last_opened_at, folder_id)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )"));
    query.addBindValue(score.id);
    query.addBindValue(score.title);
    query.addBindValue(score.composer);
    query.addBindValue(score.fileName);
    query.addBindValue(score.filePath);
    query.addBindValue(score.fileType);
    query.addBindValue(score.pageCount);
    query.addBindValue(score.thumbnailPath);
    query.addBindValue(score.favorite);
    query.addBindValue(score.lastPage);
    query.addBindValue(milliseconds(score.createdAt));
    query.addBindValue(milliseconds(score.updatedAt));
    query.addBindValue(milliseconds(score.lastOpenedAt));
    query.addBindValue(folderId.isEmpty() ? QVariant(QVariant::String) : folderId);
    if (query.exec()) {
        return true;
    }
    *error = query.lastError().text();
    return false;
}

QList<Score> ScoreRepository::listAtFolder(const QString& folderId, const QString& searchQuery, QString* error) const
{
    QSqlQuery query(m_database);
    const auto needle = QStringLiteral("%%1%").arg(searchQuery);
    query.prepare(QStringLiteral(R"(
        SELECT id, title, composer, file_name, file_path, file_type, page_count, thumbnail_path,
               favorite, last_page, created_at, updated_at, last_opened_at
        FROM scores
        WHERE ((? = '' AND folder_id IS NULL) OR folder_id = ?)
          AND (title LIKE ? OR composer LIKE ? OR EXISTS (
            SELECT 1 FROM score_tags JOIN tags ON tags.id = score_tags.tag_id
            WHERE score_tags.score_id = scores.id AND tags.name LIKE ?
          ))
        ORDER BY favorite DESC, last_opened_at DESC, created_at DESC
    )"));
    query.addBindValue(folderId);
    query.addBindValue(folderId);
    query.addBindValue(needle);
    query.addBindValue(needle);
    query.addBindValue(needle);
    if (!query.exec()) {
        *error = query.lastError().text();
        return {};
    }

    QList<Score> scores;
    while (query.next()) {
        Score score;
        score.id = query.value(0).toString();
        score.title = query.value(1).toString();
        score.composer = query.value(2).toString();
        score.fileName = query.value(3).toString();
        score.filePath = query.value(4).toString();
        score.fileType = query.value(5).toString();
        score.pageCount = query.value(6).toInt();
        score.thumbnailPath = query.value(7).toString();
        score.favorite = query.value(8).toBool();
        score.lastPage = query.value(9).toInt();
        score.createdAt = QDateTime::fromMSecsSinceEpoch(query.value(10).toLongLong());
        score.updatedAt = QDateTime::fromMSecsSinceEpoch(query.value(11).toLongLong());
        score.lastOpenedAt = QDateTime::fromMSecsSinceEpoch(query.value(12).toLongLong());
        scores.append(std::move(score));
    }
    return scores;
}

bool ScoreRepository::setFavorite(const QString& scoreId, const bool favorite, QString* error) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE scores SET favorite = ?, updated_at = ? WHERE id = ?"));
    query.addBindValue(favorite);
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    query.addBindValue(scoreId);
    if (query.exec() && query.numRowsAffected() == 1) return true;
    *error = query.lastError().text();
    return false;
}

bool ScoreRepository::rename(const QString& scoreId, const QString& title, QString* error) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE scores SET title = ?, updated_at = ? WHERE id = ?"));
    query.addBindValue(title.trimmed());
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    query.addBindValue(scoreId);
    if (query.exec() && query.numRowsAffected() == 1) return true;
    *error = query.lastError().text();
    return false;
}

bool ScoreRepository::updateThumbnail(const QString& scoreId, const QString& thumbnailPath, QString* error) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE scores SET thumbnail_path = ?, updated_at = ? WHERE id = ?"));
    query.addBindValue(thumbnailPath);
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    query.addBindValue(scoreId);
    if (query.exec() && query.numRowsAffected() == 1) return true;
    *error = query.lastError().text();
    return false;
}

bool ScoreRepository::remove(const QString& scoreId, QString* error) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM scores WHERE id = ?"));
    query.addBindValue(scoreId);
    if (query.exec() && query.numRowsAffected() == 1) return true;
    *error = query.lastError().text();
    return false;
}

bool ScoreRepository::setFolder(const QString& scoreId, const QString& folderId, QString* error) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE scores SET folder_id = ?, updated_at = ? WHERE id = ?"));
    query.addBindValue(folderId.isEmpty() ? QVariant(QVariant::String) : folderId);
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    query.addBindValue(scoreId);
    if (query.exec() && query.numRowsAffected() == 1) return true;
    *error = query.lastError().text();
    return false;
}

bool ScoreRepository::addTag(const QString& scoreId, const QString& tagId, QString* error) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO score_tags (score_id, tag_id) VALUES (?, ?)"));
    query.addBindValue(scoreId);
    query.addBindValue(tagId);
    if (query.exec()) return true;
    *error = query.lastError().text();
    return false;
}

bool ScoreRepository::removeTag(const QString& scoreId, const QString& tagId, QString* error) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM score_tags WHERE score_id = ? AND tag_id = ?"));
    query.addBindValue(scoreId);
    query.addBindValue(tagId);
    if (query.exec()) return true;
    *error = query.lastError().text();
    return false;
}

QVariantList ScoreRepository::scoreTags(const QString& scoreId, QString* error) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT tags.id, tags.name FROM score_tags JOIN tags ON tags.id = score_tags.tag_id WHERE score_tags.score_id = ? ORDER BY tags.name"));
    query.addBindValue(scoreId);
    if (!query.exec()) {
        *error = query.lastError().text();
        return {};
    }
    QVariantList result;
    while (query.next()) {
        result.append(QVariantMap{{"id", query.value(0)}, {"name", query.value(1)}});
    }
    return result;
}

QList<Score> ScoreRepository::listByFolder(const QString& folderId, const QString& searchQuery, QString* error) const
{
    QSqlQuery query(m_database);
    const auto needle = QStringLiteral("%%1%").arg(searchQuery);
    query.prepare(QStringLiteral(R"(
        SELECT id, title, composer, file_name, file_path, file_type, page_count, thumbnail_path,
               favorite, last_page, created_at, updated_at, last_opened_at
        FROM scores
        WHERE folder_id = ?
          AND (title LIKE ? OR composer LIKE ? OR EXISTS (
            SELECT 1 FROM score_tags JOIN tags ON tags.id = score_tags.tag_id
            WHERE score_tags.score_id = scores.id AND tags.name LIKE ?
          ))
        ORDER BY favorite DESC, last_opened_at DESC, created_at DESC
    )"));
    query.addBindValue(folderId);
    query.addBindValue(needle);
    query.addBindValue(needle);
    query.addBindValue(needle);
    if (!query.exec()) {
        *error = query.lastError().text();
        return {};
    }

    QList<Score> scores;
    while (query.next()) {
        Score score;
        score.id = query.value(0).toString();
        score.title = query.value(1).toString();
        score.composer = query.value(2).toString();
        score.fileName = query.value(3).toString();
        score.filePath = query.value(4).toString();
        score.fileType = query.value(5).toString();
        score.pageCount = query.value(6).toInt();
        score.thumbnailPath = query.value(7).toString();
        score.favorite = query.value(8).toBool();
        score.lastPage = query.value(9).toInt();
        score.createdAt = QDateTime::fromMSecsSinceEpoch(query.value(10).toLongLong());
        score.updatedAt = QDateTime::fromMSecsSinceEpoch(query.value(11).toLongLong());
        score.lastOpenedAt = QDateTime::fromMSecsSinceEpoch(query.value(12).toLongLong());
        scores.append(std::move(score));
    }
    return scores;
}

QList<Score> ScoreRepository::listByTag(const QString& tagId, const QString& searchQuery, QString* error) const
{
    QSqlQuery query(m_database);
    const auto needle = QStringLiteral("%%1%").arg(searchQuery);
    query.prepare(QStringLiteral(R"(
        SELECT s.id, s.title, s.composer, s.file_name, s.file_path, s.file_type, s.page_count, s.thumbnail_path,
               s.favorite, s.last_page, s.created_at, s.updated_at, s.last_opened_at
        FROM scores s
        JOIN score_tags st ON st.score_id = s.id
        WHERE st.tag_id = ?
          AND (s.title LIKE ? OR s.composer LIKE ? OR EXISTS (
            SELECT 1 FROM score_tags st2 JOIN tags t ON t.id = st2.tag_id
            WHERE st2.score_id = s.id AND t.name LIKE ?
          ))
        ORDER BY s.favorite DESC, s.last_opened_at DESC, s.created_at DESC
    )"));
    query.addBindValue(tagId);
    query.addBindValue(needle);
    query.addBindValue(needle);
    query.addBindValue(needle);
    if (!query.exec()) {
        *error = query.lastError().text();
        return {};
    }

    QList<Score> scores;
    while (query.next()) {
        Score score;
        score.id = query.value(0).toString();
        score.title = query.value(1).toString();
        score.composer = query.value(2).toString();
        score.fileName = query.value(3).toString();
        score.filePath = query.value(4).toString();
        score.fileType = query.value(5).toString();
        score.pageCount = query.value(6).toInt();
        score.thumbnailPath = query.value(7).toString();
        score.favorite = query.value(8).toBool();
        score.lastPage = query.value(9).toInt();
        score.createdAt = QDateTime::fromMSecsSinceEpoch(query.value(10).toLongLong());
        score.updatedAt = QDateTime::fromMSecsSinceEpoch(query.value(11).toLongLong());
        score.lastOpenedAt = QDateTime::fromMSecsSinceEpoch(query.value(12).toLongLong());
        scores.append(std::move(score));
    }
    return scores;
}

QVariantList ScoreRepository::folders(QString* error) const
{
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("SELECT id, name, created_at FROM folders ORDER BY name COLLATE NOCASE"))) {
        *error = query.lastError().text();
        return {};
    }
    QVariantList result;
    while (query.next()) {
        result.append(QVariantMap{{"id", query.value(0).toString()}, {"name", query.value(1).toString()},
            {"createdAt", query.value(2).toLongLong()}});
    }
    return result;
}

QVariantList ScoreRepository::childFolders(const QString& parentId, QString* error) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"(
        SELECT id, name, created_at FROM folders
        WHERE ((? = '' AND parent_id IS NULL) OR parent_id = ?)
        ORDER BY name COLLATE NOCASE
    )"));
    query.addBindValue(parentId);
    query.addBindValue(parentId);
    if (!query.exec()) {
        *error = query.lastError().text();
        return {};
    }
    QVariantList result;
    while (query.next()) {
        result.append(QVariantMap{{"id", query.value(0).toString()}, {"name", query.value(1).toString()},
            {"createdAt", query.value(2).toLongLong()}});
    }
    return result;
}

QString ScoreRepository::folderParent(const QString& folderId, QString* error) const
{
    if (folderId.isEmpty()) return {};
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT parent_id FROM folders WHERE id = ?"));
    query.addBindValue(folderId);
    if (!query.exec() || !query.next()) {
        *error = query.lastError().text();
        return {};
    }
    return query.value(0).toString();
}

QString ScoreRepository::folderName(const QString& folderId, QString* error) const
{
    if (folderId.isEmpty()) return QStringLiteral("乐谱库");
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT name FROM folders WHERE id = ?"));
    query.addBindValue(folderId);
    if (!query.exec() || !query.next()) {
        *error = query.lastError().text();
        return {};
    }
    return query.value(0).toString();
}

QString ScoreRepository::folderBreadcrumb(const QString& folderId, QString* error) const
{
    if (folderId.isEmpty()) return QStringLiteral("乐谱库");
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"(
        WITH RECURSIVE ancestors(id, name, parent_id, depth) AS (
            SELECT id, name, parent_id, 0 FROM folders WHERE id = ?
            UNION ALL
            SELECT f.id, f.name, f.parent_id, ancestors.depth + 1
            FROM folders f JOIN ancestors ON ancestors.parent_id = f.id
        )
        SELECT name FROM ancestors ORDER BY depth DESC
    )"));
    query.addBindValue(folderId);
    if (!query.exec()) {
        *error = query.lastError().text();
        return {};
    }
    QStringList names {QStringLiteral("乐谱库")};
    while (query.next()) names.append(query.value(0).toString());
    return names.join(QStringLiteral("  ›  "));
}

QVariantList ScoreRepository::folderScoresRecursive(const QString& folderId, QString* error) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"(
        WITH RECURSIVE tree(id) AS (
            SELECT id FROM folders WHERE id = ?
            UNION ALL
            SELECT folders.id FROM folders JOIN tree ON folders.parent_id = tree.id
        )
        SELECT scores.id, scores.file_path, scores.thumbnail_path
        FROM scores WHERE folder_id IN (SELECT id FROM tree)
    )"));
    query.addBindValue(folderId);
    if (!query.exec()) {
        *error = query.lastError().text();
        return {};
    }
    QVariantList result;
    while (query.next()) {
        result.append(QVariantMap{{"id", query.value(0)}, {"filePath", query.value(1)},
            {"thumbnailPath", query.value(2)}});
    }
    return result;
}

bool ScoreRepository::createFolder(const QString& name, const QString& parentId, QString* error) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT INTO folders (id, name, created_at, updated_at, parent_id) VALUES (?, ?, ?, ?, ?)"));
    query.addBindValue(QUuid::createUuid().toString(QUuid::WithoutBraces));
    query.addBindValue(name.trimmed());
    const auto now = QDateTime::currentMSecsSinceEpoch();
    query.addBindValue(now);
    query.addBindValue(parentId.isEmpty() ? QVariant(QVariant::String) : parentId);
    query.addBindValue(now);
    if (query.exec()) return true;
    *error = query.lastError().text();
    return false;
}

QVariantList ScoreRepository::tags(QString* error) const
{
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("SELECT id, name FROM tags ORDER BY name COLLATE NOCASE"))) {
        *error = query.lastError().text();
        return {};
    }
    QVariantList result;
    while (query.next()) {
        result.append(QVariantMap{{"id", query.value(0).toString()}, {"name", query.value(1).toString()}});
    }
    return result;
}

bool ScoreRepository::createTag(const QString& name, QString* error) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO tags (id, name) VALUES (?, ?)"));
    query.addBindValue(QUuid::createUuid().toString(QUuid::WithoutBraces));
    query.addBindValue(name.trimmed());
    if (query.exec()) return true;
    *error = query.lastError().text();
    return false;
}

bool ScoreRepository::renameFolder(const QString& folderId, const QString& name, QString* error) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE folders SET name = ?, updated_at = ? WHERE id = ?"));
    query.addBindValue(name.trimmed());
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    query.addBindValue(folderId);
    if (query.exec() && query.numRowsAffected() == 1) return true;
    *error = query.lastError().text();
    return false;
}

bool ScoreRepository::deleteFolder(const QString& folderId, QString* error)
{
    if (!m_database.transaction()) {
        *error = m_database.lastError().text();
        return false;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"(
        WITH RECURSIVE tree(id) AS (
            SELECT id FROM folders WHERE id = ?
            UNION ALL
            SELECT folders.id FROM folders JOIN tree ON folders.parent_id = tree.id
        )
        DELETE FROM scores WHERE folder_id IN (SELECT id FROM tree)
    )"));
    query.addBindValue(folderId);
    if (!query.exec()) {
        m_database.rollback();
        *error = query.lastError().text();
        return false;
    }
    query.prepare(QStringLiteral("DELETE FROM folders WHERE id = ?"));
    query.addBindValue(folderId);
    if (!query.exec() || query.numRowsAffected() != 1 || !m_database.commit()) {
        m_database.rollback();
        *error = query.lastError().text();
        return false;
    }
    return true;
}

bool ScoreRepository::renameTag(const QString& tagId, const QString& name, QString* error) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE tags SET name = ? WHERE id = ?"));
    query.addBindValue(name.trimmed());
    query.addBindValue(tagId);
    if (query.exec() && query.numRowsAffected() == 1) return true;
    *error = query.lastError().text();
    return false;
}

bool ScoreRepository::deleteTag(const QString& tagId, QString* error) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM tags WHERE id = ?"));
    query.addBindValue(tagId);
    if (query.exec() && query.numRowsAffected() == 1) return true;
    *error = query.lastError().text();
    return false;
}
