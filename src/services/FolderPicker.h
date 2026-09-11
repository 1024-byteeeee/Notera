#pragma once

#include <QObject>
#include <QStringList>

/** 系统原生文件夹多选对话框（macOS: NSOpenPanel / Windows: IFileDialog）。 */
class FolderPicker final : public QObject
{
    Q_OBJECT

  public:
    explicit FolderPicker(QObject* parent = nullptr);

    /** 模态弹出系统原生多选文件夹对话框，返回选中的本地绝对路径；取消时返回空列表。 */
    Q_INVOKABLE QStringList pickFolders();
};
