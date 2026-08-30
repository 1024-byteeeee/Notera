#pragma once

#include <QAbstractListModel>

#include "features/library/Score.h"

class ScoreListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        ComposerRole,
        PageCountRole,
        ThumbnailPathRole,
        FavoriteRole,
        FilePathRole,
        FileTypeRole
    };
    Q_ENUM(Role)

    explicit ScoreListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void replaceAll(QList<Score> scores);

private:
    QList<Score> m_scores;
};
