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
    [[nodiscard]] QList<Score> listAtFolder(const QString& folderId, const QString& searchQuery, QString* error) const;
    [[nodiscard]] QList<Score> listByFolder(const QString& folderId, const QString& searchQuery, QString* error) const;
    [[nodiscard]] QList<Score> listByTag(const QString& tagId, const QString& searchQuery, QString* error) const;
    [[nodiscard]] bool insert(const Score& score, const QString& folderId, QString* error) const;
    [[nodiscard]] bool setFavorite(const QString& scoreId, bool favorite, QString* error) const;
    [[nodiscard]] bool setItemFavorite(const QString& itemId, bool favorite, QString* error) const;
    [[nodiscard]] bool markScoreOpened(const QString& scoreId, QString* error) const;
    [[nodiscard]] bool rename(const QString& scoreId, const QString& title, QString* error) const;
    [[nodiscard]] bool updateThumbnail(const QString& scoreId, const QString& thumbnailPath, QString* error) const;
    [[nodiscard]] bool remove(const QString& scoreId, QString* error) const;
    [[nodiscard]] bool setFolder(const QString& scoreId, const QString& folderId, QString* error) const;
    [[nodiscard]] bool addTag(const QString& scoreId, const QString& tagId, QString* error) const;
    [[nodiscard]] bool removeTag(const QString& scoreId, const QString& tagId, QString* error) const;
    [[nodiscard]] QVariantList scoreTags(const QString& scoreId, QString* error) const;
    [[nodiscard]] QVariantList itemTags(const QString& itemId, QString* error) const;
    [[nodiscard]] bool addItemTag(const QString& itemId, const QString& tagId, QString* error) const;
    [[nodiscard]] bool removeItemTag(const QString& itemId, const QString& tagId, QString* error) const;
    [[nodiscard]] QString itemTypeById(const QString& id, QString* error) const;
    [[nodiscard]] QString filePathById(const QString& scoreId, QString* error) const;
    [[nodiscard]] QString thumbnailPathById(const QString& scoreId, QString* error) const;
    [[nodiscard]] QString scoreFolderId(const QString& scoreId, QString* error) const;

    [[nodiscard]] QVariantList folders(QString* error) const;
    [[nodiscard]] QVariantList recentFolders(const QString& searchQuery, QString* error) const;
    [[nodiscard]] QVariantList favoriteFolders(const QString& searchQuery, QString* error) const;
    [[nodiscard]] QVariantList foldersByTag(const QString& tagId, const QString& searchQuery, QString* error) const;
    [[nodiscard]] QVariantList childFolders(const QString& parentId, QString* error) const;
    [[nodiscard]] QString folderParent(const QString& folderId, QString* error) const;
    [[nodiscard]] QString folderName(const QString& folderId, QString* error) const;
    [[nodiscard]] QString folderBreadcrumb(const QString& folderId, QString* error) const;
    [[nodiscard]] QVariantList folderScoresRecursive(const QString& folderId, QString* error) const;
    [[nodiscard]] bool createFolder(const QString& name, const QString& parentId, QString* error) const;
    [[nodiscard]] bool markFolderOpened(const QString& folderId, QString* error) const;
    [[nodiscard]] bool renameFolder(const QString& folderId, const QString& name, QString* error) const;
    [[nodiscard]] bool moveFolder(const QString& folderId, const QString& parentId, QString* error) const;
    [[nodiscard]] bool canMoveFolder(const QString& folderId, const QString& parentId, QString* error) const;
    [[nodiscard]] bool deleteFolder(const QString& folderId, QString* error);
    [[nodiscard]] QVariantList tags(QString* error) const;
    [[nodiscard]] bool createTag(const QString& name, QString* error) const;
    [[nodiscard]] bool renameTag(const QString& tagId, const QString& name, QString* error) const;
    [[nodiscard]] bool deleteTag(const QString& tagId, QString* error) const;

private:
    QSqlDatabase m_database;
};
