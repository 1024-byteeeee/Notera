#pragma once

#include <QImage>
#include <QQuickImageProvider>
#include <QSize>
#include <QString>

namespace Notera
{

class PdfRenderCache;

class PdfCacheImageProvider final : public QQuickImageProvider
{
  public:
    explicit PdfCacheImageProvider(PdfRenderCache* cache);

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

  private:
    PdfRenderCache* m_cache;
};

} // namespace Notera
