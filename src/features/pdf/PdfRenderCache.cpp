#include "features/pdf/PdfRenderCache.h"

#include <QDateTime>
#include <QList>
#include <QStringList>
#include <algorithm>
#include <limits>

namespace Notera {

PdfRenderCache::PdfRenderCache(QObject* parent)
    : QObject(parent)
{
}

int PdfRenderCache::quantizeScale(qreal scale)
{
    // 4 位小数精度：renderScale 通常 0.1~3.0，4 位足够区分不同缩放级别。
    return qRound(scale * 10000.0);
}

QString PdfRenderCache::makeKey(int page, qreal scale, int rotation)
{
    return QString::number(page) + QLatin1Char('_')
        + QString::number(quantizeScale(scale)) + QLatin1Char('_')
        + QString::number(rotation);
}

size_t PdfRenderCache::imageBytes(const QImage& img)
{
    if (img.isNull())
        return 0;
    // QImage::sizeInBytes() 在 Qt 5.10+ 可用；用 width*height*depth/8 兜底。
    return static_cast<size_t>(img.width()) * static_cast<size_t>(img.height())
        * static_cast<size_t>((img.depth() + 7) / 8);
}

void PdfRenderCache::insert(int page, qreal scale, int rotation, const QImage& image)
{
    if (image.isNull())
        return;

    QMutexLocker lock(&m_mutex);
    const QString key = makeKey(page, scale, rotation);
    const size_t bytes = imageBytes(image);

    auto it = m_entries.find(key);
    if (it != m_entries.end()) {
        // 覆盖：先减旧内存
        m_currentBytes -= it->bytes;
        it->image = image;
        it->bytes = bytes;
        it->lastAccess = QDateTime::currentMSecsSinceEpoch();
    } else {
        Entry entry;
        entry.image = image;
        entry.bytes = bytes;
        entry.lastAccess = QDateTime::currentMSecsSinceEpoch();
        m_entries.insert(key, std::move(entry));
    }
    m_currentBytes += bytes;

    evictIfNeeded();
}

QImage PdfRenderCache::get(int page, qreal scale, int rotation)
{
    QMutexLocker lock(&m_mutex);
    const QString key = makeKey(page, scale, rotation);
    auto it = m_entries.find(key);
    if (it == m_entries.end())
        return {};
    it->lastAccess = QDateTime::currentMSecsSinceEpoch();
    return it->image;
}

bool PdfRenderCache::has(int page, qreal scale, int rotation) const
{
    QMutexLocker lock(&m_mutex);
    return m_entries.contains(makeKey(page, scale, rotation));
}

QString PdfRenderCache::closestKey(int page, qreal scale, int rotation) const
{
    QMutexLocker lock(&m_mutex);
    const int targetQ = quantizeScale(scale);
    QString bestKey;
    int bestDiff = std::numeric_limits<int>::max();

    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        const QString& key = it.key();
        const QStringList parts = key.split(QLatin1Char('_'));
        if (parts.size() != 3)
            continue;
        bool okPage = false, okScale = false, okRot = false;
        const int p = parts[0].toInt(&okPage);
        const int s = parts[1].toInt(&okScale);
        const int r = parts[2].toInt(&okRot);
        if (!okPage || !okScale || !okRot)
            continue;
        if (p != page || r != rotation)
            continue;
        const int diff = qAbs(s - targetQ);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestKey = key;
        }
    }
    return bestKey;
}

void PdfRenderCache::clear()
{
    QMutexLocker lock(&m_mutex);
    m_entries.clear();
    m_currentBytes = 0;
}

void PdfRenderCache::setMemoryBudgetMB(int mb)
{
    QMutexLocker lock(&m_mutex);
    m_memoryBudget = static_cast<size_t>(qMax(16, mb)) * 1024ULL * 1024ULL;
    evictIfNeeded();
}

int PdfRenderCache::pageCount() const
{
    QMutexLocker lock(&m_mutex);
    return m_entries.size();
}

qreal PdfRenderCache::memoryMB() const
{
    QMutexLocker lock(&m_mutex);
    return static_cast<qreal>(m_currentBytes) / (1024.0 * 1024.0);
}

QImage PdfRenderCache::imageByKey(const QString& key) const
{
    QMutexLocker lock(&m_mutex);
    auto it = m_entries.constFind(key);
    if (it == m_entries.constEnd())
        return {};
    return it->image;
}

void PdfRenderCache::evictIfNeeded()
{
    // 调用方已持锁。按 lastAccess 升序淘汰，直到内存低于预算。
    if (m_currentBytes <= m_memoryBudget || m_entries.isEmpty())
        return;

    QList<QPair<qint64, QString>> byTime;
    byTime.reserve(m_entries.size());
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        byTime.append({it->lastAccess, it.key()});
    }
    std::sort(byTime.begin(), byTime.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    for (const auto& [_, key] : byTime) {
        if (m_currentBytes <= m_memoryBudget)
            break;
        auto it = m_entries.find(key);
        if (it == m_entries.end())
            continue;
        m_currentBytes -= it->bytes;
        m_entries.erase(it);
    }
}

} // namespace Notera
