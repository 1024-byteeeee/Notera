#include <algorithm>
#include <cmath>

#include <QColor>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QGuiApplication>
#include <QImage>
#include <QLocale>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTimer>

#include "app/ApplicationController.h"
#include "features/library/LibraryService.h"

namespace {

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

bool clickItem(QObject* root, const QString& objectName, const Qt::MouseButton button)
{
    auto* const item = findVisualItem(root, objectName);
    if (!item || !item->isVisible() || item->width() <= 0.0 || item->height() <= 0.0 || !item->window()) {
        return false;
    }

    const auto scenePosition = item->mapToScene(QPointF(item->width() / 2.0, item->height() / 2.0));
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
    QGuiApplication app(argc, argv);
    QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::China));
    app.setOrganizationName(QStringLiteral("Notera"));
    app.setOrganizationDomain(QStringLiteral("notera.app"));
    app.setApplicationName(QStringLiteral("Notera"));

    const auto arguments = app.arguments();
    const auto isSmokeTest = arguments.contains(QStringLiteral("--theme-smoke-test"))
        || arguments.contains(QStringLiteral("--import-smoke-test"))
        || arguments.contains(QStringLiteral("--stitch-smoke-test"))
        || arguments.contains(QStringLiteral("--reader-smoke-test"))
        || arguments.contains(QStringLiteral("--ui-smoke-test"));
    if (isSmokeTest) {
        QStandardPaths::setTestModeEnabled(true);
    }
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    ApplicationController controller;
    LibraryService libraryService;

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
        controller.setThemeMode(1);
        QCoreApplication::processEvents();
        const auto lightBackground = root->property("themeBackground").value<QColor>();
        controller.setThemeMode(2);
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
    if (arguments.contains(QStringLiteral("--ui-smoke-test"))) {
        uiSmokeFile.setFileTemplate(QDir::tempPath() + QStringLiteral("/notera-ui-XXXXXX.png"));
        if (!uiSmokeFile.open()) {
            return 1;
        }
        const auto imagePath = uiSmokeFile.fileName();
        uiSmokeFile.close();
        QImage image(900, 1280, QImage::Format_RGB32);
        image.fill(Qt::white);
        if (!image.save(imagePath)) {
            return 1;
        }
        libraryService.importLocalFile(QUrl::fromLocalFile(imagePath));
        libraryService.createFolder(QStringLiteral("界面测试文件夹"));
        libraryService.createTag(QStringLiteral("界面测试标签"));

        auto* const root = engine.rootObjects().constFirst();
        QTimer::singleShot(300, root, [root, &controller, &libraryService] {
            const auto fail = [](const char* step) {
                qWarning() << "UI smoke test failed at" << step;
                QCoreApplication::exit(1);
            };
            auto* const importButton = root->findChild<QQuickItem*>(QStringLiteral("importButton"));
            const auto* const stitchButton = root->findChild<QObject*>(QStringLiteral("stitchButton"));
            const auto* const sidebarImportButton = root->findChild<QObject*>(QStringLiteral("sidebarImportButton"));
            if (!importButton || !importButton->isVisible() || importButton->width() < 96.0
                || importButton->property("symbol").toString().length() > 0
                || !stitchButton || stitchButton->property("symbol").toString().length() > 0
                || !sidebarImportButton || sidebarImportButton->property("symbol").toString().length() > 0
                || std::abs(importButton->property("visualContentCenterX").toDouble() - importButton->width() / 2.0) > 1.0
                || std::abs(stitchButton->property("visualContentCenterX").toDouble()
                    - stitchButton->property("width").toDouble() / 2.0) > 1.0
                || std::abs(sidebarImportButton->property("visualContentCenterX").toDouble()
                    - sidebarImportButton->property("width").toDouble() / 2.0) > 1.0) {
                fail("import-button-geometry");
                return;
            }
            auto* const window = qobject_cast<QQuickWindow*>(root);
            if (!window || !window->grabWindow().save(QStringLiteral("notera-library-smoke.png"))) {
                fail("library-screenshot");
                return;
            }

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
            QMetaObject::invokeMethod(scoreDelegate, "closeContextMenu");

            if (!clickItem(root, QStringLiteral("librarySurface"), Qt::RightButton)
                || !popupIsOpen(root, QStringLiteral("blankContextMenu"))) {
                fail("blank-context-menu");
                return;
            }
            closePopup(root, QStringLiteral("blankContextMenu"));

            const auto scoreIdRole = libraryService.scores()->roleNames().key("scoreId", -1);
            const auto folderIdRole = libraryService.folders()->roleNames().key("itemId", -1);
            const auto tagIdRole = libraryService.tags()->roleNames().key("itemId", -1);
            const auto scoreId = libraryService.scores()->data(libraryService.scores()->index(0, 0), scoreIdRole).toString();
            const auto folderId = libraryService.folders()->data(libraryService.folders()->index(0, 0), folderIdRole).toString();
            const auto tagId = libraryService.tags()->data(libraryService.tags()->index(0, 0), tagIdRole).toString();
            libraryService.setScoreFolder(scoreId, folderId);
            libraryService.setFilterMode(QStringLiteral("folder:") + folderId);
            if (scoreId.isEmpty() || folderId.isEmpty() || libraryService.scores()->rowCount() != 1) {
                fail("score-folder-assignment");
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

            if (!clickItem(root, QStringLiteral("scoreCardMouse"), Qt::LeftButton)
                || controller.currentPage() != QStringLiteral("reader")) {
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
            const auto* const settingsContent = root->findChild<QQuickItem*>(QStringLiteral("settingsContent"));
            const auto* const themeSelector = root->findChild<QQuickItem*>(QStringLiteral("themeSelector"));
            if (!settingsContent || !themeSelector || settingsContent->width() <= 0.0
                || themeSelector->width() < 240.0 || themeSelector->x() < 0.0) {
                fail("settings-layout");
                return;
            }
            const auto* const settingsTitle = root->findChild<QQuickItem*>(QStringLiteral("settingsTitle"));
            const auto* const brandLabel = root->findChild<QQuickItem*>(QStringLiteral("brandLabel"));
            if (!settingsTitle || !brandLabel || settingsTitle->mapToScene(QPointF {}).y() < 24.0
                || brandLabel->mapToScene(QPointF {}).y() < 12.0) {
                fail("page-top-spacing");
                return;
            }
            if (!window->grabWindow().save(QStringLiteral("notera-settings-smoke.png"))) {
                fail("settings-screenshot");
                return;
            }
            controller.setThemeMode(2);
            QCoreApplication::processEvents();
            if (root->property("themeBackground").value<QColor>().lightnessF() > 0.25
                || !window->grabWindow().save(QStringLiteral("notera-settings-dark-smoke.png"))) {
                fail("dark-theme-render");
                return;
            }
            controller.setThemeMode(1);
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
            if (!browserGrid || browserGrid->property("count").toInt() != 1) {
                fail("library-folder-score-browser");
                return;
            }
            const auto storedFilePath = controller.currentFileUrl().toLocalFile();
            libraryService.deleteItems({folderId});
            if (libraryService.scores()->rowCount() != 0 || QFileInfo::exists(storedFilePath)) {
                fail("batch-folder-cascade-delete");
                return;
            }
            QCoreApplication::exit(0);
        });
    }

    return app.exec();
}
