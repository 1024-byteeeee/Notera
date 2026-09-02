#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class ApplicationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(bool animationsEnabled READ animationsEnabled WRITE setAnimationsEnabled NOTIFY animationsEnabledChanged)
    Q_PROPERTY(QString libraryFilter READ libraryFilter WRITE setLibraryFilter NOTIFY libraryFilterChanged)
    Q_PROPERTY(QString currentScoreTitle READ currentScoreTitle NOTIFY currentScoreChanged)
    Q_PROPERTY(QUrl currentFileUrl READ currentFileUrl NOTIFY currentScoreChanged)
    Q_PROPERTY(QString currentFileType READ currentFileType NOTIFY currentScoreChanged)
    Q_PROPERTY(QString currentScoreId READ currentScoreId NOTIFY currentScoreChanged)
    Q_PROPERTY(QString currentScoreFolderId READ currentScoreFolderId NOTIFY currentScoreChanged)
    Q_PROPERTY(int currentScorePageCount READ currentScorePageCount NOTIFY currentScoreChanged)
    Q_PROPERTY(double autoScrollSpeed READ autoScrollSpeed WRITE setAutoScrollSpeed NOTIFY autoScrollSpeedChanged)
    Q_PROPERTY(double defaultScrollSpeed READ defaultScrollSpeed WRITE setDefaultScrollSpeed NOTIFY defaultScrollSpeedChanged)
    Q_PROPERTY(QString dataDirectory READ dataDirectory NOTIFY dataDirectoryChanged)
    Q_PROPERTY(QString pendingDataDirectory READ pendingDataDirectory NOTIFY dataDirectoryChanged)

public:
    explicit ApplicationController(QObject* parent = nullptr);

    [[nodiscard]] QString currentPage() const;
    void setCurrentPage(const QString& page);
    [[nodiscard]] QString libraryFilter() const;
    void setLibraryFilter(const QString& filter);
    [[nodiscard]] int themeMode() const;
    void setThemeMode(int themeMode);
    [[nodiscard]] bool animationsEnabled() const;
    void setAnimationsEnabled(bool enabled);
    [[nodiscard]] QString currentScoreTitle() const;
    [[nodiscard]] QUrl currentFileUrl() const;
    [[nodiscard]] QString currentFileType() const;
    [[nodiscard]] QString currentScoreId() const;
    [[nodiscard]] QString currentScoreFolderId() const;
    [[nodiscard]] int currentScorePageCount() const;
    [[nodiscard]] double autoScrollSpeed() const;
    void setAutoScrollSpeed(double speed);
    [[nodiscard]] double defaultScrollSpeed() const;
    void setDefaultScrollSpeed(double speed);
    [[nodiscard]] QString dataDirectory() const;
    [[nodiscard]] QString pendingDataDirectory() const;
    static bool applyPendingDataMigration(QString* error);
    static bool applyPendingDataClear(QString* error);
    static bool applyPendingBackupRestore(QString* error);
    Q_INVOKABLE QString migrateDataDirectory(const QUrl& newDirectory);
    Q_INVOKABLE QString openDataDirectory() const;
    Q_INVOKABLE QString exportDatabaseBackup(const QUrl& destinationFile) const;
    Q_INVOKABLE QString importDatabaseBackup(const QUrl& backupFile);
    Q_INVOKABLE void requestRestart();
    Q_INVOKABLE QString clearAllData(const QString& confirmation);
    Q_INVOKABLE void openScore(const QString& scoreId, const QString& title, const QString& filePath,
        const QString& fileType, int pageCount, const QString& folderId);

signals:
    void currentPageChanged();
    void themeModeChanged();
    void animationsEnabledChanged();
    void libraryFilterChanged();
    void currentScoreChanged();
    void scoreOpened(QString scoreId);
    void autoScrollSpeedChanged();
    void defaultScrollSpeedChanged();
    void dataDirectoryChanged();
    void restartRequested();

private:
    QString m_currentPage {QStringLiteral("library")};
    QString m_libraryFilter {QStringLiteral("all")};
    int m_themeMode {0};
    bool m_animationsEnabled {true};
    QString m_currentScoreTitle;
    QUrl m_currentFileUrl;
    QString m_currentFileType;
    QString m_currentScoreId;
    QString m_currentScoreFolderId;
    int m_currentScorePageCount {0};
    double m_autoScrollSpeed {15.0};
    double m_defaultScrollSpeed {15.0};
};
