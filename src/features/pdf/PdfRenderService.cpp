#include "features/pdf/PdfRenderService.h"

#include "features/pdf/PdfRenderCache.h"

#include <QSignalBlocker>
#include <QUrl>

namespace Notera
{

PdfRenderService::PdfRenderService(QObject* parent)
    : QObject(parent), m_renderer(new QPdfPageRenderer(this)), m_cache(new PdfRenderCache(this))
{
    m_renderer->setRenderMode(QPdfPageRenderer::RenderMode::MultiThreaded);
    connect(m_renderer, &QPdfPageRenderer::pageRendered, this, &PdfRenderService::onPageRendered);
}

PdfRenderService::~PdfRenderService() = default;

void PdfRenderService::onOwnedDocumentStatusChanged(QPdfDocument::Status status)
{
    if (status == QPdfDocument::Status::Ready && m_document == m_ownedDocument)
    {
        emit documentChanged();
    }
}

QPdfDocumentRenderOptions::Rotation PdfRenderService::rotationFromDegrees(int degrees)
{
    const int norm = ((degrees % 360) + 360) % 360;
    switch (norm)
    {
    case 90:
        return QPdfDocumentRenderOptions::Rotation::Clockwise90;
    case 180:
        return QPdfDocumentRenderOptions::Rotation::Clockwise180;
    case 270:
        return QPdfDocumentRenderOptions::Rotation::Clockwise270;
    default:
        return QPdfDocumentRenderOptions::Rotation::None;
    }
}

void PdfRenderService::setDocument(QObject* document)
{

    QPdfDocument* pdfDoc = nullptr;
    QString newSource;

    if (document)
    {

        pdfDoc = qobject_cast<QPdfDocument*>(document);
        if (pdfDoc)
        {

            if (m_document == pdfDoc)
                return;
        }
        else
        {

            const QVariant sourceVar = document->property("source");
            if (sourceVar.isValid() && sourceVar.canConvert<QUrl>())
            {
                const QUrl source = sourceVar.toUrl();
                if (source.isValid() && !source.isEmpty())
                {
                    newSource = source.toLocalFile();

                    if (newSource == m_currentSource && m_ownedDocument)
                        return;
                    if (!m_ownedDocument)
                    {
                        m_ownedDocument = new QPdfDocument(this);
                        connect(m_ownedDocument, &QPdfDocument::statusChanged, this,
                                &PdfRenderService::onOwnedDocumentStatusChanged);
                    }

                    const QSignalBlocker blocker(m_ownedDocument);
                    m_ownedDocument->load(newSource);
                    pdfDoc = m_ownedDocument;
                }
            }
        }
    }

    cancelAll();
    m_cache->clear();
    m_document = pdfDoc;
    m_currentSource = newSource;
    if (m_document)
    {
        m_renderer->setDocument(m_document);

        if (m_document->status() == QPdfDocument::Status::Ready)
            emit documentChanged();
    }
}

quint64 PdfRenderService::requestRender(int page, qreal scale, int rotation, QSize imageSize,
                                        int priority, int tileRow, int tileCol, int tileCount)
{
    const Priority prio = (priority == 1) ? Priority::Low : Priority::High;
    if (!m_document || m_document->status() != QPdfDocument::Status::Ready)
        return 0;
    if (page < 0 || page >= m_document->pageCount())
        return 0;
    if (imageSize.isEmpty())
        return 0;
    if (m_cache->has(page, scale, rotation, tileRow, tileCol))
        return 0;

    const quint64 id = m_nextId++;

    Request req;
    req.id = id;
    req.page = page;
    req.scale = scale;
    req.rotation = rotation;
    req.imageSize = imageSize;
    req.priority = prio;
    req.tileRow = tileRow;
    req.tileCol = tileCol;
    req.tileCount = qMax(1, tileCount);

    if (!m_inFlight)
    {

        const quint64 rendererId = m_renderer->requestPage(page, imageSize, buildOptions(req));
        if (rendererId == 0)
            return 0;
        req.rendererId = rendererId;
        req.inFlight = true;
        m_inFlight = true;
    }
    else
    {

        if (prio == Priority::High)
            m_highQueue.append(id);
        else
            m_lowQueue.append(id);
    }

    m_requests.insert(id, req);
    return id;
}

QPdfDocumentRenderOptions PdfRenderService::buildOptions(const Request& req) const
{
    QPdfDocumentRenderOptions options;
    options.setRotation(rotationFromDegrees(req.rotation));
    if (req.tileRow >= 0 && req.tileCol >= 0 && req.tileCount > 1)
    {

        const int blockW = req.imageSize.width() / req.tileCount;
        const int blockH = req.imageSize.height() / req.tileCount;
        options.setScaledSize(req.imageSize);
        options.setScaledClipRect(
            QRect(req.tileCol * blockW, req.tileRow * blockH, blockW, blockH));
    }
    return options;
}

void PdfRenderService::dispatchNext()
{
    if (m_inFlight)
        return;
    if (!m_document || m_document->status() != QPdfDocument::Status::Ready)
        return;

    quint64 id = 0;
    if (!m_highQueue.isEmpty())
        id = m_highQueue.takeFirst();
    else if (!m_lowQueue.isEmpty())
        id = m_lowQueue.takeFirst();
    else
        return;

    auto it = m_requests.find(id);
    if (it == m_requests.end())
    {

        dispatchNext();
        return;
    }

    const quint64 rendererId = m_renderer->requestPage(it->page, it->imageSize, buildOptions(*it));
    if (rendererId == 0)
    {

        m_requests.erase(it);
        dispatchNext();
        return;
    }

    it->rendererId = rendererId;
    it->inFlight = true;
    m_inFlight = true;
}

void PdfRenderService::removeRequest(quint64 id)
{
    m_highQueue.removeAll(id);
    m_lowQueue.removeAll(id);
    m_requests.remove(id);
}

void PdfRenderService::cancelRequest(quint64 requestId)
{
    if (requestId == 0)
        return;

    removeRequest(requestId);
}

void PdfRenderService::cancelAll()
{
    m_highQueue.clear();
    m_lowQueue.clear();
    m_requests.clear();

    m_inFlight = false;
}

void PdfRenderService::cancelLowPriority()
{

    for (const quint64 id : m_lowQueue)
    {
        m_requests.remove(id);
    }
    m_lowQueue.clear();

    if (m_inFlight)
    {
        for (auto it = m_requests.begin(); it != m_requests.end();)
        {
            if (it->inFlight && it->priority == Priority::Low)
            {
                it = m_requests.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}

void PdfRenderService::onPageRendered(int pageNumber, QSize, const QImage& image,
                                      const QPdfDocumentRenderOptions&, quint64 requestId)
{

    Request req;
    bool found = false;
    for (auto it = m_requests.begin(); it != m_requests.end(); ++it)
    {
        if (it->rendererId == requestId && it->inFlight)
        {
            req = it.value();
            found = true;
            m_requests.erase(it);
            break;
        }
    }

    m_inFlight = false;

    if (!found)
    {

        dispatchNext();
        return;
    }

    if (pageNumber != req.page)
    {
        dispatchNext();
        return;
    }

    if (!image.isNull())
    {
        m_cache->insert(req.page, req.scale, req.rotation, image, req.tileRow, req.tileCol);
    }
    emit renderFinished(req.id, req.page, req.scale, req.rotation);

    dispatchNext();
}

bool PdfRenderService::hasCache(int page, qreal scale, int rotation, int tileRow, int tileCol) const
{
    return m_cache->has(page, scale, rotation, tileRow, tileCol);
}

QString PdfRenderService::closestCacheKey(int page, qreal scale, int rotation) const
{
    return m_cache->closestKey(page, scale, rotation);
}

void PdfRenderService::clearCache()
{
    cancelAll();
    m_cache->clear();
}

int PdfRenderService::cachePageCount() const { return m_cache->pageCount(); }

qreal PdfRenderService::cacheMemoryMB() const { return m_cache->memoryMB(); }

} // namespace Notera
