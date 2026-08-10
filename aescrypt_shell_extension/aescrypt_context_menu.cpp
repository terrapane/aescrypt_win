/*
 *  aescrypt_context_menu.cpp
 *
 *  Copyright (C) 2006-2026
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This file implements the C++ class for integrating with the Windows
 *      shell to provide the AES Crypt context menu.
 *
 *  Portability Issues:
 *      Windows specific code.
 */

#include "pch.h"
#include <Windows.h>
#include <atlcomcli.h>
#include <atldef.h>
#include <atlbase.h>
#include <shtypes.h>
#include <Shellapi.h>
#include <ShObjIdl_core.h>
#include <wchar.h>
#include <string>
#include <vector>
#include <utility>
#include <filesystem>
#include <algorithm>
#include <limits>
#include <cstddef>
#include <span>
#include "aescrypt_context_menu.h"
#include "has_aes_extension.h"
#include "resource.h"

namespace
{

/*
 *  GetModulePath()
 *
 *  Description:
 *      Get the path to the directory holding the module identified by
 *      the module handle provided.
 *
 *  Parameters:
 *      module [in]
 *          Module handle for which to get the path.
 *
 *  Returns:
 *      A string containing the path to the module or an empty string if there
 *      was an error.
 *
 *  Comments:
 *      None.
 */
std::wstring GetModulePath(HMODULE module)
{
    std::wstring pathname(512, L'\0');

    while (true)
    {
        DWORD length = GetModuleFileName(module,
                                         pathname.data(),
                                         static_cast<DWORD>(pathname.size()));

        if (length == 0) return {};

        if (length < pathname.size() - 1)
        {
            pathname.resize(length);
            return pathname;
        }

        // Buffer was too small, so grow and retry
        pathname.resize(pathname.size() * 2);
    }
}

/*
 *  WriteToPipe()
 *
 *  Description:
 *      This function will write data to the pipe, ensuring that all data
 *      is written or return false if there is a failure.
 *
 *  Parameters:
 *      handle [in]
 *          Handle to the pipe to which to write
 *
 *      data [in]
 *          Data to write
 *
 *  Returns:
 *      A string containing the path to the module or an empty string if there
 *      was an error.
 *
 *  Comments:
 *      None.
 */
bool WriteToPipe(HANDLE handle, std::span<const std::byte> data)
{
    while (!data.empty())
    {
        // Cap single write size to DWORD maximum (4 GB)
        const DWORD bytesToWrite = static_cast<DWORD>(
            std::min<std::size_t>(data.size(),
                                  std::numeric_limits<DWORD>::max()));

        DWORD written = 0;
        if (!WriteFile(handle, data.data(), bytesToWrite, &written, nullptr))
        {
            return false;
        }

        if (written == 0)
        {
            // Pipe accepted zero bytes; treat as failure
            return false;
        }

        // Advance the span window by slicing off the written bytes
        data = data.subspan(written);
    }

    return true;
}

} // namespace

/*
 *  AESCryptContextMenu::AESCryptContextMenu()
 *
 *  Description:
 *     The class constructor loads a bitmap and performs other initialization
 *     operations.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
AESCryptContextMenu::AESCryptContextMenu() :
    context_bitmap{},
    aes_files{},
    non_aes_files{}
{
    // Load the context menu bitmap; note that LR_CREATEDIBSECTION will
    // preserve the alpha channel information stored in the bitmap while
    // LR_DEFAULTCOLOR (previously used flag in older versions of AES Crypt)
    // will not preserve the alpha channel to result in a transparent look
    context_bitmap =
        static_cast<HBITMAP>(LoadImage(ATL::_pModule->GetModuleInstance(),
                                       MAKEINTRESOURCE(IDB_CTXBITMAP),
                                       IMAGE_BITMAP,
                                       0,
                                       0,
                                       LR_CREATEDIBSECTION));

    application_name.resize(256, L'\0');
    auto length = LoadString(ATL::_pModule->GetModuleInstance(),
                             IDS_APP_TITLE,
                             application_name.data(),
                             static_cast<int>(application_name.size()));
    application_name.resize(length);
}

/*
 *  AESCryptContextMenu::~AESCryptContextMenu()
 *
 *  Description:
 *     Destructor for the AESCryptContextMenu, which only needs to destroy
 *     the bitmap (if it exists).
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
AESCryptContextMenu::~AESCryptContextMenu()
{
    // Free the bitmap object if it was loaded
    if (context_bitmap != NULL) DeleteObject(context_bitmap);
}

/*
 *  AESCryptContextMenu::Initialize()
 *
 *  Description:
 *      This function is called to initialize the context menu.  At this point,
 *      it is possible to get a list of files and insert the menu option.
 *
 *  Parameters:
 *      pidlFolder [in]
 *          Fully qualified item identifier list (PIDL) of the folder where the
 *          shell extension menu is being initialized.
 *
 *      pDO [in]
 *          Pointer to data object associated with the selected file or folder.
 *
 *      hProgID [in]
 *          A handle to the registry key associated with the programmatic
 *          identifier (ProgID) of the selected item.
 *
 *  Returns:
 *      HRESULT code indicating success or failure.
 *
 *  Comments:
 *      None.
 */
