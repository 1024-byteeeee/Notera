#include "services/FolderPicker.h"

#import <AppKit/AppKit.h>

FolderPicker::FolderPicker(QObject* parent) : QObject(parent) {}

QStringList FolderPicker::pickFolders()
{
    QStringList result;
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = YES;
    panel.message = @"选择要导入的文件夹";
    if ([panel runModal] == NSModalResponseOK)
    {
        for (NSURL* url in panel.URLs)
        {
            result.append(QString::fromNSString(url.path));
        }
    }
    return result;
}
