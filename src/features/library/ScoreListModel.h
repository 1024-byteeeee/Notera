#pragma once

#include <QAbstractListModel>

#include "features/library/Score.h"

class ScoreListModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        ComposerRole,
        CreatedDateRole,
        PageCountRole,
        ThumbnailPathRole,
        FavoriteRole,
        FilePathRole,
        FileTypeRole
    };
    Q_ENUM(Role)

    explicit ScoreListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int count() const;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void replaceAll(QList<Score> scores);

signals:
    void countChanged();

private:
    QList<Score> m_scores;
};
