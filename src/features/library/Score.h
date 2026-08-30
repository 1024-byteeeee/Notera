#pragma once

#include <QDateTime>
#include <QString>

struct Score {
    QString id;
    QString title;
    QString composer;
    QString fileName;
    QString filePath;
    QString fileType;
    int pageCount {1};
    QString thumbnailPath;
    bool favorite {false};
    int lastPage {1};
    QDateTime createdAt;
    QDateTime updatedAt;
    QDateTime lastOpenedAt;
};
