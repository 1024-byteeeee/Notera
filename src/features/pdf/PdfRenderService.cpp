#include "features/pdf/PdfRenderService.h"

#include "features/pdf/PdfRenderCache.h"

namespace Notera {

PdfRenderService::PdfRenderService(QObject* parent)
    : QObject(parent)
    , m_renderer(new QPdfPageRenderer(this))
    , m_cache(new PdfRenderCache(this))
{
    m_renderer->setRenderMode(QPdfPageRenderer::RenderMode::MultiThreaded);
    connect(m_renderer, &QPdfPageRenderer::pageRendered,
        this, &PdfRenderService::onPageRendered);
}

PdfRenderService::~PdfRenderService() = default;

QPdfDocumentRenderOptions::Rotation PdfRenderService::rotationFromDegrees(int degrees)
{
    const int norm = ((degrees % 360) + 360) % 360;
    switch (norm) {
    case 90:  return QPdfDocumentRenderOptions::Rotation::Clockwise90;
    case 180: return QPdfDocumentRenderOptions::Rotation::Clockwise180;
    case 270: return QPdfDocumentRenderOptions::Rotation::Clockwise270;
    default:  return QPdfDocumentRenderOptions::Rotation::None;
    }
}

void PdfRenderService::setDocument(QObject* document)
{
    QPdfDocument* pdfDoc = document ? qobject_cast<QPdfDocument*>(document) : nullptr;
    if (m_document == pdfDoc)
        return;
    cancelAll();
    m_cache->clear();
    m_document = pdfDoc;
    m_renderer->setDocument(pdfDoc);
}

quint64 PdfRenderService::requestRender(int page, qreal scale, int rotation, QSize imageSize)
{
    if (!m_document || m_document->status() != QPdfDocument::Status::Ready)
        return 0;
    if (page < 0 || page >= m_document->pageCount())
        return 0;
    if (imageSize.isEmpty())
        return 0;
    if (m_cache->has(page, scale, rotation))
        return 0;

    QPdfDocumentRenderOptions options;
    options.setRotation(rotationFromDegrees(rotation));

    const quint64 id = m_renderer->requestPage(page, imageSize, options);
    if (id != 0) {
        m_pending.insert(id, {page, scale, rotation});
    }
    return id;
}

void PdfRenderService::cancelRequest(quint64 requestId)
{
    // QPdfPageRenderer 没有公开的 cancelPage API，无法真正中止后台渲染。
    // 从 pending 表移除后，onPageRendered 会忽略该请求的结果（不写入缓存、不发信号）。
    if (requestId != 0)
        m_pending.remove(requestId);
}

void PdfRenderService::cancelAll()
{
    m_pending.clear();
}

void PdfRenderService::onPageRendered(int pageNumber, QSize /*imageSize*/,
    const QImage& image, const QPdfDocumentRenderOptions& /*options*/, quint64 requestId)
{
    const auto it = m_pending.constFind(requestId);
    if (it == m_pending.constEnd())
        return; // 已被取消的请求：忽略结果
    const RequestInfo info = it.value();
    m_pending.erase(it);

    // 防御：pageNumber 应与 info.page 一致
    if (pageNumber != info.page)
        return;

    if (!image.isNull()) {
        m_cache->insert(info.page, info.scale, info.rotation, image);
    }
    emit renderFinished(requestId, info.page, info.scale, info.rotation);
}

// ---- 缓存代理 ----

bool PdfRenderService::hasCache(int page, qreal scale, int rotation) const
{
    return m_cache->has(page, scale, rotation);
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

int PdfRenderService::cachePageCount() const
{
    return m_cache->pageCount();
}

qreal PdfRenderService::cacheMemoryMB() const
{
    return m_cache->memoryMB();
}

} // namespace Notera
