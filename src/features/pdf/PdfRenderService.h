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









class PdfRenderService final : public QObject
{
    Q_OBJECT
public:
    enum class Priority {
        High,
        Low
    };
    Q_ENUM(Priority)

    explicit PdfRenderService(QObject* parent = nullptr);
    ~PdfRenderService() override;



    Q_INVOKABLE void setDocument(QObject* document);
    QPdfDocument* document() const { return m_document; }




    Q_INVOKABLE quint64 requestRender(int page, qreal scale, int rotation,
        QSize imageSize, int priority = 0, int tileRow = -1, int tileCol = -1,
        int tileCount = 1);


    Q_INVOKABLE void cancelRequest(quint64 requestId);


    Q_INVOKABLE void cancelAll();


    Q_INVOKABLE void cancelLowPriority();


    Q_INVOKABLE bool hasCache(int page, qreal scale, int rotation,
        int tileRow = -1, int tileCol = -1) const;
    Q_INVOKABLE QString closestCacheKey(int page, qreal scale, int rotation) const;
    Q_INVOKABLE void clearCache();
    Q_INVOKABLE int cachePageCount() const;
    Q_INVOKABLE qreal cacheMemoryMB() const;

    PdfRenderCache* cache() const { return m_cache; }


    static QPdfDocumentRenderOptions::Rotation rotationFromDegrees(int degrees);

signals:

    void renderFinished(quint64 requestId, int page, qreal scale, int rotation);

    void documentChanged();

private slots:
    void onPageRendered(int pageNumber, QSize imageSize,
        const QImage& image, const QPdfDocumentRenderOptions& options, quint64 requestId);
    void onOwnedDocumentStatusChanged(QPdfDocument::Status status);

private:
    struct Request {
        quint64 id{0};
        quint64 rendererId{0};
        int page{0};
        qreal scale{0};
        int rotation{0};
        QSize imageSize;
        Priority priority{Priority::High};
        bool inFlight{false};
        int tileRow{-1};
        int tileCol{-1};
        int tileCount{1};
    };

    void dispatchNext();
    void removeRequest(quint64 id);

    QPdfDocumentRenderOptions buildOptions(const Request& req) const;

    QPdfPageRenderer* m_renderer{nullptr};
    PdfRenderCache* m_cache{nullptr};
    QPdfDocument* m_document{nullptr};
    QPdfDocument* m_ownedDocument{nullptr};
    QString m_currentSource;

    QHash<quint64, Request> m_requests;
    QList<quint64> m_highQueue;
    QList<quint64> m_lowQueue;
    bool m_inFlight{false};
    quint64 m_nextId{1};
};

}
