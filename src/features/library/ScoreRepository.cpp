#include "features/library/ScoreRepository.h"

#include <QSqlError>
#include <QSqlQuery>

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

bool ScoreRepository::insert(const Score& score, QString* error) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(R"(
        INSERT INTO scores (id, title, composer, file_name, file_path, file_type, page_count, thumbnail_path,
                            favorite, last_page, created_at, updated_at, last_opened_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
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
    if (query.exec()) {
        return true;
    }
    *error = query.lastError().text();
    return false;
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
