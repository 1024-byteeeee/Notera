#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

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
    QStringList tags;
    int lastPage {1};
    QDateTime createdAt;
    QDateTime updatedAt;
    QDateTime lastOpenedAt;
};
