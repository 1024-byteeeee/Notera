#pragma once

#include <QSqlDatabase>
#include <QVariantList>

#include "features/library/Score.h"

class ScoreRepository final
{
public:
    explicit ScoreRepository(QSqlDatabase database);

    [[nodiscard]] QList<Score> list(const QString& searchQuery, QString* error) const;
    [[nodiscard]] QList<Score> listFavorites(const QString& searchQuery, QString* error) const;
    [[nodiscard]] QList<Score> listRecent(const QString& searchQuery, QString* error) const;
    [[nodiscard]] QList<Score> listByFolder(const QString& folderId, const QString& searchQuery, QString* error) const;
    [[nodiscard]] QList<Score> listByTag(const QString& tagId, const QString& searchQuery, QString* error) const;
    [[nodiscard]] bool insert(const Score& score, QString* error) const;
    [[nodiscard]] bool setFavorite(const QString& scoreId, bool favorite, QString* error) const;
    [[nodiscard]] bool rename(const QString& scoreId, const QString& title, QString* error) const;
    [[nodiscard]] bool updateThumbnail(const QString& scoreId, const QString& thumbnailPath, QString* error) const;
    [[nodiscard]] bool remove(const QString& scoreId, QString* error) const;

    [[nodiscard]] QVariantList folders(QString* error) const;
    [[nodiscard]] bool createFolder(const QString& name, QString* error) const;
    [[nodiscard]] QVariantList tags(QString* error) const;
    [[nodiscard]] bool createTag(const QString& name, QString* error) const;

private:
    QSqlDatabase m_database;
};