HRESULT AESCryptContextMenu::Initialize(
                                    [[maybe_unused]] LPCITEMIDLIST pidlFolder,
                                    LPDATAOBJECT pDO,
                                    [[maybe_unused]] HKEY hProgID)
{
    // Clear the file list (paranoia)
    file_list.clear();

    // Initialize the variables that indicate the type of files we have
    aes_files = false;
    non_aes_files = false;

    // If pDO is NULL, just return
    if (!pDO) return E_INVALIDARG;

    // Attempt to use SHCreateShellItemArrayFromDataObject
    ATL::CComPtr<IShellItemArray> pItemArray;
    HRESULT hr =
        SHCreateShellItemArrayFromDataObject(pDO, IID_PPV_ARGS(&pItemArray));
    if (SUCCEEDED(hr) && pItemArray)
    {
        DWORD count;
        hr = pItemArray->GetCount(&count);
        if (FAILED(hr)) return hr;

        for (DWORD i = 0; i < count; i++)
        {
            ATL::CComPtr<IShellItem> pItem;
            hr = pItemArray->GetItemAt(i, &pItem);
            if (SUCCEEDED(hr))
            {
                ATL::CComHeapPtr<WCHAR> pszName;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszName);
                if (SUCCEEDED(hr))
                {
                    // Create a std::wstring to hold the filename
                    std::wstring filename(pszName);

                    file_list.emplace_back(std::move(filename));
                }
                else
                {
                    // Try alternative name for cloud files
                    hr = pItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING,
                                               &pszName);
                    if (SUCCEEDED(hr))
                    {
                        // Create a std::wstring to hold the filename
                        std::wstring filename(pszName);

                        file_list.emplace_back(std::move(filename));
                    }
                }
            }
        }
    }
    else
    {
        // Fall back to using HDROP
        FORMATETC etc = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM stg = {TYMED_HGLOBAL};

        // Read the list of folders
        if (FAILED(pDO->GetData(&etc, &stg))) return E_INVALIDARG;

        // Get an HDROP handle
        HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
        if (hDrop == NULL)
        {
            ReleaseStgMedium(&stg);
            return E_INVALIDARG;
        }

        // Get a count of the number of files
        UINT file_count = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

        // Iterate over the list of files
        for (UINT i = 0; i < file_count; i++)
        {
            // Determine the length of the filename
            UINT filename_length = DragQueryFile(hDrop, i, nullptr, 0);
            if (filename_length == 0) continue;

            std::vector<wchar_t> filename_buffer(filename_length + 1);

            // Try to retrieve a filename associated with this invocation
            if (!DragQueryFile(hDrop,
                               i,
                               filename_buffer.data(),
                               filename_length + 1))
            {
                continue;
            }

            // Create a std::wstring to hold the filename
            std::wstring filename(
                filename_buffer.data(),
                wcsnlen(filename_buffer.data(), filename_buffer.size()));

            file_list.emplace_back(std::move(filename));
        }

        // Release resources
        GlobalUnlock(stg.hGlobal);
        ReleaseStgMedium(&stg);
    }

    // All of the files must be either AES or non-AES files; figure this out
    for (const auto &filename : file_list)
    {
        // Determine if this is a .aes file or not
        if (HasAESExtension(filename))
        {
            aes_files = true;

            // Do not allow mixing of file types
            if (non_aes_files) break;
        }
        else
        {
            non_aes_files = true;

            // Do not allow mixing of file types
            if (aes_files) break;
        }
    }

    // Do not show the menu if both a mix of .aes and non-.aes files seen
    if (aes_files && non_aes_files)
    {
        file_list.clear();
        return E_INVALIDARG;
    }

    // If there are no files in the list, do not render a menu
    if (file_list.empty()) return E_INVALIDARG;

    return S_OK;
}

/*
 *  AESCryptContextMenu::QueryContextMenu()
 *
 *  Description:
 *      This function will render the context menu when called by the Windows
 *      shell.
 *
 *  Parameters:
 *      hMenu [in]
 *          A handle to the context menu.
 *
 *      uMenuIndex [in]
 *          Position at which the menu item is to be inserted.
 *
 *      uidFirstCmd [in]
 *          First command ID to use for custom menu additions.
 *
 *      uidLastCmd [in]
 *          Last command ID available for use in menu.
 *
 *      uFlags [in]
 *          Flags that control behavior of menu item insertion.
 *
 *  Returns:
 *      HRESULT code indicating success or failure.
 *
 *  Comments:
 *      None.
 */
