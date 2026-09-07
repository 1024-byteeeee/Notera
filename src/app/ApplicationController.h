#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QUrl>

class ApplicationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(bool animationsEnabled READ animationsEnabled WRITE setAnimationsEnabled NOTIFY animationsEnabledChanged)
    Q_PROPERTY(QString accentColor READ accentColor WRITE setAccentColor NOTIFY accentColorChanged)
    Q_PROPERTY(QString libraryFilter READ libraryFilter WRITE setLibraryFilter NOTIFY libraryFilterChanged)
    Q_PROPERTY(QString currentScoreTitle READ currentScoreTitle NOTIFY currentScoreChanged)
    Q_PROPERTY(QUrl currentFileUrl READ currentFileUrl NOTIFY currentScoreChanged)
    Q_PROPERTY(QString currentFileType READ currentFileType NOTIFY currentScoreChanged)
    Q_PROPERTY(QString currentScoreId READ currentScoreId NOTIFY currentScoreChanged)
    Q_PROPERTY(QString currentScoreFolderId READ currentScoreFolderId NOTIFY currentScoreChanged)
    Q_PROPERTY(int currentScorePageCount READ currentScorePageCount NOTIFY currentScoreChanged)
    Q_PROPERTY(double autoScrollSpeed READ autoScrollSpeed WRITE setAutoScrollSpeed NOTIFY autoScrollSpeedChanged)
    Q_PROPERTY(double defaultScrollSpeed READ defaultScrollSpeed WRITE setDefaultScrollSpeed NOTIFY defaultScrollSpeedChanged)
    Q_PROPERTY(int longPressDragMs READ longPressDragMs WRITE setLongPressDragMs NOTIFY longPressDragMsChanged)
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
    [[nodiscard]] QString accentColor() const;
    void setAccentColor(const QString& color);
    Q_INVOKABLE void resetAccentColor();
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
    [[nodiscard]] int longPressDragMs() const;
    void setLongPressDragMs(int ms);
    [[nodiscard]] QString dataDirectory() const;
    [[nodiscard]] QString pendingDataDirectory() const;
    static bool applyPendingDataMigration(QString* error);
    static bool applyPendingDataClear(QString* error);
    static bool applyPendingBackupRestore(QString* error);
    Q_INVOKABLE QString migrateDataDirectory(const QUrl& newDirectory);
    Q_INVOKABLE QString openDataDirectory() const;
    Q_INVOKABLE QString exportDatabaseBackup(const QUrl& destinationFile) const;
    Q_INVOKABLE QString importDatabaseBackup(const QUrl& backupFile);
    // 异步版本：耗时文件操作在后台线程执行，完成后发射对应 *Finished 信号，
    // 主线程 UI（加载动画）不会被阻塞。QML 侧应优先使用这两个接口。
    Q_INVOKABLE void startExportDatabaseBackup(const QUrl& destinationFile);
    Q_INVOKABLE void startImportDatabaseBackup(const QUrl& backupFile);
    Q_INVOKABLE void requestRestart();
    Q_INVOKABLE QString clearAllData(const QString& confirmation);
    Q_INVOKABLE void openScore(const QString& scoreId, const QString& title, const QString& filePath,
        const QString& fileType, int pageCount, const QString& folderId);

signals:
    void currentPageChanged();
    void themeModeChanged();
    void animationsEnabledChanged();
    void accentColorChanged();
    void libraryFilterChanged();
    void currentScoreChanged();
    void scoreOpened(QString scoreId);
    void autoScrollSpeedChanged();
    void defaultScrollSpeedChanged();
    void longPressDragMsChanged();
    void dataDirectoryChanged();
    void restartRequested();
    // 异步导入导出完成信号（success=false 时 error 为可展示的中文错误信息）
    void exportDatabaseBackupFinished(bool success, QString error);
    void importDatabaseBackupFinished(bool success, QString error);

private:
    // 后台线程执行体（静态，不依赖实例状态，便于 QtConcurrent::run 调用）
    static QString runExportBackup(const QUrl& destinationFile);
    static QString runImportBackup(const QUrl& backupFile);

    QFutureWatcher<QString>* m_exportWatcher = nullptr;
    QFutureWatcher<QString>* m_importWatcher = nullptr;
    QString m_currentPage {QStringLiteral("library")};
    QString m_libraryFilter {QStringLiteral("all")};
    int m_themeMode {0};
    bool m_animationsEnabled {true};
    QString m_accentColor;
    QString m_currentScoreTitle;
    QUrl m_currentFileUrl;
    QString m_currentFileType;
    QString m_currentScoreId;
    QString m_currentScoreFolderId;
    int m_currentScorePageCount {0};
    double m_autoScrollSpeed {15.0};
    double m_defaultScrollSpeed {15.0};
    int m_longPressDragMs {300};
};
