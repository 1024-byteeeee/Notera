#pragma once

#include <QObject>
#include <QUrl>
#include <QHash>
#include <QScopedPointer>
#include <QTemporaryDir>
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
    Q_PROPERTY(QVariantList clipboardItems READ clipboardItems NOTIFY clipboardChanged)
    Q_PROPERTY(QString clipboardMode READ clipboardMode NOTIFY clipboardChanged)

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
    [[nodiscard]] QVariantList clipboardItems() const;
    [[nodiscard]] QString clipboardMode() const;

    Q_INVOKABLE void importLocalFile(const QUrl& url);
    Q_INVOKABLE void importAndStitchImages(const QStringList& filePaths);
    Q_INVOKABLE void toggleFavorite(const QString& scoreId, bool favorite);
    Q_INVOKABLE void toggleItemFavorite(const QString& itemId, bool favorite);
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
    Q_INVOKABLE void setItemFolder(const QString& itemId, const QString& folderId);
    Q_INVOKABLE void addItemTag(const QString& itemId, const QString& tagId);
    Q_INVOKABLE void removeItemTag(const QString& itemId, const QString& tagId);
    Q_INVOKABLE QVariantList itemTags(const QString& itemId);
    Q_INVOKABLE bool itemHasTag(const QString& itemId, const QString& tagId);
    Q_INVOKABLE bool canMoveItemToFolder(const QString& itemId, const QString& folderId);
    Q_INVOKABLE QString moveItems(const QVariantList& itemIds, const QString& folderId);
    Q_INVOKABLE QVariantList childFolders(const QString& folderId);
    Q_INVOKABLE void copyItems(const QVariantList& itemIds);
    Q_INVOKABLE void cutItems(const QVariantList& itemIds);
    Q_INVOKABLE void clearClipboard();
    Q_INVOKABLE void pasteItems();
    Q_INVOKABLE void resolvePasteConflict(const QString& action, bool applyToAll);
    Q_INVOKABLE void resolvePasteFolderConflict(const QString& action, bool applyToAll);
    Q_INVOKABLE QString favoriteItems(const QVariantList& itemIds);
    Q_INVOKABLE QString tagItems(const QVariantList& itemIds, const QString& tagId);
    Q_INVOKABLE QString saveScoreAs(const QString& scoreId, const QUrl& destination);
    Q_INVOKABLE QString saveFolderAs(const QString& folderId, const QUrl& destinationDirectory);
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
    Q_INVOKABLE QVariantMap probeDatabaseBackup(const QUrl& backupFile);
    Q_INVOKABLE QString importDatabaseBackupMerged(const QUrl& backupFile);
    Q_INVOKABLE void resolveMergeConflict(const QString& action, bool applyToAll);
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
    void clipboardChanged();
    void pasteConflict(QString sourceName, QString targetName, int index, int total);
    void pasteFolderConflict(QString sourceName, QString targetName, int index, int total);
    void pasteFinished(int processedCount);
    void mergeConflict(QString sourceName, QString targetName, int index, int total);
    void mergeFinished(int processedCount);

private:
    void reload();
    void reloadFolders();
    void reloadTags();
    void importFile(const QString& sourcePath, const QString& titleOverride = {});
    void continuePaste();
    QString copyScoreToFolder(const QString& scoreId, const QString& targetFolderId, const QString& conflictAction);
    QString copyFolderRecursive(const QString& folderId, const QString& targetParentId, const QString& conflictAction);
    QString uniqueNameInFolder(const QString& baseName, const QString& folderId, bool isFolder);
    bool nameExistsInFolder(const QString& name, const QString& folderId, bool isFolder);
    QString getOrCreateFolder(const QString& name, const QString& parentId);
    void expandFolderToQueue(const QString& sourceFolderId, const QString& targetFolderId);
    void deleteEmptyFolderTree(const QString& folderId);
    void continueMerge();
    void cleanupMergeState();
    void importBackupScore(const QVariantMap& item, const QString& targetFolderId);
    static QString sha256OfFile(const QString& path);

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
    QVariantList m_clipboardItems;
    QString m_clipboardMode {QStringLiteral("none")};
    QVariantList m_pasteQueue;
    int m_pasteIndex {0};
    QString m_pasteTargetFolderId;
    QString m_pendingConflictAction;
    bool m_pasteApplyToAll {false};
    QStringList m_cutSourceFolderIds;
    QString m_pendingFolderConflictAction;
    bool m_folderConflictApplyToAll {false};
    QScopedPointer<QTemporaryDir> m_mergeTempDir;
    QString m_mergeBackupRoot;
    QVariantList m_mergeQueue;
    int m_mergeIndex {0};
    QString m_mergeConflictAction;
    bool m_mergeApplyToAll {false};
    QHash<QString, QString> m_mergeFolderMap;
    QHash<QString, QString> m_mergeTagMap;
    QHash<QString, QString> m_mergeHashIndex;
    QHash<QString, QString> m_mergeScoreTitles;
};
