/*
 *  file_selection.cpp
 *
 *  Copyright (C) 2026
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This module implements a file selection window with a IFileDialogEvents
 *      class that facilitates centering the window on the initial run.
 *      Windows will cache the location on subsequent runs per the user's
 *      preferences.
 *
 *  Portability Issues:
 *      Windows specific code.
 */

#include "pch.h"
#include <Windows.h>
#include <ShObjIdl_core.h>
#include <shlwapi.h>
#include <deque>
#include <string>
#include <string_view>
#include "file_selection.h"

namespace
{

// Center the dialog on the initial run
class DialogCenterEvents : public IFileDialogEvents
{
    public:
        // IUnknown methods
        IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv)
        {
            static const QITAB qit[] = {
                QITABENT(DialogCenterEvents, IFileDialogEvents),
                {0},
            };
            return QISearch(this, qit, riid, ppv);
        }
        IFACEMETHODIMP_(ULONG) AddRef()
        {
            return InterlockedIncrement(&reference_count);
        }
        IFACEMETHODIMP_(ULONG) Release()
        {
            LONG count = InterlockedDecrement(&reference_count);
            if (count == 0) delete this;
            return count;
        }

        // IFileDialogEvents methods
        IFACEMETHODIMP OnFolderChange(IFileDialog *pfd)
        {
            if (!centered)
            {
                centered = true;

                // Query the dialog for IOleWindow to get its HWND
                IOleWindow *pOleWindow = nullptr;
                if (SUCCEEDED(pfd->QueryInterface(IID_PPV_ARGS(&pOleWindow))))
                {
                    HWND hwnd = nullptr;
                    if (SUCCEEDED(pOleWindow->GetWindow(&hwnd)))
                    {
                        // Get the dialog's actual window size
                        RECT rc{};
                        GetWindowRect(hwnd, &rc);
                        int dialog_width = rc.right - rc.left;
                        int dialog_height = rc.bottom - rc.top;

                        // Get the screen size
                        int screen_width = GetSystemMetrics(SM_CXSCREEN);
                        int screen_height = GetSystemMetrics(SM_CYSCREEN);

                        // Determine the point that will center the dialog
                        int pos_x = (screen_width - dialog_width) / 2;
                        int pos_y = (screen_height - dialog_height) / 2;

                        SetWindowPos(hwnd,
                                     nullptr,
                                     pos_x,
                                     pos_y,
                                     0,
                                     0,
                                     SWP_NOSIZE | SWP_NOZORDER |
                                         SWP_NOACTIVATE);
                    }
                    pOleWindow->Release();
                }
            }

            return S_OK;
        }

        // Stub out the rest of the required interface methods
        IFACEMETHODIMP OnFileOk(IFileDialog *)
        {
            return S_OK;
        }
        IFACEMETHODIMP OnFolderChanging(IFileDialog *, IShellItem *)
        {
            return S_OK;
        }
        IFACEMETHODIMP OnSelectionChange(IFileDialog *)
        {
            return S_OK;
        }
        IFACEMETHODIMP OnShareViolation(IFileDialog *,
                                        IShellItem *,
                                        FDE_SHAREVIOLATION_RESPONSE *)
        {
            return S_OK;
        }
        IFACEMETHODIMP OnTypeChange(IFileDialog *)
        {
            return S_OK;
        }
        IFACEMETHODIMP OnOverwrite(IFileDialog *,
                                IShellItem *,
                                FDE_OVERWRITE_RESPONSE *)
        {
            return S_OK;
        }

    private:
        LONG reference_count = 1;
        bool centered = false;
};

} // namespace

/*
 *  SelectFiles()
 *
 *  Description:
 *      Function to open a dialog window to allow the user to select files
 *      for encrypting or decrypting.
 *
 *  Parameters:
 *      hwnd [in]
 *          Handle to the owning window.  This can be NULL if there isn't one.
 *
 *      application_title [in]
 *          The name of the application to appear at the top of the dialog.
 *
 *      description [in]
 *          Description to appear following the application title.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
std::deque<std::wstring> SelectFiles(HWND hwnd,
                                     std::wstring_view application_title,
                                     std::wstring_view description)
{
    std::deque<std::wstring> files;

    IFileOpenDialog *dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog,
                                  nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dialog));
    if (FAILED(hr)) return {};

    // Allow selection of multiple files
    DWORD options{};
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_ALLOWMULTISELECT);

    // Custom title
    std::wstring file_selection_title = std::wstring(application_title) +
                                        std::wstring(L" - ") +
                                        std::wstring(description);
    dialog->SetTitle(file_selection_title.c_str());

    DialogCenterEvents *center_events = new DialogCenterEvents();
    DWORD dwCookie{};
    bool advised = SUCCEEDED(dialog->Advise(center_events, &dwCookie));

    // Show the file selection dialog
    hr = dialog->Show(hwnd);
    if (advised) dialog->Unadvise(dwCookie);
    center_events->Release();
    if (FAILED(hr))
    {
        dialog->Release();
        return {};
    }

    // Get selected items
    IShellItemArray *items = nullptr;
    hr = dialog->GetResults(&items);
    if (SUCCEEDED(hr))
    {
        DWORD count = 0;
        items->GetCount(&count);

        // Iterate over all of the items
        for (DWORD i = 0; i < count; i++)
        {
            IShellItem *pItem = nullptr;
            if (SUCCEEDED(items->GetItemAt(i, &pItem)))
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &path)))
                {
                    files.emplace_back(path);
                    CoTaskMemFree(path);
                }
                pItem->Release();
            }
        }
        items->Release();
    }

    dialog->Release();

    return files;
}
