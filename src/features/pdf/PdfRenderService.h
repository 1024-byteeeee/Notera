#pragma once

#include <QHash>
#include <QImage>
#include <QList>
#include <QObject>
#include <QSize>
#include <QString>

#include <QPdfDocument>
#include <QPdfPageRenderer>

namespace Notera {

class PdfRenderCache;

// PDF 渲染服务：封装 QPdfPageRenderer（MultiThreaded 异步渲染）与 PdfRenderCache（LRU 缓存）。
//
// Phase 2 增强：
//  - 优先级队列：可见页 High 优先级插队，预渲染 Low 优先级排队
//  - 单请求在飞：QPdfPageRenderer 内部永远只有 0/1 个请求，优先级完全由本层控制
//  - cancelLowPriority()：快速滚动时批量取消所有预渲染请求（pdf.js RenderTask.cancel 技巧）
//
// 参考：pdf.js RenderTask 优先级与取消、Okular PixmapRequest.preload 标记。
class PdfRenderService final : public QObject
{
    Q_OBJECT
public:
    enum class Priority {
        High, // 可见页：立即插队渲染
        Low   // 预渲染相邻页：排队，可被批量取消
    };
    Q_ENUM(Priority)

    explicit PdfRenderService(QObject* parent = nullptr);
    ~PdfRenderService() override;

    // 设置文档。切换文档时自动清缓存并取消所有请求。
    // 接受 QObject* 以兼容 QML PdfDocument 类型（内部 qobject_cast 为 QPdfDocument*）。
    Q_INVOKABLE void setDocument(QObject* document);
    QPdfDocument* document() const { return m_document; }

    // 请求渲染一页或一页中的某一块。返回唯一 request ID（可用于 cancelRequest）。
    // 缓存已命中则返回 0。priority: 0=High(可见页插队), 1=Low(预渲染可批量取消)。
    // tileRow/tileCol 默认 -1 表示整页渲染；>=0 且 tileCount>1 表示分块渲染某一块。
    Q_INVOKABLE quint64 requestRender(int page, qreal scale, int rotation,
        QSize imageSize, int priority = 0, int tileRow = -1, int tileCol = -1,
        int tileCount = 1);

    // 取消单个请求（排队中的直接移除；在飞的渲染完成后忽略结果）。
    Q_INVOKABLE void cancelRequest(quint64 requestId);

    // 取消所有请求。
    Q_INVOKABLE void cancelAll();

    // 批量取消所有 Low 优先级请求（快速滚动时调用，避免浪费 CPU 渲染不再可见的预渲染页）。
    Q_INVOKABLE void cancelLowPriority();

    // ---- 缓存代理 ----
    Q_INVOKABLE bool hasCache(int page, qreal scale, int rotation,
        int tileRow = -1, int tileCol = -1) const;
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
    // 文档设置/切换完成（此时 document 已 Ready）。QML 端监听此信号触发刷新。
    void documentChanged();

private slots:
    void onPageRendered(int pageNumber, QSize imageSize,
        const QImage& image, const QPdfDocumentRenderOptions& options, quint64 requestId);
    void onOwnedDocumentStatusChanged(QPdfDocument::Status status);

private:
    struct Request {
        quint64 id{0};           // 对外请求 ID（本层自增）
        quint64 rendererId{0};   // QPdfPageRenderer 返回的 ID（发送后赋值，排队中为 0）
        int page{0};
        qreal scale{0};
        int rotation{0};
        QSize imageSize;
        Priority priority{Priority::High};
        bool inFlight{false};    // 已发送给 QPdfPageRenderer 执行中
        int tileRow{-1};         // 分块行（-1=整页）
        int tileCol{-1};         // 分块列（-1=整页）
        int tileCount{1};        // 分块网格大小（1=不分块）
    };

    void dispatchNext();   // 从队列取下一个最高优先级请求发送给 QPdfPageRenderer
    void removeRequest(quint64 id); // 从所有数据结构中移除请求
    // 根据 Request 构建渲染选项（rotation + 分块 scaledClipRect）
    QPdfDocumentRenderOptions buildOptions(const Request& req) const;

    QPdfPageRenderer* m_renderer{nullptr};
    PdfRenderCache* m_cache{nullptr};
    QPdfDocument* m_document{nullptr};       // 当前用于渲染的文档（可能是 m_ownedDocument 或外部传入）
    QPdfDocument* m_ownedDocument{nullptr};  // 自己创建的文档（从 QQuickPdfDocument 复制 source）
    QString m_currentSource;                 // 当前已加载文档的本地文件路径（用于判断是否真的切换了文档）

    QHash<quint64, Request> m_requests;   // 所有请求（在飞 + 排队）
    QList<quint64> m_highQueue;            // 高优先级排队请求 ID（FIFO）
    QList<quint64> m_lowQueue;             // 低优先级排队请求 ID（FIFO）
    bool m_inFlight{false};                 // QPdfPageRenderer 中是否有请求在执行
    quint64 m_nextId{1};                    // 自增请求 ID
};

} // namespace Notera
