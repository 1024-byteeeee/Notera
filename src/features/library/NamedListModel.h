#pragma once

#include <QAbstractListModel>
#include <QVariantList>

class NamedListModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        ItemIdRole = Qt::UserRole + 1,
        NameRole
    };
    Q_ENUM(Role)

    explicit NamedListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int count() const;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void replaceAll(const QVariantList& values);

signals:
    void countChanged();

private:
    struct Item {
        QString id;
        QString name;
    };

    QList<Item> m_items;
};
