#include "app/ApplicationController.h"

#include <algorithm>
#include <QSettings>

ApplicationController::ApplicationController(QObject* parent)
    : QObject(parent)
{
    const QSettings settings;
    m_themeMode = settings.value(QStringLiteral("appearance/themeMode"), 0).toInt();
    if (m_themeMode < 0 || m_themeMode > 2) {
        m_themeMode = 0;
    }
    m_autoScrollSpeed = settings.value(QStringLiteral("reader/autoScrollSpeed"), 45.0).toDouble();
    if (m_autoScrollSpeed < 15.0 || m_autoScrollSpeed > 160.0) {
        m_autoScrollSpeed = 45.0;
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

QString ApplicationController::currentScoreTitle() const
{
    return m_currentScoreTitle;
}

QUrl ApplicationController::currentFileUrl() const
{
    return m_currentFileUrl;
}

QString ApplicationController::currentFileType() const
{
    return m_currentFileType;
}

QString ApplicationController::currentScoreId() const
{
    return m_currentScoreId;
}

QString ApplicationController::currentScoreFolderId() const
{
    return m_currentScoreFolderId;
}

int ApplicationController::currentScorePageCount() const
{
    return m_currentScorePageCount;
}

double ApplicationController::autoScrollSpeed() const
{
    return m_autoScrollSpeed;
}

void ApplicationController::setAutoScrollSpeed(const double speed)
{
    const auto boundedSpeed = std::clamp(speed, 15.0, 160.0);
    if (qFuzzyCompare(m_autoScrollSpeed, boundedSpeed)) {
        return;
    }
    m_autoScrollSpeed = boundedSpeed;
    QSettings().setValue(QStringLiteral("reader/autoScrollSpeed"), boundedSpeed);
    emit autoScrollSpeedChanged();
}

void ApplicationController::openScore(const QString& scoreId, const QString& title, const QString& filePath,
    const QString& fileType, const int pageCount, const QString& folderId)
{
    m_currentScoreId = scoreId;
    m_currentScoreTitle = title;
    m_currentFileUrl = QUrl::fromLocalFile(filePath);
    m_currentFileType = fileType.toLower();
    m_currentScoreFolderId = folderId;
    m_currentScorePageCount = pageCount;
    emit currentScoreChanged();
    emit scoreOpened(scoreId);
    setCurrentPage(QStringLiteral("reader"));
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

QString ApplicationController::libraryFilter() const
{
    return m_libraryFilter;
}

void ApplicationController::setLibraryFilter(const QString& filter)
{
    if (m_libraryFilter == filter) {
        return;
    }
    m_libraryFilter = filter;
    emit libraryFilterChanged();
}
