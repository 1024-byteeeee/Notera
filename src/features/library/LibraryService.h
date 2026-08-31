#pragma once

#include <QObject>
#include <QUrl>
#include "features/library/LibraryEntryModel.h"
#include "features/library/LibrarySelectionModel.h"
#include "features/library/NamedListModel.h"
#include "features/library/ScoreListModel.h"
#include "features/library/ScoreRepository.h"
#include "features/library/ThumbnailGenerator.h"
#include "services/DatabaseService.h"

class LibraryService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ScoreListModel* scores READ scores CONSTANT)
    Q_PROPERTY(LibraryEntryModel* entries READ entries CONSTANT)
    Q_PROPERTY(LibrarySelectionModel* selection READ selection CONSTANT)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(QString filterMode READ filterMode WRITE setFilterMode NOTIFY filterModeChanged)
    Q_PROPERTY(NamedListModel* folders READ folders CONSTANT)
    Q_PROPERTY(NamedListModel* tags READ tags CONSTANT)
    Q_PROPERTY(QString currentFolderId READ currentFolderId NOTIFY currentFolderChanged)
    Q_PROPERTY(QString currentFolderName READ currentFolderName NOTIFY currentFolderChanged)
    Q_PROPERTY(QString currentFolderBreadcrumb READ currentFolderBreadcrumb NOTIFY currentFolderChanged)
    Q_PROPERTY(bool canGoUp READ canGoUp NOTIFY currentFolderChanged)

public:
    explicit LibraryService(QObject* parent = nullptr);

    [[nodiscard]] ScoreListModel* scores();
    [[nodiscard]] LibraryEntryModel* entries();
    [[nodiscard]] LibrarySelectionModel* selection();
    [[nodiscard]] QString searchQuery() const;
    void setSearchQuery(const QString& searchQuery);
    [[nodiscard]] QString filterMode() const;
    void setFilterMode(const QString& mode);
    [[nodiscard]] NamedListModel* folders();
    [[nodiscard]] NamedListModel* tags();
    [[nodiscard]] QString currentFolderId() const;
    [[nodiscard]] QString currentFolderName() const;
    [[nodiscard]] QString currentFolderBreadcrumb() const;
    [[nodiscard]] bool canGoUp() const;

    Q_INVOKABLE void importLocalFile(const QUrl& url);
    Q_INVOKABLE void importAndStitchImages(const QStringList& filePaths);
    Q_INVOKABLE void toggleFavorite(const QString& scoreId, bool favorite);
    Q_INVOKABLE void renameScore(const QString& scoreId, const QString& title);
    Q_INVOKABLE void deleteScore(const QString& scoreId, const QString& filePath, const QString& thumbnailPath);
    Q_INVOKABLE void deleteItems(const QVariantList& ids);
    Q_INVOKABLE QVariantList scoresInFolder(const QString& folderId);
    Q_INVOKABLE QString scoreFolderId(const QString& scoreId);
    Q_INVOKABLE void setScoreFolder(const QString& scoreId, const QString& folderId);
    Q_INVOKABLE void addScoreTag(const QString& scoreId, const QString& tagId);
    Q_INVOKABLE void removeScoreTag(const QString& scoreId, const QString& tagId);
    Q_INVOKABLE QVariantList scoreTags(const QString& scoreId);
    Q_INVOKABLE bool scoreHasTag(const QString& scoreId, const QString& tagId);
    Q_INVOKABLE void createFolder(const QString& name);
    Q_INVOKABLE void renameFolder(const QString& folderId, const QString& name);
    Q_INVOKABLE void deleteFolder(const QString& folderId);
    Q_INVOKABLE void createTag(const QString& name);
    Q_INVOKABLE void renameTag(const QString& tagId, const QString& name);
    Q_INVOKABLE void deleteTag(const QString& tagId);
    Q_INVOKABLE void requestImport();
    Q_INVOKABLE void enterFolder(const QString& folderId);
    Q_INVOKABLE void goUp();
    Q_INVOKABLE void goToLibraryRoot();
    void markScoreOpened(const QString& scoreId);

signals:
    void searchQueryChanged();
    void filterModeChanged();
    void foldersChanged();
    void tagsChanged();
    void currentFolderChanged();
    void errorOccurred(QString message);
    void noticeOccurred(QString message);
    void importRequested();

private:
    void reload();
    void reloadFolders();
    void reloadTags();
    void importFile(const QString& sourcePath, const QString& titleOverride = {});

    DatabaseService m_databaseService;
    ScoreRepository m_repository;
    ScoreListModel m_scores;
    LibraryEntryModel m_entries;
    LibrarySelectionModel m_selection;
    NamedListModel m_folders;
    NamedListModel m_tags;
    ThumbnailGenerator m_thumbnailGenerator;
    QString m_searchQuery;
    QString m_filterMode {QStringLiteral("all")};
    QString m_currentFolderId;
    QString m_currentFolderName {QStringLiteral("乐谱库")};
    QString m_currentFolderBreadcrumb {QStringLiteral("乐谱库")};
};
