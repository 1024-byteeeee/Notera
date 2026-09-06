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

quint64 PdfRenderService::requestRender(int page, qreal scale, int rotation,
    QSize imageSize, int priority, int tileRow, int tileCol, int tileCount)
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

    // 生成对外请求 ID
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

    if (!m_inFlight) {
        // 立即发送
        const quint64 rendererId = m_renderer->requestPage(page, imageSize, buildOptions(req));
        if (rendererId == 0)
            return 0;
        req.rendererId = rendererId;
        req.inFlight = true;
        m_inFlight = true;
    } else {
        // 入队：高优先级插队到低优先级前面（高优先级队列始终先于低优先级处理）
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
    if (req.tileRow >= 0 && req.tileCol >= 0 && req.tileCount > 1) {
        // 分块渲染：scaledSize = 整页渲染尺寸，scaledClipRect = 该块在整页中的矩形
        const int blockW = req.imageSize.width() / req.tileCount;
        const int blockH = req.imageSize.height() / req.tileCount;
        options.setScaledSize(req.imageSize);
        options.setScaledClipRect(QRect(
            req.tileCol * blockW, req.tileRow * blockH, blockW, blockH));
    }
    return options;
}

void PdfRenderService::dispatchNext()
{
    if (m_inFlight)
        return;
    if (!m_document || m_document->status() != QPdfDocument::Status::Ready)
        return;

    // 高优先级优先，其次低优先级
    quint64 id = 0;
    if (!m_highQueue.isEmpty())
        id = m_highQueue.takeFirst();
    else if (!m_lowQueue.isEmpty())
        id = m_lowQueue.takeFirst();
    else
        return;

    auto it = m_requests.find(id);
    if (it == m_requests.end()) {
        // 已被取消，继续取下一个
        dispatchNext();
        return;
    }

    const quint64 rendererId = m_renderer->requestPage(
        it->page, it->imageSize, buildOptions(*it));
    if (rendererId == 0) {
        // 发送失败，移除并继续
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
    // 排队中的直接移除；在飞的保留在 m_requests 中但标记逻辑取消——
    // 实际上直接移除即可，onPageRendered 用 rendererId 查找时找不到就忽略。
    removeRequest(requestId);
}

void PdfRenderService::cancelAll()
{
    m_highQueue.clear();
    m_lowQueue.clear();
    m_requests.clear();
    // 注意：QPdfPageRenderer 中可能还有一个在飞请求，完成后 onPageRendered
    // 找不到对应 Request（m_requests 已清空），会忽略结果。
    m_inFlight = false;
}

void PdfRenderService::cancelLowPriority()
{
    // 移除所有低优先级排队请求
    for (const quint64 id : m_lowQueue) {
        m_requests.remove(id);
    }
    m_lowQueue.clear();

    // 如果在飞的请求是低优先级，也移除（完成时忽略结果）
    if (m_inFlight) {
        for (auto it = m_requests.begin(); it != m_requests.end(); ) {
            if (it->inFlight && it->priority == Priority::Low) {
                it = m_requests.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void PdfRenderService::onPageRendered(int pageNumber, QSize /*imageSize*/,
    const QImage& image, const QPdfDocumentRenderOptions& /*options*/, quint64 requestId)
{
    // 用 rendererId 查找对应的 Request
    Request req;
    bool found = false;
    for (auto it = m_requests.begin(); it != m_requests.end(); ++it) {
        if (it->rendererId == requestId && it->inFlight) {
            req = it.value();
            found = true;
            m_requests.erase(it);
            break;
        }
    }

    m_inFlight = false;

    if (!found) {
        // 已被取消的请求：忽略结果，继续处理队列
        dispatchNext();
        return;
    }

    if (pageNumber != req.page) {
        dispatchNext();
        return;
    }

    if (!image.isNull()) {
        m_cache->insert(req.page, req.scale, req.rotation, image,
            req.tileRow, req.tileCol);
    }
    emit renderFinished(req.id, req.page, req.scale, req.rotation);

    // 处理下一个排队请求
    dispatchNext();
}

// ---- 缓存代理 ----

bool PdfRenderService::hasCache(int page, qreal scale, int rotation,
    int tileRow, int tileCol) const
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

int PdfRenderService::cachePageCount() const
{
    return m_cache->pageCount();
}

qreal PdfRenderService::cacheMemoryMB() const
{
    return m_cache->memoryMB();
}

} // namespace Notera
