#include <algorithm>
#include <cmath>
#include <memory>

#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPdfWriter>
#include <QPointer>
#include <QPointingDevice>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTimer>
#include <QTranslator>
#include <QWheelEvent>
#include <QtCore/private/qzipwriter_p.h>

#include "app/ApplicationController.h"
#include "features/library/LibraryService.h"
#include "features/pdf/PdfCacheImageProvider.h"
#include "features/pdf/PdfRenderService.h"
#include "platform/AppDataPaths.h"
#include "services/MetronomeService.h"

namespace
{

struct RestartGuard
{
    bool requested{false};
    QString program;
    QStringList arguments;
    QString workingDirectory;

    ~RestartGuard()
    {
        if (requested && !program.isEmpty())
        {
            QProcess::startDetached(program, arguments, workingDirectory);
        }
    }
};

class SingleInstanceGuard
{
  public:
    using ActivateCallback = std::function<void()>;

    explicit SingleInstanceGuard(ActivateCallback callback = nullptr)
        : m_callback(std::move(callback))
    {
        const QString serverName =
            QCoreApplication::applicationName() + QStringLiteral("-SingleInstance");

        auto* socket = new QLocalSocket();
        socket->connectToServer(serverName, QIODevice::WriteOnly);
        if (socket->waitForConnected(300))
        {
            socket->write("ACTIVATE\n");
            socket->flush();
            socket->waitForBytesWritten(300);
            m_isSecondary = true;
            delete socket;
            return;
        }
        delete socket;

        QLocalServer::removeServer(serverName);
        m_server = new QLocalServer();
        if (m_server->listen(serverName))
        {
            QObject::connect(m_server, &QLocalServer::newConnection,
                             [this]()
                             {
                                 auto* client = m_server->nextPendingConnection();
                                 if (!client)
                                     return;
                                 QObject::connect(client, &QLocalSocket::readyRead,
                                                  [this, client]()
                                                  {
                                                      if (client->readAll().contains("ACTIVATE") &&
                                                          m_callback)
                                                      {
                                                          m_callback();
                                                      }
                                                      client->disconnectFromServer();
                                                      client->deleteLater();
                                                  });

                                 QTimer::singleShot(1000, client, &QLocalSocket::deleteLater);
                             });
        }
        m_isSecondary = false;
    }

    ~SingleInstanceGuard()
    {
        if (m_server)
        {
            m_server->close();
            delete m_server;
        }
    }

    SingleInstanceGuard(const SingleInstanceGuard&) = delete;
    SingleInstanceGuard& operator=(const SingleInstanceGuard&) = delete;

    bool isSecondary() const { return m_isSecondary; }
    void setActivateCallback(ActivateCallback callback) { m_callback = std::move(callback); }

  private:
    QLocalServer* m_server = nullptr;
    bool m_isSecondary = false;
    ActivateCallback m_callback;
};

QQuickItem* findVisualItem(QQuickItem* parent, const QString& objectName)
{
    if (!parent)
    {
        return nullptr;
    }
    if (parent->objectName() == objectName)
    {
        return parent;
    }
    for (auto* const child : parent->childItems())
    {
        if (auto* const match = findVisualItem(child, objectName))
        {
            return match;
        }
    }
    return nullptr;
}

QQuickItem* findVisualItem(QObject* root, const QString& objectName)
{
    if (auto* const window = qobject_cast<QQuickWindow*>(root))
    {
        return findVisualItem(window->contentItem(), objectName);
    }
    return findVisualItem(qobject_cast<QQuickItem*>(root), objectName);
}

QList<QQuickItem*> findItemsByObjectName(QQuickItem* parent, const QString& objectName)
{
    QList<QQuickItem*> result;
    if (!parent)
        return result;
    if (parent->objectName() == objectName)
        result.append(parent);
    for (auto* const child : parent->childItems())
    {
        result.append(findItemsByObjectName(child, objectName));
    }
    return result;
}

QList<QQuickItem*> findItemsByObjectName(QObject* root, const QString& objectName)
{
    if (auto* const window = qobject_cast<QQuickWindow*>(root))
    {
        return findItemsByObjectName(window->contentItem(), objectName);
    }
    return findItemsByObjectName(qobject_cast<QQuickItem*>(root), objectName);
}

bool clickItemAt(QObject* root, const QString& objectName, const Qt::MouseButton button,
                 const double relX, const double relY)
{
    auto* const item = findVisualItem(root, objectName);
    if (!item || !item->isVisible() || item->width() <= 0.0 || item->height() <= 0.0 ||
        !item->window())
    {
        return false;
    }

    const auto scenePosition =
        item->mapToScene(QPointF(item->width() * relX, item->height() * relY));
    const auto globalPosition = QPointF(item->window()->mapToGlobal(scenePosition.toPoint()));
    QMouseEvent pressEvent(QEvent::MouseButtonPress, scenePosition, scenePosition, globalPosition,
                           button, button, Qt::NoModifier,
                           QPointingDevice::primaryPointingDevice());
    QCoreApplication::sendEvent(item->window(), &pressEvent);
    QMouseEvent releaseEvent(QEvent::MouseButtonRelease, scenePosition, scenePosition,
                             globalPosition, button, Qt::NoButton, Qt::NoModifier,
                             QPointingDevice::primaryPointingDevice());
    QCoreApplication::sendEvent(item->window(), &releaseEvent);
    QCoreApplication::processEvents();
    return true;
}

bool clickItem(QObject* root, const QString& objectName, const Qt::MouseButton button)
{
    return clickItemAt(root, objectName, button, 0.5, 0.5);
}

void sendMouseEvent(QQuickWindow* window, const QEvent::Type type, const QPointF& scenePosition,
                    const Qt::MouseButton button, const Qt::MouseButtons buttons)
{
    const auto globalPosition = QPointF(window->mapToGlobal(scenePosition.toPoint()));
    QMouseEvent event(type, scenePosition, scenePosition, globalPosition, button, buttons,
                      Qt::NoModifier, QPointingDevice::primaryPointingDevice());
    QCoreApplication::sendEvent(window, &event);
    QCoreApplication::processEvents();
}

bool dragItemToItem(QObject* root, const QString& sourceName, const QString& targetName)
{
    auto* const source = findVisualItem(root, sourceName);
    auto* const target = findVisualItem(root, targetName);
    auto* const preview = findVisualItem(root, QStringLiteral("dragPreview"));
    auto* const previewImage = findVisualItem(root, QStringLiteral("dragPreviewImage"));
    auto* const window = qobject_cast<QQuickWindow*>(root);
    if (!source || !target || !preview || !previewImage || !window)
        return false;

    const auto start = source->mapToScene(QPointF(source->width() / 2.0, source->height() / 2.0));
    const auto end = target->mapToScene(QPointF(target->width() / 2.0, target->height() / 2.0));
    sendMouseEvent(window, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    if (preview->isVisible())
    {
        sendMouseEvent(window, QEvent::MouseButtonRelease, start, Qt::LeftButton, Qt::NoButton);
        return false;
    }
    bool previewReady = false;
    for (int step = 1; step <= 8; ++step)
    {
        sendMouseEvent(window, QEvent::MouseMove,
                       start + (end - start) * (static_cast<double>(step) / 8.0), Qt::NoButton,
                       Qt::LeftButton);
        if (step == 4)
        {
            const auto previewSource = previewImage->property("source").toUrl();
            previewReady =
                preview->isVisible() && preview->z() >= 100.0 && previewSource.isLocalFile();
        }
    }
    sendMouseEvent(window, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
    QCoreApplication::processEvents();
    return previewReady;
}

bool popupIsOpen(QObject* root, const QString& objectName)
{
    const auto* const popup = root->findChild<QObject*>(objectName);
    if (!popup)
    {
        return false;
    }
    const auto opened =
        popup->property("visible").toBool() || popup->property("openedOnce").toBool();
    const auto width =
        std::max(popup->property("width").toDouble(), popup->property("implicitWidth").toDouble());
    const auto height = std::max(popup->property("height").toDouble(),
                                 popup->property("implicitHeight").toDouble());
    return opened && width > 0.0 && height > 0.0;
}

bool waitForPropertyFalse(QObject* obj, const QByteArray& propertyName, int timeoutMs = 6000)
{
    if (!obj)
        return true;

    QPointer<QObject> guard(obj);
    const auto met = [guard, propertyName]
    { return !guard || !guard->property(propertyName).toBool(); };
    if (met())
        return true;
    QEventLoop loop;
    QTimer poll;
    poll.setInterval(10);
    QObject::connect(&poll, &QTimer::timeout, &loop,
                     [&]
                     {
                         if (met())
                             loop.quit();
                     });
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    poll.start();
    loop.exec();
    return met();
}

bool closePopup(QObject* root, const QString& objectName)
{
    auto* const popup = root->findChild<QObject*>(objectName);
    if (!popup || !QMetaObject::invokeMethod(popup, "close"))
    {
        return false;
    }
    return waitForPropertyFalse(popup, "visible");
}

} // namespace

int main(int argc, char* argv[])
{
    RestartGuard restartGuard;
    QGuiApplication app(argc, argv);
    restartGuard.program = QCoreApplication::applicationFilePath();
    restartGuard.workingDirectory = QDir::currentPath();
    QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::China));
    app.setOrganizationName(QStringLiteral("Notera"));
    app.setOrganizationDomain(QStringLiteral("notera.app"));
    app.setApplicationName(QStringLiteral("Notera"));
    app.setWindowIcon(QIcon(QStringLiteral(":/src/assets/notera-icon.png")));

