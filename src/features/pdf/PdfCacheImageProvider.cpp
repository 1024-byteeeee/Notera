#include "features/pdf/PdfCacheImageProvider.h"

#include "features/pdf/PdfRenderCache.h"

namespace Notera {

PdfCacheImageProvider::PdfCacheImageProvider(PdfRenderCache* cache)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_cache(cache)
{
}

QImage PdfCacheImageProvider::requestImage(const QString& id, QSize* size,
    const QSize& requestedSize)
{
    if (!m_cache)
        return {};

    // id 可能带查询参数（如 "0_3670_0?cachebuster=123"），取 '?' 前部分
    const QString key = id.section(QLatin1Char('?'), 0, 0);
    QImage img = m_cache->imageByKey(key);
    if (img.isNull())
        return {};

    if (size)
        *size = img.size();

    if (!requestedSize.isEmpty() && requestedSize != img.size()) {
        return img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return img;
}

} // namespace Notera
