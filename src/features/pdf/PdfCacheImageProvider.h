#pragma once

#include <QImage>
#include <QQuickImageProvider>
#include <QSize>
#include <QString>

namespace Notera {

class PdfRenderCache;

// QQuickImageProvider：QML Image 通过 "image://pdfcache/<key>" 从渲染缓存取 QImage。
// key 格式与 PdfRenderCache::makeKey 一致："<page>_<scaleQuantized>_<rotation>"
class PdfCacheImageProvider final : public QQuickImageProvider
{
public:
    explicit PdfCacheImageProvider(PdfRenderCache* cache);

    QImage requestImage(const QString& id, QSize* size,
        const QSize& requestedSize) override;

private:
    PdfRenderCache* m_cache; // 不持有所有权
};

} // namespace Notera
