#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "app/ApplicationController.h"
#include "features/library/LibraryService.h"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Notera"));
    app.setOrganizationDomain(QStringLiteral("notera.app"));
    app.setApplicationName(QStringLiteral("Notera"));

    ApplicationController controller;
    LibraryService libraryService;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("libraryService"), &libraryService);
    engine.loadFromModule("Notera", "Main");

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return app.exec();
}
