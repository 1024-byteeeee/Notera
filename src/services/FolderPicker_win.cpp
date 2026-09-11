#include "services/FolderPicker.h"

#include <QGuiApplication>
#include <QWindow>

#include <shobjidl.h>
#include <windows.h>

FolderPicker::FolderPicker(QObject* parent) : QObject(parent) {}

QStringList FolderPicker::pickFolders()
{
    QStringList result;

    IFileOpenDialog* pDialog = nullptr;
    const HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                        IID_PPV_ARGS(&pDialog));
    if (FAILED(hr))
        return result;

    DWORD options = 0;
    if (SUCCEEDED(pDialog->GetOptions(&options)))
    {
        pDialog->SetOptions(options | FOS_PICKFOLDERS | FOS_ALLOWMULTISELECT);
    }

    HWND ownerWindow = nullptr;
    if (QWindow* window = QGuiApplication::focusWindow())
        ownerWindow = reinterpret_cast<HWND>(window->winId());

    if (SUCCEEDED(pDialog->Show(ownerWindow)))
    {
        IShellItemArray* items = nullptr;
        if (SUCCEEDED(pDialog->GetResults(&items)) && items)
        {
            DWORD count = 0;
            items->GetCount(&count);
            for (DWORD i = 0; i < count; ++i)
            {
                IShellItem* item = nullptr;
                if (SUCCEEDED(items->GetItemAt(i, &item)) && item)
                {
                    PWSTR path = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path)
                    {
                        result.append(QString::fromWCharArray(path));
                        CoTaskMemFree(path);
                    }
                    item->Release();
                }
            }
            items->Release();
        }
    }

    pDialog->Release();
    return result;
}
