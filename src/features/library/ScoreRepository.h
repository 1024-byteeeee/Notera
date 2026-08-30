#pragma once

#include <QSqlDatabase>

#include "features/library/Score.h"

class ScoreRepository final
{
public:
    explicit ScoreRepository(QSqlDatabase database);

    [[nodiscard]] QList<Score> list(const QString& searchQuery, QString* error) const;
    [[nodiscard]] bool insert(const Score& score, QString* error) const;
    [[nodiscard]] bool setFavorite(const QString& scoreId, bool favorite, QString* error) const;
    [[nodiscard]] bool rename(const QString& scoreId, const QString& title, QString* error) const;
    [[nodiscard]] bool updateThumbnail(const QString& scoreId, const QString& thumbnailPath, QString* error) const;
    [[nodiscard]] bool remove(const QString& scoreId, QString* error) const;

private:
    QSqlDatabase m_database;
};
