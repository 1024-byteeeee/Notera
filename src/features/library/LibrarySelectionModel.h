#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantList>

class LibrarySelectionModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList selectedIds READ selectedIds NOTIFY selectionChanged)

public:
    explicit LibrarySelectionModel(QObject* parent = nullptr);

    [[nodiscard]] int count() const;
    [[nodiscard]] QVariantList selectedIds() const;

    Q_INVOKABLE bool contains(const QString& itemId) const;
    Q_INVOKABLE void toggle(const QString& itemId);
    Q_INVOKABLE void replace(const QVariantList& itemIds);
    Q_INVOKABLE void clear();

signals:
    void selectionChanged();

private:
    QSet<QString> m_selectedIds;
};
