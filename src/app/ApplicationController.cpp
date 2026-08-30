#include "app/ApplicationController.h"

ApplicationController::ApplicationController(QObject* parent)
    : QObject(parent)
{
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
