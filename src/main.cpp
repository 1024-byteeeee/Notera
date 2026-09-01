#include <algorithm>
#include <cmath>
#include <memory>

#include <QColor>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QGuiApplication>
#include <QImage>
#include <QIcon>
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
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTimer>

#include "app/ApplicationController.h"
#include "features/library/LibraryService.h"
#include "platform/AppDataPaths.h"

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
    bool previewReady = false;
    for (int step = 1; step <= 8; ++step) {
        sendMouseEvent(window, QEvent::MouseMove,
            start + (end - start) * (static_cast<double>(step) / 8.0), Qt::NoButton, Qt::LeftButton);
        if (step == 4) {
            const auto previewSource = previewImage->property("source").toUrl();
            previewReady = preview->z() >= 100.0 && previewSource.isLocalFile();
        }
    }
    sendMouseEvent(window, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
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

bool closePopup(QObject* root, const QString& objectName)
{
    auto* const popup = root->findChild<QObject*>(objectName);
    return popup && QMetaObject::invokeMethod(popup, "close");
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

    const auto arguments = app.arguments();
    const auto isSmokeTest = arguments.contains(QStringLiteral("--theme-smoke-test"))
        || arguments.contains(QStringLiteral("--import-smoke-test"))
        || arguments.contains(QStringLiteral("--stitch-smoke-test"))
        || arguments.contains(QStringLiteral("--reader-smoke-test"))
        || arguments.contains(QStringLiteral("--ui-smoke-test"))
        || arguments.contains(QStringLiteral("--storage-migration-smoke-test"))
        || arguments.contains(QStringLiteral("--clear-data-smoke-test"));
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
    QObject::connect(&controller, &ApplicationController::restartRequested, &app, [&restartGuard] {
        restartGuard.requested = true;
        QCoreApplication::quit();
    });
    QObject::connect(&controller, &ApplicationController::scoreOpened,
        &libraryService, &LibraryService::markScoreOpened);

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
        return libraryService.scores()->rowCount() == previousCount + 1 ? 0 : 1;
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
        return libraryService.scores()->rowCount() == previousCount + 1 ? 0 : 1;
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("libraryService"), &libraryService);
    engine.loadFromModule("Notera", "Main");

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    if (arguments.contains(QStringLiteral("--theme-smoke-test"))) {
        auto* const root = engine.rootObjects().constFirst();
        const auto originalMode = controller.themeMode();
        controller.setThemeMode(0);
        QCoreApplication::processEvents();
        const auto lightBackground = root->property("themeBackground").value<QColor>();
        controller.setThemeMode(1);
        QCoreApplication::processEvents();
        const auto darkBackground = root->property("themeBackground").value<QColor>();
        controller.setThemeMode(originalMode);
        return lightBackground.isValid() && darkBackground.isValid() && lightBackground != darkBackground ? 0 : 1;
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
            if (!libraryPage || !librarySurface || !rubberSelectionGrid || !selectionBox || !window) {
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
            if (!libraryService.moveItems({scoreId, secondScoreId}, folderId).isEmpty()) {
                fail("batch-score-folder-assignment");
                return;
            }
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
