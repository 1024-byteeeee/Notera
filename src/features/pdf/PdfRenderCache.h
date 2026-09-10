#pragma once

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QString>

namespace Notera {





class PdfRenderCache final : public QObject
{
    Q_OBJECT
public:
    explicit PdfRenderCache(QObject* parent = nullptr);



    Q_INVOKABLE void insert(int page, qreal scale, int rotation, const QImage& image,
        int tileRow = -1, int tileCol = -1);


    Q_INVOKABLE QImage get(int page, qreal scale, int rotation,
        int tileRow = -1, int tileCol = -1);


    Q_INVOKABLE bool has(int page, qreal scale, int rotation,
        int tileRow = -1, int tileCol = -1) const;




    Q_INVOKABLE QString closestKey(int page, qreal scale, int rotation) const;


    Q_INVOKABLE void clear();


    Q_INVOKABLE void setMemoryBudgetMB(int mb);


    Q_INVOKABLE int pageCount() const;
    Q_INVOKABLE qreal memoryMB() const;


    QImage imageByKey(const QString& key) const;


    static int quantizeScale(qreal scale);

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

}