HRESULT AESCryptContextMenu::QueryContextMenu(HMENU hMenu,
                                              UINT uMenuIndex,
                                              UINT uidFirstCmd,
                                              [[maybe_unused]] UINT uidLastCmd,
                                              UINT uFlags)
{
    // If the flags include CMF_DEFAULTONLY, do nothing
    if (uFlags & CMF_DEFAULTONLY)
    {
        return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);
    }

    // Should not happen, but do nothing if both .aes and non-.aes files seen
    // or if the file list is empty
    if ((aes_files && non_aes_files) || file_list.empty())
    {
        return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);
    }

    // Insert the menu choice
    if (aes_files)
    {
        InsertMenu(hMenu,
                   uMenuIndex,
                   MF_STRING | MF_BYPOSITION,
                   uidFirstCmd,
                   L"AES Decrypt");
    }
    else
    {
        InsertMenu(hMenu,
                   uMenuIndex,
                   MF_STRING | MF_BYPOSITION,
                   uidFirstCmd,
                   L"AES Encrypt");
    }

    // Insert the context menu icon
    if (context_bitmap != NULL)
    {
        SetMenuItemBitmaps(hMenu,
                           uMenuIndex,
                           MF_BYPOSITION,
                           context_bitmap,
                           NULL);
    }

    // Tell the shell that we added one menu item
    return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 1);
}

