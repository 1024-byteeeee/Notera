#pragma once

#include <QAbstractListModel>
#include <QVariantList>

#include "features/library/Score.h"

class LibraryEntryModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        ItemTypeRole = Qt::UserRole + 1,
        ItemIdRole,
        TitleRole,
        CreatedDateRole,
        PageCountRole,
        ThumbnailPathRole,
        FavoriteRole,
        FilePathRole,
        FileTypeRole,
        TagsRole
    };
    Q_ENUM(Role)

    explicit LibraryEntryModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int count() const;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariantList itemIds() const;

    void replaceAll(const QVariantList& folders, const QList<Score>& scores);

    // 局部更新单个条目的标签（只发 dataChanged，不重建模型，保留视图滚动位置）
    bool updateEntryTags(const QString& itemId, const QStringList& tags);
    // 局部更新单个条目的标题（重命名）
    bool updateEntryTitle(const QString& itemId, const QString& title);
    // 局部更新单个条目的收藏状态
    bool updateEntryFavorite(const QString& itemId, bool favorite);
    // 局部更新单个条目的缩略图路径
    bool updateEntryThumbnail(const QString& itemId, const QString& thumbnailPath);

signals:
    void countChanged();
    // 整表重建前后发出，供视图做滚动位置保护（resetStarted 记录、resetFinished 恢复）
    void resetStarted();
    void resetFinished();

private:
    struct Entry {
        QString itemType;
        QString itemId;
        QString title;
        QDateTime createdAt;
        int pageCount {0};
        QString thumbnailPath;
        bool favorite {false};
        QString filePath;
        QString fileType;
        QStringList tags;
    };

    QList<Entry> m_entries;
};
