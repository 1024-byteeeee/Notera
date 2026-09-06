#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>

#include <QPdfDocument>
#include <QPdfPageRenderer>

namespace Notera {

class PdfRenderCache;

// PDF 渲染服务：封装 QPdfPageRenderer（MultiThreaded 异步渲染队列 + 可取消请求）
// 与 PdfRenderCache（LRU 位图缓存）。
//
// 参考：
//  - QtPdf QPdfPageRenderer：requestPage 返回 quint64 ID，可 cancelPage，MultiThreaded 模式
//  - pdf.js RenderTask.cancel()：快速滚动时取消不再可见页的渲染
//  - Okular PixmapRequest.preload()：预渲染标记
class PdfRenderService final : public QObject
{
    Q_OBJECT
public:
    explicit PdfRenderService(QObject* parent = nullptr);
    ~PdfRenderService() override;

    // 设置文档。切换文档时自动清缓存并取消所有进行中请求。
    // 接受 QObject* 以兼容 QML PdfDocument 类型（内部 qobject_cast 为 QPdfDocument*）。
    Q_INVOKABLE void setDocument(QObject* document);
    QPdfDocument* document() const { return m_document; }

    // 请求渲染一页。返回 request ID（可用于取消）。
    // 若缓存已命中则不发起渲染，返回 0（QML 端应先查 hasCache）。
    Q_INVOKABLE quint64 requestRender(int page, qreal scale, int rotation, QSize imageSize);

    // 取消单个请求。
    Q_INVOKABLE void cancelRequest(quint64 requestId);

    // 取消所有进行中请求（切换文档 / 快速滚动时用）。
    Q_INVOKABLE void cancelAll();

    // ---- 缓存代理 ----
    Q_INVOKABLE bool hasCache(int page, qreal scale, int rotation) const;
    Q_INVOKABLE QString closestCacheKey(int page, qreal scale, int rotation) const;
    Q_INVOKABLE void clearCache();
    Q_INVOKABLE int cachePageCount() const;
    Q_INVOKABLE qreal cacheMemoryMB() const;

    PdfRenderCache* cache() const { return m_cache; }

    // 度数 → QPdfDocumentRenderOptions::Rotation 枚举。
    static QPdfDocumentRenderOptions::Rotation rotationFromDegrees(int degrees);

signals:
    // 渲染完成（QImage 已写入缓存）。QML 端通过 image://pdfcache/<key> 取图显示。
    void renderFinished(quint64 requestId, int page, qreal scale, int rotation);

private slots:
    void onPageRendered(int pageNumber, QSize imageSize,
        const QImage& image, const QPdfDocumentRenderOptions& options, quint64 requestId);

private:
    struct RequestInfo {
        int page;
        qreal scale;
        int rotation;
    };

    QPdfPageRenderer* m_renderer{nullptr};
    PdfRenderCache* m_cache{nullptr};
    QPdfDocument* m_document{nullptr};
    QHash<quint64, RequestInfo> m_pending;
};

} // namespace Notera
