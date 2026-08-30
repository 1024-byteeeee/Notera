#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class ApplicationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString currentScoreTitle READ currentScoreTitle NOTIFY currentScoreChanged)
    Q_PROPERTY(QUrl currentFileUrl READ currentFileUrl NOTIFY currentScoreChanged)
    Q_PROPERTY(QString currentFileType READ currentFileType NOTIFY currentScoreChanged)
    Q_PROPERTY(int currentScorePageCount READ currentScorePageCount NOTIFY currentScoreChanged)
    Q_PROPERTY(double autoScrollSpeed READ autoScrollSpeed WRITE setAutoScrollSpeed NOTIFY autoScrollSpeedChanged)

public:
    explicit ApplicationController(QObject* parent = nullptr);

    [[nodiscard]] QString currentPage() const;
    void setCurrentPage(const QString& page);
    [[nodiscard]] int themeMode() const;
    void setThemeMode(int themeMode);
    [[nodiscard]] QString currentScoreTitle() const;
    [[nodiscard]] QUrl currentFileUrl() const;
    [[nodiscard]] QString currentFileType() const;
    [[nodiscard]] int currentScorePageCount() const;
    [[nodiscard]] double autoScrollSpeed() const;
    void setAutoScrollSpeed(double speed);
    Q_INVOKABLE void openScore(const QString& title, const QString& filePath, const QString& fileType, int pageCount);

signals:
    void currentPageChanged();
    void themeModeChanged();
    void currentScoreChanged();
    void autoScrollSpeedChanged();

private:
    QString m_currentPage {QStringLiteral("library")};
    int m_themeMode {0};
    QString m_currentScoreTitle;
    QUrl m_currentFileUrl;
    QString m_currentFileType;
    int m_currentScorePageCount {0};
    double m_autoScrollSpeed {45.0};
};
