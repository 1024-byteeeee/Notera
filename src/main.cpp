#include <QColor>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTemporaryFile>
#include <QTimer>

#include "app/ApplicationController.h"
#include "features/library/LibraryService.h"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::China));
    app.setOrganizationName(QStringLiteral("Notera"));
    app.setOrganizationDomain(QStringLiteral("notera.app"));
    app.setApplicationName(QStringLiteral("Notera"));

    ApplicationController controller;
    LibraryService libraryService;

    QTemporaryFile importSmokeFile;
    if (app.arguments().contains(QStringLiteral("--import-smoke-test"))) {
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

    if (app.arguments().contains(QStringLiteral("--theme-smoke-test"))) {
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
    if (app.arguments().contains(QStringLiteral("--reader-smoke-test"))) {
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

    return app.exec();
}
