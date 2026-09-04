#pragma once

#include <QFutureSynchronizer>
#include <QObject>
#include <QString>
#include <QThreadPool>

class ThumbnailGenerator final : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailGenerator(QObject* parent = nullptr);
    void generate(const QString& scoreId, const QString& scorePath, const QString& fileType);

signals:
    void generated(QString scoreId, QString thumbnailPath);
    void failed(QString scoreId, QString message);

private:
    QFutureSynchronizer<void> m_tasks;
    QThreadPool m_threadPool;
};
