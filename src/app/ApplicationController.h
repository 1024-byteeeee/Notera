#pragma once

#include <QObject>
#include <QString>

class ApplicationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)

public:
    explicit ApplicationController(QObject* parent = nullptr);

    [[nodiscard]] QString currentPage() const;
    void setCurrentPage(const QString& page);
    [[nodiscard]] int themeMode() const;
    void setThemeMode(int themeMode);

signals:
    void currentPageChanged();
    void themeModeChanged();

private:
    QString m_currentPage {QStringLiteral("library")};
    int m_themeMode {0};
};