    QTranslator qtBaseTranslator;
    if (!qtBaseTranslator.load(QStringLiteral("qtbase_zh_CN"),
                               QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
    {
        qWarning() << "Qt base Chinese translation is unavailable";
    }
    app.installTranslator(&qtBaseTranslator);
    QTranslator qtTranslator;
    if (!qtTranslator.load(QStringLiteral("qt_zh_CN"),
                           QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
    {
        qWarning() << "Qt Chinese translation is unavailable";
    }
    app.installTranslator(&qtTranslator);

    const auto arguments = app.arguments();
    const auto isSmokeTest = arguments.contains(QStringLiteral("--theme-smoke-test")) ||
                             arguments.contains(QStringLiteral("--import-smoke-test")) ||
                             arguments.contains(QStringLiteral("--folder-import-smoke-test")) ||
                             arguments.contains(QStringLiteral("--stitch-smoke-test")) ||
                             arguments.contains(QStringLiteral("--reader-smoke-test")) ||
                             arguments.contains(QStringLiteral("--ui-smoke-test")) ||
                             arguments.contains(QStringLiteral("--folder-rename-smoke-test")) ||
                             arguments.contains(QStringLiteral("--storage-migration-smoke-test")) ||
                             arguments.contains(QStringLiteral("--clear-data-smoke-test")) ||
                             arguments.contains(QStringLiteral("--clipboard-smoke-test")) ||
                             arguments.contains(QStringLiteral("--tag-smoke-test"));
    if (isSmokeTest)
    {
        QStandardPaths::setTestModeEnabled(true);
        app.setApplicationName(QStringLiteral("NoteraTest"));
    }

    std::unique_ptr<SingleInstanceGuard> singleInstance;
    if (!isSmokeTest)
    {
        singleInstance = std::make_unique<SingleInstanceGuard>();
        if (singleInstance->isSecondary())
        {
            return 0;
        }
    }

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    std::unique_ptr<QTemporaryDir> migrationSmokeRoot;
    std::unique_ptr<QTemporaryDir> generalSmokeRoot;
    QString expectedMigratedFile;
    if (isSmokeTest && !arguments.contains(QStringLiteral("--storage-migration-smoke-test")))
    {
        generalSmokeRoot = std::make_unique<QTemporaryDir>();
        if (!generalSmokeRoot->isValid())
            return 1;
        AppDataPaths::setCustomRoot(generalSmokeRoot->path());
    }
    if (arguments.contains(QStringLiteral("--storage-migration-smoke-test")))
    {
        migrationSmokeRoot = std::make_unique<QTemporaryDir>();
        if (!migrationSmokeRoot->isValid())
            return 1;
        const auto oldRoot = migrationSmokeRoot->filePath(QStringLiteral("old"));
        const auto newRoot = migrationSmokeRoot->filePath(QStringLiteral("new"));
        QDir().mkpath(oldRoot + QStringLiteral("/database"));
        QDir().mkpath(oldRoot + QStringLiteral("/library/scores"));
        expectedMigratedFile = newRoot + QStringLiteral("/library/scores/test.png");
        QFile marker(oldRoot + QStringLiteral("/library/scores/test.png"));
        if (!marker.open(QIODevice::WriteOnly) || marker.write("notera") != 6)
            return 1;
        marker.close();
        const auto connectionName = QStringLiteral("storage_migration_fixture");
        {
            auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(oldRoot + QStringLiteral("/database/notera.db"));
            if (!database.open())
                return 1;
            QSqlQuery query(database);
            if (!query.exec(QStringLiteral(
                    "CREATE TABLE scores (file_path TEXT NOT NULL, thumbnail_path TEXT)")))
                return 1;
            query.prepare(QStringLiteral("INSERT INTO scores VALUES (?, NULL)"));
            query.addBindValue(oldRoot + QStringLiteral("/library/scores/test.png"));
            if (!query.exec())
                return 1;
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);
        QSettings settings;
        settings.setValue(QStringLiteral("storage/dataDirectory"), oldRoot);
        settings.remove(QStringLiteral("storage/pendingDataDirectory"));
        settings.sync();
        ApplicationController migrationController;
        bool restartRequested = false;
        QObject::connect(&migrationController, &ApplicationController::restartRequested,
                         [&restartRequested] { restartRequested = true; });
        if (migrationController.migrateDataDirectory(QUrl::fromLocalFile(newRoot)).isEmpty() ==
                false ||
            migrationController.pendingDataDirectory() != newRoot ||
            migrationController
                .migrateDataDirectory(QUrl(QStringLiteral("https://example.com/notera")))
                .isEmpty())
        {
            return 1;
        }
        migrationController.requestRestart();
        if (!restartRequested)
            return 1;
    }

    QString clearError;
    if (!ApplicationController::applyPendingDataClear(&clearError))
    {
        qWarning() << "Data clear failed:" << clearError;
        return 1;
    }

    QString migrationError;
    if (!ApplicationController::applyPendingDataMigration(&migrationError))
    {
        qWarning() << "Data directory migration failed:" << migrationError;
        if (arguments.contains(QStringLiteral("--storage-migration-smoke-test")))
            return 1;
    }
    QString restoreError;
    if (!ApplicationController::applyPendingBackupRestore(&restoreError))
    {
        qWarning() << "Database restore failed:" << restoreError;
        return 1;
    }
    if (arguments.contains(QStringLiteral("--clear-data-smoke-test")))
    {
        const auto clearRoot = AppDataPaths::root();
        QDir().mkpath(clearRoot + QStringLiteral("/database"));
        QFile databaseMarker(clearRoot + QStringLiteral("/database/notera.db"));
        if (!databaseMarker.open(QIODevice::WriteOnly) || databaseMarker.write("notera") != 6)
            return 1;
        databaseMarker.close();
        QFile marker(clearRoot + QStringLiteral("/clear-marker"));
        if (!marker.open(QIODevice::WriteOnly) || marker.write("notera") != 6)
            return 1;
        marker.close();
        QSettings().setValue(QStringLiteral("storage/pendingClearRoot"), clearRoot);
        QString error;
        if (!ApplicationController::applyPendingDataClear(&error) || QDir(clearRoot).exists())
            return 1;

        ApplicationController clearController;
        QDir().mkpath(AppDataPaths::databaseDirectory());
        QFile scheduledDatabase(AppDataPaths::databaseDirectory() + QStringLiteral("/notera.db"));
        if (!scheduledDatabase.open(QIODevice::WriteOnly))
            return 1;
        scheduledDatabase.close();
        bool restartRequested = false;
        QObject::connect(&clearController, &ApplicationController::restartRequested,
                         [&restartRequested] { restartRequested = true; });
        if (clearController.clearAllData(QStringLiteral("清空所有数据")).isEmpty() ||
            !clearController.clearAllData(QStringLiteral("确认清空所有数据")).isEmpty() ||
            !restartRequested ||
            QSettings().value(QStringLiteral("storage/pendingClearRoot")).toString().isEmpty())
            return 1;
        QSettings().clear();
        return 0;
    }
    if (arguments.contains(QStringLiteral("--storage-migration-smoke-test")))
    {
        QSqlDatabase migratedDatabase = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), QStringLiteral("storage_migration_verification"));
        migratedDatabase.setDatabaseName(AppDataPaths::databaseDirectory() +
                                         QStringLiteral("/notera.db"));
        if (!QFileInfo::exists(expectedMigratedFile) || !migratedDatabase.open())
            return 1;
        QSqlQuery query(migratedDatabase);
        const auto valid = query.exec(QStringLiteral("SELECT file_path FROM scores")) &&
                           query.next() && query.value(0).toString() == expectedMigratedFile;
        query.finish();
        migratedDatabase.close();
        migratedDatabase = {};
        QSqlDatabase::removeDatabase(QStringLiteral("storage_migration_verification"));
        return valid ? 0 : 1;
    }

    ApplicationController controller;
    LibraryService libraryService;
    MetronomeService metronome;
    QObject::connect(&controller, &ApplicationController::restartRequested, &app,
                     [&restartGuard]
                     {
                         restartGuard.requested = true;
                         QCoreApplication::quit();
                     });
    QObject::connect(&controller, &ApplicationController::scoreOpened, &libraryService,
                     &LibraryService::markScoreOpened);

    if (arguments.contains(QStringLiteral("--merge-smoke-test")))
    {

        {
            const auto connection = QStringLiteral("notera_merge_smoke_pretest");
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(AppDataPaths::databaseDirectory() +
                                         QStringLiteral("/notera.db"));
                if (database.open())
                {
                    QVariantList paths;
                    {
                        QSqlQuery query(database);
                        query.exec(QStringLiteral(
                            "SELECT id, file_path FROM scores WHERE title LIKE 'MERGE-SMOKE%'"));
                        while (query.next())
                            paths.append(query.value(1).toString());
                        query.exec(
                            QStringLiteral("DELETE FROM scores WHERE title LIKE 'MERGE-SMOKE%'"));
                        query.exec(
                            QStringLiteral("DELETE FROM folders WHERE name LIKE 'MERGE-SMOKE%'"));
                        query.exec(
                            QStringLiteral("DELETE FROM tags WHERE name LIKE 'MERGE-SMOKE%'"));
                    }
                    database.close();
                    for (const auto& path : paths)
                        QFile::remove(path.toString());
                }
            }
            QSqlDatabase::removeDatabase(connection);
        }

        QTemporaryDir backupRoot(QDir::tempPath() + QStringLiteral("/notera-merge-backup-XXXXXX"));
        QTemporaryDir zipDir(QDir::tempPath() + QStringLiteral("/notera-merge-zip-XXXXXX"));
        if (!backupRoot.isValid() || !zipDir.isValid())
            return 1;
        const auto backupPath = backupRoot.path();

        {
            QFile manifestFile(backupPath + QStringLiteral("/manifest.json"));
            if (!manifestFile.open(QIODevice::WriteOnly))
                return 1;
            QJsonObject manifest{
                {QStringLiteral("format"), QStringLiteral("notera-backup")},
                {QStringLiteral("formatVersion"), 1},
                {QStringLiteral("applicationVersion"), QCoreApplication::applicationVersion()},
                {QStringLiteral("createdAt"),
                 QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                {QStringLiteral("sourceRoot"), backupPath}};
            manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
            manifestFile.close();
        }
        if (!QDir().mkpath(backupPath + QStringLiteral("/database")) ||
            !QDir().mkpath(backupPath + QStringLiteral("/library/scores")) ||
            !QDir().mkpath(backupPath + QStringLiteral("/thumbnails")))
            return 1;

        QImage scoreImage(160, 220, QImage::Format_RGB32);
        scoreImage.fill(Qt::darkBlue);
        if (!scoreImage.save(backupPath + QStringLiteral("/library/scores/score1.png")))
            return 1;

        {
            auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                      QStringLiteral("notera_merge_smoke_backup"));
            database.setDatabaseName(backupPath + QStringLiteral("/database/notera.db"));
            if (!database.open())
                return 1;
            QSqlQuery query(database);
            query.exec(QStringLiteral(
                "CREATE TABLE scores (id TEXT PRIMARY KEY, title TEXT NOT NULL, composer TEXT, "
                "file_name TEXT NOT NULL, file_path TEXT NOT NULL, file_type TEXT NOT NULL, "
                "page_count INTEGER NOT NULL DEFAULT 1, thumbnail_path TEXT, favorite INTEGER NOT "
                "NULL DEFAULT 0, last_page INTEGER NOT NULL DEFAULT 1, created_at INTEGER NOT "
                "NULL, updated_at INTEGER NOT NULL, last_opened_at INTEGER, folder_id TEXT)"));
            query.exec(QStringLiteral(
                "CREATE TABLE folders (id TEXT PRIMARY KEY, name TEXT NOT NULL, created_at INTEGER "
                "NOT NULL, updated_at INTEGER NOT NULL, parent_id TEXT, last_opened_at INTEGER, "
                "favorite INTEGER NOT NULL DEFAULT 0)"));
            query.exec(QStringLiteral(
                "CREATE TABLE tags (id TEXT PRIMARY KEY, name TEXT NOT NULL UNIQUE)"));
            query.exec(QStringLiteral("CREATE TABLE score_tags (score_id TEXT NOT NULL, tag_id "
                                      "TEXT NOT NULL, PRIMARY KEY (score_id, tag_id))"));
            query.exec(QStringLiteral("CREATE TABLE folder_tags (folder_id TEXT NOT NULL, tag_id "
                                      "TEXT NOT NULL, PRIMARY KEY (folder_id, tag_id))"));
            query.exec(
                QStringLiteral("CREATE TABLE annotations (id TEXT PRIMARY KEY, score_id TEXT NOT "
                               "NULL, page INTEGER NOT NULL, type TEXT NOT NULL, data TEXT NOT "
                               "NULL, created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL)"));
            const auto now = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
            query.exec(QStringLiteral(
                "INSERT INTO tags (id, name) VALUES ('smoke-tag', 'MERGE-SMOKE-TAG')"));
            query.exec(QStringLiteral(
                "INSERT INTO folders (id, name, created_at, updated_at, parent_id, favorite) "
                "VALUES ('smoke-folder', 'MERGE-SMOKE-FOLDER', 1, 1, NULL, 0)"));
            query.exec(QStringLiteral("INSERT INTO folder_tags (folder_id, tag_id) VALUES "
                                      "('smoke-folder', 'smoke-tag')"));
            QSqlQuery insert(database);
            insert.prepare(
                QStringLiteral("INSERT INTO scores (id, title, composer, file_name, file_path, "
                               "file_type, page_count, favorite, last_page, created_at, "
                               "updated_at, folder_id) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"));
            insert.addBindValue(QStringLiteral("smoke-score"));
            insert.addBindValue(QStringLiteral("MERGE-SMOKE-SCORE"));
            insert.addBindValue(QString());
            insert.addBindValue(QStringLiteral("score1.png"));
            insert.addBindValue(backupPath + QStringLiteral("/library/scores/score1.png"));
            insert.addBindValue(QStringLiteral("png"));
            insert.addBindValue(1);
            insert.addBindValue(0);
            insert.addBindValue(1);
            insert.addBindValue(now);
            insert.addBindValue(now);
            insert.addBindValue(QStringLiteral("smoke-folder"));
            if (!insert.exec())
                return 1;
            query.exec(QStringLiteral(
                "INSERT INTO score_tags (score_id, tag_id) VALUES ('smoke-score', 'smoke-tag')"));
            database.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("notera_merge_smoke_backup"));

        const auto zipPath = zipDir.filePath(QStringLiteral("backup.notera-backup.zip"));
        {
            QZipWriter writer(zipPath);
            writer.setCompressionPolicy(QZipWriter::AutoCompress);
            if (writer.status() != QZipWriter::NoError)
                return 1;
            QDirIterator iterator(backupPath,
                                  QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden,
                                  QDirIterator::Subdirectories);
            while (iterator.hasNext())
            {
                const auto filePath = iterator.next();
                const auto relativePath = QDir(backupPath).relativeFilePath(filePath);
                const QFileInfo info(filePath);
                if (info.isDir())
                {
                    writer.addDirectory(relativePath);
                    continue;
                }
                QFile file(filePath);
                if (!file.open(QIODevice::ReadOnly))
                    return 1;
                writer.addFile(relativePath, file.readAll());
                file.close();
            }
            writer.close();
            if (writer.status() != QZipWriter::NoError)
                return 1;
        }

        const auto probe = libraryService.probeDatabaseBackup(QUrl::fromLocalFile(zipPath));
        if (!probe.value(QStringLiteral("valid")).toBool() ||
            probe.value(QStringLiteral("scoreCount")).toInt() != 1 ||
            probe.value(QStringLiteral("folderCount")).toInt() != 1 ||
            probe.value(QStringLiteral("tagCount")).toInt() != 1)
        {
            qWarning() << "[merge-smoke] FAIL probe:"
                       << QJsonDocument::fromVariant(probe).toJson(QJsonDocument::Compact);
            return 1;
        }

        const auto mergeError =
            libraryService.importDatabaseBackupMerged(QUrl::fromLocalFile(zipPath));
        if (!mergeError.isEmpty())
        {
            qWarning() << "[merge-smoke] FAIL first merge:" << mergeError;
            return 1;
        }
        {

            const auto connection = QStringLiteral("notera_merge_smoke_check");
            bool okScore = false;
            bool okFolder = false;
            bool okTag = false;
            bool okTime = false;
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(AppDataPaths::databaseDirectory() +
                                         QStringLiteral("/notera.db"));
                if (!database.open())
                {
                    qWarning() << "[merge-smoke] FAIL open db for assert";
                    return 1;
                }
                {
                    QSqlQuery query(database);
                    if (query.exec(QStringLiteral(
                            "SELECT COUNT(*) FROM scores WHERE title = 'MERGE-SMOKE-SCORE'")) &&
                        query.next())
                        okScore = query.value(0).toInt() == 1;
                    if (query.exec(QStringLiteral(
                            "SELECT COUNT(*) FROM scores s JOIN folders f ON s.folder_id = f.id "
                            "WHERE s.title = 'MERGE-SMOKE-SCORE' AND f.name = "
                            "'MERGE-SMOKE-FOLDER'")) &&
                        query.next())
                        okFolder = query.value(0).toInt() == 1;
                    if (query.exec(
                            QStringLiteral("SELECT COUNT(*) FROM score_tags st JOIN scores s ON "
                                           "st.score_id = s.id "
                                           "JOIN tags t ON st.tag_id = t.id WHERE s.title = "
                                           "'MERGE-SMOKE-SCORE' AND t.name = 'MERGE-SMOKE-TAG'")) &&
                        query.next())
                        okTag = query.value(0).toInt() == 1;

                    if (query.exec(QStringLiteral(
                            "SELECT created_at FROM scores WHERE title = 'MERGE-SMOKE-SCORE'")) &&
                        query.next())
                        okTime = query.value(0).toLongLong() > 1577836800000LL;
                }
                database.close();
            }
            QSqlDatabase::removeDatabase(connection);
            if (!okScore || !okFolder || !okTag || !okTime)
            {
                qWarning() << "[merge-smoke] FAIL assert: score" << okScore << "folder" << okFolder
                           << "tag" << okTag << "time" << okTime;
                return 1;
            }
        }
        {
            bool foundFolder = false;
            bool foundTag = false;
            for (int i = 0; i < libraryService.folders()->rowCount(); ++i)
            {
                if (libraryService.folders()
                        ->get(i)
                        .toMap()
                        .value(QStringLiteral("name"))
                        .toString() == QStringLiteral("MERGE-SMOKE-FOLDER"))
                    foundFolder = true;
            }
            for (int i = 0; i < libraryService.tags()->rowCount(); ++i)
            {
                if (libraryService.tags()
                        ->get(i)
                        .toMap()
                        .value(QStringLiteral("name"))
                        .toString() == QStringLiteral("MERGE-SMOKE-TAG"))
                    foundTag = true;
            }
            if (!foundFolder || !foundTag)
            {
                qWarning() << "[merge-smoke] FAIL folders/tags refresh: folder" << foundFolder
                           << "tag" << foundTag;
                return 1;
            }
        }

        bool conflicted = false;
        QObject::connect(&libraryService, &LibraryService::mergeConflict, &libraryService,
                         [&conflicted, &libraryService](const QString&, const QString&, int, int)
                         {
                             conflicted = true;
                             libraryService.resolveMergeConflict(QStringLiteral("skip"), true);
                         });
        const auto countBeforeSecond = []()
        {
            const auto connection = QStringLiteral("notera_merge_smoke_count");
            int count = -1;
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(AppDataPaths::databaseDirectory() +
                                         QStringLiteral("/notera.db"));
                if (database.open())
                {
                    {
                        QSqlQuery query(database);
                        if (query.exec(QStringLiteral(
                                "SELECT COUNT(*) FROM scores WHERE title LIKE 'MERGE-SMOKE%'")) &&
                            query.next())
                            count = query.value(0).toInt();
                    }
                    database.close();
                }
            }
            QSqlDatabase::removeDatabase(connection);
            return count;
        }();
        const auto mergeError2 =
            libraryService.importDatabaseBackupMerged(QUrl::fromLocalFile(zipPath));
        const auto countAfterSecond = []()
        {
            const auto connection = QStringLiteral("notera_merge_smoke_count2");
            int count = -1;
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(AppDataPaths::databaseDirectory() +
                                         QStringLiteral("/notera.db"));
                if (database.open())
                {
                    {
                        QSqlQuery query(database);
                        if (query.exec(QStringLiteral(
                                "SELECT COUNT(*) FROM scores WHERE title LIKE 'MERGE-SMOKE%'")) &&
                            query.next())
                            count = query.value(0).toInt();
                    }
                    database.close();
                }
            }
            QSqlDatabase::removeDatabase(connection);
            return count;
        }();
        qWarning() << "[merge-smoke] second merge: conflicted" << conflicted << "before"
                   << countBeforeSecond << "after" << countAfterSecond << "err" << mergeError2;
        if (!mergeError2.isEmpty())
            return 1;
        if (!conflicted || countBeforeSecond != countAfterSecond)
            return 1;

        {
            const auto connection = QStringLiteral("notera_merge_smoke_cleanup");
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(AppDataPaths::databaseDirectory() +
                                         QStringLiteral("/notera.db"));
                if (database.open())
                {
                    QVariantList paths;
                    {
                        QSqlQuery query(database);
                        query.exec(QStringLiteral(
                            "SELECT id, file_path FROM scores WHERE title LIKE 'MERGE-SMOKE%'"));
                        while (query.next())
                        {
                            paths.append(query.value(1).toString());
                            paths.append(AppDataPaths::thumbnailDirectory() + QLatin1Char('/') +
                                         query.value(0).toString() + QStringLiteral(".png"));
                        }
                        query.exec(
                            QStringLiteral("DELETE FROM scores WHERE title LIKE 'MERGE-SMOKE%'"));
                        query.exec(
                            QStringLiteral("DELETE FROM folders WHERE name LIKE 'MERGE-SMOKE%'"));
                        query.exec(
                            QStringLiteral("DELETE FROM tags WHERE name LIKE 'MERGE-SMOKE%'"));
                    }
                    database.close();
                    for (const auto& path : paths)
                        QFile::remove(path.toString());
                }
            }
            QSqlDatabase::removeDatabase(connection);
        }
        return 0;
    }

    if (arguments.contains(QStringLiteral("--clipboard-smoke-test")))
    {

        const auto libDir = AppDataPaths::libraryDirectory();
        const auto dbPath = AppDataPaths::databaseDirectory() + QStringLiteral("/notera.db");
        QDir().mkpath(libDir);
        QDir().mkpath(AppDataPaths::databaseDirectory());

        {
            const auto connection = QStringLiteral("clipboard_smoke_preclean");
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(dbPath);
                if (database.open())
                {
                    QVariantList paths;
                    {
                        QSqlQuery query(database);
                        query.exec(QStringLiteral(
                            "SELECT file_path FROM scores WHERE title LIKE 'CLIPBOARD-SMOKE%'"));
                        while (query.next())
                            paths.append(query.value(0).toString());
                        query.exec(QStringLiteral(
                            "DELETE FROM scores WHERE title LIKE 'CLIPBOARD-SMOKE%'"));
                        query.exec(QStringLiteral(
                            "DELETE FROM folders WHERE name LIKE 'CLIPBOARD-SMOKE%'"));
                    }
                    database.close();
                    for (const auto& p : paths)
                        QFile::remove(p.toString());
                }
            }
            QSqlDatabase::removeDatabase(connection);
        }

        QImage testImage(160, 220, QImage::Format_RGB32);
        testImage.fill(Qt::blue);
        const auto file1 = libDir + QStringLiteral("/clipboard-smoke-1.png");
        const auto file2 = libDir + QStringLiteral("/clipboard-smoke-2.png");
        if (!testImage.save(file1) || !testImage.save(file2))
        {
            qWarning() << "[clipboard-smoke] FAIL: cannot save test images";
            return 1;
        }
        const auto now = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
        {
            const auto connection = QStringLiteral("clipboard_smoke_setup");
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(dbPath);
                if (!database.open())
                {
                    qWarning() << "[clipboard-smoke] FAIL: cannot open database";
                    return 1;
                }
                QSqlQuery insert(database);
                insert.prepare(
                    QStringLiteral("INSERT INTO folders (id, name, created_at, updated_at, "
                                   "parent_id, favorite) VALUES (?,?,?,?,NULL,0)"));
                insert.addBindValue(QStringLiteral("clip-smoke-target"));
                insert.addBindValue(QStringLiteral("CLIPBOARD-SMOKE-TARGET"));
                insert.addBindValue(now);
                insert.addBindValue(now);
                if (!insert.exec())
                {
                    qWarning() << "[clipboard-smoke] FAIL: insert folder"
                               << insert.lastError().text();
                    return 1;
                }
                insert.prepare(
                    QStringLiteral("INSERT INTO scores (id, title, composer, file_name, file_path, "
                                   "file_type, page_count, favorite, last_page, created_at, "
                                   "updated_at, folder_id) VALUES (?,?,?,?,?,?,?,?,?,?,?,NULL)"));
                const QList<QPair<QString, QString>> scores = {
                    {QStringLiteral("clip-smoke-1"), QStringLiteral("CLIPBOARD-SMOKE-1")},
                    {QStringLiteral("clip-smoke-2"), QStringLiteral("CLIPBOARD-SMOKE-2")}};
                for (const auto& [id, title] : scores)
                {
                    insert.addBindValue(id);
                    insert.addBindValue(title);
                    insert.addBindValue(QString());
                    insert.addBindValue(QStringLiteral("clipboard-smoke-") + id.right(1) +
                                        QStringLiteral(".png"));
                    insert.addBindValue(libDir + QStringLiteral("/clipboard-smoke-") + id.right(1) +
                                        QStringLiteral(".png"));
                    insert.addBindValue(QStringLiteral("png"));
                    insert.addBindValue(1);
                    insert.addBindValue(0);
                    insert.addBindValue(1);
                    insert.addBindValue(now);
                    insert.addBindValue(now);
                    if (!insert.exec())
                    {
                        qWarning() << "[clipboard-smoke] FAIL: insert score" << id
                                   << insert.lastError().text();
                        return 1;
                    }
                }

                testImage.save(libDir + QStringLiteral("/clipboard-smoke-cut.png"));
                QSqlQuery insertCut(database);
                insertCut.prepare(
                    QStringLiteral("INSERT INTO scores (id, title, composer, file_name, file_path, "
                                   "file_type, page_count, favorite, last_page, created_at, "
                                   "updated_at, folder_id) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"));
                insertCut.addBindValue(QStringLiteral("clip-smoke-cut"));
                insertCut.addBindValue(QStringLiteral("CLIPBOARD-SMOKE-CUT"));
                insertCut.addBindValue(QString());
                insertCut.addBindValue(QStringLiteral("clipboard-smoke-cut.png"));
                insertCut.addBindValue(libDir + QStringLiteral("/clipboard-smoke-cut.png"));
                insertCut.addBindValue(QStringLiteral("png"));
                insertCut.addBindValue(1);
                insertCut.addBindValue(0);
                insertCut.addBindValue(1);
                insertCut.addBindValue(now);
                insertCut.addBindValue(now);
                insertCut.addBindValue(QString());
                if (!insertCut.exec())
                {
                    qWarning() << "[clipboard-smoke] FAIL: insert cut score"
                               << insertCut.lastError().text();
                    return 1;
                }

                testImage.save(libDir + QStringLiteral("/clip-smoke-inner-score.png"));
                QSqlQuery insertInnerFolder(database);
                insertInnerFolder.prepare(
                    QStringLiteral("INSERT INTO folders (id, name, parent_id, created_at, "
                                   "updated_at) VALUES (?,?,?,?,?)"));
                insertInnerFolder.addBindValue(QStringLiteral("clip-smoke-inner-folder"));
                insertInnerFolder.addBindValue(QStringLiteral("CLIP-SMOKE-INNER-FOLDER"));
                insertInnerFolder.addBindValue(QVariant());
                insertInnerFolder.addBindValue(now);
                insertInnerFolder.addBindValue(now);
                if (!insertInnerFolder.exec())
                {
                    qWarning() << "[clipboard-smoke] FAIL: insert inner folder"
                               << insertInnerFolder.lastError().text();
                    return 1;
                }
                QSqlQuery insertInnerScore(database);
                insertInnerScore.prepare(
                    QStringLiteral("INSERT INTO scores (id, title, composer, file_name, file_path, "
                                   "file_type, page_count, favorite, last_page, created_at, "
                                   "updated_at, folder_id) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"));
                insertInnerScore.addBindValue(QStringLiteral("clip-smoke-inner-score"));
                insertInnerScore.addBindValue(QStringLiteral("CLIP-SMOKE-INNER-SCORE"));
                insertInnerScore.addBindValue(QString());
                insertInnerScore.addBindValue(QStringLiteral("clip-smoke-inner-score.png"));
                insertInnerScore.addBindValue(libDir +
                                              QStringLiteral("/clip-smoke-inner-score.png"));
                insertInnerScore.addBindValue(QStringLiteral("png"));
                insertInnerScore.addBindValue(1);
                insertInnerScore.addBindValue(0);
                insertInnerScore.addBindValue(1);
                insertInnerScore.addBindValue(now);
                insertInnerScore.addBindValue(now);
                insertInnerScore.addBindValue(QStringLiteral("clip-smoke-inner-folder"));
                if (!insertInnerScore.exec())
                {
                    qWarning() << "[clipboard-smoke] FAIL: insert inner score"
                               << insertInnerScore.lastError().text();
                    return 1;
                }

                testImage.save(libDir + QStringLiteral("/nested-score.png"));
                QSqlQuery insertNestedA(database);
                insertNestedA.prepare(QStringLiteral("INSERT INTO folders (id, name, parent_id, "
                                                     "created_at, updated_at) VALUES (?,?,?,?,?)"));
                insertNestedA.addBindValue(QStringLiteral("nested-a"));
                insertNestedA.addBindValue(QStringLiteral("NESTED-A"));
                insertNestedA.addBindValue(QVariant());
                insertNestedA.addBindValue(now);
                insertNestedA.addBindValue(now);
                if (!insertNestedA.exec())
                {
                    qWarning() << "[clipboard-smoke] FAIL: insert nested-a"
                               << insertNestedA.lastError().text();
                    return 1;
                }
                QSqlQuery insertNestedB(database);
                insertNestedB.prepare(QStringLiteral("INSERT INTO folders (id, name, parent_id, "
                                                     "created_at, updated_at) VALUES (?,?,?,?,?)"));
                insertNestedB.addBindValue(QStringLiteral("nested-b"));
                insertNestedB.addBindValue(QStringLiteral("NESTED-B"));
                insertNestedB.addBindValue(QStringLiteral("nested-a"));
                insertNestedB.addBindValue(now);
                insertNestedB.addBindValue(now);
                if (!insertNestedB.exec())
                {
                    qWarning() << "[clipboard-smoke] FAIL: insert nested-b"
                               << insertNestedB.lastError().text();
                    return 1;
                }
                QSqlQuery insertNestedScore(database);
                insertNestedScore.prepare(
                    QStringLiteral("INSERT INTO scores (id, title, composer, file_name, file_path, "
                                   "file_type, page_count, favorite, last_page, created_at, "
                                   "updated_at, folder_id) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"));
                insertNestedScore.addBindValue(QStringLiteral("nested-score"));
                insertNestedScore.addBindValue(QStringLiteral("NESTED-SCORE"));
                insertNestedScore.addBindValue(QString());
                insertNestedScore.addBindValue(QStringLiteral("nested-score.png"));
                insertNestedScore.addBindValue(libDir + QStringLiteral("/nested-score.png"));
                insertNestedScore.addBindValue(QStringLiteral("png"));
                insertNestedScore.addBindValue(1);
                insertNestedScore.addBindValue(0);
                insertNestedScore.addBindValue(1);
                insertNestedScore.addBindValue(now);
                insertNestedScore.addBindValue(now);
                insertNestedScore.addBindValue(QStringLiteral("nested-b"));
                if (!insertNestedScore.exec())
                {
                    qWarning() << "[clipboard-smoke] FAIL: insert nested score"
                               << insertNestedScore.lastError().text();
                    return 1;
                }

                QSqlQuery insertMultiFolder(database);
                insertMultiFolder.prepare(
                    QStringLiteral("INSERT INTO folders (id, name, parent_id, created_at, "
                                   "updated_at) VALUES (?,?,?,?,?)"));
                insertMultiFolder.addBindValue(QStringLiteral("multi-conflict-folder"));
                insertMultiFolder.addBindValue(QStringLiteral("MULTI-CONFLICT-FOLDER"));
                insertMultiFolder.addBindValue(QVariant());
                insertMultiFolder.addBindValue(now);
                insertMultiFolder.addBindValue(now);
                if (!insertMultiFolder.exec())
                {
                    qWarning() << "[clipboard-smoke] FAIL: insert multi folder"
                               << insertMultiFolder.lastError().text();
                    return 1;
                }
                for (int i = 1; i <= 2; ++i)
                {
                    const auto scoreId = QStringLiteral("multi-conflict-%1").arg(i);
                    const auto scoreName = QStringLiteral("MULTI-CONFLICT-%1").arg(i);
                    const auto fileName = QStringLiteral("multi-conflict-%1.png").arg(i);
                    testImage.save(libDir + "/" + fileName);
                    QSqlQuery insertMultiScore(database);
                    insertMultiScore.prepare(QStringLiteral(
                        "INSERT INTO scores (id, title, composer, file_name, file_path, file_type, "
                        "page_count, favorite, last_page, created_at, updated_at, folder_id) "
                        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"));
                    insertMultiScore.addBindValue(scoreId);
                    insertMultiScore.addBindValue(scoreName);
                    insertMultiScore.addBindValue(QString());
                    insertMultiScore.addBindValue(fileName);
                    insertMultiScore.addBindValue(libDir + "/" + fileName);
                    insertMultiScore.addBindValue(QStringLiteral("png"));
                    insertMultiScore.addBindValue(1);
                    insertMultiScore.addBindValue(0);
                    insertMultiScore.addBindValue(1);
                    insertMultiScore.addBindValue(now);
                    insertMultiScore.addBindValue(now);
                    insertMultiScore.addBindValue(QStringLiteral("multi-conflict-folder"));
                    if (!insertMultiScore.exec())
                    {
                        qWarning() << "[clipboard-smoke] FAIL: insert multi score" << i
                                   << insertMultiScore.lastError().text();
                        return 1;
                    }
                }
                database.close();
            }
            QSqlDatabase::removeDatabase(connection);
        }

        libraryService.goToLibraryRoot();
        libraryService.enterFolder(QStringLiteral("clip-smoke-target"));
        if (libraryService.currentFolderId() != QStringLiteral("clip-smoke-target"))
        {
            qWarning() << "[clipboard-smoke] FAIL: cannot enter target folder";
            return 1;
        }
        libraryService.copyItems({QStringLiteral("clip-smoke-1"), QStringLiteral("clip-smoke-2")});
        if (libraryService.clipboardItems().size() != 2 ||
            libraryService.clipboardMode() != QStringLiteral("copy"))
        {
            qWarning() << "[clipboard-smoke] FAIL: clipboard not set after multi-copy";
            return 1;
        }
        libraryService.pasteItems();
        {
            const auto inTarget =
                libraryService.scoresInFolder(QStringLiteral("clip-smoke-target"));
            if (inTarget.size() != 2)
            {
                qWarning() << "[clipboard-smoke] FAIL: expected 2 scores after multi-copy, got"
                           << inTarget.size();
                return 1;
            }
        }

        libraryService.copyItems({QStringLiteral("clip-smoke-1"), QStringLiteral("clip-smoke-2")});
        libraryService.pasteItems();

        libraryService.resolvePasteConflict(QStringLiteral("rename"), false);

        libraryService.resolvePasteConflict(QStringLiteral("skip"), false);
        {
            const auto inTarget =
                libraryService.scoresInFolder(QStringLiteral("clip-smoke-target"));

            if (inTarget.size() != 3)
            {
                qWarning()
                    << "[clipboard-smoke] FAIL: expected 3 scores after conflict (rename+skip), got"
                    << inTarget.size();
                for (const auto& s : inTarget)
                    qWarning() << "  " << s.toMap().value("title").toString();
                return 1;
            }
            bool hasRenamed = false;
            for (const auto& s : inTarget)
            {
                const auto t = s.toMap().value(QStringLiteral("title")).toString();
                if (t.startsWith(QStringLiteral("CLIPBOARD-SMOKE-1")) &&
                    t != QStringLiteral("CLIPBOARD-SMOKE-1"))
                    hasRenamed = true;
            }
            if (!hasRenamed)
            {
                qWarning() << "[clipboard-smoke] FAIL: renamed copy not found";
                return 1;
            }
        }

        {
            QObject ctx;
            QEventLoop copyLoop;
            bool conflictFired = false;
            QObject::connect(&libraryService, &LibraryService::pasteConflict, &ctx,
                             [&](const QString&, const QString&, int, int)
                             {
                                 conflictFired = true;
                                 copyLoop.quit();
                             });
            QObject::connect(&libraryService, &LibraryService::pasteFinished, &ctx,
                             [&](int) { copyLoop.quit(); });
            QTimer::singleShot(800, &copyLoop, &QEventLoop::quit);
            libraryService.copyItems({QStringLiteral("clip-smoke-1")});
            libraryService.goToLibraryRoot();
            libraryService.pasteItems();
            copyLoop.exec();
            if (!conflictFired)
            {
                qWarning() << "[clipboard-smoke] FAIL: same-folder score copy should fire conflict";
                return 1;
            }

            {
                const auto inRoot = libraryService.scoresInFolder(QString());
                int count = 0;
                for (const auto& s : inRoot)
                {
                    if (s.toMap().value(QStringLiteral("title")).toString() ==
                        QStringLiteral("CLIPBOARD-SMOKE-1"))
                        ++count;
                }
                if (count != 1)
                {
                    qWarning() << "[clipboard-smoke] FAIL: same-folder copy should not duplicate "
                                  "before resolving conflict";
                    return 1;
                }
            }

            libraryService.resolvePasteConflict(QStringLiteral("rename"), false);
            {
                const auto inRoot = libraryService.scoresInFolder(QString());
                bool foundOriginal = false;
                bool foundCopy = false;
                for (const auto& s : inRoot)
                {
                    const auto t = s.toMap().value(QStringLiteral("title")).toString();
                    if (t == QStringLiteral("CLIPBOARD-SMOKE-1"))
                        foundOriginal = true;
                    if (t == QStringLiteral("CLIPBOARD-SMOKE-1 (2)"))
                        foundCopy = true;
                }
                if (!foundOriginal || !foundCopy)
                {
                    qWarning() << "[clipboard-smoke] FAIL: expected original + ' (2)' copy for "
                                  "same-folder score copy";
                    return 1;
                }
            }

            {
                QObject ctx2;
                QEventLoop loop2;
                bool conflictFired2 = false;
                QObject::connect(&libraryService, &LibraryService::pasteConflict, &ctx2,
                                 [&](const QString&, const QString&, int, int)
                                 {
                                     conflictFired2 = true;
                                     loop2.quit();
                                 });
                QObject::connect(&libraryService, &LibraryService::pasteFinished, &ctx2,
                                 [&](int) { loop2.quit(); });
                QTimer::singleShot(800, &loop2, &QEventLoop::quit);
                libraryService.copyItems({QStringLiteral("clip-smoke-1")});
                libraryService.goToLibraryRoot();
                libraryService.pasteItems();
                loop2.exec();
                if (!conflictFired2)
                {
                    qWarning() << "[clipboard-smoke] FAIL: same-folder score copy should fire "
                                  "conflict again";
                    return 1;
                }
                libraryService.resolvePasteConflict(QStringLiteral("overwrite"), false);
                const auto inRoot = libraryService.scoresInFolder(QString());
                bool foundOriginal = false;
                bool foundCopy = false;
                for (const auto& s : inRoot)
                {
                    const auto t = s.toMap().value(QStringLiteral("title")).toString();
                    if (t == QStringLiteral("CLIPBOARD-SMOKE-1"))
                        foundOriginal = true;
                    if (t == QStringLiteral("CLIPBOARD-SMOKE-1 (2)"))
                        foundCopy = true;
                }

                if (!foundOriginal || !foundCopy)
                {
                    qWarning() << "[clipboard-smoke] FAIL: same-folder overwrite should be a no-op "
                                  "preserving all items";
                    return 1;
                }
            }
        }

        libraryService.goToLibraryRoot();
        libraryService.cutItems({QStringLiteral("clip-smoke-cut")});
        if (libraryService.clipboardMode() != QStringLiteral("cut"))
        {
            qWarning() << "[clipboard-smoke] FAIL: clipboard mode not cut";
            return 1;
        }
        libraryService.enterFolder(QStringLiteral("clip-smoke-target"));
        libraryService.pasteItems();
        {
            const auto inRoot = libraryService.scoresInFolder(QString());
            bool foundInRoot = false;
            for (const auto& s : inRoot)
            {
                if (s.toMap().value(QStringLiteral("id")).toString() ==
                    QStringLiteral("clip-smoke-cut"))
                    foundInRoot = true;
            }
            const auto inTarget =
                libraryService.scoresInFolder(QStringLiteral("clip-smoke-target"));
            bool foundInTarget = false;
            for (const auto& s : inTarget)
            {
                if (s.toMap().value(QStringLiteral("id")).toString() ==
                    QStringLiteral("clip-smoke-cut"))
                    foundInTarget = true;
            }
            if (foundInRoot || !foundInTarget)
            {
                qWarning() << "[clipboard-smoke] FAIL: cut did not move score (inRoot="
                           << foundInRoot << " inTarget=" << foundInTarget << ")";
                return 1;
            }

            if (libraryService.clipboardMode() != QStringLiteral("none") ||
                !libraryService.clipboardItems().isEmpty())
            {
                qWarning() << "[clipboard-smoke] FAIL: clipboard not cleared after cut paste";
                return 1;
            }
        }

        {
            QObject ctx;
            QEventLoop moveLoop;
            bool conflictFired = false;
            QObject::connect(&libraryService, &LibraryService::pasteConflict, &ctx,
                             [&](const QString&, const QString&, int, int)
                             {
                                 conflictFired = true;
                                 moveLoop.quit();
                             });
            QObject::connect(&libraryService, &LibraryService::pasteFinished, &ctx,
                             [&](int) { moveLoop.quit(); });
            QTimer::singleShot(800, &moveLoop, &QEventLoop::quit);
            libraryService.moveItems({QStringLiteral("clip-smoke-2")},
                                     QStringLiteral("clip-smoke-target"));
            moveLoop.exec();
            if (!conflictFired)
            {
                qWarning()
                    << "[clipboard-smoke] FAIL: move with same-name target should fire conflict";
                return 1;
            }
            libraryService.resolvePasteConflict(QStringLiteral("rename"), false);

            const auto inRoot = libraryService.scoresInFolder(QString());
            const auto inTarget =
                libraryService.scoresInFolder(QStringLiteral("clip-smoke-target"));
            bool rootHasOriginal = false;
            for (const auto& s : inRoot)
            {
                if (s.toMap().value(QStringLiteral("title")).toString() ==
                    QStringLiteral("CLIPBOARD-SMOKE-2"))
                    rootHasOriginal = true;
            }
            bool targetHasOriginal = false;
            bool targetHasRenamed = false;
            for (const auto& s : inTarget)
            {
                const auto t = s.toMap().value(QStringLiteral("title")).toString();
                if (t == QStringLiteral("CLIPBOARD-SMOKE-2"))
                    targetHasOriginal = true;
                if (t == QStringLiteral("CLIPBOARD-SMOKE-2 (2)"))
                    targetHasRenamed = true;
            }
            if (rootHasOriginal || !targetHasOriginal || !targetHasRenamed)
            {
                qWarning() << "[clipboard-smoke] FAIL: move-with-conflict rename did not move "
                              "correctly (rootHasOriginal="
                           << rootHasOriginal << " targetHasOriginal=" << targetHasOriginal
                           << " targetHasRenamed=" << targetHasRenamed << ")";
                return 1;
            }
        }

        {
            QObject ctx;
            QEventLoop copyLoop;
            bool folderConflictFired = false;
            QObject::connect(&libraryService, &LibraryService::pasteFolderConflict, &ctx,
                             [&](const QString&, const QString&, int, int)
                             {
                                 folderConflictFired = true;
                                 copyLoop.quit();
                             });
            QObject::connect(&libraryService, &LibraryService::pasteFinished, &ctx,
                             [&](int) { copyLoop.quit(); });
            QTimer::singleShot(800, &copyLoop, &QEventLoop::quit);
            libraryService.copyItems({QStringLiteral("clip-smoke-inner-folder")});
            libraryService.goToLibraryRoot();
            libraryService.pasteItems();
            copyLoop.exec();
            if (!folderConflictFired)
            {
                qWarning() << "[clipboard-smoke] FAIL: same-folder folder copy should fire folder "
                              "conflict";
                return 1;
            }

            {
                const auto rootFolders = libraryService.childFolders(QString());
                int count = 0;
                for (const auto& f : rootFolders)
                {
                    if (f.toMap().value(QStringLiteral("name")).toString() ==
                        QStringLiteral("CLIP-SMOKE-INNER-FOLDER"))
                        ++count;
                }
                if (count != 1)
                {
                    qWarning() << "[clipboard-smoke] FAIL: same-folder folder copy should not "
                                  "duplicate before resolving conflict";
                    return 1;
                }
            }
            libraryService.resolvePasteFolderConflict(QStringLiteral("rename"), false);

            const auto rootFolders = libraryService.childFolders(QString());
            bool foundOriginal = false;
            bool foundCopy = false;
            QString copyFolderId;
            for (const auto& f : rootFolders)
            {
                const auto name = f.toMap().value(QStringLiteral("name")).toString();
                if (name == QStringLiteral("CLIP-SMOKE-INNER-FOLDER"))
                    foundOriginal = true;
                if (name == QStringLiteral("CLIP-SMOKE-INNER-FOLDER (2)"))
                {
                    foundCopy = true;
                    copyFolderId = f.toMap().value(QStringLiteral("id")).toString();
                }
            }
            if (!foundOriginal || !foundCopy)
            {
                qWarning() << "[clipboard-smoke] FAIL: expected original and ' (2)' copy after "
                              "same-folder copy";
                return 1;
            }
            const auto copyScores = libraryService.scoresInFolder(copyFolderId);
            if (copyScores.size() != 1)
            {
                qWarning()
                    << "[clipboard-smoke] FAIL: copied folder should contain inner score, got"
                    << copyScores.size();
                return 1;
            }
        }

        {
            QObject ctx;
            QEventLoop copyLoop;
            bool folderConflictFired = false;
            QObject::connect(&libraryService, &LibraryService::pasteFolderConflict, &ctx,
                             [&](const QString&, const QString&, int, int)
                             {
                                 folderConflictFired = true;
                                 copyLoop.quit();
                             });
            QObject::connect(&libraryService, &LibraryService::pasteFinished, &ctx,
                             [&](int) { copyLoop.quit(); });
            QTimer::singleShot(800, &copyLoop, &QEventLoop::quit);
            libraryService.copyItems({QStringLiteral("nested-a")});
            libraryService.goToLibraryRoot();
            libraryService.pasteItems();
            copyLoop.exec();
            if (!folderConflictFired)
            {
                qWarning() << "[clipboard-smoke] FAIL: same-folder nested copy should fire folder "
                              "conflict";
                return 1;
            }
            libraryService.resolvePasteFolderConflict(QStringLiteral("rename"), false);

            const auto rootFolders = libraryService.childFolders(QString());
            QString copyId;
            for (const auto& f : rootFolders)
            {
                if (f.toMap().value(QStringLiteral("name")).toString() ==
                    QStringLiteral("NESTED-A (2)"))
                {
                    copyId = f.toMap().value(QStringLiteral("id")).toString();
                    break;
                }
            }
            if (copyId.isEmpty())
            {
                qWarning() << "[clipboard-smoke] FAIL: expected 'NESTED-A (2)' copy";
                return 1;
            }
            bool foundNestedB = false;
            const auto subFolders = libraryService.childFolders(copyId);
            for (const auto& f : subFolders)
            {
                if (f.toMap().value(QStringLiteral("name")).toString() !=
                    QStringLiteral("NESTED-B"))
                    continue;
                foundNestedB = true;
                const auto bScores =
                    libraryService.scoresInFolder(f.toMap().value(QStringLiteral("id")).toString());
                if (bScores.size() != 1)
                {
                    qWarning()
                        << "[clipboard-smoke] FAIL: copied NESTED-B should contain its score";
                    return 1;
                }
            }
            if (!foundNestedB)
            {
                qWarning() << "[clipboard-smoke] FAIL: copied folder missing NESTED-B";
                return 1;
            }
        }

        {
            QObject ctx;
            libraryService.createFolder(QStringLiteral("MULTI-CONFLICT-DEST"));
            QString destId;
            for (int i = 0; i < libraryService.folders()->rowCount(); ++i)
            {
                const auto f = libraryService.folders()->get(i).toMap();
                if (f.value(QStringLiteral("name")).toString() ==
                    QStringLiteral("MULTI-CONFLICT-DEST"))
                {
                    destId = f.value(QStringLiteral("itemId")).toString();
                    break;
                }
            }
            if (destId.isEmpty())
            {
                qWarning() << "[clipboard-smoke] FAIL: cannot find MULTI-CONFLICT-DEST";
                return 1;
            }

            libraryService.enterFolder(destId);
            libraryService.createFolder(QStringLiteral("MULTI-CONFLICT-FOLDER"));

            int folderConflictCount = 0;
            QEventLoop multiLoop;
            QObject::connect(&libraryService, &LibraryService::pasteFolderConflict, &ctx,
                             [&](const QString&, const QString&, int, int)
                             {
                                 ++folderConflictCount;

                                 libraryService.resolvePasteFolderConflict(QStringLiteral("rename"),
                                                                           false);
                             });
            QObject::connect(&libraryService, &LibraryService::pasteFinished, &ctx,
                             [&](int) { multiLoop.quit(); });
            QTimer::singleShot(1500, &multiLoop, &QEventLoop::quit);
            libraryService.copyItems({QStringLiteral("multi-conflict-folder")});
            libraryService.pasteItems();
            multiLoop.exec();
            if (folderConflictCount < 1)
            {
                qWarning() << "[clipboard-smoke] FAIL: expected at least 1 folder conflict dialog "
                              "for cross-folder copy, got"
                           << folderConflictCount;
                return 1;
            }

            const auto destSubs = libraryService.childFolders(destId);
            bool foundOriginal = false;
            bool foundRenamed = false;
            for (const auto& f : destSubs)
            {
                const auto name = f.toMap().value(QStringLiteral("name")).toString();
                if (name == QStringLiteral("MULTI-CONFLICT-FOLDER"))
                    foundOriginal = true;
                if (name == QStringLiteral("MULTI-CONFLICT-FOLDER (2)"))
                    foundRenamed = true;
            }
            if (!foundOriginal || !foundRenamed)
            {
                qWarning() << "[clipboard-smoke] FAIL: expected both original and renamed folder "
                              "inside DEST";
                return 1;
            }
            libraryService.goToLibraryRoot();
        }

        {
            QObject ctx;
            libraryService.createFolder(QStringLiteral("CUT-DEST"));
            QString destId;
            for (int i = 0; i < libraryService.folders()->rowCount(); ++i)
            {
                const auto f = libraryService.folders()->get(i).toMap();
                if (f.value(QStringLiteral("name")).toString() == QStringLiteral("CUT-DEST"))
                {
                    destId = f.value(QStringLiteral("itemId")).toString();
                    break;
                }
            }
            if (destId.isEmpty())
            {
                qWarning() << "[clipboard-smoke] FAIL: cannot find CUT-DEST";
                return 1;
            }
            libraryService.cutItems({QStringLiteral("multi-conflict-folder")});
            libraryService.goToLibraryRoot();
            libraryService.enterFolder(destId);
            QEventLoop cutFolderLoop;
            QObject::connect(&libraryService, &LibraryService::pasteFinished, &ctx,
                             [&](int) { cutFolderLoop.quit(); });
            QTimer::singleShot(1000, &cutFolderLoop, &QEventLoop::quit);
            libraryService.pasteItems();
            cutFolderLoop.exec();

            const auto destSubs = libraryService.childFolders(destId);
            bool foundMoved = false;
            for (const auto& f : destSubs)
            {
                if (f.toMap().value(QStringLiteral("name")).toString() ==
                    QStringLiteral("MULTI-CONFLICT-FOLDER"))
                {
                    foundMoved = true;
                    break;
                }
            }
            if (!foundMoved)
            {
                qWarning() << "[clipboard-smoke] FAIL: cut folder did not appear in destination "
                              "(file lost?)";
                return 1;
            }

            libraryService.goToLibraryRoot();
            const auto rootFolders = libraryService.childFolders(QString());
            bool foundInRoot = false;
            for (const auto& f : rootFolders)
            {
                if (f.toMap().value(QStringLiteral("name")).toString() ==
                    QStringLiteral("MULTI-CONFLICT-FOLDER"))
                {
                    foundInRoot = true;
                    break;
                }
            }
            if (foundInRoot)
            {
                qWarning() << "[clipboard-smoke] FAIL: cut folder still in root (not moved)";
                return 1;
            }

            QString movedFolderId;
            for (const auto& f : destSubs)
            {
                if (f.toMap().value(QStringLiteral("name")).toString() ==
                    QStringLiteral("MULTI-CONFLICT-FOLDER"))
                {
                    movedFolderId = f.toMap().value(QStringLiteral("id")).toString();
                    break;
                }
            }
            const auto movedScores = libraryService.scoresInFolder(movedFolderId);
            if (movedScores.size() != 2)
            {
                qWarning() << "[clipboard-smoke] FAIL: moved folder expected 2 scores, got"
                           << movedScores.size() << "(file lost!)";
                return 1;
            }
        }

        qWarning() << "[clipboard-smoke] PASS: multi-copy, conflict per-item, cut, "
                      "same-folder-copy-conflict-dialog, nested-same-folder-copy-conflict, "
                      "cross-folder-folder-conflict, cut-folder-no-loss all work";
        return 0;
    }

    QTemporaryFile importSmokeFile;
    if (arguments.contains(QStringLiteral("--import-smoke-test")))
    {
        importSmokeFile.setFileTemplate(QDir::tempPath() +
                                        QStringLiteral("/notera-import-XXXXXX.png"));
        if (!importSmokeFile.open())
        {
            return 1;
        }
        const auto imagePath = importSmokeFile.fileName();
        importSmokeFile.close();
        QImage image(1754, 2480, QImage::Format_Indexed8);
        image.setColorTable({qRgb(255, 255, 255), qRgb(0, 0, 0)});
        image.fill(0);
        if (!image.save(imagePath))
        {
            return 1;
        }
        const auto previousCount = libraryService.scores()->rowCount();
        auto waitForImport = [&libraryService](int timeoutMs = 3000)
        {
            QEventLoop loop;
            QObject::connect(&libraryService, &LibraryService::importFinished, &loop,
                             &QEventLoop::quit);
            QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
            loop.exec();
        };
        libraryService.importLocalFile(QUrl::fromLocalFile(imagePath));
        waitForImport();
        if (libraryService.scores()->rowCount() != previousCount + 1)
            return 1;

        {
            QObject ctx;
            bool conflictFired = false;
            QObject::connect(&libraryService, &LibraryService::importConflict, &ctx,
                             [&](const QString&, const QString&, int, int)
                             { conflictFired = true; });
            libraryService.importLocalFile(QUrl::fromLocalFile(imagePath));
            if (!conflictFired)
                return 1;

            if (libraryService.scores()->rowCount() != previousCount + 1)
                return 1;

            libraryService.resolveImportConflict(QStringLiteral("overwrite"), false);
            waitForImport();
            if (libraryService.scores()->rowCount() != previousCount + 1)
                return 1;

            conflictFired = false;
            libraryService.importLocalFile(QUrl::fromLocalFile(imagePath));
            if (!conflictFired)
                return 1;
            libraryService.resolveImportConflict(QStringLiteral("rename"), false);
            waitForImport();
            if (libraryService.scores()->rowCount() != previousCount + 2)
                return 1;
        }
        return 0;
    }

    if (arguments.contains(QStringLiteral("--folder-import-smoke-test")))
    {
        QTemporaryDir folderImportRoot;
        if (!folderImportRoot.isValid())
            return 1;
        const auto makeImage = [](const QString& path) -> bool
        {
            QImage image(64, 64, QImage::Format_Indexed8);
            image.setColorTable({qRgb(255, 255, 255), qRgb(0, 0, 0)});
            image.fill(0);
            return image.save(path);
        };
        const auto root = folderImportRoot.path();
        const auto subA = root + QStringLiteral("/子目录A");
        const auto subB = root + QStringLiteral("/子目录B");
        const auto deep = subB + QStringLiteral("/深层");
        const auto emptyDir = root + QStringLiteral("/空目录");
        if (!makeImage(root + QStringLiteral("/根级乐谱.png")) || !QDir().mkpath(subA) ||
            !QDir().mkpath(deep) || !QDir().mkpath(emptyDir) ||
            !makeImage(subA + QStringLiteral("/乐谱A.png")) ||
            !makeImage(deep + QStringLiteral("/乐谱B.png")))
        {
            return 1;
        }

        auto waitForImport = [&libraryService](int timeoutMs = 5000)
        {
            QEventLoop loop;
            QObject::connect(&libraryService, &LibraryService::importFinished, &loop,
                             &QEventLoop::quit);
            QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
            loop.exec();
        };

        libraryService.importFolder(QUrl::fromLocalFile(root));
        waitForImport();

        // 导入的文件夹本身应作为 Notera 文件夹保留在库根下（而非平铺成单独乐谱）
        const auto rootName = QDir(root).dirName();
        QString rootFolderId;
        for (const auto& item : libraryService.childFolders(QString()))
        {
            const auto map = item.toMap();
            if (map.value(QStringLiteral("name")).toString() == rootName)
            {
                rootFolderId = map.value(QStringLiteral("id")).toString();
                break;
            }
        }
        if (rootFolderId.isEmpty())
            return 1;

        // 根文件夹下：根级乐谱 1 份 + 子目录A/子目录B；空目录不应被创建
        if (libraryService.scoresInFolder(rootFolderId).size() != 1)
            return 1;
        QString subAId, subBId;
        for (const auto& item : libraryService.childFolders(rootFolderId))
        {
            const auto map = item.toMap();
            const auto name = map.value(QStringLiteral("name")).toString();
            if (name == QStringLiteral("子目录A"))
                subAId = map.value(QStringLiteral("id")).toString();
            else if (name == QStringLiteral("子目录B"))
                subBId = map.value(QStringLiteral("id")).toString();
            else if (name == QStringLiteral("空目录"))
                return 1;
        }
        if (subAId.isEmpty() || subBId.isEmpty())
            return 1;

        // 子目录A：仅 1 份乐谱，无子文件夹
        if (libraryService.scoresInFolder(subAId).size() != 1 ||
            !libraryService.childFolders(subAId).isEmpty())
        {
            return 1;
        }

        // 子目录B → 深层：中间层目录（本身无直接乐谱）也应被保留，深层内有 1 份乐谱
        QString deepId;
        for (const auto& item : libraryService.childFolders(subBId))
        {
            const auto map = item.toMap();
            if (map.value(QStringLiteral("name")).toString() == QStringLiteral("深层"))
            {
                deepId = map.value(QStringLiteral("id")).toString();
                break;
            }
        }
        if (deepId.isEmpty() || libraryService.scoresInFolder(deepId).size() != 1)
            return 1;

        // 乐谱标题与文件名一致，证明文件归入了各自目录
        const auto rootScores = libraryService.scoresInFolder(rootFolderId);
        if (rootScores.constFirst().toMap().value(QStringLiteral("title")).toString() !=
            QStringLiteral("根级乐谱"))
        {
            return 1;
        }
        return 0;
    }

    if (arguments.contains(QStringLiteral("--tag-smoke-test")))
    {

        const auto libDir = AppDataPaths::libraryDirectory();
        const auto dbPath = AppDataPaths::databaseDirectory() + QStringLiteral("/notera.db");

        {
            const auto connection = QStringLiteral("tag_smoke_preclean");
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(dbPath);
                if (database.open())
                {
                    QSqlQuery query(database);
                    query.exec(QStringLiteral("DELETE FROM scores WHERE title LIKE 'TAG-SMOKE%'"));
                    query.exec(QStringLiteral("DELETE FROM folders WHERE name LIKE 'TAG-SMOKE%'"));
                    query.exec(QStringLiteral("DELETE FROM tags WHERE name LIKE 'TAG-SMOKE%'"));
                    database.close();
                }
            }
            QSqlDatabase::removeDatabase(connection);
        }

        QImage testImage(100, 100, QImage::Format_RGB32);
        testImage.fill(Qt::green);
        const auto file1 = libDir + QStringLiteral("/TAG-SMOKE-SCORE.png");
        if (!testImage.save(file1))
        {
            qWarning() << "[tag-smoke] FAIL: cannot save test image";
            return 1;
        }
        libraryService.createFolder(QStringLiteral("TAG-SMOKE-FOLDER"));
        libraryService.importLocalFile(QUrl::fromLocalFile(file1));
        {
            QEventLoop importLoop;
            QObject::connect(&libraryService, &LibraryService::importFinished, &importLoop,
                             &QEventLoop::quit);
            QTimer::singleShot(3000, &importLoop, &QEventLoop::quit);
            importLoop.exec();
        }
        const auto scoreId = [&dbPath]()
        {
            const auto connection = QStringLiteral("tag_smoke_lookup_score");
            QString id;
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(dbPath);
                if (database.open())
                {
                    QSqlQuery query(database);
                    if (query.exec(QStringLiteral(
                            "SELECT id FROM scores WHERE title = 'TAG-SMOKE-SCORE'")) &&
                        query.next())
                        id = query.value(0).toString();
                    database.close();
                }
            }
            QSqlDatabase::removeDatabase(connection);
            return id;
        }();
        const auto folderId = [&dbPath]()
        {
            const auto connection = QStringLiteral("tag_smoke_lookup_folder");
            QString id;
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(dbPath);
                if (database.open())
                {
                    QSqlQuery query(database);
                    if (query.exec(QStringLiteral(
                            "SELECT id FROM folders WHERE name = 'TAG-SMOKE-FOLDER'")) &&
                        query.next())
                        id = query.value(0).toString();
                    database.close();
                }
            }
            QSqlDatabase::removeDatabase(connection);
            return id;
        }();
        if (scoreId.isEmpty() || folderId.isEmpty())
        {
            qWarning() << "[tag-smoke] FAIL: cannot create test score/folder (scoreId=" << scoreId
                       << " folderId=" << folderId << ")";
            return 1;
        }

        libraryService.createTag(QStringLiteral("TAG-SMOKE-A"));
        if (libraryService.tags()->count() != 1)
        {
            qWarning() << "[tag-smoke] FAIL: createTag did not create exactly 1 tag, got"
                       << libraryService.tags()->count();
            return 1;
        }
        const auto tagA =
            libraryService.tags()->get(0).toMap().value(QStringLiteral("itemId")).toString();
        if (tagA.isEmpty())
            return 1;

        {
            QObject ctx;
            QString errorMessage;
            QObject::connect(&libraryService, &LibraryService::errorOccurred, &ctx,
                             [&](const QString& m) { errorMessage = m; });
            libraryService.createTag(QStringLiteral("TAG-SMOKE-A"));
            if (libraryService.tags()->count() != 1)
            {
                qWarning() << "[tag-smoke] FAIL: exact duplicate createTag created extra tag";
                return 1;
            }
            if (errorMessage.isEmpty())
            {
                qWarning()
                    << "[tag-smoke] FAIL: exact duplicate createTag should reject with error";
                return 1;
            }
        }

        {
            QObject ctx;
            QString errorMessage;
            QObject::connect(&libraryService, &LibraryService::errorOccurred, &ctx,
                             [&](const QString& m) { errorMessage = m; });
            libraryService.createTag(QStringLiteral("tag-smoke-a"));
            if (libraryService.tags()->count() != 1)
            {
                qWarning() << "[tag-smoke] FAIL: case-insensitive duplicate createTag should be "
                              "rejected, got"
                           << libraryService.tags()->count();
                return 1;
            }
        }

        libraryService.createTag(QStringLiteral("TAG-SMOKE-B"));
        if (libraryService.tags()->count() != 2)
            return 1;
        const auto tagB =
            libraryService.tags()->get(1).toMap().value(QStringLiteral("itemId")).toString();
        {
            QObject ctx;
            QString errorMessage;
            QObject::connect(&libraryService, &LibraryService::errorOccurred, &ctx,
                             [&](const QString& m) { errorMessage = m; });
            libraryService.renameTag(tagB, QStringLiteral("TAG-SMOKE-A"));
            if (errorMessage.isEmpty())
            {
                qWarning() << "[tag-smoke] FAIL: renameTag to existing name should reject";
                return 1;
            }
        }

        {
            QObject ctx;
            QString errorMessage;
            QObject::connect(&libraryService, &LibraryService::errorOccurred, &ctx,
                             [&](const QString& m) { errorMessage = m; });
            libraryService.renameTag(tagB, QStringLiteral("tag-smoke-a"));
            if (errorMessage.isEmpty())
            {
                qWarning()
                    << "[tag-smoke] FAIL: renameTag to case-variant of existing tag should reject";
                return 1;
            }
        }

        {
            QString error;
            const auto tags =
                libraryService.tags()->get(0).toMap().value(QStringLiteral("itemId")).toString();
            (void)tags;
            bool foundB = false;
            for (int i = 0; i < libraryService.tags()->count(); ++i)
            {
                const auto t = libraryService.tags()->get(i).toMap();
                if (t.value(QStringLiteral("itemId")).toString() == tagB &&
                    t.value(QStringLiteral("name")).toString() == QStringLiteral("TAG-SMOKE-B"))
                    foundB = true;
            }
            if (!foundB)
            {
                qWarning() << "[tag-smoke] FAIL: renameTag to duplicate changed the tag name";
                return 1;
            }
        }

        libraryService.addItemTag(scoreId, tagA);
        if (!libraryService.itemHasTag(scoreId, tagA))
        {
            qWarning() << "[tag-smoke] FAIL: addItemTag score";
            return 1;
        }
        libraryService.addItemTag(folderId, tagA);
        if (!libraryService.itemHasTag(folderId, tagA))
        {
            qWarning() << "[tag-smoke] FAIL: addItemTag folder";
            return 1;
        }

        libraryService.removeItemTag(scoreId, tagA);
        if (libraryService.itemHasTag(scoreId, tagA))
        {
            qWarning() << "[tag-smoke] FAIL: removeItemTag score";
            return 1;
        }

        libraryService.addItemTag(scoreId, tagA);
        libraryService.setFilterMode(QStringLiteral("tag:") + tagA);
        {
            const auto ids = libraryService.entries()->itemIds();
            if (!ids.contains(scoreId) || !ids.contains(folderId))
            {
                qWarning() << "[tag-smoke] FAIL: tag filter should show tagged score and folder";
                return 1;
            }
        }
        libraryService.setFilterMode(QStringLiteral("all"));

        libraryService.deleteTag(tagA);
        if (libraryService.itemHasTag(scoreId, tagA) || libraryService.itemHasTag(folderId, tagA))
        {
            qWarning() << "[tag-smoke] FAIL: deleteTag should cascade-remove associations";
            return 1;
        }
        if (libraryService.tags()->count() != 1)
        {
            qWarning() << "[tag-smoke] FAIL: deleteTag should leave only TAG-SMOKE-B, got"
                       << libraryService.tags()->count();
            return 1;
        }

        libraryService.deleteTag(tagB);
        qWarning() << "[tag-smoke] PASS: "
                      "create/reject-duplicate/case-insensitive/add/remove/filter/delete all work";
        return 0;
    }

    if (arguments.contains(QStringLiteral("--stitch-smoke-test")))
    {
        QTemporaryDir directory(QDir::tempPath() + QStringLiteral("/notera-stitch-XXXXXX"));
        if (!directory.isValid())
        {
            return 1;
        }
        const auto firstPath = directory.filePath(QStringLiteral("first.png"));
        const auto secondPath = directory.filePath(QStringLiteral("second.png"));
        QImage first(320, 480, QImage::Format_RGB32);
        QImage second(400, 360, QImage::Format_RGB32);
        first.fill(Qt::white);
        second.fill(Qt::lightGray);
        if (!first.save(firstPath) || !second.save(secondPath))
        {
            return 1;
        }
        const auto previousCount = libraryService.scores()->rowCount();
        auto waitForStitchImport = [&libraryService](int timeoutMs = 3000)
        {
            QEventLoop loop;
            QObject::connect(&libraryService, &LibraryService::importFinished, &loop,
                             &QEventLoop::quit);
            QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
            loop.exec();
        };
        libraryService.importAndStitchImages({QUrl::fromLocalFile(firstPath).toString(),
                                              QUrl::fromLocalFile(secondPath).toString()});
        waitForStitchImport();
        if (libraryService.scores()->rowCount() != previousCount + 1)
            return 1;

        {
            QObject ctx;
            bool conflictFired = false;
            QObject::connect(&libraryService, &LibraryService::importConflict, &ctx,
                             [&](const QString&, const QString&, int, int)
                             { conflictFired = true; });
            libraryService.importAndStitchImages({QUrl::fromLocalFile(firstPath).toString(),
                                                  QUrl::fromLocalFile(secondPath).toString()});
            if (!conflictFired)
            {
                qWarning()
                    << "[stitch-smoke] FAIL: duplicate stitch import should fire import conflict";
                return 1;
            }

            if (libraryService.scores()->rowCount() != previousCount + 1)
                return 1;
            libraryService.resolveImportConflict(QStringLiteral("rename"), false);
            waitForStitchImport();
            if (libraryService.scores()->rowCount() != previousCount + 2)
                return 1;
        }
        return 0;
    }

    QString renameSmokeFolderId;
    if (arguments.contains(QStringLiteral("--folder-rename-smoke-test")))
    {
        libraryService.createFolder(QStringLiteral("重命名前"));
        if (libraryService.folders()->rowCount() != 1)
            return 1;
        renameSmokeFolderId =
            libraryService.folders()->get(0).toMap().value(QStringLiteral("itemId")).toString();
        if (renameSmokeFolderId.isEmpty())
            return 1;
    }

    Notera::PdfRenderService pdfRenderService;

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("pdfcache"),
                            new Notera::PdfCacheImageProvider(pdfRenderService.cache()));
    engine.rootContext()->setContextProperty(QStringLiteral("pdfRender"), &pdfRenderService);

    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("libraryService"), &libraryService);
    engine.rootContext()->setContextProperty(QStringLiteral("metronome"), &metronome);
    engine.loadFromModule("Notera", "Main");

    if (engine.rootObjects().isEmpty())
    {
        return 1;
    }

    if (singleInstance)
    {
        singleInstance->setActivateCallback(
            [&engine]()
            {
                for (QObject* const obj : engine.rootObjects())
                {
                    if (auto* const window = qobject_cast<QQuickWindow*>(obj))
                    {
                        window->show();
                        window->raise();
                        window->requestActivate();
                    }
                }
            });
    }

    if (arguments.contains(QStringLiteral("--folder-rename-smoke-test")))
    {
        auto* const root = engine.rootObjects().constFirst();
        QTimer::singleShot(
            250, root,
            [root, renameSmokeFolderId, &libraryService]
            {
                auto findFolderNavItem = [root, &renameSmokeFolderId]() -> QObject*
                {
                    auto* const sidebar = findVisualItem(root, QStringLiteral("sidebar"));
                    if (!sidebar)
                        return nullptr;
                    const auto visit = [&](auto&& self, QQuickItem* item) -> QObject*
                    {
                        if (item->objectName() == QStringLiteral("folderNavItem") &&
                            item->property("navId").toString() ==
                                QStringLiteral("folder:") + renameSmokeFolderId)
                        {
                            return item;
                        }
                        for (auto* const child : item->childItems())
                        {
                            if (auto* const match = self(self, child))
                                return match;
                        }
                        return nullptr;
                    };
                    return visit(visit, sidebar);
                };
                auto* const before = findFolderNavItem();
                if (!before || before->property("label").toString() != QStringLiteral("重命名前"))
                {
                    QCoreApplication::exit(1);
                    return;
                }
                libraryService.renameFolder(renameSmokeFolderId, QStringLiteral("重命名后"));
                QCoreApplication::processEvents();
                auto* const after = findFolderNavItem();
                QCoreApplication::exit(
                    after && after->property("label").toString() == QStringLiteral("重命名后") ? 0
                                                                                               : 1);
            });
    }

    if (arguments.contains(QStringLiteral("--theme-smoke-test")))
    {
        auto* const root = engine.rootObjects().constFirst();
        auto* const selectionBox = findVisualItem(root, QStringLiteral("selectionBox"));
        if (!selectionBox)
            return 1;
        const auto originalMode = controller.themeMode();
        controller.setThemeMode(0);
        QCoreApplication::processEvents();
        const auto lightBackground = root->property("themeBackground").value<QColor>();
        const auto lightMarqueeFill = selectionBox->property("color").value<QColor>();
        const auto lightMarqueeBorderColor =
            selectionBox->property("appliedBorderColor").value<QColor>();
        controller.setThemeMode(1);
        QCoreApplication::processEvents();
        const auto darkBackground = root->property("themeBackground").value<QColor>();
        const auto darkMarqueeFill = selectionBox->property("color").value<QColor>();
        const auto darkMarqueeBorderColor =
            selectionBox->property("appliedBorderColor").value<QColor>();
        controller.setThemeMode(originalMode);
        const auto marqueeColorsAreThemeIndependent =
            lightMarqueeFill.isValid() && lightMarqueeBorderColor.isValid() &&
            lightMarqueeFill == darkMarqueeFill &&
            lightMarqueeBorderColor == darkMarqueeBorderColor;
        const auto marqueeIsMoreTransparent =
            lightMarqueeFill.alphaF() > 0.0 && lightMarqueeFill.alphaF() < (0x22 / 255.0);
        return lightBackground.isValid() && darkBackground.isValid() &&
                       lightBackground != darkBackground && marqueeColorsAreThemeIndependent &&
                       marqueeIsMoreTransparent
                   ? 0
                   : 1;
    }

    QTemporaryFile readerSmokeFile;
    if (arguments.contains(QStringLiteral("--reader-smoke-test")))
    {
        readerSmokeFile.setFileTemplate(QDir::tempPath() +
                                        QStringLiteral("/notera-reader-XXXXXX.png"));
        if (!readerSmokeFile.open())
        {
            return 1;
        }
        const auto imagePath = readerSmokeFile.fileName();
        readerSmokeFile.close();
        QImage image(400, 2400, QImage::Format_RGB32);
        image.fill(Qt::white);
        if (!image.save(imagePath))
        {
            return 1;
        }
        controller.openScore(QStringLiteral("test-score-1"), QStringLiteral("自动滚动测试"),
                             imagePath, QStringLiteral("png"), 1, QString());
        auto* const root = engine.rootObjects().constFirst();
        QTimer::singleShot(250, root,
                           [root]
                           {
                               if (auto* const readerPage =
                                       root->findChild<QObject*>(QStringLiteral("readerPage")))
                               {
                                   readerPage->setProperty("scrollSpeed", 160.0);
                                   readerPage->setProperty("autoScrolling", true);
                               }
                           });
        QTimer::singleShot(
            1250, root,
            [root]
            {
                const auto* const imageFlick =
                    root->findChild<QObject*>(QStringLiteral("imageFlick"));
                QCoreApplication::exit(
                    imageFlick && imageFlick->property("contentY").toDouble() > 0.0 ? 0 : 1);
            });
    }

    QTemporaryFile uiSmokeFile;
    QTemporaryFile uiSmokeSecondFile;
    if (arguments.contains(QStringLiteral("--ui-smoke-test")))
    {
        uiSmokeFile.setFileTemplate(QDir::tempPath() + QStringLiteral("/notera-ui-z-XXXXXX.png"));
        uiSmokeSecondFile.setFileTemplate(QDir::tempPath() +
                                          QStringLiteral("/notera-ui-a-XXXXXX.png"));
        if (!uiSmokeFile.open() || !uiSmokeSecondFile.open())
        {
            return 1;
        }
        const auto imagePath = uiSmokeFile.fileName();
        const auto secondImagePath = uiSmokeSecondFile.fileName();
        uiSmokeFile.close();
        uiSmokeSecondFile.close();
        QImage image(900, 1280, QImage::Format_RGB32);
        image.fill(Qt::white);
        if (!image.save(imagePath) || !image.save(secondImagePath))
        {
            return 1;
        }
        libraryService.importFiles({QVariant::fromValue(QUrl::fromLocalFile(imagePath)),
                                    QVariant::fromValue(QUrl::fromLocalFile(secondImagePath))});
        {
            QEventLoop importLoop;
            QObject::connect(&libraryService, &LibraryService::importFinished, &importLoop,
                             &QEventLoop::quit);
            QTimer::singleShot(5000, &importLoop, &QEventLoop::quit);
            importLoop.exec();
        }
        libraryService.createFolder(QStringLiteral("Z界面测试文件夹"));
        libraryService.createFolder(QStringLiteral("A界面测试文件夹"));
        libraryService.createTag(QStringLiteral("界面测试标签"));

        auto* const root = engine.rootObjects().constFirst();
        QTimer::singleShot(
            300, root,
            [root, &controller, &libraryService]
            {
                const auto fail = [](const char* step)
                {
                    qWarning() << "UI smoke test failed at" << step;
                    QCoreApplication::exit(1);
                };
                auto* const importButton =
                    root->findChild<QQuickItem*>(QStringLiteral("importButton"));
                if (!importButton || !importButton->isVisible() || importButton->width() < 96.0 ||
                    importButton->property("symbol").toString().length() > 0 ||
                    importButton->property("hoverTransitionDuration").toInt() != 0 ||
                    std::abs(importButton->property("visualContentCenterX").toDouble() -
                             importButton->width() / 2.0) > 1.0)
                {
                    fail("import-button-geometry");
                    return;
                }

                auto* const importMenu = root->findChild<QObject*>(QStringLiteral("importMenu"));
                if (!importMenu ||
                    !clickItem(root, QStringLiteral("importButton"), Qt::LeftButton) ||
                    !importMenu->property("visible").toBool() ||
                    importMenu->property("count").toInt() < 3)
                {
                    fail("import-menu-opens-from-button");
                    return;
                }
                closePopup(root, QStringLiteral("importMenu"));
                auto* const window = qobject_cast<QQuickWindow*>(root);
                if (!window ||
                    !window->grabWindow().save(QStringLiteral("notera-library-smoke.png")))
                {
                    fail("library-screenshot");
                    return;
                }

                const auto* const entryCheckBox =
                    findVisualItem(root, QStringLiteral("entryCheckBox"));
                const auto* const favoriteButton =
                    findVisualItem(root, QStringLiteral("favoriteButton"));
                if (!entryCheckBox || !entryCheckBox->isVisible() ||
                    !clickItem(root, QStringLiteral("entryCheckBox"), Qt::LeftButton) ||
                    libraryService.selection()->count() != 1)
                {
                    fail("library-entry-checkbox-selection");
                    return;
                }
                libraryService.selection()->clear();
                if (!favoriteButton ||
                    std::abs(entryCheckBox
                                 ->mapToScene(QPointF(entryCheckBox->width() / 2.0,
                                                      entryCheckBox->height() / 2.0))
                                 .y() -
                             favoriteButton
                                 ->mapToScene(QPointF(favoriteButton->width() / 2.0,
                                                      favoriteButton->height() / 2.0))
                                 .y()) > 1.0)
                {
                    fail("library-card-actions-alignment");
                    return;
                }

                const auto* const libraryNavItem =
                    root->findChild<QObject*>(QStringLiteral("libraryNavItem"));
                if (!libraryNavItem ||
                    !libraryNavItem->property("hoverTransitionDuration").isValid() ||
                    libraryNavItem->property("hoverTransitionDuration").toInt() != 0)
                {
                    fail("sidebar-hover-transition");
                    return;
                }
                const auto* const appShell = root->findChild<QObject*>(QStringLiteral("appShell"));
                const auto transitionCountBeforeFilter =
                    appShell ? appShell->property("transitionRunCount").toInt() : -1;
                controller.setLibraryFilter(QStringLiteral("recent"));
                QCoreApplication::processEvents();
                controller.setLibraryFilter(QStringLiteral("all"));
                QCoreApplication::processEvents();
                if (!appShell || appShell->property("transitionDuration").toInt() <= 0 ||
                    appShell->property("transitionRunCount").toInt() <
                        transitionCountBeforeFilter + 2)
                {
                    fail("library-section-transition");
                    return;
                }
                if (QGuiApplication::windowIcon().isNull())
                {
                    fail("application-icon");
                    return;
                }

                const auto entryTypeRole =
                    libraryService.entries()->roleNames().key("itemType", -1);
                const auto entryTitleRole = libraryService.entries()->roleNames().key("title", -1);
                if (libraryService.entries()->rowCount() != 4 ||
                    libraryService.entries()
                            ->data(libraryService.entries()->index(0, 0), entryTypeRole)
                            .toString() != QStringLiteral("folder") ||
                    !libraryService.entries()
                         ->data(libraryService.entries()->index(0, 0), entryTitleRole)
                         .toString()
                         .startsWith(QStringLiteral("A")) ||
                    !libraryService.entries()
                         ->data(libraryService.entries()->index(1, 0), entryTitleRole)
                         .toString()
                         .startsWith(QStringLiteral("Z")) ||
                    !libraryService.entries()
                         ->data(libraryService.entries()->index(2, 0), entryTitleRole)
                         .toString()
                         .startsWith(QStringLiteral("notera-ui-a-")) ||
                    !libraryService.entries()
                         ->data(libraryService.entries()->index(3, 0), entryTitleRole)
                         .toString()
                         .startsWith(QStringLiteral("notera-ui-z-")))
                {
                    fail("library-folder-first-file-name-sort");
                    return;
                }

                auto* const libraryPage = root->findChild<QObject*>(QStringLiteral("libraryPage"));
                auto* const librarySurface = findVisualItem(root, QStringLiteral("librarySurface"));
                auto* const rubberSelectionGrid =
                    findVisualItem(root, QStringLiteral("browserGrid"));
                auto* const selectionBox = findVisualItem(root, QStringLiteral("selectionBox"));
                auto* const rubberBandHandler =
                    root->findChild<QObject*>(QStringLiteral("gridRubberBand"));
                const auto acceptedSelectionDevices =
                    rubberBandHandler ? rubberBandHandler->property("acceptedDevices").toInt() : 0;
                const auto mouseAndTouchPad = static_cast<int>(QInputDevice::DeviceType::Mouse) |
                                              static_cast<int>(QInputDevice::DeviceType::TouchPad);
                if (!libraryPage || !librarySurface || !rubberSelectionGrid || !selectionBox ||
                    !window || !rubberBandHandler ||
                    rubberBandHandler->parent() != librarySurface ||
                    (acceptedSelectionDevices & mouseAndTouchPad) != mouseAndTouchPad)
                {
                    fail("library-rubber-selection-objects");
                    return;
                }
                selectionBox->setX(0);
                selectionBox->setY(0);
                selectionBox->setWidth(librarySurface->width());
                selectionBox->setHeight(librarySurface->height());
                QMetaObject::invokeMethod(libraryPage, "updateRubberSelection");
                if (libraryService.selection()->count() != libraryService.entries()->count())
                {
                    fail("library-rubber-selection-expand");
                    return;
                }
                selectionBox->setWidth(0);
                selectionBox->setHeight(0);
                QMetaObject::invokeMethod(libraryPage, "updateRubberSelection");
                if (libraryService.selection()->count() != 0)
                {
                    fail("library-rubber-selection-shrink");
                    return;
                }

                const auto rubberStart = rubberSelectionGrid->mapToScene(QPointF(
                    rubberSelectionGrid->width() * 0.62, rubberSelectionGrid->height() * 0.72));
                const auto rubberEnd = rubberStart + QPointF(120.0, 90.0);
                sendMouseEvent(window, QEvent::MouseButtonPress, rubberStart, Qt::LeftButton,
                               Qt::LeftButton);
                sendMouseEvent(window, QEvent::MouseMove, rubberEnd, Qt::NoButton, Qt::LeftButton);
                const auto expectedStart = librarySurface->mapFromScene(rubberStart);

                bool rubberOriginReady = false;
                for (int attempt = 0; attempt < 40; ++attempt)
                {
                    if (selectionBox->isVisible() &&
                        std::abs(selectionBox->x() - expectedStart.x()) <= 4.0 &&
                        std::abs(selectionBox->y() - expectedStart.y()) <= 4.0)
                    {
                        rubberOriginReady = true;
                        break;
                    }
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
                    QThread::msleep(10);
                }
                if (!rubberOriginReady)
                {
                    fail("library-rubber-selection-real-pointer-origin");
                    return;
                }
                sendMouseEvent(window, QEvent::MouseButtonRelease, rubberEnd, Qt::LeftButton,
                               Qt::NoButton);

                auto* const firstEntryDelegate =
                    findVisualItem(root, QStringLiteral("folderDelegate"));
                if (!firstEntryDelegate)
                {
                    fail("library-rubber-selection-delegate-gap-object");
                    return;
                }
                const auto occupiedCellGap = firstEntryDelegate->mapToScene(QPointF(2.0, 2.0));
                sendMouseEvent(window, QEvent::MouseButtonPress, occupiedCellGap, Qt::LeftButton,
                               Qt::LeftButton);
                sendMouseEvent(window, QEvent::MouseMove, occupiedCellGap + QPointF(80.0, 60.0),
                               Qt::NoButton, Qt::LeftButton);
                if (!selectionBox->isVisible())
                {
                    fail("library-rubber-selection-allows-surface-gap");
                    return;
                }
                sendMouseEvent(window, QEvent::MouseButtonRelease,
                               occupiedCellGap + QPointF(80.0, 60.0), Qt::LeftButton, Qt::NoButton);

                const auto outsideSurface =
                    librarySurface->mapToScene(QPointF(librarySurface->width() / 2.0, -12.0));
                sendMouseEvent(window, QEvent::MouseButtonPress, outsideSurface, Qt::LeftButton,
                               Qt::LeftButton);
                sendMouseEvent(window, QEvent::MouseMove, outsideSurface + QPointF(80.0, 40.0),
                               Qt::NoButton, Qt::LeftButton);
                if (selectionBox->isVisible())
                {
                    fail("library-rubber-selection-rejects-outside-surface");
                    return;
                }
                sendMouseEvent(window, QEvent::MouseButtonRelease,
                               outsideSurface + QPointF(80.0, 40.0), Qt::LeftButton, Qt::NoButton);

                const auto selectStart = rubberSelectionGrid->mapToScene(QPointF(
                    rubberSelectionGrid->width() * 0.9, rubberSelectionGrid->height() * 0.75));
                const auto selectEnd = rubberSelectionGrid->mapToScene(QPointF(
                    rubberSelectionGrid->width() * 0.05, rubberSelectionGrid->height() * 0.05));
                sendMouseEvent(window, QEvent::MouseButtonPress, selectStart, Qt::LeftButton,
                               Qt::LeftButton);
                sendMouseEvent(window, QEvent::MouseMove, selectEnd, Qt::NoButton, Qt::LeftButton);
                sendMouseEvent(window, QEvent::MouseButtonRelease, selectEnd, Qt::LeftButton,
                               Qt::NoButton);
                if (libraryService.selection()->count() == 0)
                {
                    fail("library-rubber-selection-persists-after-release");
                    return;
                }
                libraryService.selection()->clear();

                {
                    const int baseEntryCount = libraryService.entries()->rowCount();

                    QTemporaryDir rollDir;
                    if (!rollDir.isValid())
                        return;
                    QStringList extraPaths;
                    for (int i = 0; i < 10; ++i)
                    {
                        const auto extraPath =
                            rollDir.filePath(QStringLiteral("notera-roll-%1.png").arg(i));
                        QImage extraImage(900, 1280, QImage::Format_RGB32);
                        extraImage.fill(Qt::white);
                        if (!extraImage.save(extraPath))
                            return;
                        extraPaths.append(extraPath);
                    }
                    QVariantList extraUrls;
                    for (const auto& path : extraPaths)
                        extraUrls.append(QVariant::fromValue(QUrl::fromLocalFile(path)));
                    libraryService.importFiles(extraUrls);
                    {
                        QEventLoop importLoop;
                        QObject::connect(&libraryService, &LibraryService::importFinished,
                                         &importLoop, &QEventLoop::quit);
                        QTimer::singleShot(8000, &importLoop, &QEventLoop::quit);
                        importLoop.exec();
                    }
                    if (libraryService.entries()->rowCount() <= baseEntryCount)
                    {
                        fail("rubber-scroll-regression-import");
                        return;
                    }

                    selectionBox->setX(0);
                    selectionBox->setY(0);
                    selectionBox->setWidth(librarySurface->width());
                    selectionBox->setHeight(rubberSelectionGrid->property("cellHeight").toDouble());
                    QMetaObject::invokeMethod(libraryPage, "updateRubberSelection");
                    const auto selectedBeforeScroll = libraryService.selection()->selectedIds();
                    if (selectedBeforeScroll.isEmpty())
                    {
                        fail("rubber-scroll-regression-preselect");
                        return;
                    }

                    const auto maxContentY =
                        qMax<qreal>(0.0, rubberSelectionGrid->property("contentHeight").toDouble() -
                                             rubberSelectionGrid->property("height").toDouble());
                    rubberSelectionGrid->setProperty("contentY", maxContentY);
                    for (int i = 0; i < 5; ++i)
                        QCoreApplication::processEvents();

                    QMetaObject::invokeMethod(libraryPage, "updateRubberSelection");
                    const auto selectedAfterScroll = libraryService.selection()->selectedIds();
                    for (const auto& id : selectedBeforeScroll)
                    {
                        if (!selectedAfterScroll.contains(id))
                        {
                            fail("rubber-selection-kept-after-scroll");
                            return;
                        }
                    }
                    libraryService.selection()->clear();

                    {
                        const auto maxContentY = qMax<qreal>(
                            0.0, rubberSelectionGrid->property("contentHeight").toDouble() -
                                     rubberSelectionGrid->property("height").toDouble());
                        if (maxContentY <= 0.0)
                        {
                            fail("wheel-clamp-grid-not-scrollable");
                            return;
                        }
                        const auto gridCenter = rubberSelectionGrid->mapToScene(
                            QPointF(rubberSelectionGrid->width() / 2.0,
                                    rubberSelectionGrid->height() / 2.0));
                        const auto globalCenter =
                            QPointF(window->mapToGlobal(gridCenter.toPoint()));

                        sendMouseEvent(window, QEvent::MouseMove, gridCenter, Qt::NoButton,
                                       Qt::NoButton);

                        rubberSelectionGrid->setProperty("contentY", 0.0);
                        for (int i = 0; i < 5; ++i)
                            QCoreApplication::processEvents();
                        QWheelEvent upEvent(gridCenter, globalCenter, QPoint(0, 0), QPoint(0, 240),
                                            Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
                        QCoreApplication::sendEvent(window, &upEvent);
                        QCoreApplication::processEvents();
                        if (rubberSelectionGrid->property("contentY").toDouble() < 0.0)
                        {
                            fail("wheel-clamp-top");
                            return;
                        }

                        rubberSelectionGrid->setProperty("contentY", maxContentY);
                        for (int i = 0; i < 5; ++i)
                            QCoreApplication::processEvents();
                        QWheelEvent downEvent(gridCenter, globalCenter, QPoint(0, 0),
                                              QPoint(0, -240), Qt::NoButton, Qt::NoModifier,
                                              Qt::NoScrollPhase, false);
                        QCoreApplication::sendEvent(window, &downEvent);
                        QCoreApplication::processEvents();
                        if (rubberSelectionGrid->property("contentY").toDouble() >
                            maxContentY + 0.5)
                        {
                            fail("wheel-clamp-bottom");
                            return;
                        }

                        rubberSelectionGrid->setProperty("contentY", 0.0);
                        for (int i = 0; i < 5; ++i)
                            QCoreApplication::processEvents();
                    }

                    const auto entryIdRole =
                        libraryService.entries()->roleNames().key("itemId", -1);
                    const auto rollTitleRole =
                        libraryService.entries()->roleNames().key("title", -1);
                    QVariantList extraIds;
                    for (int i = 0; i < libraryService.entries()->rowCount(); ++i)
                    {
                        const auto idx = libraryService.entries()->index(i, 0);
                        if (libraryService.entries()
                                ->data(idx, rollTitleRole)
                                .toString()
                                .startsWith(QStringLiteral("notera-roll-")))
                        {
                            extraIds.append(libraryService.entries()->data(idx, entryIdRole));
                        }
                    }
                    if (!extraIds.isEmpty())
                        libraryService.deleteItems(extraIds);
                    if (libraryService.entries()->rowCount() != baseEntryCount)
                    {
                        fail("rubber-scroll-regression-cleanup");
                        return;
                    }
                }

                {
                    const int baseEntryCount = libraryService.entries()->rowCount();
                    const int baseFolderCount = libraryService.folders()->rowCount();
                    QTemporaryDir folderRoot;
                    if (!folderRoot.isValid())
                        return;
                    const auto writeImageFile = [](const QString& path)
                    {
                        QImage img(64, 64, QImage::Format_RGB32);
                        img.fill(Qt::white);
                        return img.save(path);
                    };
                    QDir().mkpath(folderRoot.filePath(QStringLiteral("sub/deep")));
                    QDir().mkpath(folderRoot.filePath(QStringLiteral("empty")));
                    if (!writeImageFile(folderRoot.filePath(QStringLiteral("a.png"))) ||
                        !writeImageFile(folderRoot.filePath(QStringLiteral("sub/b.png"))) ||
                        !writeImageFile(folderRoot.filePath(QStringLiteral("sub/deep/c.jpg"))))
                    {
                        return;
                    }
                    {
                        QFile ignored(folderRoot.filePath(QStringLiteral("ignore.txt")));
                        if (!ignored.open(QIODevice::WriteOnly) || ignored.write("x") != 1)
                            return;
                        QFile placeholder(
                            folderRoot.filePath(QStringLiteral("empty/placeholder.txt")));
                        if (!placeholder.open(QIODevice::WriteOnly) || placeholder.write("x") != 1)
                            return;
                    }
                    libraryService.importFolder(
                        QVariant::fromValue(QUrl::fromLocalFile(folderRoot.path())));
                    bool importDone = false;
                    {
                        QEventLoop importLoop;
                        QObject::connect(&libraryService, &LibraryService::importFinished,
                                         &importLoop,
                                         [&]
                                         {
                                             importDone = true;
                                             importLoop.quit();
                                         });
                        QTimer::singleShot(8000, &importLoop, &QEventLoop::quit);
                        importLoop.exec();
                    }
                    if (!importDone)
                    {
                        fail("folder-import-timeout");
                        return;
                    }
                    const auto entryTypeRoleF =
                        libraryService.entries()->roleNames().key("itemType", -1);
                    const auto entryTitleRoleF =
                        libraryService.entries()->roleNames().key("title", -1);
                    const auto entryIdRoleF =
                        libraryService.entries()->roleNames().key("itemId", -1);
                    const auto importRootName = QDir(folderRoot.path()).dirName();
                    QString importRootFolderId;
                    QStringList rootTitles;
                    for (int i = 0; i < libraryService.entries()->rowCount(); ++i)
                    {
                        const auto idx = libraryService.entries()->index(i, 0);
                        const auto title =
                            libraryService.entries()->data(idx, entryTitleRoleF).toString();
                        rootTitles << title;
                        if (libraryService.entries()->data(idx, entryTypeRoleF).toString() ==
                                QStringLiteral("folder") &&
                            title == importRootName)
                        {
                            importRootFolderId =
                                libraryService.entries()->data(idx, entryIdRoleF).toString();
                        }
                    }
                    // 导入的文件夹本身应保留在库根下，内部内容不应平铺出来
                    if (importRootFolderId.isEmpty() || rootTitles.contains(QStringLiteral("a")) ||
                        rootTitles.contains(QStringLiteral("b")) ||
                        rootTitles.contains(QStringLiteral("sub")) ||
                        rootTitles.contains(QStringLiteral("empty")) ||
                        rootTitles.contains(QStringLiteral("ignore")))
                    {
                        fail("folder-import-hierarchy-root");
                        return;
                    }

                    libraryService.setFilterMode(QStringLiteral("folder:") + importRootFolderId);
                    {
                        QStringList importRootTitles;
                        QString subFolderId;
                        for (int i = 0; i < libraryService.entries()->rowCount(); ++i)
                        {
                            const auto idx = libraryService.entries()->index(i, 0);
                            const auto title =
                                libraryService.entries()->data(idx, entryTitleRoleF).toString();
                            importRootTitles << title;
                            if (libraryService.entries()->data(idx, entryTypeRoleF).toString() ==
                                    QStringLiteral("folder") &&
                                title == QStringLiteral("sub"))
                            {
                                subFolderId =
                                    libraryService.entries()->data(idx, entryIdRoleF).toString();
                            }
                        }
                        if (subFolderId.isEmpty() || !importRootTitles.contains(QStringLiteral("a")))
                        {
                            fail("folder-import-hierarchy-root-content");
                            return;
                        }

                        libraryService.setFilterMode(QStringLiteral("folder:") + subFolderId);
                        {
                            QStringList subTitles;
                            QString deepFolderId;
                            for (int i = 0; i < libraryService.entries()->rowCount(); ++i)
                            {
                                const auto idx = libraryService.entries()->index(i, 0);
                                const auto title =
                                    libraryService.entries()->data(idx, entryTitleRoleF).toString();
                                subTitles << title;
                                if (libraryService.entries()->data(idx, entryTypeRoleF).toString() ==
                                        QStringLiteral("folder") &&
                                    title == QStringLiteral("deep"))
                                {
                                    deepFolderId =
                                        libraryService.entries()->data(idx, entryIdRoleF).toString();
                                }
                            }
                            if (deepFolderId.isEmpty() || !subTitles.contains(QStringLiteral("b")))
                            {
                                fail("folder-import-hierarchy-sub");
                                return;
                            }

                            libraryService.setFilterMode(QStringLiteral("folder:") + deepFolderId);
                            {
                                QStringList deepTitles;
                                for (int i = 0; i < libraryService.entries()->rowCount(); ++i)
                                {
                                    deepTitles << libraryService.entries()
                                                      ->data(libraryService.entries()->index(i, 0),
                                                             entryTitleRoleF)
                                                      .toString();
                                }
                                if (!deepTitles.contains(QStringLiteral("c")))
                                {
                                    fail("folder-import-hierarchy-deep");
                                    return;
                                }
                            }
                        }
                    }

                    libraryService.setFilterMode(QStringLiteral("all"));
                    {
                        QVariantList importedIds;
                        for (int i = 0; i < libraryService.entries()->rowCount(); ++i)
                        {
                            const auto idx = libraryService.entries()->index(i, 0);
                            if (libraryService.entries()
                                    ->data(idx, entryIdRoleF)
                                    .toString() == importRootFolderId)
                            {
                                importedIds.append(
                                    libraryService.entries()->data(idx, entryIdRoleF));
                            }
                        }
                        if (!importedIds.isEmpty())
                            libraryService.deleteItems(importedIds);
                    }
                    if (libraryService.entries()->rowCount() != baseEntryCount ||
                        libraryService.folders()->rowCount() != baseFolderCount)
                    {
                        fail("folder-import-cleanup");
                        return;
                    }

                    {
                        QEventLoop settleLoop;
                        QTimer::singleShot(800, &settleLoop, &QEventLoop::quit);
                        settleLoop.exec();
                    }
                }

                const auto dragScoreIdRole =
                    libraryService.scores()->roleNames().key("scoreId", -1);
                const auto dragFolderIdRole =
                    libraryService.folders()->roleNames().key("itemId", -1);
                const auto draggedScoreId =
                    libraryService.scores()
                        ->data(libraryService.scores()->index(0, 0), dragScoreIdRole)
                        .toString();
                const auto dropFolderId =
                    libraryService.folders()
                        ->data(libraryService.folders()->index(0, 0), dragFolderIdRole)
                        .toString();
                if (!dragItemToItem(root, QStringLiteral("scoreCardMouse"),
                                    QStringLiteral("folderCardMouse")) ||
                    libraryService.scoreFolderId(draggedScoreId) != dropFolderId)
                {
                    fail("library-card-drag-moves-score-to-folder");
                    return;
                }
                libraryService.setItemFolder(draggedScoreId, QString{});
                for (int i = 0; i < 10; ++i)
                    QCoreApplication::processEvents();

                bool folderNavReady = false;
                for (int attempt = 0; attempt < 80; ++attempt)
                {
                    const auto* win = qobject_cast<QQuickWindow*>(root);
                    if (win)
                    {
                        std::function<QQuickItem*(QQuickItem*)> findReadyNav =
                            [&](QQuickItem* it) -> QQuickItem*
                        {
                            if (!it)
                                return nullptr;
                            if (it->objectName() == QStringLiteral("folderNavItem") &&
                                it->isVisible() && it->width() > 1.0 && it->height() > 1.0 &&
                                it->window())
                            {
                                return it;
                            }
                            for (auto* const c : it->childItems())
                            {
                                if (auto* const m = findReadyNav(c))
                                    return m;
                            }
                            return nullptr;
                        };
                        folderNavReady = findReadyNav(win->contentItem()) != nullptr;
                    }
                    if (folderNavReady)
                        break;
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
                    QThread::msleep(20);
                }

                const bool folderNavClicked =
                    folderNavReady &&
                    clickItem(root, QStringLiteral("folderNavItem"), Qt::RightButton);
                if (!folderNavClicked || !popupIsOpen(root, QStringLiteral("folderContextMenu")))
                {
                    fail("folder-context-menu");
                    return;
                }
                closePopup(root, QStringLiteral("folderContextMenu"));

                if (!clickItem(root, QStringLiteral("tagNavItem"), Qt::RightButton) ||
                    !popupIsOpen(root, QStringLiteral("tagContextMenu")))
                {
                    fail("tag-context-menu");
                    return;
                }
                closePopup(root, QStringLiteral("tagContextMenu"));

                controller.setCurrentPage(QStringLiteral("library"));
                auto* const scoreDelegate = findVisualItem(root, QStringLiteral("scoreDelegate"));
                if (!clickItem(root, QStringLiteral("scoreCardMouse"), Qt::RightButton) ||
                    controller.currentPage() != QStringLiteral("library") || !scoreDelegate ||
                    !scoreDelegate->property("contextMenuOpenedOnce").toBool() ||
                    !scoreDelegate->property("folderSubmenuEnabled").toBool() ||
                    !scoreDelegate->property("tagSubmenuEnabled").toBool() ||
                    scoreDelegate->property("folderSubmenuItemCount").toInt() <
                        libraryService.folders()->rowCount() + 2 ||
                    scoreDelegate->property("tagSubmenuItemCount").toInt() <
                        libraryService.tags()->rowCount() ||
                    popupIsOpen(root, QStringLiteral("blankContextMenu")))
                {
                    fail("score-context-menu");
                    return;
                }
                if (scoreDelegate->property("normalMenuArrowCount").toInt() != 0)
                {
                    fail("normal-menu-item-has-arrow");
                    return;
                }
                if (scoreDelegate->property("folderSubmenuArrowCount").toInt() != 1)
                {
                    fail("submenu-arrow-count");
                    return;
                }
                if (scoreDelegate->property("contextMenuWidth").toDouble() > 220.0 ||
                    scoreDelegate->property("contextMenuWidth").toDouble() < 180.0 ||
                    scoreDelegate->property("folderSubmenuArrowCount").toInt() != 1 ||
                    scoreDelegate->property("folderSubmenuArrowWidth").toDouble() > 14.0 ||
                    scoreDelegate->property("folderSubmenuArrowRightInset").toDouble() > 12.0)
                {
                    fail("compact-menu-style");
                    return;
                }
                if (scoreDelegate->property("tagMenuHasDefaultCheckIndicator").toBool())
                {
                    fail("tag-menu-single-check-indicator");
                    return;
                }
                if (!QMetaObject::invokeMethod(scoreDelegate, "closeContextMenu"))
                {
                    fail("score-context-menu-close-invoke");
                    return;
                }
                if (!waitForPropertyFalse(scoreDelegate, "contextMenuVisible"))
                {
                    fail("score-context-menu-close");
                    return;
                }

                if (!clickItemAt(root, QStringLiteral("librarySurface"), Qt::RightButton, 0.5,
                                 0.92) ||
                    !popupIsOpen(root, QStringLiteral("blankContextMenu")))
                {
                    fail("blank-context-menu");
                    return;
                }
                closePopup(root, QStringLiteral("blankContextMenu"));

                const auto scoreIdRole = libraryService.scores()->roleNames().key("scoreId", -1);
                const auto folderIdRole = libraryService.folders()->roleNames().key("itemId", -1);
                const auto tagIdRole = libraryService.tags()->roleNames().key("itemId", -1);
                const auto scoreId = libraryService.scores()
                                         ->data(libraryService.scores()->index(0, 0), scoreIdRole)
                                         .toString();
                const auto secondScoreId =
                    libraryService.scores()
                        ->data(libraryService.scores()->index(1, 0), scoreIdRole)
                        .toString();
                const auto folderId =
                    libraryService.folders()
                        ->data(libraryService.folders()->index(0, 0), folderIdRole)
                        .toString();
                const auto recentFirstFolderId =
                    libraryService.folders()
                        ->data(libraryService.folders()->index(1, 0), folderIdRole)
                        .toString();
                const auto tagId = libraryService.tags()
                                       ->data(libraryService.tags()->index(0, 0), tagIdRole)
                                       .toString();
                int moveNoticeCount = 0;
                const auto moveNoticeConnection =
                    QObject::connect(&libraryService, &LibraryService::noticeOccurred, root,
                                     [&moveNoticeCount](const QString& message)
                                     {
                                         if (message.startsWith(QStringLiteral("已移动")))
                                             ++moveNoticeCount;
                                     });
                if (!libraryService.moveItems({scoreId, secondScoreId}, folderId).isEmpty())
                {
                    fail("batch-score-folder-assignment");
                    return;
                }
                libraryService.setItemFolder(scoreId, folderId);
                if (moveNoticeCount != 1)
                {
                    fail("single-move-noop-does-not-notify");
                    return;
                }
                if (!libraryService.moveItems({scoreId, secondScoreId}, folderId).isEmpty() ||
                    moveNoticeCount != 1)
                {
                    fail("batch-move-noop-does-not-notify");
                    return;
                }
                QObject::disconnect(moveNoticeConnection);
                libraryService.setFilterMode(QStringLiteral("folder:") + folderId);
                if (scoreId.isEmpty() || secondScoreId.isEmpty() || folderId.isEmpty() ||
                    libraryService.scores()->rowCount() != 2)
                {
                    fail("score-folder-assignment");
                    return;
                }
                const auto firstSortedScoreId =
                    libraryService.scores()
                        ->data(libraryService.scores()->index(0, 0), scoreIdRole)
                        .toString();
                const auto secondSortedScoreId =
                    libraryService.scores()
                        ->data(libraryService.scores()->index(1, 0), scoreIdRole)
                        .toString();
                libraryService.toggleFavorite(secondSortedScoreId, true);
                if (libraryService.scores()
                        ->data(libraryService.scores()->index(0, 0), scoreIdRole)
                        .toString() != firstSortedScoreId)
                {
                    fail("library-favorite-sort-stability");
                    return;
                }
                libraryService.setFilterMode(QStringLiteral("all"));
                libraryService.addScoreTag(scoreId, tagId);
                libraryService.setFilterMode(QStringLiteral("tag:") + tagId);
                if (tagId.isEmpty() || libraryService.scores()->rowCount() != 1 ||
                    !libraryService.scoreHasTag(scoreId, tagId))
                {
                    fail("score-tag-assignment");
                    return;
                }
                libraryService.setFilterMode(QStringLiteral("all"));
                libraryService.removeScoreTag(scoreId, tagId);
                if (libraryService.scoreHasTag(scoreId, tagId))
                {
                    fail("score-tag-removal");
                    return;
                }
                libraryService.addItemTag(folderId, tagId);
                libraryService.setFilterMode(QStringLiteral("tag:") + tagId);
                const auto entryTypeRoleForTags =
                    libraryService.entries()->roleNames().key("itemType", -1);
                const auto entryTagsRole = libraryService.entries()->roleNames().key("tags", -1);
                if (!libraryService.itemHasTag(folderId, tagId) ||
                    libraryService.entries()->rowCount() != 1 ||
                    libraryService.entries()
                            ->data(libraryService.entries()->index(0, 0), entryTypeRoleForTags)
                            .toString() != QStringLiteral("folder") ||
                    libraryService.entries()
                        ->data(libraryService.entries()->index(0, 0), entryTagsRole)
                        .toStringList()
                        .isEmpty())
                {
                    fail("folder-tag-assignment-and-display");
                    return;
                }
                libraryService.toggleItemFavorite(folderId, true);
                libraryService.setFilterMode(QStringLiteral("favorites"));
                if (libraryService.entries()->rowCount() < 1 ||
                    libraryService.entries()
                            ->data(libraryService.entries()->index(0, 0), entryTypeRoleForTags)
                            .toString() != QStringLiteral("folder"))
                {
                    fail("folder-favorite-filter");
                    return;
                }
                if (!libraryService.canMoveItemToFolder(folderId, recentFirstFolderId))
                {
                    fail("folder-move-valid-target");
                    return;
                }
                libraryService.setItemFolder(folderId, recentFirstFolderId);
                if (libraryService.canMoveItemToFolder(recentFirstFolderId, folderId))
                {
                    fail("folder-move-cycle-guard");
                    return;
                }
                libraryService.setItemFolder(folderId, QString{});
                libraryService.setFilterMode(QStringLiteral("folder:") + folderId);

                const auto favoriteRole = libraryService.scores()->roleNames().key("favorite", -1);
                const auto createdDateRole =
                    libraryService.scores()->roleNames().key("createdDate", -1);
                if (createdDateRole < 0)
                {
                    fail("score-created-date-role");
                    return;
                }
                const auto firstIndex = libraryService.scores()->index(0, 0);
                const auto favoriteBefore =
                    libraryService.scores()->data(firstIndex, favoriteRole).toBool();
                if (!clickItem(root, QStringLiteral("favoriteButton"), Qt::LeftButton) ||
                    controller.currentPage() != QStringLiteral("library"))
                {
                    fail("favorite-button-page");
                    return;
                }
                const auto favoriteAfter =
                    libraryService.scores()
                        ->data(libraryService.scores()->index(0, 0), favoriteRole)
                        .toBool();
                if (favoriteBefore == favoriteAfter)
                {
                    fail("favorite-button-state");
                    return;
                }

                libraryService.setFilterMode(QStringLiteral("all"));
                libraryService.setFilterMode(QStringLiteral("favorites"));
                controller.setCurrentPage(QStringLiteral("library"));
                QCoreApplication::processEvents();

                if (!clickItem(root, QStringLiteral("scoreCardMouse"), Qt::LeftButton) ||
                    controller.currentPage() != QStringLiteral("reader") ||
                    controller.currentScoreFolderId() != folderId)
                {
                    fail("score-single-click");
                    return;
                }
                QCoreApplication::processEvents();
                const auto* const sidebar = root->findChild<QQuickItem*>(QStringLiteral("sidebar"));
                auto* const readerPage = root->findChild<QObject*>(QStringLiteral("readerPage"));
                auto* const pdfView = root->findChild<QObject*>(QStringLiteral("pdfView"));
                const auto isPdfScore = readerPage->property("isPdf").toBool();
                auto* const readerView =
                    isPdfScore ? pdfView : root->findChild<QObject*>(QStringLiteral("imageFlick"));
                if (!sidebar || sidebar->isVisible() || !readerView)
                {
                    fail("reader-focus-layout");
                    return;
                }
                const auto openedScoreId = controller.currentScoreId();
                const auto hasPrevious = readerPage->property("hasPrev").toBool();
                const auto hasNext = readerPage->property("hasNext").toBool();
                QMetaObject::invokeMethod(readerPage,
                                          hasPrevious ? "goToPrevScore" : "goToNextScore");
                QCoreApplication::processEvents();
                if ((!hasPrevious && !hasNext) || controller.currentScoreId() == openedScoreId)
                {
                    fail("reader-folder-sibling-navigation");
                    return;
                }
                const auto centerBefore = (readerView->property("contentX").toDouble() +
                                           readerView->property("width").toDouble() / 2.0) /
                                          readerView->property("contentWidth").toDouble();
                QMetaObject::invokeMethod(readerPage, "zoomIn");
                QCoreApplication::processEvents();
                const auto centerAfter = (readerView->property("contentX").toDouble() +
                                          readerView->property("width").toDouble() / 2.0) /
                                         readerView->property("contentWidth").toDouble();
                if (std::abs(centerBefore - centerAfter) > 0.02)
                {
                    fail("reader-centered-zoom");
                    return;
                }

                if (!QMetaObject::invokeMethod(readerPage, "rotateRight") ||
                    readerPage->property("viewRotation").toInt() != 90 ||
                    !QMetaObject::invokeMethod(readerPage, "resetReaderView"))
                {
                    fail("reader-rotation-controls");
                    return;
                }
                QCoreApplication::processEvents();
                if (readerPage->property("viewRotation").toInt() != 0 ||
                    std::abs(readerPage->property("zoomLevel").toDouble() - 1.0) > 0.001)
                {
                    fail("reader-reset-view");
                    return;
                }

                const auto dirtyMaxY =
                    qMax<qreal>(0.0, readerView->property("contentHeight").toDouble() -
                                         readerView->property("height").toDouble());
                readerView->setProperty("contentY", dirtyMaxY * 0.6);

                controller.setCurrentPage(QStringLiteral("library"));
                controller.openScore(controller.currentScoreId(), QStringLiteral("再次打开测试"),
                                     controller.currentFileUrl().toLocalFile(),
                                     controller.currentFileType(), 1,
                                     controller.currentScoreFolderId());
                QEventLoop reopenWait;
                QTimer::singleShot(250, &reopenWait, &QEventLoop::quit);
                reopenWait.exec();
                const auto reopenedZoom = readerPage->property("zoomLevel").toDouble();
                const auto reopenedContentY = readerView->property("contentY").toDouble();
                const auto reopenedContentHeight = readerView->property("contentHeight").toDouble();
                const auto reopenedViewportHeight = readerView->property("height").toDouble();
                if (controller.currentPage() != QStringLiteral("reader") ||
                    std::abs(reopenedZoom - 1.0) > 0.001 || std::abs(reopenedContentY) > 1.0 ||
                    reopenedContentHeight <= reopenedViewportHeight)
                {
                    fail("reader-reopen-default-view");
                    return;
                }
                if (!window->grabWindow().save(QStringLiteral("notera-reader-smoke.png")))
                {
                    fail("reader-screenshot");
                    return;
                }

                controller.setCurrentPage(QStringLiteral("settings"));
                QCoreApplication::processEvents();
                QEventLoop settingsLayoutWait;
                QTimer::singleShot(200, &settingsLayoutWait, &QEventLoop::quit);
                settingsLayoutWait.exec();
                const auto* const settingsContent =
                    root->findChild<QQuickItem*>(QStringLiteral("settingsContent"));
                const auto* const themeSelector =
                    root->findChild<QQuickItem*>(QStringLiteral("themeSelector"));
                const auto* const changeDataDirectoryButton =
                    root->findChild<QQuickItem*>(QStringLiteral("changeDataDirectoryButton"));
                const auto* const openDataDirectoryButton =
                    root->findChild<QQuickItem*>(QStringLiteral("openDataDirectoryButton"));
                const auto* const versionLabel =
                    root->findChild<QQuickItem*>(QStringLiteral("versionLabel"));
                const auto* const animationsSwitch =
                    root->findChild<QQuickItem*>(QStringLiteral("animationsSwitch"));
                if (!settingsContent || !themeSelector || settingsContent->width() <= 0.0 ||
                    themeSelector->width() < 240.0 || themeSelector->x() < 0.0 ||
                    !changeDataDirectoryButton || !changeDataDirectoryButton->isVisible() ||
                    !changeDataDirectoryButton->isEnabled() || !openDataDirectoryButton ||
                    !openDataDirectoryButton->isVisible() || !versionLabel || !animationsSwitch ||
                    !controller.animationsEnabled() ||
                    animationsSwitch->property("independentAnimationDuration").toInt() <= 0 ||
                    std::abs(
                        animationsSwitch->mapToScene(QPointF(animationsSwitch->width(), 0)).x() -
                        themeSelector->mapToScene(QPointF(themeSelector->width(), 0)).x()) > 1.0 ||
                    std::abs(versionLabel->mapToScene(QPointF(versionLabel->width(), 0)).x() -
                             themeSelector->mapToScene(QPointF(themeSelector->width(), 0)).x()) >
                        1.0)
                {
                    fail("settings-layout");
                    return;
                }
                controller.setAnimationsEnabled(false);
                QCoreApplication::processEvents();
                ApplicationController persistedMotionController;
                if (controller.animationsEnabled() ||
                    persistedMotionController.animationsEnabled() ||
                    libraryNavItem->property("hoverTransitionDuration").toInt() != 0 ||
                    animationsSwitch->property("independentAnimationDuration").toInt() <= 0)
                {
                    fail("animations-setting-persistence");
                    return;
                }
                controller.setAnimationsEnabled(true);
                if (!findVisualItem(root, QStringLiteral("tagEntryIcon")))
                {
                    fail("tag-entry-icon");
                    return;
                }
                auto* const dataDirectoryDialog =
                    root->findChild<QObject*>(QStringLiteral("dataDirectoryDialog"));
                if (!dataDirectoryDialog)
                {
                    fail("data-directory-dialog-object");
                    return;
                }
                dataDirectoryDialog->setProperty("selectedFolder",
                                                 QUrl::fromLocalFile(QDir::temp().filePath(
                                                     QStringLiteral("notera-ui-selected-folder"))));
                if (!QMetaObject::invokeMethod(dataDirectoryDialog, "accepted") ||
                    !popupIsOpen(root, QStringLiteral("migrationConfirmDialog")))
                {
                    fail("data-directory-dialog-accepted");
                    return;
                }
                closePopup(root, QStringLiteral("migrationConfirmDialog"));
                QEventLoop migrationDialogCloseWait;
                QTimer::singleShot(200, &migrationDialogCloseWait, &QEventLoop::quit);
                migrationDialogCloseWait.exec();
                const auto* const settingsTitle =
                    root->findChild<QQuickItem*>(QStringLiteral("settingsTitle"));
                const auto* const brandLabel =
                    root->findChild<QQuickItem*>(QStringLiteral("brandLabel"));
                if (!settingsTitle || !brandLabel ||
                    settingsTitle->mapToScene(QPointF{}).y() < 24.0 ||
                    brandLabel->mapToScene(QPointF{}).y() < 12.0)
                {
                    fail("page-top-spacing");
                    return;
                }
                auto* const clearAllDataButton =
                    root->findChild<QObject*>(QStringLiteral("clearAllDataButton"));
                auto* const clearWarning =
                    root->findChild<QObject*>(QStringLiteral("clearWarningDialog"));
                if (!clearAllDataButton || !clearWarning ||
                    !QMetaObject::invokeMethod(clearWarning, "open") ||
                    !popupIsOpen(root, QStringLiteral("clearWarningDialog")))
                {
                    fail("clear-data-first-confirmation");
                    return;
                }
                if (!QMetaObject::invokeMethod(clearWarning, "accept"))
                {
                    fail("clear-data-second-confirmation-open");
                    return;
                }
                QCoreApplication::processEvents();
                auto* const clearInput =
                    root->findChild<QObject*>(QStringLiteral("clearConfirmInput"));
                auto* const clearButton =
                    root->findChild<QObject*>(QStringLiteral("confirmClearAllDataButton"));
                if (!popupIsOpen(root, QStringLiteral("clearTypedDialog")) || !clearInput ||
                    !clearButton || clearButton->property("enabled").toBool())
                {
                    fail("clear-data-typed-confirmation-disabled");
                    return;
                }
                clearInput->setProperty("text", QStringLiteral("确认清空所有数据"));
                QCoreApplication::processEvents();
                if (!clearButton->property("enabled").toBool())
                {
                    fail("clear-data-typed-confirmation-enabled");
                    return;
                }
                closePopup(root, QStringLiteral("clearTypedDialog"));
                QEventLoop clearDialogCloseWait;
                QTimer::singleShot(200, &clearDialogCloseWait, &QEventLoop::quit);
                clearDialogCloseWait.exec();
                if (!window->grabWindow().save(QStringLiteral("notera-settings-smoke.png")))
                {
                    fail("settings-screenshot");
                    return;
                }
                controller.setThemeMode(1);
                QCoreApplication::processEvents();
                if (root->property("themeBackground").value<QColor>().lightnessF() > 0.25 ||
                    !window->grabWindow().save(QStringLiteral("notera-settings-dark-smoke.png")))
                {
                    fail("dark-theme-render");
                    return;
                }
                controller.setThemeMode(0);
                QCoreApplication::processEvents();

                controller.setCurrentPage(QStringLiteral("library"));
                QCoreApplication::processEvents();
                if (!clickItem(root, QStringLiteral("newFolderButton"), Qt::LeftButton) ||
                    !popupIsOpen(root, QStringLiteral("folderEditorDialog")))
                {
                    fail("new-folder-dialog");
                    return;
                }
                if (!window->grabWindow().save(QStringLiteral("notera-dialog-smoke.png")))
                {
                    fail("dialog-screenshot");
                    return;
                }

                libraryService.goToLibraryRoot();
                QCoreApplication::processEvents();
                const auto* const browserGrid =
                    root->findChild<QObject*>(QStringLiteral("browserGrid"));
                if (!browserGrid || browserGrid->property("count").toInt() != 2)
                {
                    fail("library-folder-score-browser");
                    return;
                }
                libraryService.enterFolder(folderId);
                libraryService.goToLibraryRoot();
                libraryService.enterFolder(recentFirstFolderId);
                libraryService.goToLibraryRoot();
                libraryService.setFilterMode(QStringLiteral("recent"));
                QCoreApplication::processEvents();
                const auto entryIdRole = libraryService.entries()->roleNames().key("itemId", -1);
                if (libraryService.entries()->rowCount() != 4 ||
                    libraryService.entries()
                            ->data(libraryService.entries()->index(0, 0), entryTypeRole)
                            .toString() != QStringLiteral("folder") ||
                    libraryService.entries()
                            ->data(libraryService.entries()->index(0, 0), entryIdRole)
                            .toString() != recentFirstFolderId ||
                    libraryService.entries()
                            ->data(libraryService.entries()->index(1, 0), entryIdRole)
                            .toString() != folderId ||
                    libraryService.entries()
                            ->data(libraryService.entries()->index(2, 0), entryIdRole)
                            .toString() != controller.currentScoreId())
                {
                    fail("recent-folder-first-last-opened-sort");
                    return;
                }
                libraryService.goToLibraryRoot();

                {
                    QObject ctx;
                    bool sameFolderConflictFired = false;
                    QObject::connect(&libraryService, &LibraryService::pasteFolderConflict, &ctx,
                                     [&](const QString&, const QString&, int, int)
                                     { sameFolderConflictFired = true; });
                    libraryService.copyItems({folderId});
                    libraryService.pasteItems();
                    {
                        QEventLoop conflictWait;
                        QTimer::singleShot(300, &conflictWait, &QEventLoop::quit);
                        conflictWait.exec();
                    }
                    if (!sameFolderConflictFired)
                    {
                        fail("same-folder-copy-should-conflict");
                        return;
                    }
                    libraryService.resolvePasteFolderConflict(QStringLiteral("skip"), false);
                    QCoreApplication::processEvents();
                    {
                        const auto rootFolders = libraryService.childFolders(QString());
                        int count = 0;
                        for (const auto& f : rootFolders)
                        {
                            if (f.toMap().value(QStringLiteral("name")).toString() ==
                                QStringLiteral("A界面测试文件夹"))
                                ++count;
                        }
                        if (count != 1)
                        {
                            fail("same-folder-copy-skip-no-duplicate");
                            return;
                        }
                    }
                }

                libraryService.enterFolder(recentFirstFolderId);
                libraryService.createFolder(QStringLiteral("A界面测试文件夹"));
                libraryService.copyItems({folderId});
                libraryService.pasteItems();
                {
                    QEventLoop conflictWait;
                    QTimer::singleShot(300, &conflictWait, &QEventLoop::quit);
                    conflictWait.exec();
                }
                if (!window->grabWindow().save(QStringLiteral("notera-conflict-dialog-smoke.png")))
                {
                    fail("conflict-dialog-screenshot");
                    return;
                }
                libraryService.resolvePasteFolderConflict(QStringLiteral("cancel"), false);
                QCoreApplication::processEvents();

                const auto folderScores = libraryService.scoresInFolder(folderId);
                QStringList storedFilePaths;
                for (const auto& value : folderScores)
                {
                    storedFilePaths.append(
                        value.toMap().value(QStringLiteral("filePath")).toString());
                }
                libraryService.deleteItems({folderId});
                const auto hasRemainingFile =
                    std::any_of(storedFilePaths.cbegin(), storedFilePaths.cend(),
                                [](const QString& path) { return QFileInfo::exists(path); });
                if (libraryService.scores()->rowCount() != 0 || hasRemainingFile)
                {
                    fail("batch-folder-cascade-delete");
                    return;
                }

                {
                    auto* const pdfWindow = qobject_cast<QQuickWindow*>(root);
                    auto* const pdfView = root->findChild<QObject*>(QStringLiteral("pdfView"));
                    if (!pdfWindow || !pdfView)
                    {
                        fail("pdf-lazy-env");
                        return;
                    }
                    QTemporaryFile pdfLazyFile;
                    pdfLazyFile.setFileTemplate(QDir::tempPath() +
                                                QStringLiteral("/notera-pdf-lazy-XXXXXX.pdf"));
                    if (!pdfLazyFile.open())
                    {
                        fail("pdf-lazy-tempfile");
                        return;
                    }
                    const auto pdfLazyPath = pdfLazyFile.fileName();
                    pdfLazyFile.close();
                    {
                        QPdfWriter pdfLazyWriter(pdfLazyPath);
                        pdfLazyWriter.setPageSize(QPageSize(QPageSize::A4));
                        QPainter pdfLazyPainter(&pdfLazyWriter);
                        for (int page = 0; page < 82; ++page)
                        {
                            pdfLazyPainter.drawText(
                                120, 200, QStringLiteral("PDF 虚拟化回归测试页 %1").arg(page + 1));
                            if (page < 81)
                                pdfLazyWriter.newPage();
                        }
                        pdfLazyPainter.end();
                    }
                    controller.openScore(QStringLiteral("pdf-lazy-regression"),
                                         QStringLiteral("PDF虚拟化回归"), pdfLazyPath,
                                         QStringLiteral("pdf"), 82, QString());
                    {
                        QEventLoop pdfLazyWait;
                        QTimer::singleShot(600, &pdfLazyWait, &QEventLoop::quit);
                        pdfLazyWait.exec();
                    }
                    if (controller.currentPage() != QStringLiteral("reader") ||
                        pdfView->property("pageCount").toInt() != 82)
                    {
                        fail("pdf-lazy-open");
                        return;
                    }
                    const auto countRenderedPages = [&]()
                    {
                        return static_cast<int>(
                            findItemsByObjectName(pdfWindow, QStringLiteral("pdfPageImageItem"))
                                .size());
                    };
                    const auto pdfRendered = countRenderedPages();
                    if (pdfRendered == 0 || pdfRendered > 25)
                    {
                        fail("pdf-lazy-virtualized-range");
                        return;
                    }

                    const auto pdfMaxY =
                        qMax<qreal>(0.0, pdfView->property("contentHeight").toDouble() -
                                             pdfView->property("height").toDouble());
                    pdfView->setProperty("contentY", pdfMaxY);
                    {
                        QEventLoop pdfLazyScrollWait;
                        QTimer::singleShot(500, &pdfLazyScrollWait, &QEventLoop::quit);
                        pdfLazyScrollWait.exec();
                    }
                    const auto pdfRenderedAfterScroll = countRenderedPages();
                    if (pdfRenderedAfterScroll == 0 || pdfRenderedAfterScroll > 25)
                    {
                        fail("pdf-lazy-scroll-virtualized-range");
                        return;
                    }
                    const auto pdfLastPage = pdfView->property("currentPage").toInt();
                    if (pdfLastPage < 78)
                    {
                        fail("pdf-lazy-scroll-current-page");
                        return;
                    }
                }

                {
                    auto* const pdfTileWindow = qobject_cast<QQuickWindow*>(root);
                    auto* const pdfTileView = root->findChild<QObject*>(QStringLiteral("pdfView"));
                    if (!pdfTileWindow || !pdfTileView)
                    {
                        fail("pdf-tile-env");
                        return;
                    }
                    QTemporaryFile pdfTileFile;
                    pdfTileFile.setFileTemplate(QDir::tempPath() +
                                                QStringLiteral("/notera-pdf-tile-XXXXXX.pdf"));
                    if (!pdfTileFile.open())
                    {
                        fail("pdf-tile-tempfile");
                        return;
                    }
                    const auto pdfTilePath = pdfTileFile.fileName();
                    pdfTileFile.close();
                    {
                        QPdfWriter pdfTileWriter(pdfTilePath);
                        pdfTileWriter.setPageSize(QPageSize(QSizeF(1200, 1600), QPageSize::Point));
                        QPainter pdfTilePainter(&pdfTileWriter);
                        for (int page = 0; page < 5; ++page)
                        {
                            pdfTilePainter.drawText(
                                200, 300, QStringLiteral("PDF 分块渲染测试页 %1").arg(page + 1));
                            if (page < 4)
                                pdfTileWriter.newPage();
                        }
                        pdfTilePainter.end();
                    }
                    controller.openScore(QStringLiteral("pdf-tile-regression"),
                                         QStringLiteral("PDF分块渲染回归"), pdfTilePath,
                                         QStringLiteral("pdf"), 5, QString());

                    {
                        QEventLoop docWait;
                        QTimer docTimer;
                        docTimer.setInterval(100);
                        bool docReady = false;
                        int docAttempts = 0;
                        QObject::connect(
                            &docTimer, &QTimer::timeout,
                            [&]()
                            {
                                auto* const doc =
                                    pdfTileView->property("document").value<QObject*>();

                                const int s = doc ? doc->property("status").toInt() : -1;
                                if (s == 2)
                                {
                                    docReady = true;
                                    docWait.quit();
                                }
                                else if (++docAttempts > 30)
                                {
                                    qWarning() << "pdf-tile-open: document not ready, status=" << s;
                                    docWait.quit();
                                }
                            });
                        docTimer.start();
                        docWait.exec();
                        docTimer.stop();
                        if (!docReady)
                        {
                            fail("pdf-tile-document-not-ready");
                            return;
                        }
                    }

                    QEventLoop tileWait;
                    QTimer tileTimer;
                    tileTimer.setInterval(100);
                    bool tileReady = false;
                    int tileAttempts = 0;
                    QObject::connect(
                        &tileTimer, &QTimer::timeout,
                        [&]()
                        {
                            const int status =
                                pdfTileView->property("currentPageRenderingStatus").toInt();
                            if (status == 1)
                            {
                                tileReady = true;
                                tileWait.quit();
                            }
                            else if (++tileAttempts > 30)
                            {
                                tileWait.quit();
                            }
                        });
                    tileTimer.start();
                    tileWait.exec();
                    tileTimer.stop();
                    if (!tileReady)
                    {
                        const int finalStatus =
                            pdfTileView->property("currentPageRenderingStatus").toInt();
                        qWarning() << "pdf-tile-open: currentPageRenderingStatus stuck at"
                                   << finalStatus << "(expected 1=Ready)";
                        fail("pdf-tile-open-status-not-ready");
                        return;
                    }
                }

                QCoreApplication::exit(0);
            });
    }

    return app.exec();
}
