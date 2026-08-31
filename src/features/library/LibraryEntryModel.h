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
        FileTypeRole
    };
    Q_ENUM(Role)

    explicit LibraryEntryModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int count() const;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE QVariantList itemIds() const;

    void replaceAll(const QVariantList& folders, const QList<Score>& scores);

signals:
    void countChanged();

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
    };

    QList<Entry> m_entries;
};
