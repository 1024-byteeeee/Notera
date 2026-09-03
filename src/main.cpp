#include <algorithm>
#include <cmath>
#include <memory>

#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QGuiApplication>
#include <QImage>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QProcess>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFile>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTimer>
#include <QtCore/private/qzipwriter_p.h>

#include "app/ApplicationController.h"
#include "features/library/LibraryService.h"
#include "platform/AppDataPaths.h"
#include "services/MetronomeService.h"

namespace {

struct RestartGuard {
    bool requested {false};
    QString program;
    QStringList arguments;
    QString workingDirectory;

    ~RestartGuard()
    {
        if (requested && !program.isEmpty()) {
            QProcess::startDetached(program, arguments, workingDirectory);
        }
    }
};

QQuickItem* findVisualItem(QQuickItem* parent, const QString& objectName)
{
    if (!parent) {
        return nullptr;
    }
    if (parent->objectName() == objectName) {
        return parent;
    }
    for (auto* const child : parent->childItems()) {
        if (auto* const match = findVisualItem(child, objectName)) {
            return match;
        }
    }
    return nullptr;
}

QQuickItem* findVisualItem(QObject* root, const QString& objectName)
{
    if (auto* const window = qobject_cast<QQuickWindow*>(root)) {
        return findVisualItem(window->contentItem(), objectName);
    }
    return findVisualItem(qobject_cast<QQuickItem*>(root), objectName);
}

bool clickItemAt(QObject* root, const QString& objectName, const Qt::MouseButton button,
    const double relX, const double relY)
{
    auto* const item = findVisualItem(root, objectName);
    if (!item || !item->isVisible() || item->width() <= 0.0 || item->height() <= 0.0 || !item->window()) {
        return false;
    }

    const auto scenePosition = item->mapToScene(QPointF(item->width() * relX, item->height() * relY));
    const auto globalPosition = QPointF(item->window()->mapToGlobal(scenePosition.toPoint()));
    QMouseEvent pressEvent(QEvent::MouseButtonPress, scenePosition, scenePosition, globalPosition,
        button, button, Qt::NoModifier, QPointingDevice::primaryPointingDevice());
    QCoreApplication::sendEvent(item->window(), &pressEvent);
    QMouseEvent releaseEvent(QEvent::MouseButtonRelease, scenePosition, scenePosition, globalPosition,
        button, Qt::NoButton, Qt::NoModifier, QPointingDevice::primaryPointingDevice());
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
    QMouseEvent event(type, scenePosition, scenePosition, globalPosition,
        button, buttons, Qt::NoModifier, QPointingDevice::primaryPointingDevice());
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
    if (!source || !target || !preview || !previewImage || !window) return false;

    const auto start = source->mapToScene(QPointF(source->width() / 2.0, source->height() / 2.0));
    const auto end = target->mapToScene(QPointF(target->width() / 2.0, target->height() / 2.0));
    sendMouseEvent(window, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    if (preview->isVisible()) {
        sendMouseEvent(window, QEvent::MouseButtonRelease, start, Qt::LeftButton, Qt::NoButton);
        return false;
    }
    bool previewReady = false;
    for (int step = 1; step <= 8; ++step) {
        sendMouseEvent(window, QEvent::MouseMove,
            start + (end - start) * (static_cast<double>(step) / 8.0), Qt::NoButton, Qt::LeftButton);
        if (step == 4) {
            const auto previewSource = previewImage->property("source").toUrl();
            previewReady = preview->isVisible() && preview->z() >= 100.0 && previewSource.isLocalFile();
        }
    }
    sendMouseEvent(window, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
    QCoreApplication::processEvents();
    return previewReady;
}

bool popupIsOpen(QObject* root, const QString& objectName)
{
    const auto* const popup = root->findChild<QObject*>(objectName);
    if (!popup) {
        return false;
    }
    const auto opened = popup->property("visible").toBool() || popup->property("openedOnce").toBool();
    const auto width = std::max(popup->property("width").toDouble(), popup->property("implicitWidth").toDouble());
    const auto height = std::max(popup->property("height").toDouble(), popup->property("implicitHeight").toDouble());
    return opened && width > 0.0 && height > 0.0;
}

// 等待对象某个布尔属性变为 false。用嵌套事件循环驱动动画计时器，
// 确保 Popup 的关闭动画（exit Transition）跑完、遮罩真正撤销后再返回，
// 避免"关闭后立刻点击"被仍在关闭的弹窗遮罩吞掉。
bool waitForPropertyFalse(QObject* obj, const QByteArray& propertyName, int timeoutMs = 3000)
{
    if (!obj) return true;
    const auto met = [obj, propertyName] { return !obj->property(propertyName).toBool(); };
    if (met()) return true;
    QEventLoop loop;
    QTimer poll;
    poll.setInterval(10);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (met()) loop.quit();
    });
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    poll.start();
    loop.exec();
    return met();
}

bool closePopup(QObject* root, const QString& objectName)
{
    auto* const popup = root->findChild<QObject*>(objectName);
    if (!popup || !QMetaObject::invokeMethod(popup, "close")) {
        return false;
    }
    return waitForPropertyFalse(popup, "visible");
}

}

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

    const auto arguments = app.arguments();
    const auto isSmokeTest = arguments.contains(QStringLiteral("--theme-smoke-test"))
        || arguments.contains(QStringLiteral("--import-smoke-test"))
        || arguments.contains(QStringLiteral("--stitch-smoke-test"))
        || arguments.contains(QStringLiteral("--reader-smoke-test"))
        || arguments.contains(QStringLiteral("--ui-smoke-test"))
        || arguments.contains(QStringLiteral("--folder-rename-smoke-test"))
        || arguments.contains(QStringLiteral("--storage-migration-smoke-test"))
        || arguments.contains(QStringLiteral("--clear-data-smoke-test"))
        || arguments.contains(QStringLiteral("--clipboard-smoke-test"))
        || arguments.contains(QStringLiteral("--tag-smoke-test"));
    if (isSmokeTest) {
        QStandardPaths::setTestModeEnabled(true);
        app.setApplicationName(QStringLiteral("NoteraTest"));
    }
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    std::unique_ptr<QTemporaryDir> migrationSmokeRoot;
    std::unique_ptr<QTemporaryDir> generalSmokeRoot;
    QString expectedMigratedFile;
    if (isSmokeTest && !arguments.contains(QStringLiteral("--storage-migration-smoke-test"))) {
        generalSmokeRoot = std::make_unique<QTemporaryDir>();
        if (!generalSmokeRoot->isValid()) return 1;
        AppDataPaths::setCustomRoot(generalSmokeRoot->path());
    }
    if (arguments.contains(QStringLiteral("--storage-migration-smoke-test"))) {
        migrationSmokeRoot = std::make_unique<QTemporaryDir>();
        if (!migrationSmokeRoot->isValid()) return 1;
        const auto oldRoot = migrationSmokeRoot->filePath(QStringLiteral("old"));
        const auto newRoot = migrationSmokeRoot->filePath(QStringLiteral("new"));
        QDir().mkpath(oldRoot + QStringLiteral("/database"));
        QDir().mkpath(oldRoot + QStringLiteral("/library/scores"));
        expectedMigratedFile = newRoot + QStringLiteral("/library/scores/test.png");
        QFile marker(oldRoot + QStringLiteral("/library/scores/test.png"));
        if (!marker.open(QIODevice::WriteOnly) || marker.write("notera") != 6) return 1;
        marker.close();
        const auto connectionName = QStringLiteral("storage_migration_fixture");
        {
            auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(oldRoot + QStringLiteral("/database/notera.db"));
            if (!database.open()) return 1;
            QSqlQuery query(database);
            if (!query.exec(QStringLiteral(
                "CREATE TABLE scores (file_path TEXT NOT NULL, thumbnail_path TEXT)"))) return 1;
            query.prepare(QStringLiteral("INSERT INTO scores VALUES (?, NULL)"));
            query.addBindValue(oldRoot + QStringLiteral("/library/scores/test.png"));
            if (!query.exec()) return 1;
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
        if (migrationController.migrateDataDirectory(QUrl::fromLocalFile(newRoot)).isEmpty() == false
            || migrationController.pendingDataDirectory() != newRoot
            || migrationController.migrateDataDirectory(QUrl(QStringLiteral("https://example.com/notera"))).isEmpty()) {
            return 1;
        }
        migrationController.requestRestart();
        if (!restartRequested) return 1;
    }

    QString clearError;
    if (!ApplicationController::applyPendingDataClear(&clearError)) {
        qWarning() << "Data clear failed:" << clearError;
        return 1;
    }

    QString migrationError;
    if (!ApplicationController::applyPendingDataMigration(&migrationError)) {
        qWarning() << "Data directory migration failed:" << migrationError;
        if (arguments.contains(QStringLiteral("--storage-migration-smoke-test"))) return 1;
    }
    QString restoreError;
    if (!ApplicationController::applyPendingBackupRestore(&restoreError)) {
        qWarning() << "Database restore failed:" << restoreError;
        return 1;
    }
    if (arguments.contains(QStringLiteral("--clear-data-smoke-test"))) {
        const auto clearRoot = AppDataPaths::root();
        QDir().mkpath(clearRoot + QStringLiteral("/database"));
        QFile databaseMarker(clearRoot + QStringLiteral("/database/notera.db"));
        if (!databaseMarker.open(QIODevice::WriteOnly) || databaseMarker.write("notera") != 6) return 1;
        databaseMarker.close();
        QFile marker(clearRoot + QStringLiteral("/clear-marker"));
        if (!marker.open(QIODevice::WriteOnly) || marker.write("notera") != 6) return 1;
        marker.close();
        QSettings().setValue(QStringLiteral("storage/pendingClearRoot"), clearRoot);
        QString error;
        if (!ApplicationController::applyPendingDataClear(&error) || QDir(clearRoot).exists()) return 1;

        ApplicationController clearController;
        QDir().mkpath(AppDataPaths::databaseDirectory());
        QFile scheduledDatabase(AppDataPaths::databaseDirectory() + QStringLiteral("/notera.db"));
        if (!scheduledDatabase.open(QIODevice::WriteOnly)) return 1;
        scheduledDatabase.close();
        bool restartRequested = false;
        QObject::connect(&clearController, &ApplicationController::restartRequested,
            [&restartRequested] { restartRequested = true; });
        if (clearController.clearAllData(QStringLiteral("清空所有数据")).isEmpty()
            || !clearController.clearAllData(QStringLiteral("确认清空所有数据")).isEmpty()
            || !restartRequested
            || QSettings().value(QStringLiteral("storage/pendingClearRoot")).toString().isEmpty()) return 1;
        QSettings().clear();
        return 0;
    }
    if (arguments.contains(QStringLiteral("--storage-migration-smoke-test"))) {
        QSqlDatabase migratedDatabase = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"), QStringLiteral("storage_migration_verification"));
        migratedDatabase.setDatabaseName(AppDataPaths::databaseDirectory() + QStringLiteral("/notera.db"));
        if (!QFileInfo::exists(expectedMigratedFile) || !migratedDatabase.open()) return 1;
        QSqlQuery query(migratedDatabase);
        const auto valid = query.exec(QStringLiteral("SELECT file_path FROM scores")) && query.next()
            && query.value(0).toString() == expectedMigratedFile;
        query.finish();
        migratedDatabase.close();
        migratedDatabase = {};
        QSqlDatabase::removeDatabase(QStringLiteral("storage_migration_verification"));
        return valid ? 0 : 1;
    }

    ApplicationController controller;
    LibraryService libraryService;
    MetronomeService metronome;
    QObject::connect(&controller, &ApplicationController::restartRequested, &app, [&restartGuard] {
        restartGuard.requested = true;
        QCoreApplication::quit();
    });
    QObject::connect(&controller, &ApplicationController::scoreOpened,
        &libraryService, &LibraryService::markScoreOpened);

    if (arguments.contains(QStringLiteral("--merge-smoke-test"))) {
        // 预清理历史残留，避免脏库导致误判
        {
            const auto connection = QStringLiteral("notera_merge_smoke_pretest");
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(AppDataPaths::databaseDirectory() + QStringLiteral("/notera.db"));
                if (database.open()) {
                    QVariantList paths;
                    {
                        QSqlQuery query(database);
                        query.exec(QStringLiteral("SELECT id, file_path FROM scores WHERE title LIKE 'MERGE-SMOKE%'"));
                        while (query.next()) paths.append(query.value(1).toString());
                        query.exec(QStringLiteral("DELETE FROM scores WHERE title LIKE 'MERGE-SMOKE%'"));
                        query.exec(QStringLiteral("DELETE FROM folders WHERE name LIKE 'MERGE-SMOKE%'"));
                        query.exec(QStringLiteral("DELETE FROM tags WHERE name LIKE 'MERGE-SMOKE%'"));
                    }
                    database.close();
                    for (const auto& path : paths) QFile::remove(path.toString());
                }
            }
            QSqlDatabase::removeDatabase(connection);
        }
        // 构造一份符合备份格式的压缩包：manifest + 数据库(标签/文件夹/乐谱) + library/scores 源文件
        QTemporaryDir backupRoot(QDir::tempPath() + QStringLiteral("/notera-merge-backup-XXXXXX"));
        QTemporaryDir zipDir(QDir::tempPath() + QStringLiteral("/notera-merge-zip-XXXXXX"));
        if (!backupRoot.isValid() || !zipDir.isValid()) return 1;
        const auto backupPath = backupRoot.path();

        {
            QFile manifestFile(backupPath + QStringLiteral("/manifest.json"));
            if (!manifestFile.open(QIODevice::WriteOnly)) return 1;
            QJsonObject manifest{{QStringLiteral("format"), QStringLiteral("notera-backup")},
                {QStringLiteral("formatVersion"), 1},
                {QStringLiteral("applicationVersion"), QCoreApplication::applicationVersion()},
                {QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                {QStringLiteral("sourceRoot"), backupPath}};
            manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
            manifestFile.close();
        }
        if (!QDir().mkpath(backupPath + QStringLiteral("/database"))
            || !QDir().mkpath(backupPath + QStringLiteral("/library/scores"))
            || !QDir().mkpath(backupPath + QStringLiteral("/thumbnails"))) return 1;

        QImage scoreImage(160, 220, QImage::Format_RGB32);
        scoreImage.fill(Qt::darkBlue);
        if (!scoreImage.save(backupPath + QStringLiteral("/library/scores/score1.png"))) return 1;

        {
            auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("notera_merge_smoke_backup"));
            database.setDatabaseName(backupPath + QStringLiteral("/database/notera.db"));
            if (!database.open()) return 1;
            QSqlQuery query(database);
            query.exec(QStringLiteral("CREATE TABLE scores (id TEXT PRIMARY KEY, title TEXT NOT NULL, composer TEXT, file_name TEXT NOT NULL, file_path TEXT NOT NULL, file_type TEXT NOT NULL, page_count INTEGER NOT NULL DEFAULT 1, thumbnail_path TEXT, favorite INTEGER NOT NULL DEFAULT 0, last_page INTEGER NOT NULL DEFAULT 1, created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL, last_opened_at INTEGER, folder_id TEXT)"));
            query.exec(QStringLiteral("CREATE TABLE folders (id TEXT PRIMARY KEY, name TEXT NOT NULL, created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL, parent_id TEXT, last_opened_at INTEGER, favorite INTEGER NOT NULL DEFAULT 0)"));
            query.exec(QStringLiteral("CREATE TABLE tags (id TEXT PRIMARY KEY, name TEXT NOT NULL UNIQUE)"));
            query.exec(QStringLiteral("CREATE TABLE score_tags (score_id TEXT NOT NULL, tag_id TEXT NOT NULL, PRIMARY KEY (score_id, tag_id))"));
            query.exec(QStringLiteral("CREATE TABLE folder_tags (folder_id TEXT NOT NULL, tag_id TEXT NOT NULL, PRIMARY KEY (folder_id, tag_id))"));
            query.exec(QStringLiteral("CREATE TABLE annotations (id TEXT PRIMARY KEY, score_id TEXT NOT NULL, page INTEGER NOT NULL, type TEXT NOT NULL, data TEXT NOT NULL, created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL)"));
            const auto now = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
            query.exec(QStringLiteral("INSERT INTO tags (id, name) VALUES ('smoke-tag', 'MERGE-SMOKE-TAG')"));
            query.exec(QStringLiteral("INSERT INTO folders (id, name, created_at, updated_at, parent_id, favorite) VALUES ('smoke-folder', 'MERGE-SMOKE-FOLDER', 1, 1, NULL, 0)"));
            query.exec(QStringLiteral("INSERT INTO folder_tags (folder_id, tag_id) VALUES ('smoke-folder', 'smoke-tag')"));
            QSqlQuery insert(database);
            insert.prepare(QStringLiteral("INSERT INTO scores (id, title, composer, file_name, file_path, file_type, page_count, favorite, last_page, created_at, updated_at, folder_id) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"));
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
            if (!insert.exec()) return 1;
            query.exec(QStringLiteral("INSERT INTO score_tags (score_id, tag_id) VALUES ('smoke-score', 'smoke-tag')"));
            database.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("notera_merge_smoke_backup"));

        const auto zipPath = zipDir.filePath(QStringLiteral("backup.notera-backup.zip"));
        {
            QZipWriter writer(zipPath);
            writer.setCompressionPolicy(QZipWriter::AutoCompress);
            if (writer.status() != QZipWriter::NoError) return 1;
            QDirIterator iterator(backupPath, QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden,
                QDirIterator::Subdirectories);
            while (iterator.hasNext()) {
                const auto filePath = iterator.next();
                const auto relativePath = QDir(backupPath).relativeFilePath(filePath);
                const QFileInfo info(filePath);
                if (info.isDir()) {
                    writer.addDirectory(relativePath);
                    continue;
                }
                QFile file(filePath);
                if (!file.open(QIODevice::ReadOnly)) return 1;
                writer.addFile(relativePath, file.readAll());
                file.close();
            }
            writer.close();
            if (writer.status() != QZipWriter::NoError) return 1;
        }

        // 探测备份信息
        const auto probe = libraryService.probeDatabaseBackup(QUrl::fromLocalFile(zipPath));
        if (!probe.value(QStringLiteral("valid")).toBool()
            || probe.value(QStringLiteral("scoreCount")).toInt() != 1
            || probe.value(QStringLiteral("folderCount")).toInt() != 1
            || probe.value(QStringLiteral("tagCount")).toInt() != 1) {
            qWarning() << "[merge-smoke] FAIL probe:" << QJsonDocument::fromVariant(probe).toJson(QJsonDocument::Compact);
            return 1;
        }

        // 首次合并：标签/文件夹/乐谱并入当前库
        const auto mergeError = libraryService.importDatabaseBackupMerged(QUrl::fromLocalFile(zipPath));
        if (!mergeError.isEmpty()) {
            qWarning() << "[merge-smoke] FAIL first merge:" << mergeError;
            return 1;
        }
        {
            // 乐谱在非根目录（挂在合并文件夹下），默认列表不显示，直接用 SQL 断言
            const auto connection = QStringLiteral("notera_merge_smoke_check");
            bool okScore = false;
            bool okFolder = false;
            bool okTag = false;
            bool okTime = false;
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(AppDataPaths::databaseDirectory() + QStringLiteral("/notera.db"));
                if (!database.open()) {
                    qWarning() << "[merge-smoke] FAIL open db for assert";
                    return 1;
                }
                {
                    QSqlQuery query(database);
                    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM scores WHERE title = 'MERGE-SMOKE-SCORE'"))
                        && query.next()) okScore = query.value(0).toInt() == 1;
                    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM scores s JOIN folders f ON s.folder_id = f.id "
                            "WHERE s.title = 'MERGE-SMOKE-SCORE' AND f.name = 'MERGE-SMOKE-FOLDER'"))
                        && query.next()) okFolder = query.value(0).toInt() == 1;
                    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM score_tags st JOIN scores s ON st.score_id = s.id "
                            "JOIN tags t ON st.tag_id = t.id WHERE s.title = 'MERGE-SMOKE-SCORE' AND t.name = 'MERGE-SMOKE-TAG'"))
                        && query.next()) okTag = query.value(0).toInt() == 1;
                    // created_at 单位为毫秒（2020-01-01 之后），秒单位错误会落回 1970 附近
                    if (query.exec(QStringLiteral("SELECT created_at FROM scores WHERE title = 'MERGE-SMOKE-SCORE'"))
                        && query.next()) okTime = query.value(0).toLongLong() > 1577836800000LL;
                }
                database.close();
            }
            QSqlDatabase::removeDatabase(connection);
            if (!okScore || !okFolder || !okTag || !okTime) {
                qWarning() << "[merge-smoke] FAIL assert: score" << okScore << "folder" << okFolder << "tag" << okTag << "time" << okTime;
                return 1;
            }
        }
        {
            bool foundFolder = false;
            bool foundTag = false;
            for (int i = 0; i < libraryService.folders()->rowCount(); ++i) {
                if (libraryService.folders()->get(i).toMap().value(QStringLiteral("name")).toString()
                    == QStringLiteral("MERGE-SMOKE-FOLDER")) foundFolder = true;
            }
            for (int i = 0; i < libraryService.tags()->rowCount(); ++i) {
                if (libraryService.tags()->get(i).toMap().value(QStringLiteral("name")).toString()
                    == QStringLiteral("MERGE-SMOKE-TAG")) foundTag = true;
            }
            if (!foundFolder || !foundTag) {
                qWarning() << "[merge-smoke] FAIL folders/tags refresh: folder" << foundFolder << "tag" << foundTag;
                return 1;
            }
        }

        // 二次合并：同一备份再次导入 → 文件内容重复 → 哈希判重 → 冲突 → 跳过，不新增
        bool conflicted = false;
        QObject::connect(&libraryService, &LibraryService::mergeConflict, &libraryService,
            [&conflicted, &libraryService](const QString&, const QString&, int, int) {
                conflicted = true;
                libraryService.resolveMergeConflict(QStringLiteral("skip"), true);
            });
        const auto countBeforeSecond = []() {
            const auto connection = QStringLiteral("notera_merge_smoke_count");
            int count = -1;
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(AppDataPaths::databaseDirectory() + QStringLiteral("/notera.db"));
                if (database.open()) {
                    {
                        QSqlQuery query(database);
                        if (query.exec(QStringLiteral("SELECT COUNT(*) FROM scores WHERE title LIKE 'MERGE-SMOKE%'"))
                            && query.next()) count = query.value(0).toInt();
                    }
                    database.close();
                }
            }
            QSqlDatabase::removeDatabase(connection);
            return count;
        }();
        const auto mergeError2 = libraryService.importDatabaseBackupMerged(QUrl::fromLocalFile(zipPath));
        const auto countAfterSecond = []() {
            const auto connection = QStringLiteral("notera_merge_smoke_count2");
            int count = -1;
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(AppDataPaths::databaseDirectory() + QStringLiteral("/notera.db"));
                if (database.open()) {
                    {
                        QSqlQuery query(database);
                        if (query.exec(QStringLiteral("SELECT COUNT(*) FROM scores WHERE title LIKE 'MERGE-SMOKE%'"))
                            && query.next()) count = query.value(0).toInt();
                    }
                    database.close();
                }
            }
            QSqlDatabase::removeDatabase(connection);
            return count;
        }();
        qWarning() << "[merge-smoke] second merge: conflicted" << conflicted
            << "before" << countBeforeSecond << "after" << countAfterSecond
            << "err" << mergeError2;
        if (!mergeError2.isEmpty()) return 1;
        if (!conflicted || countBeforeSecond != countAfterSecond) return 1;
        // 清理测试数据，避免污染开发库
        {
            const auto connection = QStringLiteral("notera_merge_smoke_cleanup");
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(AppDataPaths::databaseDirectory() + QStringLiteral("/notera.db"));
                if (database.open()) {
                    QVariantList paths;
                    {
                        QSqlQuery query(database);
                        query.exec(QStringLiteral("SELECT id, file_path FROM scores WHERE title LIKE 'MERGE-SMOKE%'"));
                        while (query.next()) {
                            paths.append(query.value(1).toString());
                            paths.append(AppDataPaths::thumbnailDirectory() + QLatin1Char('/')
                                + query.value(0).toString() + QStringLiteral(".png"));
                        }
                        query.exec(QStringLiteral("DELETE FROM scores WHERE title LIKE 'MERGE-SMOKE%'"));
                        query.exec(QStringLiteral("DELETE FROM folders WHERE name LIKE 'MERGE-SMOKE%'"));
                        query.exec(QStringLiteral("DELETE FROM tags WHERE name LIKE 'MERGE-SMOKE%'"));
                    }
                    database.close();
                    for (const auto& path : paths) QFile::remove(path.toString());
                }
            }
            QSqlDatabase::removeDatabase(connection);
        }
        return 0;
    }

    if (arguments.contains(QStringLiteral("--clipboard-smoke-test"))) {
        // 验证：多选复制/剪切、冲突弹窗不勾选"应用到所有"时单次决策生效、剪切后源消失
        const auto libDir = AppDataPaths::libraryDirectory();
        const auto dbPath = AppDataPaths::databaseDirectory() + QStringLiteral("/notera.db");
        QDir().mkpath(libDir);
        QDir().mkpath(AppDataPaths::databaseDirectory());

        // 1. 预清理
        {
            const auto connection = QStringLiteral("clipboard_smoke_preclean");
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(dbPath);
                if (database.open()) {
                    QVariantList paths;
                    {
                        QSqlQuery query(database);
                        query.exec(QStringLiteral("SELECT file_path FROM scores WHERE title LIKE 'CLIPBOARD-SMOKE%'"));
                        while (query.next()) paths.append(query.value(0).toString());
                        query.exec(QStringLiteral("DELETE FROM scores WHERE title LIKE 'CLIPBOARD-SMOKE%'"));
                        query.exec(QStringLiteral("DELETE FROM folders WHERE name LIKE 'CLIPBOARD-SMOKE%'"));
                    }
                    database.close();
                    for (const auto& p : paths) QFile::remove(p.toString());
                }
            }
            QSqlDatabase::removeDatabase(connection);
        }

        // 2. 创建测试数据：2 个乐谱（根目录）+ 1 个目标文件夹
        QImage testImage(160, 220, QImage::Format_RGB32);
        testImage.fill(Qt::blue);
        const auto file1 = libDir + QStringLiteral("/clipboard-smoke-1.png");
        const auto file2 = libDir + QStringLiteral("/clipboard-smoke-2.png");
        if (!testImage.save(file1) || !testImage.save(file2)) {
            qWarning() << "[clipboard-smoke] FAIL: cannot save test images";
            return 1;
        }
        const auto now = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
        {
            const auto connection = QStringLiteral("clipboard_smoke_setup");
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(dbPath);
                if (!database.open()) {
                    qWarning() << "[clipboard-smoke] FAIL: cannot open database";
                    return 1;
                }
                QSqlQuery insert(database);
                insert.prepare(QStringLiteral("INSERT INTO folders (id, name, created_at, updated_at, parent_id, favorite) VALUES (?,?,?,?,NULL,0)"));
                insert.addBindValue(QStringLiteral("clip-smoke-target"));
                insert.addBindValue(QStringLiteral("CLIPBOARD-SMOKE-TARGET"));
                insert.addBindValue(now);
                insert.addBindValue(now);
                if (!insert.exec()) {
                    qWarning() << "[clipboard-smoke] FAIL: insert folder" << insert.lastError().text();
                    return 1;
                }
                insert.prepare(QStringLiteral("INSERT INTO scores (id, title, composer, file_name, file_path, file_type, page_count, favorite, last_page, created_at, updated_at, folder_id) VALUES (?,?,?,?,?,?,?,?,?,?,?,NULL)"));
                const QList<QPair<QString,QString>> scores = {
                    {QStringLiteral("clip-smoke-1"), QStringLiteral("CLIPBOARD-SMOKE-1")},
                    {QStringLiteral("clip-smoke-2"), QStringLiteral("CLIPBOARD-SMOKE-2")}
                };
                for (const auto& [id, title] : scores) {
                    insert.addBindValue(id);
                    insert.addBindValue(title);
                    insert.addBindValue(QString());
                    insert.addBindValue(QStringLiteral("clipboard-smoke-") + id.right(1) + QStringLiteral(".png"));
                    insert.addBindValue(libDir + QStringLiteral("/clipboard-smoke-") + id.right(1) + QStringLiteral(".png"));
                    insert.addBindValue(QStringLiteral("png"));
                    insert.addBindValue(1);
                    insert.addBindValue(0);
                    insert.addBindValue(1);
                    insert.addBindValue(now);
                    insert.addBindValue(now);
                    if (!insert.exec()) {
                        qWarning() << "[clipboard-smoke] FAIL: insert score" << id << insert.lastError().text();
                        return 1;
                    }
                }
                // 额外创建一个用于剪切测试的乐谱，放在目标文件夹里，名字唯一避免冲突
                testImage.save(libDir + QStringLiteral("/clipboard-smoke-cut.png"));
                QSqlQuery insertCut(database);
                insertCut.prepare(QStringLiteral("INSERT INTO scores (id, title, composer, file_name, file_path, file_type, page_count, favorite, last_page, created_at, updated_at, folder_id) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"));
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
                insertCut.addBindValue(QString()); // 放在根目录，避免干扰复制测试计数
                if (!insertCut.exec()) {
                    qWarning() << "[clipboard-smoke] FAIL: insert cut score" << insertCut.lastError().text();
                    return 1;
                }
                // 文件夹内部冲突测试：创建文件夹 + 内部同名乐谱
                testImage.save(libDir + QStringLiteral("/clip-smoke-inner-score.png"));
                QSqlQuery insertInnerFolder(database);
                insertInnerFolder.prepare(QStringLiteral("INSERT INTO folders (id, name, parent_id, created_at, updated_at) VALUES (?,?,?,?,?)"));
                insertInnerFolder.addBindValue(QStringLiteral("clip-smoke-inner-folder"));
                insertInnerFolder.addBindValue(QStringLiteral("CLIP-SMOKE-INNER-FOLDER"));
                insertInnerFolder.addBindValue(QVariant()); // 根目录 parent_id 必须为 NULL，不能是空字符串
                insertInnerFolder.addBindValue(now);
                insertInnerFolder.addBindValue(now);
                if (!insertInnerFolder.exec()) {
                    qWarning() << "[clipboard-smoke] FAIL: insert inner folder" << insertInnerFolder.lastError().text();
                    return 1;
                }
                QSqlQuery insertInnerScore(database);
                insertInnerScore.prepare(QStringLiteral("INSERT INTO scores (id, title, composer, file_name, file_path, file_type, page_count, favorite, last_page, created_at, updated_at, folder_id) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"));
                insertInnerScore.addBindValue(QStringLiteral("clip-smoke-inner-score"));
                insertInnerScore.addBindValue(QStringLiteral("CLIP-SMOKE-INNER-SCORE"));
                insertInnerScore.addBindValue(QString());
                insertInnerScore.addBindValue(QStringLiteral("clip-smoke-inner-score.png"));
                insertInnerScore.addBindValue(libDir + QStringLiteral("/clip-smoke-inner-score.png"));
                insertInnerScore.addBindValue(QStringLiteral("png"));
                insertInnerScore.addBindValue(1);
                insertInnerScore.addBindValue(0);
                insertInnerScore.addBindValue(1);
                insertInnerScore.addBindValue(now);
                insertInnerScore.addBindValue(now);
                insertInnerScore.addBindValue(QStringLiteral("clip-smoke-inner-folder"));
                if (!insertInnerScore.exec()) {
                    qWarning() << "[clipboard-smoke] FAIL: insert inner score" << insertInnerScore.lastError().text();
                    return 1;
                }
                // 嵌套文件夹冲突测试：A 内含 B，B 内含乐谱
                testImage.save(libDir + QStringLiteral("/nested-score.png"));
                QSqlQuery insertNestedA(database);
                insertNestedA.prepare(QStringLiteral("INSERT INTO folders (id, name, parent_id, created_at, updated_at) VALUES (?,?,?,?,?)"));
                insertNestedA.addBindValue(QStringLiteral("nested-a"));
                insertNestedA.addBindValue(QStringLiteral("NESTED-A"));
                insertNestedA.addBindValue(QVariant()); // 根目录 parent_id 必须为 NULL
                insertNestedA.addBindValue(now);
                insertNestedA.addBindValue(now);
                if (!insertNestedA.exec()) {
                    qWarning() << "[clipboard-smoke] FAIL: insert nested-a" << insertNestedA.lastError().text();
                    return 1;
                }
                QSqlQuery insertNestedB(database);
                insertNestedB.prepare(QStringLiteral("INSERT INTO folders (id, name, parent_id, created_at, updated_at) VALUES (?,?,?,?,?)"));
                insertNestedB.addBindValue(QStringLiteral("nested-b"));
                insertNestedB.addBindValue(QStringLiteral("NESTED-B"));
                insertNestedB.addBindValue(QStringLiteral("nested-a"));
                insertNestedB.addBindValue(now);
                insertNestedB.addBindValue(now);
                if (!insertNestedB.exec()) {
                    qWarning() << "[clipboard-smoke] FAIL: insert nested-b" << insertNestedB.lastError().text();
                    return 1;
                }
                QSqlQuery insertNestedScore(database);
                insertNestedScore.prepare(QStringLiteral("INSERT INTO scores (id, title, composer, file_name, file_path, file_type, page_count, favorite, last_page, created_at, updated_at, folder_id) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"));
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
                if (!insertNestedScore.exec()) {
                    qWarning() << "[clipboard-smoke] FAIL: insert nested score" << insertNestedScore.lastError().text();
                    return 1;
                }
                // 逐个冲突测试：文件夹内含 2 个乐谱，用于验证不勾选应用到所有时逐个弹窗
                QSqlQuery insertMultiFolder(database);
                insertMultiFolder.prepare(QStringLiteral("INSERT INTO folders (id, name, parent_id, created_at, updated_at) VALUES (?,?,?,?,?)"));
                insertMultiFolder.addBindValue(QStringLiteral("multi-conflict-folder"));
                insertMultiFolder.addBindValue(QStringLiteral("MULTI-CONFLICT-FOLDER"));
                insertMultiFolder.addBindValue(QVariant());
                insertMultiFolder.addBindValue(now);
                insertMultiFolder.addBindValue(now);
                if (!insertMultiFolder.exec()) {
                    qWarning() << "[clipboard-smoke] FAIL: insert multi folder" << insertMultiFolder.lastError().text();
                    return 1;
                }
                for (int i = 1; i <= 2; ++i) {
                    const auto scoreId = QStringLiteral("multi-conflict-%1").arg(i);
                    const auto scoreName = QStringLiteral("MULTI-CONFLICT-%1").arg(i);
                    const auto fileName = QStringLiteral("multi-conflict-%1.png").arg(i);
                    testImage.save(libDir + "/" + fileName);
                    QSqlQuery insertMultiScore(database);
                    insertMultiScore.prepare(QStringLiteral("INSERT INTO scores (id, title, composer, file_name, file_path, file_type, page_count, favorite, last_page, created_at, updated_at, folder_id) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"));
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
                    if (!insertMultiScore.exec()) {
                        qWarning() << "[clipboard-smoke] FAIL: insert multi score" << i << insertMultiScore.lastError().text();
                        return 1;
                    }
                }
                database.close();
            }
            QSqlDatabase::removeDatabase(connection);
        }

        // 3. 多选复制到目标文件夹（无冲突）
        libraryService.goToLibraryRoot();
        libraryService.enterFolder(QStringLiteral("clip-smoke-target"));
        if (libraryService.currentFolderId() != QStringLiteral("clip-smoke-target")) {
            qWarning() << "[clipboard-smoke] FAIL: cannot enter target folder";
            return 1;
        }
        libraryService.copyItems({QStringLiteral("clip-smoke-1"), QStringLiteral("clip-smoke-2")});
        if (libraryService.clipboardItems().size() != 2 || libraryService.clipboardMode() != QStringLiteral("copy")) {
            qWarning() << "[clipboard-smoke] FAIL: clipboard not set after multi-copy";
            return 1;
        }
        libraryService.pasteItems();
        {
            const auto inTarget = libraryService.scoresInFolder(QStringLiteral("clip-smoke-target"));
            if (inTarget.size() != 2) {
                qWarning() << "[clipboard-smoke] FAIL: expected 2 scores after multi-copy, got" << inTarget.size();
                return 1;
            }
        }

        // 4. 再次复制到同一目标文件夹（有冲突），不勾选"应用到所有"：
        //    第一个冲突选"保留两者"(rename)，第二个选"跳过"(skip)
        libraryService.copyItems({QStringLiteral("clip-smoke-1"), QStringLiteral("clip-smoke-2")});
        libraryService.pasteItems(); // 遇到第一个冲突，emit pasteConflict 后暂停
        // 不勾选 applyToAll，选 rename → 应只对当前项生效，然后遇到第二个冲突暂停
        libraryService.resolvePasteConflict(QStringLiteral("rename"), false);
        // 第二个冲突选 skip → 应跳过，完成
        libraryService.resolvePasteConflict(QStringLiteral("skip"), false);
        {
            const auto inTarget = libraryService.scoresInFolder(QStringLiteral("clip-smoke-target"));
            // 原始 2 个 + 重命名 1 个（CLIPBOARD-SMOKE-1 副本）= 3 个；第二个被跳过
            if (inTarget.size() != 3) {
                qWarning() << "[clipboard-smoke] FAIL: expected 3 scores after conflict (rename+skip), got" << inTarget.size();
                for (const auto& s : inTarget) qWarning() << "  " << s.toMap().value("title").toString();
                return 1;
            }
            bool hasRenamed = false;
            for (const auto& s : inTarget) {
                const auto t = s.toMap().value(QStringLiteral("title")).toString();
                if (t.startsWith(QStringLiteral("CLIPBOARD-SMOKE-1")) && t != QStringLiteral("CLIPBOARD-SMOKE-1")) hasRenamed = true;
            }
            if (!hasRenamed) {
                qWarning() << "[clipboard-smoke] FAIL: renamed copy not found";
                return 1;
            }
        }

        // 4b. 同目录复制乐谱（对齐 Windows）：复制到同一文件夹应弹冲突窗，而不是静默生成“ - 副本”
        {
            QObject ctx;
            QEventLoop copyLoop;
            bool conflictFired = false;
            QObject::connect(&libraryService, &LibraryService::pasteConflict, &ctx, [&](const QString&, const QString&, int, int) {
                conflictFired = true;
                copyLoop.quit();
            });
            QObject::connect(&libraryService, &LibraryService::pasteFinished, &ctx, [&](int) { copyLoop.quit(); });
            QTimer::singleShot(800, &copyLoop, &QEventLoop::quit);
            libraryService.copyItems({QStringLiteral("clip-smoke-1")});
            libraryService.goToLibraryRoot();
            libraryService.pasteItems(); // 同目录复制：应弹冲突窗
            copyLoop.exec();
            if (!conflictFired) {
                qWarning() << "[clipboard-smoke] FAIL: same-folder score copy should fire conflict";
                return 1;
            }
            // 冲突未解决前不应产生重复项
            {
                const auto inRoot = libraryService.scoresInFolder(QString());
                int count = 0;
                for (const auto& s : inRoot) {
                    if (s.toMap().value(QStringLiteral("title")).toString() == QStringLiteral("CLIPBOARD-SMOKE-1")) ++count;
                }
                if (count != 1) {
                    qWarning() << "[clipboard-smoke] FAIL: same-folder copy should not duplicate before resolving conflict";
                    return 1;
                }
            }
            // 选“保留两者”：新增“(2)”副本，源不被删除
            libraryService.resolvePasteConflict(QStringLiteral("rename"), false);
            {
                const auto inRoot = libraryService.scoresInFolder(QString());
                bool foundOriginal = false;
                bool foundCopy = false;
                for (const auto& s : inRoot) {
                    const auto t = s.toMap().value(QStringLiteral("title")).toString();
                    if (t == QStringLiteral("CLIPBOARD-SMOKE-1")) foundOriginal = true;
                    if (t == QStringLiteral("CLIPBOARD-SMOKE-1 (2)")) foundCopy = true;
                }
                if (!foundOriginal || !foundCopy) {
                    qWarning() << "[clipboard-smoke] FAIL: expected original + ' (2)' copy for same-folder score copy";
                    return 1;
                }
            }
            // 再次同目录复制，选“替换”：替换自身无意义，不应删除源文件也不应新增重复项
            {
                QObject ctx2;
                QEventLoop loop2;
                bool conflictFired2 = false;
                QObject::connect(&libraryService, &LibraryService::pasteConflict, &ctx2, [&](const QString&, const QString&, int, int) {
                    conflictFired2 = true;
                    loop2.quit();
                });
                QObject::connect(&libraryService, &LibraryService::pasteFinished, &ctx2, [&](int) { loop2.quit(); });
                QTimer::singleShot(800, &loop2, &QEventLoop::quit);
                libraryService.copyItems({QStringLiteral("clip-smoke-1")});
                libraryService.goToLibraryRoot();
                libraryService.pasteItems();
                loop2.exec();
                if (!conflictFired2) {
                    qWarning() << "[clipboard-smoke] FAIL: same-folder score copy should fire conflict again";
                    return 1;
                }
                libraryService.resolvePasteConflict(QStringLiteral("overwrite"), false);
                const auto inRoot = libraryService.scoresInFolder(QString());
                bool foundOriginal = false;
                bool foundCopy = false;
                for (const auto& s : inRoot) {
                    const auto t = s.toMap().value(QStringLiteral("title")).toString();
                    if (t == QStringLiteral("CLIPBOARD-SMOKE-1")) foundOriginal = true;
                    if (t == QStringLiteral("CLIPBOARD-SMOKE-1 (2)")) foundCopy = true;
                }
                // 源必须保留，且不应出现“(3)”或删除“(2)”
                if (!foundOriginal || !foundCopy) {
                    qWarning() << "[clipboard-smoke] FAIL: same-folder overwrite should be a no-op preserving all items";
                    return 1;
                }
            }
        }

        // 5. 测试剪切：把根目录的 CLIPBOARD-SMOKE-CUT 剪切到目标文件夹，验证源消失、目标出现
        libraryService.goToLibraryRoot();
        libraryService.cutItems({QStringLiteral("clip-smoke-cut")});
        if (libraryService.clipboardMode() != QStringLiteral("cut")) {
            qWarning() << "[clipboard-smoke] FAIL: clipboard mode not cut";
            return 1;
        }
        libraryService.enterFolder(QStringLiteral("clip-smoke-target"));
        libraryService.pasteItems(); // 目标文件夹无同名冲突，直接移动
        {
            const auto inRoot = libraryService.scoresInFolder(QString());
            bool foundInRoot = false;
            for (const auto& s : inRoot) {
                if (s.toMap().value(QStringLiteral("id")).toString() == QStringLiteral("clip-smoke-cut")) foundInRoot = true;
            }
            const auto inTarget = libraryService.scoresInFolder(QStringLiteral("clip-smoke-target"));
            bool foundInTarget = false;
            for (const auto& s : inTarget) {
                if (s.toMap().value(QStringLiteral("id")).toString() == QStringLiteral("clip-smoke-cut")) foundInTarget = true;
            }
            if (foundInRoot || !foundInTarget) {
                qWarning() << "[clipboard-smoke] FAIL: cut did not move score (inRoot=" << foundInRoot << " inTarget=" << foundInTarget << ")";
                return 1;
            }
            // 剪切完成后 clipboard 应被清空
            if (libraryService.clipboardMode() != QStringLiteral("none") || !libraryService.clipboardItems().isEmpty()) {
                qWarning() << "[clipboard-smoke] FAIL: clipboard not cleared after cut paste";
                return 1;
            }
        }

        // 5b. 移动（拖拽 / 菜单“移动到文件夹”）遇同名冲突（对齐 Windows）：弹冲突窗而非静默重复
        {
            QObject ctx;
            QEventLoop moveLoop;
            bool conflictFired = false;
            QObject::connect(&libraryService, &LibraryService::pasteConflict, &ctx, [&](const QString&, const QString&, int, int) {
                conflictFired = true;
                moveLoop.quit();
            });
            QObject::connect(&libraryService, &LibraryService::pasteFinished, &ctx, [&](int) { moveLoop.quit(); });
            QTimer::singleShot(800, &moveLoop, &QEventLoop::quit);
            libraryService.moveItems({QStringLiteral("clip-smoke-2")}, QStringLiteral("clip-smoke-target"));
            moveLoop.exec();
            if (!conflictFired) {
                qWarning() << "[clipboard-smoke] FAIL: move with same-name target should fire conflict";
                return 1;
            }
            libraryService.resolvePasteConflict(QStringLiteral("rename"), false);
            // 验证：根目录不再有 CLIPBOARD-SMOKE-2；目标内有原 CLIPBOARD-SMOKE-2 与“(2)”副本
            const auto inRoot = libraryService.scoresInFolder(QString());
            const auto inTarget = libraryService.scoresInFolder(QStringLiteral("clip-smoke-target"));
            bool rootHasOriginal = false;
            for (const auto& s : inRoot) {
                if (s.toMap().value(QStringLiteral("title")).toString() == QStringLiteral("CLIPBOARD-SMOKE-2")) rootHasOriginal = true;
            }
            bool targetHasOriginal = false;
            bool targetHasRenamed = false;
            for (const auto& s : inTarget) {
                const auto t = s.toMap().value(QStringLiteral("title")).toString();
                if (t == QStringLiteral("CLIPBOARD-SMOKE-2")) targetHasOriginal = true;
                if (t == QStringLiteral("CLIPBOARD-SMOKE-2 (2)")) targetHasRenamed = true;
            }
            if (rootHasOriginal || !targetHasOriginal || !targetHasRenamed) {
                qWarning() << "[clipboard-smoke] FAIL: move-with-conflict rename did not move correctly (rootHasOriginal=" << rootHasOriginal << " targetHasOriginal=" << targetHasOriginal << " targetHasRenamed=" << targetHasRenamed << ")";
                return 1;
            }
        }

        // 同目录复制文件夹（对齐 Windows）：复制到同一父文件夹应弹冲突窗，选"保留两者"生成" (2)"副本，内部乐谱一并复制
        {
            QObject ctx; // 块结束时自动断开所有以此为 context 的连接
            QEventLoop copyLoop;
            bool folderConflictFired = false;
            QObject::connect(&libraryService, &LibraryService::pasteFolderConflict, &ctx, [&](const QString&, const QString&, int, int) {
                folderConflictFired = true;
                copyLoop.quit();
            });
            QObject::connect(&libraryService, &LibraryService::pasteFinished, &ctx, [&](int) { copyLoop.quit(); });
            QTimer::singleShot(800, &copyLoop, &QEventLoop::quit);
            libraryService.copyItems({QStringLiteral("clip-smoke-inner-folder")});
            libraryService.goToLibraryRoot();
            libraryService.pasteItems(); // 同目录复制：应弹冲突窗
            copyLoop.exec();
            if (!folderConflictFired) {
                qWarning() << "[clipboard-smoke] FAIL: same-folder folder copy should fire folder conflict";
                return 1;
            }
            // 冲突未解决前不应产生重复文件夹
            {
                const auto rootFolders = libraryService.childFolders(QString());
                int count = 0;
                for (const auto& f : rootFolders) {
                    if (f.toMap().value(QStringLiteral("name")).toString() == QStringLiteral("CLIP-SMOKE-INNER-FOLDER")) ++count;
                }
                if (count != 1) {
                    qWarning() << "[clipboard-smoke] FAIL: same-folder folder copy should not duplicate before resolving conflict";
                    return 1;
                }
            }
            libraryService.resolvePasteFolderConflict(QStringLiteral("rename"), false);
            // 验证：根目录下有原始文件夹和“(2)”副本文件夹，副本内含内部乐谱
            const auto rootFolders = libraryService.childFolders(QString());
            bool foundOriginal = false;
            bool foundCopy = false;
            QString copyFolderId;
            for (const auto& f : rootFolders) {
                const auto name = f.toMap().value(QStringLiteral("name")).toString();
                if (name == QStringLiteral("CLIP-SMOKE-INNER-FOLDER")) foundOriginal = true;
                if (name == QStringLiteral("CLIP-SMOKE-INNER-FOLDER (2)")) {
                    foundCopy = true;
                    copyFolderId = f.toMap().value(QStringLiteral("id")).toString();
                }
            }
            if (!foundOriginal || !foundCopy) {
                qWarning() << "[clipboard-smoke] FAIL: expected original and ' (2)' copy after same-folder copy";
                return 1;
            }
            const auto copyScores = libraryService.scoresInFolder(copyFolderId);
            if (copyScores.size() != 1) {
                qWarning() << "[clipboard-smoke] FAIL: copied folder should contain inner score, got" << copyScores.size();
                return 1;
            }
        }

        // 嵌套文件夹同目录复制（对齐 Windows）：A 内含 B，B 内含乐谱；选"保留两者"生成"A (2)"副本，结构完整
        {
            QObject ctx; // 块结束时自动断开所有以此为 context 的连接
            QEventLoop copyLoop;
            bool folderConflictFired = false;
            QObject::connect(&libraryService, &LibraryService::pasteFolderConflict, &ctx, [&](const QString&, const QString&, int, int) {
                folderConflictFired = true;
                copyLoop.quit();
            });
            QObject::connect(&libraryService, &LibraryService::pasteFinished, &ctx, [&](int) { copyLoop.quit(); });
            QTimer::singleShot(800, &copyLoop, &QEventLoop::quit);
            libraryService.copyItems({QStringLiteral("nested-a")});
            libraryService.goToLibraryRoot();
            libraryService.pasteItems(); // 同目录复制：应弹冲突窗
            copyLoop.exec();
            if (!folderConflictFired) {
                qWarning() << "[clipboard-smoke] FAIL: same-folder nested copy should fire folder conflict";
                return 1;
            }
            libraryService.resolvePasteFolderConflict(QStringLiteral("rename"), false);
            // 验证：根目录下存在“NESTED-A (2)”，其内 NESTED-B 及乐谱结构完整
            const auto rootFolders = libraryService.childFolders(QString());
            QString copyId;
            for (const auto& f : rootFolders) {
                if (f.toMap().value(QStringLiteral("name")).toString() == QStringLiteral("NESTED-A (2)")) {
                    copyId = f.toMap().value(QStringLiteral("id")).toString();
                    break;
                }
            }
            if (copyId.isEmpty()) {
                qWarning() << "[clipboard-smoke] FAIL: expected 'NESTED-A (2)' copy";
                return 1;
            }
            bool foundNestedB = false;
            const auto subFolders = libraryService.childFolders(copyId);
            for (const auto& f : subFolders) {
                if (f.toMap().value(QStringLiteral("name")).toString() != QStringLiteral("NESTED-B")) continue;
                foundNestedB = true;
                const auto bScores = libraryService.scoresInFolder(f.toMap().value(QStringLiteral("id")).toString());
                if (bScores.size() != 1) {
                    qWarning() << "[clipboard-smoke] FAIL: copied NESTED-B should contain its score";
                    return 1;
                }
            }
            if (!foundNestedB) {
                qWarning() << "[clipboard-smoke] FAIL: copied folder missing NESTED-B";
                return 1;
            }
        }

        // 跨文件夹复制同名文件夹（对齐 Windows）：文件夹级冲突弹窗，选"保留两者"生成" (2)"副本
        {
            QObject ctx;
            libraryService.createFolder(QStringLiteral("MULTI-CONFLICT-DEST"));
            QString destId;
            for (int i = 0; i < libraryService.folders()->rowCount(); ++i) {
                const auto f = libraryService.folders()->get(i).toMap();
                if (f.value(QStringLiteral("name")).toString() == QStringLiteral("MULTI-CONFLICT-DEST")) {
                    destId = f.value(QStringLiteral("itemId")).toString();
                    break;
                }
            }
            if (destId.isEmpty()) {
                qWarning() << "[clipboard-smoke] FAIL: cannot find MULTI-CONFLICT-DEST";
                return 1;
            }
            // 在 DEST 内建同名子文件夹，制造跨文件夹同名冲突
            libraryService.enterFolder(destId);
            libraryService.createFolder(QStringLiteral("MULTI-CONFLICT-FOLDER"));

            int folderConflictCount = 0;
            QEventLoop multiLoop;
            QObject::connect(&libraryService, &LibraryService::pasteFolderConflict, &ctx, [&](const QString&, const QString&, int, int) {
                ++folderConflictCount;
                // 第一个冲突：选保留两者(rename)，不应用到所有
                libraryService.resolvePasteFolderConflict(QStringLiteral("rename"), false);
            });
            QObject::connect(&libraryService, &LibraryService::pasteFinished, &ctx, [&](int) { multiLoop.quit(); });
            QTimer::singleShot(1500, &multiLoop, &QEventLoop::quit);
            libraryService.copyItems({QStringLiteral("multi-conflict-folder")});
            libraryService.pasteItems(); // 粘贴到当前文件夹 DEST：DEST 内已有同名文件夹 → 冲突弹窗
            multiLoop.exec();
            if (folderConflictCount < 1) {
                qWarning() << "[clipboard-smoke] FAIL: expected at least 1 folder conflict dialog for cross-folder copy, got" << folderConflictCount;
                return 1;
            }
            // 验证：DEST 内同时有原同名文件夹与“(2)”副本
            const auto destSubs = libraryService.childFolders(destId);
            bool foundOriginal = false;
            bool foundRenamed = false;
            for (const auto& f : destSubs) {
                const auto name = f.toMap().value(QStringLiteral("name")).toString();
                if (name == QStringLiteral("MULTI-CONFLICT-FOLDER")) foundOriginal = true;
                if (name == QStringLiteral("MULTI-CONFLICT-FOLDER (2)")) foundRenamed = true;
            }
            if (!foundOriginal || !foundRenamed) {
                qWarning() << "[clipboard-smoke] FAIL: expected both original and renamed folder inside DEST";
                return 1;
            }
            libraryService.goToLibraryRoot();
        }

        // 剪切文件夹（无冲突）：验证整个文件夹被移动到目标，文件不丢失，源文件夹被清理
        {
            QObject ctx;
            libraryService.createFolder(QStringLiteral("CUT-DEST"));
            QString destId;
            for (int i = 0; i < libraryService.folders()->rowCount(); ++i) {
                const auto f = libraryService.folders()->get(i).toMap();
                if (f.value(QStringLiteral("name")).toString() == QStringLiteral("CUT-DEST")) {
                    destId = f.value(QStringLiteral("itemId")).toString();
                    break;
                }
            }
            if (destId.isEmpty()) {
                qWarning() << "[clipboard-smoke] FAIL: cannot find CUT-DEST";
                return 1;
            }
            libraryService.cutItems({QStringLiteral("multi-conflict-folder")});
            libraryService.goToLibraryRoot();
            libraryService.enterFolder(destId);
            QEventLoop cutFolderLoop;
            QObject::connect(&libraryService, &LibraryService::pasteFinished, &ctx, [&](int) { cutFolderLoop.quit(); });
            QTimer::singleShot(1000, &cutFolderLoop, &QEventLoop::quit);
            libraryService.pasteItems();
            cutFolderLoop.exec();
            // 验证：目标文件夹内有 MULTI-CONFLICT-FOLDER
            const auto destSubs = libraryService.childFolders(destId);
            bool foundMoved = false;
            for (const auto& f : destSubs) {
                if (f.toMap().value(QStringLiteral("name")).toString() == QStringLiteral("MULTI-CONFLICT-FOLDER")) {
                    foundMoved = true;
                    break;
                }
            }
            if (!foundMoved) {
                qWarning() << "[clipboard-smoke] FAIL: cut folder did not appear in destination (file lost?)";
                return 1;
            }
            // 验证：根目录不再有 MULTI-CONFLICT-FOLDER（已被移动）
            libraryService.goToLibraryRoot();
            const auto rootFolders = libraryService.childFolders(QString());
            bool foundInRoot = false;
            for (const auto& f : rootFolders) {
                if (f.toMap().value(QStringLiteral("name")).toString() == QStringLiteral("MULTI-CONFLICT-FOLDER")) {
                    foundInRoot = true;
                    break;
                }
            }
            if (foundInRoot) {
                qWarning() << "[clipboard-smoke] FAIL: cut folder still in root (not moved)";
                return 1;
            }
            // 验证：移动后的文件夹内仍有 2 个乐谱（文件未丢失）
            QString movedFolderId;
            for (const auto& f : destSubs) {
                if (f.toMap().value(QStringLiteral("name")).toString() == QStringLiteral("MULTI-CONFLICT-FOLDER")) {
                    movedFolderId = f.toMap().value(QStringLiteral("id")).toString();
                    break;
                }
            }
            const auto movedScores = libraryService.scoresInFolder(movedFolderId);
            if (movedScores.size() != 2) {
                qWarning() << "[clipboard-smoke] FAIL: moved folder expected 2 scores, got" << movedScores.size() << "(file lost!)";
                return 1;
            }
        }

        qWarning() << "[clipboard-smoke] PASS: multi-copy, conflict per-item, cut, same-folder-copy-conflict-dialog, nested-same-folder-copy-conflict, cross-folder-folder-conflict, cut-folder-no-loss all work";
        return 0;
    }

    QTemporaryFile importSmokeFile;
    if (arguments.contains(QStringLiteral("--import-smoke-test"))) {
        importSmokeFile.setFileTemplate(QDir::tempPath() + QStringLiteral("/notera-import-XXXXXX.png"));
        if (!importSmokeFile.open()) {
            return 1;
        }
        const auto imagePath = importSmokeFile.fileName();
        importSmokeFile.close();
        QImage image(1754, 2480, QImage::Format_Indexed8);
        image.setColorTable({qRgb(255, 255, 255), qRgb(0, 0, 0)});
        image.fill(0);
        if (!image.save(imagePath)) {
            return 1;
        }
        const auto previousCount = libraryService.scores()->rowCount();
        libraryService.importLocalFile(QUrl::fromLocalFile(imagePath));
        if (libraryService.scores()->rowCount() != previousCount + 1) return 1;
        // 再次导入同名文件：对齐 Windows，应触发导入冲突弹窗，而不是静默产生重复项
        {
            QObject ctx;
            bool conflictFired = false;
            QObject::connect(&libraryService, &LibraryService::importConflict, &ctx,
                [&](const QString&, const QString&, int, int) { conflictFired = true; });
            libraryService.importLocalFile(QUrl::fromLocalFile(imagePath));
            if (!conflictFired) return 1;
            // 冲突未解决前不应产生重复项
            if (libraryService.scores()->rowCount() != previousCount + 1) return 1;
            // 选“替换”：总数不变（替换而非新增）
            libraryService.resolveImportConflict(QStringLiteral("overwrite"), false);
            if (libraryService.scores()->rowCount() != previousCount + 1) return 1;
            // 选“保留两者”：新增一个“(2)”副本
            conflictFired = false;
            libraryService.importLocalFile(QUrl::fromLocalFile(imagePath));
            if (!conflictFired) return 1;
            libraryService.resolveImportConflict(QStringLiteral("rename"), false);
            if (libraryService.scores()->rowCount() != previousCount + 2) return 1;
        }
        return 0;
    }

    if (arguments.contains(QStringLiteral("--tag-smoke-test"))) {
        // 验证标签全生命周期：创建/重名拒绝/大小写不敏感/乐谱与文件夹打标/标签筛选/删除级联
        const auto libDir = AppDataPaths::libraryDirectory();
        const auto dbPath = AppDataPaths::databaseDirectory() + QStringLiteral("/notera.db");
        // 1. 清理旧测试数据
        {
            const auto connection = QStringLiteral("tag_smoke_preclean");
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(dbPath);
                if (database.open()) {
                    QSqlQuery query(database);
                    query.exec(QStringLiteral("DELETE FROM scores WHERE title LIKE 'TAG-SMOKE%'"));
                    query.exec(QStringLiteral("DELETE FROM folders WHERE name LIKE 'TAG-SMOKE%'"));
                    query.exec(QStringLiteral("DELETE FROM tags WHERE name LIKE 'TAG-SMOKE%'"));
                    database.close();
                }
            }
            QSqlDatabase::removeDatabase(connection);
        }
        // 2. 通过公共服务 API 创建测试乐谱 + 文件夹（自动刷新模型）
        QImage testImage(100, 100, QImage::Format_RGB32);
        testImage.fill(Qt::green);
        const auto file1 = libDir + QStringLiteral("/TAG-SMOKE-SCORE.png");
        if (!testImage.save(file1)) {
            qWarning() << "[tag-smoke] FAIL: cannot save test image";
            return 1;
        }
        libraryService.createFolder(QStringLiteral("TAG-SMOKE-FOLDER"));
        libraryService.importLocalFile(QUrl::fromLocalFile(file1));
        const auto scoreId = [&dbPath]() {
            const auto connection = QStringLiteral("tag_smoke_lookup_score");
            QString id;
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(dbPath);
                if (database.open()) {
                    QSqlQuery query(database);
                    if (query.exec(QStringLiteral("SELECT id FROM scores WHERE title = 'TAG-SMOKE-SCORE'"))
                        && query.next()) id = query.value(0).toString();
                    database.close();
                }
            }
            QSqlDatabase::removeDatabase(connection);
            return id;
        }();
        const auto folderId = [&dbPath]() {
            const auto connection = QStringLiteral("tag_smoke_lookup_folder");
            QString id;
            {
                auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
                database.setDatabaseName(dbPath);
                if (database.open()) {
                    QSqlQuery query(database);
                    if (query.exec(QStringLiteral("SELECT id FROM folders WHERE name = 'TAG-SMOKE-FOLDER'"))
                        && query.next()) id = query.value(0).toString();
                    database.close();
                }
            }
            QSqlDatabase::removeDatabase(connection);
            return id;
        }();
        if (scoreId.isEmpty() || folderId.isEmpty()) {
            qWarning() << "[tag-smoke] FAIL: cannot create test score/folder (scoreId=" << scoreId << " folderId=" << folderId << ")";
            return 1;
        }

        // 3. 创建标签
        libraryService.createTag(QStringLiteral("TAG-SMOKE-A"));
        if (libraryService.tags()->count() != 1) {
            qWarning() << "[tag-smoke] FAIL: createTag did not create exactly 1 tag, got" << libraryService.tags()->count();
            return 1;
        }
        const auto tagA = libraryService.tags()->get(0).toMap().value(QStringLiteral("itemId")).toString();
        if (tagA.isEmpty()) return 1;

        // 4. 精确同名重复创建：应被拒绝并提示，而不是静默报"已创建"
        {
            QObject ctx;
            QString errorMessage;
            QObject::connect(&libraryService, &LibraryService::errorOccurred, &ctx, [&](const QString& m) { errorMessage = m; });
            libraryService.createTag(QStringLiteral("TAG-SMOKE-A"));
            if (libraryService.tags()->count() != 1) {
                qWarning() << "[tag-smoke] FAIL: exact duplicate createTag created extra tag";
                return 1;
            }
            if (errorMessage.isEmpty()) {
                qWarning() << "[tag-smoke] FAIL: exact duplicate createTag should reject with error";
                return 1;
            }
        }

        // 5. 大小写不同的同名标签：应视为重复拒绝（对齐 Windows 大小写不敏感）
        {
            QObject ctx;
            QString errorMessage;
            QObject::connect(&libraryService, &LibraryService::errorOccurred, &ctx, [&](const QString& m) { errorMessage = m; });
            libraryService.createTag(QStringLiteral("tag-smoke-a"));
            if (libraryService.tags()->count() != 1) {
                qWarning() << "[tag-smoke] FAIL: case-insensitive duplicate createTag should be rejected, got" << libraryService.tags()->count();
                return 1;
            }
        }

        // 6. 重命名为已存在的标签名（含大小写变体）：应拒绝且名称不变
        libraryService.createTag(QStringLiteral("TAG-SMOKE-B"));
        if (libraryService.tags()->count() != 2) return 1;
        const auto tagB = libraryService.tags()->get(1).toMap().value(QStringLiteral("itemId")).toString();
        {
            QObject ctx;
            QString errorMessage;
            QObject::connect(&libraryService, &LibraryService::errorOccurred, &ctx, [&](const QString& m) { errorMessage = m; });
            libraryService.renameTag(tagB, QStringLiteral("TAG-SMOKE-A"));
            if (errorMessage.isEmpty()) {
                qWarning() << "[tag-smoke] FAIL: renameTag to existing name should reject";
                return 1;
            }
        }
        // 重命名为 A 的大小写变体：也应拒绝
        {
            QObject ctx;
            QString errorMessage;
            QObject::connect(&libraryService, &LibraryService::errorOccurred, &ctx, [&](const QString& m) { errorMessage = m; });
            libraryService.renameTag(tagB, QStringLiteral("tag-smoke-a"));
            if (errorMessage.isEmpty()) {
                qWarning() << "[tag-smoke] FAIL: renameTag to case-variant of existing tag should reject";
                return 1;
            }
        }
        // 名称应保持未变
        {
            QString error;
            const auto tags = libraryService.tags()->get(0).toMap().value(QStringLiteral("itemId")).toString();
            (void)tags;
            bool foundB = false;
            for (int i = 0; i < libraryService.tags()->count(); ++i) {
                const auto t = libraryService.tags()->get(i).toMap();
                if (t.value(QStringLiteral("itemId")).toString() == tagB && t.value(QStringLiteral("name")).toString() == QStringLiteral("TAG-SMOKE-B")) foundB = true;
            }
            if (!foundB) {
                qWarning() << "[tag-smoke] FAIL: renameTag to duplicate changed the tag name";
                return 1;
            }
        }

        // 7. 给乐谱和文件夹打标签
        libraryService.addItemTag(scoreId, tagA);
        if (!libraryService.itemHasTag(scoreId, tagA)) {
            qWarning() << "[tag-smoke] FAIL: addItemTag score";
            return 1;
        }
        libraryService.addItemTag(folderId, tagA);
        if (!libraryService.itemHasTag(folderId, tagA)) {
            qWarning() << "[tag-smoke] FAIL: addItemTag folder";
            return 1;
        }
        // 移除乐谱标签
        libraryService.removeItemTag(scoreId, tagA);
        if (libraryService.itemHasTag(scoreId, tagA)) {
            qWarning() << "[tag-smoke] FAIL: removeItemTag score";
            return 1;
        }

        // 8. 标签筛选：tag: 模式下应同时显示带该标签的文件夹与乐谱
        libraryService.addItemTag(scoreId, tagA);
        libraryService.setFilterMode(QStringLiteral("tag:") + tagA);
        {
            const auto ids = libraryService.entries()->itemIds();
            if (!ids.contains(scoreId) || !ids.contains(folderId)) {
                qWarning() << "[tag-smoke] FAIL: tag filter should show tagged score and folder";
                return 1;
            }
        }
        libraryService.setFilterMode(QStringLiteral("all"));

        // 9. 删除标签：关联应级联清除，不再能筛选到
        libraryService.deleteTag(tagA);
        if (libraryService.itemHasTag(scoreId, tagA)
            || libraryService.itemHasTag(folderId, tagA)) {
            qWarning() << "[tag-smoke] FAIL: deleteTag should cascade-remove associations";
            return 1;
        }
        if (libraryService.tags()->count() != 1) {
            qWarning() << "[tag-smoke] FAIL: deleteTag should leave only TAG-SMOKE-B, got" << libraryService.tags()->count();
            return 1;
        }
        // 删除后清理剩余测试标签
        libraryService.deleteTag(tagB);
        qWarning() << "[tag-smoke] PASS: create/reject-duplicate/case-insensitive/add/remove/filter/delete all work";
        return 0;
    }

    if (arguments.contains(QStringLiteral("--stitch-smoke-test"))) {
        QTemporaryDir directory(QDir::tempPath() + QStringLiteral("/notera-stitch-XXXXXX"));
        if (!directory.isValid()) {
            return 1;
        }
        const auto firstPath = directory.filePath(QStringLiteral("first.png"));
        const auto secondPath = directory.filePath(QStringLiteral("second.png"));
        QImage first(320, 480, QImage::Format_RGB32);
        QImage second(400, 360, QImage::Format_RGB32);
        first.fill(Qt::white);
        second.fill(Qt::lightGray);
        if (!first.save(firstPath) || !second.save(secondPath)) {
            return 1;
        }
        const auto previousCount = libraryService.scores()->rowCount();
        libraryService.importAndStitchImages({QUrl::fromLocalFile(firstPath).toString(),
            QUrl::fromLocalFile(secondPath).toString()});
        if (libraryService.scores()->rowCount() != previousCount + 1) return 1;
        // 再次拼接同名图片：对齐 Windows，目标已存在同名“拼接乐谱”时应触发导入冲突弹窗，
        // 而不是静默产生重复项；选“保留两者”后生成“(2)”副本
        {
            QObject ctx;
            bool conflictFired = false;
            QObject::connect(&libraryService, &LibraryService::importConflict, &ctx,
                [&](const QString&, const QString&, int, int) { conflictFired = true; });
            libraryService.importAndStitchImages({QUrl::fromLocalFile(firstPath).toString(),
                QUrl::fromLocalFile(secondPath).toString()});
            if (!conflictFired) {
                qWarning() << "[stitch-smoke] FAIL: duplicate stitch import should fire import conflict";
                return 1;
            }
            // 冲突未解决前不应产生重复项
            if (libraryService.scores()->rowCount() != previousCount + 1) return 1;
            libraryService.resolveImportConflict(QStringLiteral("rename"), false);
            if (libraryService.scores()->rowCount() != previousCount + 2) return 1;
        }
        return 0;
    }

    QString renameSmokeFolderId;
    if (arguments.contains(QStringLiteral("--folder-rename-smoke-test"))) {
        libraryService.createFolder(QStringLiteral("重命名前"));
        if (libraryService.folders()->rowCount() != 1) return 1;
        renameSmokeFolderId = libraryService.folders()->get(0).toMap()
            .value(QStringLiteral("itemId")).toString();
        if (renameSmokeFolderId.isEmpty()) return 1;
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("libraryService"), &libraryService);
    engine.rootContext()->setContextProperty(QStringLiteral("metronome"), &metronome);
    engine.loadFromModule("Notera", "Main");

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    if (arguments.contains(QStringLiteral("--folder-rename-smoke-test"))) {
        auto* const root = engine.rootObjects().constFirst();
        QTimer::singleShot(250, root, [root, renameSmokeFolderId, &libraryService] {
            auto findFolderNavItem = [root, &renameSmokeFolderId]() -> QObject* {
                auto* const sidebar = findVisualItem(root, QStringLiteral("sidebar"));
                if (!sidebar) return nullptr;
                const auto visit = [&](auto&& self, QQuickItem* item) -> QObject* {
                    if (item->objectName() == QStringLiteral("folderNavItem")
                        && item->property("navId").toString() == QStringLiteral("folder:") + renameSmokeFolderId) {
                        return item;
                    }
                    for (auto* const child : item->childItems()) {
                        if (auto* const match = self(self, child)) return match;
                    }
                    return nullptr;
                };
                return visit(visit, sidebar);
            };
            auto* const before = findFolderNavItem();
            if (!before || before->property("label").toString() != QStringLiteral("重命名前")) {
                QCoreApplication::exit(1);
                return;
            }
            libraryService.renameFolder(renameSmokeFolderId, QStringLiteral("重命名后"));
            QCoreApplication::processEvents();
            auto* const after = findFolderNavItem();
            QCoreApplication::exit(after && after->property("label").toString() == QStringLiteral("重命名后") ? 0 : 1);
        });
    }

    if (arguments.contains(QStringLiteral("--theme-smoke-test"))) {
        auto* const root = engine.rootObjects().constFirst();
        auto* const selectionBox = findVisualItem(root, QStringLiteral("selectionBox"));
        if (!selectionBox) return 1;
        const auto originalMode = controller.themeMode();
        controller.setThemeMode(0);
        QCoreApplication::processEvents();
        const auto lightBackground = root->property("themeBackground").value<QColor>();
        const auto lightMarqueeFill = selectionBox->property("color").value<QColor>();
        const auto lightMarqueeBorderColor = selectionBox->property("appliedBorderColor").value<QColor>();
        controller.setThemeMode(1);
        QCoreApplication::processEvents();
        const auto darkBackground = root->property("themeBackground").value<QColor>();
        const auto darkMarqueeFill = selectionBox->property("color").value<QColor>();
        const auto darkMarqueeBorderColor = selectionBox->property("appliedBorderColor").value<QColor>();
        controller.setThemeMode(originalMode);
        const auto marqueeColorsAreThemeIndependent = lightMarqueeFill.isValid()
            && lightMarqueeBorderColor.isValid()
            && lightMarqueeFill == darkMarqueeFill
            && lightMarqueeBorderColor == darkMarqueeBorderColor;
        const auto marqueeIsMoreTransparent = lightMarqueeFill.alphaF() > 0.0
            && lightMarqueeFill.alphaF() < (0x22 / 255.0);
        return lightBackground.isValid() && darkBackground.isValid()
                && lightBackground != darkBackground
                && marqueeColorsAreThemeIndependent && marqueeIsMoreTransparent
            ? 0 : 1;
    }

    QTemporaryFile readerSmokeFile;
    if (arguments.contains(QStringLiteral("--reader-smoke-test"))) {
        readerSmokeFile.setFileTemplate(QDir::tempPath() + QStringLiteral("/notera-reader-XXXXXX.png"));
        if (!readerSmokeFile.open()) {
            return 1;
        }
        const auto imagePath = readerSmokeFile.fileName();
        readerSmokeFile.close();
        QImage image(400, 2400, QImage::Format_RGB32);
        image.fill(Qt::white);
        if (!image.save(imagePath)) {
            return 1;
        }
        controller.openScore(QStringLiteral("test-score-1"), QStringLiteral("自动滚动测试"), imagePath, QStringLiteral("png"), 1, QString());
        auto* const root = engine.rootObjects().constFirst();
        QTimer::singleShot(250, root, [root] {
            if (auto* const readerPage = root->findChild<QObject*>(QStringLiteral("readerPage"))) {
                readerPage->setProperty("scrollSpeed", 160.0);
                readerPage->setProperty("autoScrolling", true);
            }
        });
        QTimer::singleShot(1250, root, [root] {
            const auto* const flickable = root->findChild<QObject*>(QStringLiteral("readerFlick"));
            QCoreApplication::exit(flickable && flickable->property("contentY").toDouble() > 0.0 ? 0 : 1);
        });
    }

    QTemporaryFile uiSmokeFile;
    QTemporaryFile uiSmokeSecondFile;
    if (arguments.contains(QStringLiteral("--ui-smoke-test"))) {
        uiSmokeFile.setFileTemplate(QDir::tempPath() + QStringLiteral("/notera-ui-z-XXXXXX.png"));
        uiSmokeSecondFile.setFileTemplate(QDir::tempPath() + QStringLiteral("/notera-ui-a-XXXXXX.png"));
        if (!uiSmokeFile.open() || !uiSmokeSecondFile.open()) {
            return 1;
        }
        const auto imagePath = uiSmokeFile.fileName();
        const auto secondImagePath = uiSmokeSecondFile.fileName();
        uiSmokeFile.close();
        uiSmokeSecondFile.close();
        QImage image(900, 1280, QImage::Format_RGB32);
        image.fill(Qt::white);
        if (!image.save(imagePath) || !image.save(secondImagePath)) {
            return 1;
        }
        libraryService.importLocalFile(QUrl::fromLocalFile(imagePath));
        libraryService.importLocalFile(QUrl::fromLocalFile(secondImagePath));
        libraryService.createFolder(QStringLiteral("Z界面测试文件夹"));
        libraryService.createFolder(QStringLiteral("A界面测试文件夹"));
        libraryService.createTag(QStringLiteral("界面测试标签"));

        auto* const root = engine.rootObjects().constFirst();
        QTimer::singleShot(300, root, [root, &controller, &libraryService] {
            const auto fail = [](const char* step) {
                qWarning() << "UI smoke test failed at" << step;
                QCoreApplication::exit(1);
            };
            auto* const importButton = root->findChild<QQuickItem*>(QStringLiteral("importButton"));
            const auto* const stitchButton = root->findChild<QObject*>(QStringLiteral("stitchButton"));
            if (!importButton || !importButton->isVisible() || importButton->width() < 96.0
                || importButton->property("symbol").toString().length() > 0
                || importButton->property("hoverTransitionDuration").toInt() != 0
                || !stitchButton || stitchButton->property("symbol").toString().length() > 0
                || std::abs(importButton->property("visualContentCenterX").toDouble() - importButton->width() / 2.0) > 1.0
                || std::abs(stitchButton->property("visualContentCenterX").toDouble()
                    - stitchButton->property("width").toDouble() / 2.0) > 1.0) {
                fail("import-button-geometry");
                return;
            }
            auto* const window = qobject_cast<QQuickWindow*>(root);
            if (!window || !window->grabWindow().save(QStringLiteral("notera-library-smoke.png"))) {
                fail("library-screenshot");
                return;
            }

            const auto* const entryCheckBox = findVisualItem(root, QStringLiteral("entryCheckBox"));
            const auto* const favoriteButton = findVisualItem(root, QStringLiteral("favoriteButton"));
            if (!entryCheckBox || !entryCheckBox->isVisible()
                || !clickItem(root, QStringLiteral("entryCheckBox"), Qt::LeftButton)
                || libraryService.selection()->count() != 1) {
                fail("library-entry-checkbox-selection");
                return;
            }
            libraryService.selection()->clear();
            if (!favoriteButton
                || std::abs(entryCheckBox->mapToScene(QPointF(entryCheckBox->width() / 2.0, entryCheckBox->height() / 2.0)).y()
                    - favoriteButton->mapToScene(QPointF(favoriteButton->width() / 2.0, favoriteButton->height() / 2.0)).y()) > 1.0) {
                fail("library-card-actions-alignment");
                return;
            }

            const auto* const libraryNavItem = root->findChild<QObject*>(QStringLiteral("libraryNavItem"));
            if (!libraryNavItem || !libraryNavItem->property("hoverTransitionDuration").isValid()
                || libraryNavItem->property("hoverTransitionDuration").toInt() != 0) {
                fail("sidebar-hover-transition");
                return;
            }
            const auto* const appShell = root->findChild<QObject*>(QStringLiteral("appShell"));
            const auto transitionCountBeforeFilter = appShell
                ? appShell->property("transitionRunCount").toInt() : -1;
            controller.setLibraryFilter(QStringLiteral("recent"));
            QCoreApplication::processEvents();
            controller.setLibraryFilter(QStringLiteral("all"));
            QCoreApplication::processEvents();
            if (!appShell || appShell->property("transitionDuration").toInt() <= 0
                || appShell->property("transitionRunCount").toInt() < transitionCountBeforeFilter + 2) {
                fail("library-section-transition");
                return;
            }
            if (QGuiApplication::windowIcon().isNull()) {
                fail("application-icon");
                return;
            }

            const auto entryTypeRole = libraryService.entries()->roleNames().key("itemType", -1);
            const auto entryTitleRole = libraryService.entries()->roleNames().key("title", -1);
            if (libraryService.entries()->rowCount() != 4
                || libraryService.entries()->data(libraryService.entries()->index(0, 0), entryTypeRole).toString()
                    != QStringLiteral("folder")
                || !libraryService.entries()->data(libraryService.entries()->index(0, 0), entryTitleRole).toString()
                    .startsWith(QStringLiteral("A"))
                || !libraryService.entries()->data(libraryService.entries()->index(1, 0), entryTitleRole).toString()
                    .startsWith(QStringLiteral("Z"))
                || !libraryService.entries()->data(libraryService.entries()->index(2, 0), entryTitleRole).toString()
                    .startsWith(QStringLiteral("notera-ui-a-"))
                || !libraryService.entries()->data(libraryService.entries()->index(3, 0), entryTitleRole).toString()
                    .startsWith(QStringLiteral("notera-ui-z-"))) {
                fail("library-folder-first-file-name-sort");
                return;
            }

            auto* const libraryPage = root->findChild<QObject*>(QStringLiteral("libraryPage"));
            auto* const librarySurface = findVisualItem(root, QStringLiteral("librarySurface"));
            auto* const rubberSelectionGrid = findVisualItem(root, QStringLiteral("browserGrid"));
            auto* const selectionBox = findVisualItem(root, QStringLiteral("selectionBox"));
            auto* const rubberBandHandler = root->findChild<QObject*>(QStringLiteral("gridRubberBand"));
            const auto acceptedSelectionDevices = rubberBandHandler
                ? rubberBandHandler->property("acceptedDevices").toInt() : 0;
            const auto mouseAndTouchPad = static_cast<int>(QInputDevice::DeviceType::Mouse)
                | static_cast<int>(QInputDevice::DeviceType::TouchPad);
            if (!libraryPage || !librarySurface || !rubberSelectionGrid || !selectionBox || !window
                || !rubberBandHandler
                || rubberBandHandler->parent() != librarySurface
                || (acceptedSelectionDevices & mouseAndTouchPad) != mouseAndTouchPad) {
                fail("library-rubber-selection-objects");
                return;
            }
            selectionBox->setX(0);
            selectionBox->setY(0);
            selectionBox->setWidth(librarySurface->width());
            selectionBox->setHeight(librarySurface->height());
            QMetaObject::invokeMethod(libraryPage, "updateRubberSelection");
            if (libraryService.selection()->count() != libraryService.entries()->count()) {
                fail("library-rubber-selection-expand");
                return;
            }
            selectionBox->setWidth(0);
            selectionBox->setHeight(0);
            QMetaObject::invokeMethod(libraryPage, "updateRubberSelection");
            if (libraryService.selection()->count() != 0) {
                fail("library-rubber-selection-shrink");
                return;
            }

            const auto rubberStart = rubberSelectionGrid->mapToScene(QPointF(rubberSelectionGrid->width() * 0.62,
                rubberSelectionGrid->height() * 0.72));
            const auto rubberEnd = rubberStart + QPointF(120.0, 90.0);
            sendMouseEvent(window, QEvent::MouseButtonPress, rubberStart, Qt::LeftButton, Qt::LeftButton);
            sendMouseEvent(window, QEvent::MouseMove, rubberEnd, Qt::NoButton, Qt::LeftButton);
            const auto expectedStart = librarySurface->mapFromScene(rubberStart);
            if (!selectionBox->isVisible()
                || std::abs(selectionBox->x() - expectedStart.x()) > 4.0
                || std::abs(selectionBox->y() - expectedStart.y()) > 4.0) {
                fail("library-rubber-selection-real-pointer-origin");
                return;
            }
            sendMouseEvent(window, QEvent::MouseButtonRelease, rubberEnd, Qt::LeftButton, Qt::NoButton);

            auto* const firstEntryDelegate = findVisualItem(root, QStringLiteral("folderDelegate"));
            if (!firstEntryDelegate) {
                fail("library-rubber-selection-delegate-gap-object");
                return;
            }
            const auto occupiedCellGap = firstEntryDelegate->mapToScene(QPointF(2.0, 2.0));
            sendMouseEvent(window, QEvent::MouseButtonPress, occupiedCellGap,
                Qt::LeftButton, Qt::LeftButton);
            sendMouseEvent(window, QEvent::MouseMove, occupiedCellGap + QPointF(80.0, 60.0),
                Qt::NoButton, Qt::LeftButton);
            if (!selectionBox->isVisible()) {
                fail("library-rubber-selection-allows-surface-gap");
                return;
            }
            sendMouseEvent(window, QEvent::MouseButtonRelease, occupiedCellGap + QPointF(80.0, 60.0),
                Qt::LeftButton, Qt::NoButton);

            const auto outsideSurface = librarySurface->mapToScene(
                QPointF(librarySurface->width() / 2.0, -12.0));
            sendMouseEvent(window, QEvent::MouseButtonPress, outsideSurface,
                Qt::LeftButton, Qt::LeftButton);
            sendMouseEvent(window, QEvent::MouseMove, outsideSurface + QPointF(80.0, 40.0),
                Qt::NoButton, Qt::LeftButton);
            if (selectionBox->isVisible()) {
                fail("library-rubber-selection-rejects-outside-surface");
                return;
            }
            sendMouseEvent(window, QEvent::MouseButtonRelease, outsideSurface + QPointF(80.0, 40.0),
                Qt::LeftButton, Qt::NoButton);

            const auto selectStart = rubberSelectionGrid->mapToScene(QPointF(
                rubberSelectionGrid->width() * 0.9, rubberSelectionGrid->height() * 0.75));
            const auto selectEnd = rubberSelectionGrid->mapToScene(QPointF(
                rubberSelectionGrid->width() * 0.05, rubberSelectionGrid->height() * 0.05));
            sendMouseEvent(window, QEvent::MouseButtonPress, selectStart, Qt::LeftButton, Qt::LeftButton);
            sendMouseEvent(window, QEvent::MouseMove, selectEnd, Qt::NoButton, Qt::LeftButton);
            sendMouseEvent(window, QEvent::MouseButtonRelease, selectEnd, Qt::LeftButton, Qt::NoButton);
            if (libraryService.selection()->count() == 0) {
                fail("library-rubber-selection-persists-after-release");
                return;
            }
            libraryService.selection()->clear();

            const auto dragScoreIdRole = libraryService.scores()->roleNames().key("scoreId", -1);
            const auto dragFolderIdRole = libraryService.folders()->roleNames().key("itemId", -1);
            const auto draggedScoreId = libraryService.scores()->data(
                libraryService.scores()->index(0, 0), dragScoreIdRole).toString();
            const auto dropFolderId = libraryService.folders()->data(
                libraryService.folders()->index(0, 0), dragFolderIdRole).toString();
            if (!dragItemToItem(root, QStringLiteral("scoreCardMouse"), QStringLiteral("folderCardMouse"))
                || libraryService.scoreFolderId(draggedScoreId) != dropFolderId) {
                fail("library-card-drag-moves-score-to-folder");
                return;
            }
            libraryService.setItemFolder(draggedScoreId, QString {});
            for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

            if (!clickItem(root, QStringLiteral("folderNavItem"), Qt::RightButton)
                || !popupIsOpen(root, QStringLiteral("folderContextMenu"))) {
                fail("folder-context-menu");
                return;
            }
            closePopup(root, QStringLiteral("folderContextMenu"));

            if (!clickItem(root, QStringLiteral("tagNavItem"), Qt::RightButton)
                || !popupIsOpen(root, QStringLiteral("tagContextMenu"))) {
                fail("tag-context-menu");
                return;
            }
            closePopup(root, QStringLiteral("tagContextMenu"));

            controller.setCurrentPage(QStringLiteral("library"));
            auto* const scoreDelegate = findVisualItem(root, QStringLiteral("scoreDelegate"));
            if (!clickItem(root, QStringLiteral("scoreCardMouse"), Qt::RightButton)
                || controller.currentPage() != QStringLiteral("library")
                || !scoreDelegate
                || !scoreDelegate->property("contextMenuOpenedOnce").toBool()
                || !scoreDelegate->property("folderSubmenuEnabled").toBool()
                || !scoreDelegate->property("tagSubmenuEnabled").toBool()
                || scoreDelegate->property("folderSubmenuItemCount").toInt() < libraryService.folders()->rowCount() + 2
                || scoreDelegate->property("tagSubmenuItemCount").toInt() < libraryService.tags()->rowCount()
                || popupIsOpen(root, QStringLiteral("blankContextMenu"))) {
                fail("score-context-menu");
                return;
            }
            if (scoreDelegate->property("normalMenuArrowCount").toInt() != 0) {
                fail("normal-menu-item-has-arrow");
                return;
            }
            if (scoreDelegate->property("folderSubmenuArrowCount").toInt() != 1) {
                fail("submenu-arrow-count");
                return;
            }
            if (scoreDelegate->property("contextMenuWidth").toDouble() > 220.0
                || scoreDelegate->property("contextMenuWidth").toDouble() < 180.0
                || scoreDelegate->property("folderSubmenuArrowCount").toInt() != 1
                || scoreDelegate->property("folderSubmenuArrowWidth").toDouble() > 14.0
                || scoreDelegate->property("folderSubmenuArrowRightInset").toDouble() > 12.0) {
                fail("compact-menu-style");
                return;
            }
            if (scoreDelegate->property("tagMenuHasDefaultCheckIndicator").toBool()) {
                fail("tag-menu-single-check-indicator");
                return;
            }
            QMetaObject::invokeMethod(scoreDelegate, "closeContextMenu");
            if (!waitForPropertyFalse(scoreDelegate, "contextMenuVisible")) {
                fail("score-context-menu-close");
                return;
            }

            if (!clickItemAt(root, QStringLiteral("librarySurface"), Qt::RightButton, 0.5, 0.92)
                || !popupIsOpen(root, QStringLiteral("blankContextMenu"))) {
                fail("blank-context-menu");
                return;
            }
            closePopup(root, QStringLiteral("blankContextMenu"));

            const auto scoreIdRole = libraryService.scores()->roleNames().key("scoreId", -1);
            const auto folderIdRole = libraryService.folders()->roleNames().key("itemId", -1);
            const auto tagIdRole = libraryService.tags()->roleNames().key("itemId", -1);
            const auto scoreId = libraryService.scores()->data(libraryService.scores()->index(0, 0), scoreIdRole).toString();
            const auto secondScoreId = libraryService.scores()->data(libraryService.scores()->index(1, 0), scoreIdRole).toString();
            const auto folderId = libraryService.folders()->data(libraryService.folders()->index(0, 0), folderIdRole).toString();
            const auto recentFirstFolderId = libraryService.folders()->data(
                libraryService.folders()->index(1, 0), folderIdRole).toString();
            const auto tagId = libraryService.tags()->data(libraryService.tags()->index(0, 0), tagIdRole).toString();
            int moveNoticeCount = 0;
            const auto moveNoticeConnection = QObject::connect(&libraryService, &LibraryService::noticeOccurred,
                root, [&moveNoticeCount](const QString& message) {
                    if (message.startsWith(QStringLiteral("已移动"))) ++moveNoticeCount;
                });
            if (!libraryService.moveItems({scoreId, secondScoreId}, folderId).isEmpty()) {
                fail("batch-score-folder-assignment");
                return;
            }
            libraryService.setItemFolder(scoreId, folderId);
            if (moveNoticeCount != 1) {
                fail("single-move-noop-does-not-notify");
                return;
            }
            if (!libraryService.moveItems({scoreId, secondScoreId}, folderId).isEmpty()
                || moveNoticeCount != 1) {
                fail("batch-move-noop-does-not-notify");
                return;
            }
            QObject::disconnect(moveNoticeConnection);
            libraryService.setFilterMode(QStringLiteral("folder:") + folderId);
            if (scoreId.isEmpty() || secondScoreId.isEmpty() || folderId.isEmpty()
                || libraryService.scores()->rowCount() != 2) {
                fail("score-folder-assignment");
                return;
            }
            const auto firstSortedScoreId = libraryService.scores()->data(
                libraryService.scores()->index(0, 0), scoreIdRole).toString();
            const auto secondSortedScoreId = libraryService.scores()->data(
                libraryService.scores()->index(1, 0), scoreIdRole).toString();
            libraryService.toggleFavorite(secondSortedScoreId, true);
            if (libraryService.scores()->data(libraryService.scores()->index(0, 0), scoreIdRole).toString()
                != firstSortedScoreId) {
                fail("library-favorite-sort-stability");
                return;
            }
            libraryService.setFilterMode(QStringLiteral("all"));
            libraryService.addScoreTag(scoreId, tagId);
            libraryService.setFilterMode(QStringLiteral("tag:") + tagId);
            if (tagId.isEmpty() || libraryService.scores()->rowCount() != 1
                || !libraryService.scoreHasTag(scoreId, tagId)) {
                fail("score-tag-assignment");
                return;
            }
            libraryService.setFilterMode(QStringLiteral("all"));
            libraryService.removeScoreTag(scoreId, tagId);
            if (libraryService.scoreHasTag(scoreId, tagId)) {
                fail("score-tag-removal");
                return;
            }
            libraryService.addItemTag(folderId, tagId);
            libraryService.setFilterMode(QStringLiteral("tag:") + tagId);
            const auto entryTypeRoleForTags = libraryService.entries()->roleNames().key("itemType", -1);
            const auto entryTagsRole = libraryService.entries()->roleNames().key("tags", -1);
            if (!libraryService.itemHasTag(folderId, tagId)
                || libraryService.entries()->rowCount() != 1
                || libraryService.entries()->data(libraryService.entries()->index(0, 0), entryTypeRoleForTags).toString()
                    != QStringLiteral("folder")
                || libraryService.entries()->data(libraryService.entries()->index(0, 0), entryTagsRole).toStringList().isEmpty()) {
                fail("folder-tag-assignment-and-display");
                return;
            }
            libraryService.toggleItemFavorite(folderId, true);
            libraryService.setFilterMode(QStringLiteral("favorites"));
            if (libraryService.entries()->rowCount() < 1
                || libraryService.entries()->data(libraryService.entries()->index(0, 0), entryTypeRoleForTags).toString()
                    != QStringLiteral("folder")) {
                fail("folder-favorite-filter");
                return;
            }
            if (!libraryService.canMoveItemToFolder(folderId, recentFirstFolderId)) {
                fail("folder-move-valid-target");
                return;
            }
            libraryService.setItemFolder(folderId, recentFirstFolderId);
            if (libraryService.canMoveItemToFolder(recentFirstFolderId, folderId)) {
                fail("folder-move-cycle-guard");
                return;
            }
            libraryService.setItemFolder(folderId, QString {});
            libraryService.setFilterMode(QStringLiteral("folder:") + folderId);

            const auto favoriteRole = libraryService.scores()->roleNames().key("favorite", -1);
            const auto createdDateRole = libraryService.scores()->roleNames().key("createdDate", -1);
            if (createdDateRole < 0) {
                fail("score-created-date-role");
                return;
            }
            const auto firstIndex = libraryService.scores()->index(0, 0);
            const auto favoriteBefore = libraryService.scores()->data(firstIndex, favoriteRole).toBool();
            if (!clickItem(root, QStringLiteral("favoriteButton"), Qt::LeftButton)
                || controller.currentPage() != QStringLiteral("library")) {
                fail("favorite-button-page");
                return;
            }
            const auto favoriteAfter = libraryService.scores()->data(libraryService.scores()->index(0, 0), favoriteRole).toBool();
            if (favoriteBefore == favoriteAfter) {
                fail("favorite-button-state");
                return;
            }

            libraryService.setFilterMode(QStringLiteral("all"));
            libraryService.setFilterMode(QStringLiteral("favorites"));
            controller.setCurrentPage(QStringLiteral("library"));
            QCoreApplication::processEvents();

            if (!clickItem(root, QStringLiteral("scoreCardMouse"), Qt::LeftButton)
                || controller.currentPage() != QStringLiteral("reader")
                || controller.currentScoreFolderId() != folderId) {
                fail("score-single-click");
                return;
            }
            QCoreApplication::processEvents();
            const auto* const sidebar = root->findChild<QQuickItem*>(QStringLiteral("sidebar"));
            auto* const readerPage = root->findChild<QObject*>(QStringLiteral("readerPage"));
            auto* const readerFlick = root->findChild<QObject*>(QStringLiteral("readerFlick"));
            if (!sidebar || sidebar->isVisible()) {
                fail("reader-focus-layout");
                return;
            }
            const auto openedScoreId = controller.currentScoreId();
            const auto hasPrevious = readerPage->property("hasPrev").toBool();
            const auto hasNext = readerPage->property("hasNext").toBool();
            QMetaObject::invokeMethod(readerPage, hasPrevious ? "goToPrevScore" : "goToNextScore");
            QCoreApplication::processEvents();
            if ((!hasPrevious && !hasNext) || controller.currentScoreId() == openedScoreId) {
                fail("reader-folder-sibling-navigation");
                return;
            }
            const auto centerBefore = (readerFlick->property("contentX").toDouble()
                + readerFlick->property("width").toDouble() / 2.0)
                / readerFlick->property("contentWidth").toDouble();
            QMetaObject::invokeMethod(readerPage, "zoomIn");
            QCoreApplication::processEvents();
            const auto centerAfter = (readerFlick->property("contentX").toDouble()
                + readerFlick->property("width").toDouble() / 2.0)
                / readerFlick->property("contentWidth").toDouble();
            if (std::abs(centerBefore - centerAfter) > 0.02) {
                fail("reader-centered-zoom");
                return;
            }

            if (!QMetaObject::invokeMethod(readerPage, "rotateRight")
                || readerPage->property("viewRotation").toInt() != 90
                || !QMetaObject::invokeMethod(readerPage, "resetReaderView")) {
                fail("reader-rotation-controls");
                return;
            }
            QCoreApplication::processEvents();
            if (readerPage->property("viewRotation").toInt() != 0
                || std::abs(readerPage->property("zoomLevel").toDouble() - 1.0) > 0.001) {
                fail("reader-reset-view");
                return;
            }

            readerFlick->setProperty("rotation", 17.0);
            readerFlick->setProperty("scale", 0.75);

            controller.setCurrentPage(QStringLiteral("library"));
            controller.openScore(controller.currentScoreId(), QStringLiteral("再次打开测试"), controller.currentFileUrl().toLocalFile(),
                controller.currentFileType(), 1, controller.currentScoreFolderId());
            QEventLoop reopenWait;
            QTimer::singleShot(250, &reopenWait, &QEventLoop::quit);
            reopenWait.exec();
            const auto reopenedZoom = readerPage->property("zoomLevel").toDouble();
            const auto reopenedContentY = readerFlick->property("contentY").toDouble();
            const auto reopenedContentHeight = readerFlick->property("contentHeight").toDouble();
            const auto reopenedViewportHeight = readerFlick->property("height").toDouble();
            if (controller.currentPage() != QStringLiteral("reader")
                || std::abs(reopenedZoom - 1.0) > 0.001
                || std::abs(reopenedContentY) > 1.0
                || std::abs(readerFlick->property("rotation").toDouble()) > 0.001
                || std::abs(readerFlick->property("scale").toDouble() - 1.0) > 0.001
                || reopenedContentHeight <= reopenedViewportHeight) {
                fail("reader-reopen-default-view");
                return;
            }
            if (!window->grabWindow().save(QStringLiteral("notera-reader-smoke.png"))) {
                fail("reader-screenshot");
                return;
            }

            controller.setCurrentPage(QStringLiteral("settings"));
            QCoreApplication::processEvents();
            QEventLoop settingsLayoutWait;
            QTimer::singleShot(200, &settingsLayoutWait, &QEventLoop::quit);
            settingsLayoutWait.exec();
            const auto* const settingsContent = root->findChild<QQuickItem*>(QStringLiteral("settingsContent"));
            const auto* const themeSelector = root->findChild<QQuickItem*>(QStringLiteral("themeSelector"));
            const auto* const changeDataDirectoryButton = root->findChild<QQuickItem*>(
                QStringLiteral("changeDataDirectoryButton"));
            const auto* const openDataDirectoryButton = root->findChild<QQuickItem*>(
                QStringLiteral("openDataDirectoryButton"));
            const auto* const versionLabel = root->findChild<QQuickItem*>(QStringLiteral("versionLabel"));
            const auto* const animationsSwitch = root->findChild<QQuickItem*>(QStringLiteral("animationsSwitch"));
            if (!settingsContent || !themeSelector || settingsContent->width() <= 0.0
                || themeSelector->width() < 240.0 || themeSelector->x() < 0.0
                || !changeDataDirectoryButton || !changeDataDirectoryButton->isVisible()
                || !changeDataDirectoryButton->isEnabled() || !openDataDirectoryButton
                || !openDataDirectoryButton->isVisible() || !versionLabel || !animationsSwitch
                || !controller.animationsEnabled()
                || animationsSwitch->property("independentAnimationDuration").toInt() <= 0
                || std::abs(animationsSwitch->mapToScene(QPointF(animationsSwitch->width(), 0)).x()
                    - themeSelector->mapToScene(QPointF(themeSelector->width(), 0)).x()) > 1.0
                || std::abs(versionLabel->mapToScene(QPointF(versionLabel->width(), 0)).x()
                    - themeSelector->mapToScene(QPointF(themeSelector->width(), 0)).x()) > 1.0) {
                fail("settings-layout");
                return;
            }
            controller.setAnimationsEnabled(false);
            QCoreApplication::processEvents();
            ApplicationController persistedMotionController;
            if (controller.animationsEnabled() || persistedMotionController.animationsEnabled()
                || libraryNavItem->property("hoverTransitionDuration").toInt() != 0
                || animationsSwitch->property("independentAnimationDuration").toInt() <= 0) {
                fail("animations-setting-persistence");
                return;
            }
            controller.setAnimationsEnabled(true);
            if (!findVisualItem(root, QStringLiteral("tagEntryIcon"))) {
                fail("tag-entry-icon");
                return;
            }
            auto* const dataDirectoryDialog = root->findChild<QObject*>(QStringLiteral("dataDirectoryDialog"));
            if (!dataDirectoryDialog) {
                fail("data-directory-dialog-object");
                return;
            }
            dataDirectoryDialog->setProperty("selectedFolder",
                QUrl::fromLocalFile(QDir::temp().filePath(QStringLiteral("notera-ui-selected-folder"))));
            if (!QMetaObject::invokeMethod(dataDirectoryDialog, "accepted")
                || !popupIsOpen(root, QStringLiteral("migrationConfirmDialog"))) {
                fail("data-directory-dialog-accepted");
                return;
            }
            closePopup(root, QStringLiteral("migrationConfirmDialog"));
            QEventLoop migrationDialogCloseWait;
            QTimer::singleShot(200, &migrationDialogCloseWait, &QEventLoop::quit);
            migrationDialogCloseWait.exec();
            const auto* const settingsTitle = root->findChild<QQuickItem*>(QStringLiteral("settingsTitle"));
            const auto* const brandLabel = root->findChild<QQuickItem*>(QStringLiteral("brandLabel"));
            if (!settingsTitle || !brandLabel || settingsTitle->mapToScene(QPointF {}).y() < 24.0
                || brandLabel->mapToScene(QPointF {}).y() < 12.0) {
                fail("page-top-spacing");
                return;
            }
            auto* const clearAllDataButton = root->findChild<QObject*>(QStringLiteral("clearAllDataButton"));
            auto* const clearWarning = root->findChild<QObject*>(QStringLiteral("clearWarningDialog"));
            if (!clearAllDataButton || !clearWarning
                || !QMetaObject::invokeMethod(clearWarning, "open")
                || !popupIsOpen(root, QStringLiteral("clearWarningDialog"))) {
                fail("clear-data-first-confirmation");
                return;
            }
            if (!QMetaObject::invokeMethod(clearWarning, "accept")) {
                fail("clear-data-second-confirmation-open");
                return;
            }
            QCoreApplication::processEvents();
            auto* const clearInput = root->findChild<QObject*>(QStringLiteral("clearConfirmInput"));
            auto* const clearButton = root->findChild<QObject*>(QStringLiteral("confirmClearAllDataButton"));
            if (!popupIsOpen(root, QStringLiteral("clearTypedDialog")) || !clearInput || !clearButton
                || clearButton->property("enabled").toBool()) {
                fail("clear-data-typed-confirmation-disabled");
                return;
            }
            clearInput->setProperty("text", QStringLiteral("确认清空所有数据"));
            QCoreApplication::processEvents();
            if (!clearButton->property("enabled").toBool()) {
                fail("clear-data-typed-confirmation-enabled");
                return;
            }
            closePopup(root, QStringLiteral("clearTypedDialog"));
            QEventLoop clearDialogCloseWait;
            QTimer::singleShot(200, &clearDialogCloseWait, &QEventLoop::quit);
            clearDialogCloseWait.exec();
            if (!window->grabWindow().save(QStringLiteral("notera-settings-smoke.png"))) {
                fail("settings-screenshot");
                return;
            }
            controller.setThemeMode(1);
            QCoreApplication::processEvents();
            if (root->property("themeBackground").value<QColor>().lightnessF() > 0.25
                || !window->grabWindow().save(QStringLiteral("notera-settings-dark-smoke.png"))) {
                fail("dark-theme-render");
                return;
            }
            controller.setThemeMode(0);
            QCoreApplication::processEvents();

            controller.setCurrentPage(QStringLiteral("library"));
            QCoreApplication::processEvents();
            if (!clickItem(root, QStringLiteral("newFolderButton"), Qt::LeftButton)
                || !popupIsOpen(root, QStringLiteral("folderEditorDialog"))) {
                fail("new-folder-dialog");
                return;
            }
            if (!window->grabWindow().save(QStringLiteral("notera-dialog-smoke.png"))) {
                fail("dialog-screenshot");
                return;
            }

            libraryService.goToLibraryRoot();
            QCoreApplication::processEvents();
            const auto* const browserGrid = root->findChild<QObject*>(QStringLiteral("browserGrid"));
            if (!browserGrid || browserGrid->property("count").toInt() != 2) {
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
            if (libraryService.entries()->rowCount() != 4
                || libraryService.entries()->data(libraryService.entries()->index(0, 0), entryTypeRole).toString()
                    != QStringLiteral("folder")
                || libraryService.entries()->data(libraryService.entries()->index(0, 0), entryIdRole).toString()
                    != recentFirstFolderId
                || libraryService.entries()->data(libraryService.entries()->index(1, 0), entryIdRole).toString()
                    != folderId
                || libraryService.entries()->data(libraryService.entries()->index(2, 0), entryIdRole).toString()
                    != controller.currentScoreId()) {
                fail("recent-folder-first-last-opened-sort");
                return;
            }
            libraryService.goToLibraryRoot();
            // 对齐 Windows：同目录复制也弹冲突窗（不再静默生成“ - 副本”），选“跳过”不产生副本、源保留
            {
                QObject ctx;
                bool sameFolderConflictFired = false;
                QObject::connect(&libraryService, &LibraryService::pasteFolderConflict, &ctx, [&](const QString&, const QString&, int, int) {
                    sameFolderConflictFired = true;
                });
                libraryService.copyItems({folderId});
                libraryService.pasteItems();
                {
                    QEventLoop conflictWait;
                    QTimer::singleShot(300, &conflictWait, &QEventLoop::quit);
                    conflictWait.exec();
                }
                if (!sameFolderConflictFired) {
                    fail("same-folder-copy-should-conflict");
                    return;
                }
                libraryService.resolvePasteFolderConflict(QStringLiteral("skip"), false);
                QCoreApplication::processEvents();
                {
                    const auto rootFolders = libraryService.childFolders(QString());
                    int count = 0;
                    for (const auto& f : rootFolders) {
                        if (f.toMap().value(QStringLiteral("name")).toString() == QStringLiteral("A界面测试文件夹")) ++count;
                    }
                    if (count != 1) {
                        fail("same-folder-copy-skip-no-duplicate");
                        return;
                    }
                }
            }
            // 制造跨文件夹同名冲突验证冲突弹窗布局：
            // 在 Z 界面测试文件夹内建同名子文件夹，再把根目录的 A 界面测试文件夹复制进去
            libraryService.enterFolder(recentFirstFolderId);
            libraryService.createFolder(QStringLiteral("A界面测试文件夹"));
            libraryService.copyItems({folderId});
            libraryService.pasteItems();
            {
                QEventLoop conflictWait;
                QTimer::singleShot(300, &conflictWait, &QEventLoop::quit);
                conflictWait.exec();
            }
            if (!window->grabWindow().save(QStringLiteral("notera-conflict-dialog-smoke.png"))) {
                fail("conflict-dialog-screenshot");
                return;
            }
            libraryService.resolvePasteFolderConflict(QStringLiteral("cancel"), false);
            QCoreApplication::processEvents();

            const auto folderScores = libraryService.scoresInFolder(folderId);
            QStringList storedFilePaths;
            for (const auto& value : folderScores) {
                storedFilePaths.append(value.toMap().value(QStringLiteral("filePath")).toString());
            }
            libraryService.deleteItems({folderId});
            const auto hasRemainingFile = std::any_of(storedFilePaths.cbegin(), storedFilePaths.cend(), [](const QString& path) {
                return QFileInfo::exists(path);
            });
            if (libraryService.scores()->rowCount() != 0 || hasRemainingFile) {
                fail("batch-folder-cascade-delete");
                return;
            }
            QCoreApplication::exit(0);
        });
    }

    return app.exec();
}
