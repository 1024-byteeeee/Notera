#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>

#include "features/library/ScoreListModel.h"
#include "features/library/ScoreRepository.h"
#include "features/library/ThumbnailGenerator.h"
#include "services/DatabaseService.h"

class LibraryService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ScoreListModel* scores READ scores CONSTANT)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(QString filterMode READ filterMode WRITE setFilterMode NOTIFY filterModeChanged)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(QVariantList tags READ tags NOTIFY tagsChanged)

public:
    explicit LibraryService(QObject* parent = nullptr);

    [[nodiscard]] ScoreListModel* scores();
    [[nodiscard]] QString searchQuery() const;
    void setSearchQuery(const QString& searchQuery);
    [[nodiscard]] QString filterMode() const;
    void setFilterMode(const QString& mode);
    [[nodiscard]] QVariantList folders() const;
    [[nodiscard]] QVariantList tags() const;

    Q_INVOKABLE void importLocalFile(const QUrl& url);
    Q_INVOKABLE void toggleFavorite(const QString& scoreId, bool favorite);
    Q_INVOKABLE void renameScore(const QString& scoreId, const QString& title);
    Q_INVOKABLE void deleteScore(const QString& scoreId, const QString& filePath, const QString& thumbnailPath);
    Q_INVOKABLE void createFolder(const QString& name);
    Q_INVOKABLE void renameFolder(const QString& folderId, const QString& name);
    Q_INVOKABLE void deleteFolder(const QString& folderId);
    Q_INVOKABLE void createTag(const QString& name);
    Q_INVOKABLE void renameTag(const QString& tagId, const QString& name);
    Q_INVOKABLE void deleteTag(const QString& tagId);

signals:
    void searchQueryChanged();
    void filterModeChanged();
    void foldersChanged();
    void tagsChanged();
    void errorOccurred(QString message);
    void noticeOccurred(QString message);

private:
    void reload();
    void reloadFolders();
    void reloadTags();
    void importFile(const QString& sourcePath, const QString& titleOverride = {});

    DatabaseService m_databaseService;
    ScoreRepository m_repository;
    ScoreListModel m_scores;
    ThumbnailGenerator m_thumbnailGenerator;
    QString m_searchQuery;
    QString m_filterMode {QStringLiteral("all")};
    QVariantList m_folders;
    QVariantList m_tags;
};
