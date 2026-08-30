#pragma once

#include <QObject>
#include <QString>

class ThumbnailGenerator final : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailGenerator(QObject* parent = nullptr);
    void generate(const QString& scoreId, const QString& scorePath, const QString& fileType);

signals:
    void generated(QString scoreId, QString thumbnailPath);
    void failed(QString scoreId, QString message);
};
