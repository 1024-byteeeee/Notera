#pragma once

#include <QObject>
#include <QString>

class ApplicationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)

public:
    explicit ApplicationController(QObject* parent = nullptr);

    [[nodiscard]] QString currentPage() const;
    void setCurrentPage(const QString& page);

signals:
    void currentPageChanged();

private:
    QString m_currentPage {QStringLiteral("library")};
};
