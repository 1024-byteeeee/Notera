#include "features/library/ThumbnailGenerator.h"

#include <QImage>
#include <QMetaObject>
#include <QPdfDocument>
#include <QPromise>
#include <QtConcurrent>

#include "platform/AppDataPaths.h"

ThumbnailGenerator::ThumbnailGenerator(QObject* parent)
    : QObject(parent)
{
}

void ThumbnailGenerator::generate(const QString& scoreId, const QString& scorePath, const QString& fileType)
{
    m_tasks.addFuture(QtConcurrent::run([this, scoreId, scorePath, fileType] {
        QImage image;
        if (fileType == QStringLiteral("pdf")) {
            QPdfDocument document;
            document.load(scorePath);
            if (document.status() != QPdfDocument::Status::Ready || document.pageCount() < 1) {
                QMetaObject::invokeMethod(this, [this, scoreId] { emit failed(scoreId, QStringLiteral("Notera 无法生成 PDF 缩略图。")); }, Qt::QueuedConnection);
                return;
            }
            const auto pointSize = document.pagePointSize(0);
            image = document.render(0, pointSize.scaled(300, 400, Qt::KeepAspectRatio).toSize());
        } else {
            image.load(scorePath);
            image = image.scaled(300, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }

        const auto thumbnailPath = AppDataPaths::thumbnailDirectory() + QLatin1Char('/') + scoreId + QStringLiteral(".png");
        if (image.isNull() || !image.save(thumbnailPath, "PNG")) {
            QMetaObject::invokeMethod(this, [this, scoreId] { emit failed(scoreId, QStringLiteral("Notera 无法保存乐谱缩略图。")); }, Qt::QueuedConnection);
            return;
        }
        QMetaObject::invokeMethod(this, [this, scoreId, thumbnailPath] { emit generated(scoreId, thumbnailPath); }, Qt::QueuedConnection);
    }));
}
