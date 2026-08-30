#include "app/ApplicationController.h"

#include <QSettings>

ApplicationController::ApplicationController(QObject* parent)
    : QObject(parent)
{
    const QSettings settings;
    m_themeMode = settings.value(QStringLiteral("appearance/themeMode"), 0).toInt();
    if (m_themeMode < 0 || m_themeMode > 2) {
        m_themeMode = 0;
    }
}

int ApplicationController::themeMode() const
{
    return m_themeMode;
}

void ApplicationController::setThemeMode(const int themeMode)
{
    if (themeMode < 0 || themeMode > 2 || m_themeMode == themeMode) {
        return;
    }

    m_themeMode = themeMode;
    QSettings().setValue(QStringLiteral("appearance/themeMode"), themeMode);
    emit themeModeChanged();
}

QString ApplicationController::currentPage() const
{
    return m_currentPage;
}

void ApplicationController::setCurrentPage(const QString& page)
{
    if (m_currentPage == page) {
        return;
    }

    m_currentPage = page;
    emit currentPageChanged();
}
