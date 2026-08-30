#pragma once

#include <QObject>
#include <QNetworkAccessManager>
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

public:
    explicit LibraryService(QObject* parent = nullptr);

    [[nodiscard]] ScoreListModel* scores();
    [[nodiscard]] QString searchQuery() const;
    void setSearchQuery(const QString& searchQuery);

    Q_INVOKABLE void importUrls(const QVariantList& urls);
    Q_INVOKABLE void importUrl(const QString& url);
    Q_INVOKABLE void toggleFavorite(const QString& scoreId, bool favorite);
    Q_INVOKABLE void renameScore(const QString& scoreId, const QString& title);
    Q_INVOKABLE void deleteScore(const QString& scoreId, const QString& filePath, const QString& thumbnailPath);

signals:
    void searchQueryChanged();
    void errorOccurred(QString message);
    void noticeOccurred(QString message);

private:
    void reload();
    void importFile(const QString& sourcePath, const QString& titleOverride = {});
    void downloadFile(const QUrl& url);

    DatabaseService m_databaseService;
    ScoreRepository m_repository;
    ScoreListModel m_scores;
    ThumbnailGenerator m_thumbnailGenerator;
    QString m_searchQuery;
    QNetworkAccessManager m_network;
};