/*
 *  AESCryptContextMenu::GetCommandString()
 *
 *  Description:
 *      This function provides help information.
 *
 *  Parameters:
 *      uCmdID [in]
 *          Offset of the menu command identifier.
 *
 *      uFlags [in]
 *          Flags that control behavior of the query operation.
 *
 *      pwReserved [in]
 *          Reserved.
 *
 *      szName [in]
 *          Address of a buffer into which a NULL-terminated string is written.
 *
 *      cchMax [in]
 *          Size of the above buffer in characters.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
#ifdef _M_X64
HRESULT AESCryptContextMenu::GetCommandString(UINT_PTR idCmd,
                                              UINT uType,
                                              [[maybe_unused]] UINT *pwReserved,
                                              LPSTR szName,
                                              UINT cchMax)
#else
HRESULT AESCryptContextMenu::GetCommandString(UINT idCmd,
                                              UINT uType,
                                              [[maybe_unused]] UINT *pwReserved,
                                              LPSTR szName,
                                              UINT cchMax)
#endif
{
    const wchar_t *command_text;

    // There is only one command, so the idCmd should always be 0
    if (idCmd != 0)
    {
        ATLASSERT(idCmd != 0);                  // should never get here
        return E_INVALIDARG;
    }

    // What command was given?
    switch (uType)
    {
        case GCS_HELPTEXT:
            if (aes_files)
            {
                command_text = L"Decrypt selected AES file(s)";
            }
            else
            {
                command_text = L"AES Encrypt selected file(s)";
            }

            // Copy the help text into the supplied buffer
            if (!lstrcpyn(reinterpret_cast<PWSTR>(szName),
                          command_text,
                          cchMax))
            {
                return E_FAIL;
            }

            break;

        case GCS_VERB:
            if (aes_files)
            {
                command_text = L"AES Decrypt";
            }
            else
            {
                command_text = L"AES Encrypt";
            }

            // Copy the verb text into the supplied buffer
            if (!lstrcpyn(reinterpret_cast<PWSTR>(szName),
                          command_text,
                          cchMax))
            {
                return E_FAIL;
            }

            break;

        default:
            {
                // No need to handle other values
            }
    }

    return S_OK;
}

/*
 *  AESCryptContextMenu::InvokeCommand()
 *
 *  Description:
 *      This function will start the work of encrypting or decrypting when
 *      the user selects this shell extension from the context menu.
 *
 *  Parameters:
 *      pInfo [in]
 *          Command invocation information.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
HRESULT AESCryptContextMenu::InvokeCommand(LPCMINVOKECOMMANDINFO pInfo)
{
    // If lpVerb really points to a string, ignore this function call
    if (HIWORD(pInfo->lpVerb) != 0) return E_INVALIDARG;

    // AES Crypt inserts only one menu, so the command value should be 0
    if (LOWORD(pInfo->lpVerb) != 0)
    {
        ATLASSERT(0);                           // should never get here
        return E_INVALIDARG;
    }

    // The menu item was invoked, so process the list of files
    auto result = ProcessFiles();

    // Clear the file list
    file_list.clear();

    return result;
}

/*
 *  AESCryptContextMenu::ProcessFiles()
 *
 *  Description:
 *      This function will invoke the background process to process to user's
 *      selected files.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
HRESULT AESCryptContextMenu::ProcessFiles()
{
    // Locate the location of the shell extension DLL
    HMODULE hModule = reinterpret_cast<HMODULE>(&__ImageBase);

    // Determine the location of the AES Crypt Shell Extension DLL and
    // assume the same directory for the AES Crypt Launcher
    std::wstring dll_path = GetModulePath(hModule);
    std::filesystem::path dll_dir =
        std::filesystem::path(dll_path).parent_path();
    std::wstring launcher = (dll_dir / L"aescrypt_launcher.exe").wstring();

    // Create an anonymous pipe
    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
    HANDLE handle_read = nullptr;
    HANDLE handle_write = nullptr;

    // Request a 64K buffer for this pipe
    DWORD pipe_buffer_size = 65536;
    if (!CreatePipe(&handle_read, &handle_write, &sa, pipe_buffer_size))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Build command line
    std::wstring cmdLine =
        L"\"" + launcher + L"\" /pipe=" +
        std::to_wstring(reinterpret_cast<unsigned long long>(handle_read));

    // Ensure the write end is strictly local to the shell extension
    SetHandleInformation(handle_write, HANDLE_FLAG_INHERIT, 0);

    // Set up explicit handle inheritance list (only inherit handle_read)
    SIZE_T attributeSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);

    // Ensure the attribute list size is not zero
    if (attributeSize == 0)
    {
        DWORD error = GetLastError();
        CloseHandle(handle_read);
        CloseHandle(handle_write);
        return HRESULT_FROM_WIN32(error);
    }

    std::vector<BYTE> attributeListBuffer(attributeSize);
    PPROC_THREAD_ATTRIBUTE_LIST pAttributeList =
        reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(
            attributeListBuffer.data());

    // Initialize the attribute list
    if (!InitializeProcThreadAttributeList(pAttributeList,
                                           1,
                                           0,
                                           &attributeSize))
    {
        DWORD error = GetLastError();
        CloseHandle(handle_read);
        CloseHandle(handle_write);
        return HRESULT_FROM_WIN32(error);
    }

    // Update the attribute list
    if (!UpdateProcThreadAttribute(pAttributeList,
                                   0,
                                   PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                   &handle_read,
                                   sizeof(HANDLE),
                                   nullptr,
                                   nullptr))
    {
        DWORD error = GetLastError();
        DeleteProcThreadAttributeList(pAttributeList);
        CloseHandle(handle_read);
        CloseHandle(handle_write);
        return HRESULT_FROM_WIN32(error);
    }

    // Configure STARTUPINFOEXW
    STARTUPINFOEXW siEx = {};
    siEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    siEx.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    siEx.StartupInfo.hStdInput = handle_read;
    siEx.StartupInfo.hStdOutput = INVALID_HANDLE_VALUE;
    siEx.StartupInfo.hStdError = INVALID_HANDLE_VALUE;
    siEx.lpAttributeList = pAttributeList;

    PROCESS_INFORMATION pi = {};

    // Launch the AES Crypt Launcher process
    BOOL created = CreateProcess(nullptr,
                                 cmdLine.data(),
                                 nullptr,
                                 nullptr,
                                 TRUE, // Inherit handles
                                 EXTENDED_STARTUPINFO_PRESENT,
                                 nullptr,
                                 nullptr,
                                 &siEx.StartupInfo,
                                 &pi);

    // Clean up attribute list structure
    DeleteProcThreadAttributeList(pAttributeList);

    if (!created)
    {
        DWORD error = GetLastError();
        CloseHandle(handle_read);
        CloseHandle(handle_write);

        ::MessageBox(NULL,
                     L"Failed to start the AES Crypt",
                     application_name.c_str(),
                     MB_ICONERROR | MB_OK);

        return HRESULT_FROM_WIN32(error);
    }

    //  Close read handle so child owns the sole read handle
    CloseHandle(handle_read);

    // Write file list to the pipe
    bool write_ok = true;
    for (const auto &filename : file_list)
    {
        // Write out the length of the filename + the null terminator
        write_ok = WriteToPipe(
            handle_write,
            std::as_bytes(std::span{filename.c_str(), filename.length() + 1}));

        if (!write_ok) break;
    }

    // Terminate the child process if there is an error
    DWORD write_error{};
    if (!write_ok)
    {
        write_error = GetLastError();
        TerminateProcess(pi.hProcess, 1);
    }

    // Signal EOF
    CloseHandle(handle_write);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return write_ok ? S_OK : HRESULT_FROM_WIN32(write_error);
}
