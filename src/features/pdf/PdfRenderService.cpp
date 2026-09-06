#include "features/pdf/PdfRenderService.h"

#include "features/pdf/PdfRenderCache.h"

#include <QSignalBlocker>
#include <QUrl>

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

void PdfRenderService::onOwnedDocumentStatusChanged(QPdfDocument::Status status)
{
    if (status == QPdfDocument::Status::Ready && m_document == m_ownedDocument) {
        emit documentChanged();
    }
}

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
    // QML 的 PdfDocument 实际上是 QQuickPdfDocument（QML 包装类），它通过
    // QML_EXTENDED(QPdfDocument) 机制内部持有一个真正的 QPdfDocument 实例。
    // qobject_cast<QPdfDocument*> 会失败（对象实际是 QQuickPdfDocument），
    // 且 QQuickPdfDocument::document() 是私有的无法访问。
    // 解决方案：创建自己的 QPdfDocument，从 QQuickPdfDocument 复制 source 加载。
    QPdfDocument* pdfDoc = nullptr;
    QString newSource; // 本次要加载的本地文件路径（空表示外部文档或无文档）

    if (document) {
        // 1. 尝试直接 qobject_cast（如果外部直接传入 QPdfDocument*）
        pdfDoc = qobject_cast<QPdfDocument*>(document);
        if (pdfDoc) {
            // 外部文档：指针相同则无需重置
            if (m_document == pdfDoc)
                return;
        } else {
            // 2. QQuickPdfDocument（QML 的 PdfDocument）：读取 source 加载到自己的文档
            const QVariant sourceVar = document->property("source");
            if (sourceVar.isValid() && sourceVar.canConvert<QUrl>()) {
                const QUrl source = sourceVar.toUrl();
                if (source.isValid() && !source.isEmpty()) {
                    newSource = source.toLocalFile();
                    // 同一个文件：无需重新加载，直接返回（避免触发重复渲染）
                    if (newSource == m_currentSource && m_ownedDocument)
                        return;
                    if (!m_ownedDocument) {
                        m_ownedDocument = new QPdfDocument(this);
                        connect(m_ownedDocument, &QPdfDocument::statusChanged,
                            this, &PdfRenderService::onOwnedDocumentStatusChanged);
                    }
                    // 阻塞 load 过程中的 statusChanged 信号：
                    // load() 是同步的，status 会在内部变为 Ready 并触发信号，
                    // 若此时发射 documentChanged() 会在 m_renderer 尚未 setDocument
                    // 时就驱动 QML 发起渲染请求，导致请求丢失 / m_inFlight 卡住。
                    const QSignalBlocker blocker(m_ownedDocument);
                    m_ownedDocument->load(newSource);
                    pdfDoc = m_ownedDocument;
                }
            }
        }
    }

    // 真正切换了文档（或关闭文档）：完整重置
    cancelAll();   // 清空请求队列并重置 m_inFlight（防止旧文档残留请求卡住渲染）
    m_cache->clear();
    m_document = pdfDoc;
    m_currentSource = newSource;
    if (m_document) {
        m_renderer->setDocument(m_document);
        // 文档已 Ready（同步 load 的正常情况）：立即通知 QML 刷新
        if (m_document->status() == QPdfDocument::Status::Ready)
            emit documentChanged();
    }
    // 若文档尚未 Ready（异步加载场景），等 onOwnedDocumentStatusChanged 通知
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
