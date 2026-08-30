#include <QColor>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QLocale>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>

#include "app/ApplicationController.h"
#include "features/library/LibraryService.h"

namespace {

bool clickItem(QObject* root, const QString& objectName, const Qt::MouseButton button)
{
    auto* const item = root->findChild<QQuickItem*>(objectName);
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
    return popup && popup->property("visible").toBool() && popup->property("width").toDouble() > 0.0
        && popup->property("height").toDouble() > 0.0;
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
        || arguments.contains(QStringLiteral("--reader-smoke-test"))
        || arguments.contains(QStringLiteral("--ui-smoke-test"));
    if (isSmokeTest) {
        QStandardPaths::setTestModeEnabled(true);
    }

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
        controller.openScore(QStringLiteral("自动滚动测试"), imagePath, QStringLiteral("png"), 1);
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
            const auto fail = [] { QCoreApplication::exit(1); };
            auto* const importButton = root->findChild<QQuickItem*>(QStringLiteral("importButton"));
            if (!importButton || !importButton->isVisible() || importButton->width() < 96.0) {
                fail();
                return;
            }

            if (!clickItem(root, QStringLiteral("newFolderButton"), Qt::LeftButton)
                || !popupIsOpen(root, QStringLiteral("folderEditorDialog"))) {
                fail();
                return;
            }
            closePopup(root, QStringLiteral("folderEditorDialog"));

            if (!clickItem(root, QStringLiteral("folderNavMouse"), Qt::RightButton)
                || !popupIsOpen(root, QStringLiteral("folderContextMenu"))) {
                fail();
                return;
            }
            closePopup(root, QStringLiteral("folderContextMenu"));

            controller.setCurrentPage(QStringLiteral("library"));
            if (!clickItem(root, QStringLiteral("scoreCardMouse"), Qt::RightButton)
                || controller.currentPage() != QStringLiteral("library")
                || !popupIsOpen(root, QStringLiteral("scoreContextMenu"))) {
                fail();
                return;
            }
            closePopup(root, QStringLiteral("scoreContextMenu"));

            const auto favoriteRole = libraryService.scores()->roleNames().key("favorite", -1);
            const auto firstIndex = libraryService.scores()->index(0, 0);
            const auto favoriteBefore = libraryService.scores()->data(firstIndex, favoriteRole).toBool();
            if (!clickItem(root, QStringLiteral("favoriteButton"), Qt::LeftButton)
                || controller.currentPage() != QStringLiteral("library")) {
                fail();
                return;
            }
            const auto favoriteAfter = libraryService.scores()->data(firstIndex, favoriteRole).toBool();
            if (favoriteBefore == favoriteAfter) {
                fail();
                return;
            }

            if (!clickItem(root, QStringLiteral("scoreCardMouse"), Qt::LeftButton)
                || controller.currentPage() != QStringLiteral("reader")) {
                fail();
                return;
            }

            controller.setCurrentPage(QStringLiteral("settings"));
            QCoreApplication::processEvents();
            const auto* const settingsContent = root->findChild<QQuickItem*>(QStringLiteral("settingsContent"));
            const auto* const themeSelector = root->findChild<QQuickItem*>(QStringLiteral("themeSelector"));
            if (!settingsContent || !themeSelector || settingsContent->width() <= 0.0
                || themeSelector->width() < 240.0 || themeSelector->x() < 0.0) {
                fail();
                return;
            }
            QCoreApplication::exit(0);
        });
    }

    return app.exec();
}
