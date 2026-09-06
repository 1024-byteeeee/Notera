#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QString>

namespace Notera {

// LRU 渲染页面位图缓存。
// key = (pageIndex, renderScale, rotation)，按内存预算自动淘汰最久未用页。
// 参考 SumatraPDF RenderCache（按页数上限 + 优先释放不可见页）与
// Sioyek cached_responses（时间 LRU + try_closest_rendered_page）。
class PdfRenderCache final : public QObject
{
    Q_OBJECT
public:
    explicit PdfRenderCache(QObject* parent = nullptr);

    // 写入缓存。若已存在同 key 则覆盖并更新内存统计。
    // tileRow/tileCol 默认 -1 表示整页；>=0 表示分块渲染的某一块。
    Q_INVOKABLE void insert(int page, qreal scale, int rotation, const QImage& image,
        int tileRow = -1, int tileCol = -1);

    // 精确匹配取图，命中则更新 lastAccess。未命中返回空 QImage。
    Q_INVOKABLE QImage get(int page, qreal scale, int rotation,
        int tileRow = -1, int tileCol = -1);

    // 精确匹配是否存在（不更新 lastAccess）。
    Q_INVOKABLE bool has(int page, qreal scale, int rotation,
        int tileRow = -1, int tileCol = -1) const;

    // 返回同一 (page, rotation) 下 scale 最接近的缓存 key 字符串；
    // 无缓存返回空字符串。用于缩放时先用旧分辨率拉伸显示（Sioyek closest 技巧）。
    // key 格式："<page>_<scale4>_<rotation>"
    Q_INVOKABLE QString closestKey(int page, qreal scale, int rotation) const;

    // 清空全部缓存。
    Q_INVOKABLE void clear();

    // 设置内存预算（MB），默认 256。超预算时淘汰最久未用页。
    Q_INVOKABLE void setMemoryBudgetMB(int mb);

    // 当前缓存页数 / 占用 MB（调试用）。
    Q_INVOKABLE int pageCount() const;
    Q_INVOKABLE qreal memoryMB() const;

    // QQuickImageProvider 用：按 key 字符串取图（不更新 lastAccess，因为 provider 可能在任意线程）。
    QImage imageByKey(const QString& key) const;

    // 量化 scale 到 4 位小数，避免浮点 hash 不稳定。
    static int quantizeScale(qreal scale);
    // 生成缓存 key。tileRow/tileCol 默认 -1 表示整页；>=0 生成分块 key。
    static QString makeKey(int page, qreal scale, int rotation,
        int tileRow = -1, int tileCol = -1);

private:
    struct Entry {
        QImage image;
        qint64 lastAccess{0};
        size_t bytes{0};
    };

    void evictIfNeeded();
    static size_t imageBytes(const QImage& img);

    mutable QMutex m_mutex;
    QHash<QString, Entry> m_entries;
    size_t m_memoryBudget{256ULL * 1024 * 1024};
    size_t m_currentBytes{0};
};

} // namespace Notera
