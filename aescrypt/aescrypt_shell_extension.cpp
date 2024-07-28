/*
 *  aescrypt_shell_extension.cpp
 *
 *  Copyright (C) 2006, 2008, 2013, 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This file implements the C++ class for integrating with the Windows
 *      shell.
 *
 *  Portability Issues:
 *      Windows specific code.
 */

#include "pch.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <filesystem>
#include <algorithm>
#include <commctrl.h>
#include "aescrypt.h"
#include "aescrypt_shell_extension.h"
#include "worker_threads.h"
#include "has_aes_extension.h"

// Make the global worker thread object visible in this module
extern WorkerThreads Worker_Threads;

/*
 *  AESCryptShellExtension::AESCryptShellExtension()
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
AESCryptShellExtension::AESCryptShellExtension() :
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
}

/*
 *  AESCryptShellExtension::~AESCryptShellExtension()
 *
 *  Description:
 *     Destructor for the AESCryptShellExtension, which only needs to destroy
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
AESCryptShellExtension::~AESCryptShellExtension()
{
    // Free the bitmap object if it was loaded
    if (context_bitmap != NULL) DeleteObject(context_bitmap);
}

/*
 *  AESCryptShellExtension::Initialize()
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
HRESULT AESCryptShellExtension::Initialize(LPCITEMIDLIST pidlFolder,
                                           LPDATAOBJECT pDO,
                                           HKEY hProgID)
{
    FORMATETC etc = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM stg = {TYMED_HGLOBAL};

    // Read the list of folders
    if (FAILED(pDO->GetData(&etc,&stg))) return E_INVALIDARG;

    // Get an HDROP handle
    HDROP hDrop = static_cast<HDROP>(GlobalLock(stg.hGlobal));
    if (hDrop == NULL)
    {
        ReleaseStgMedium(&stg);
        return E_INVALIDARG;
    }

    // Clear the file list (paranoia)
    file_list.clear();

    // Initialize the variables that indicate the type of files we have
    aes_files = false;
    non_aes_files = false;

    // Get a count of the number of files
    UINT file_count = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

    // Iterate over the list of files
    for (UINT i = 0; i < file_count; i++)
    {
        std::wstring filename;
        wchar_t filename_buffer[MAX_PATH];

        // Try to retrieve a filename associated with this invocation
        if (!DragQueryFile(hDrop, i, filename_buffer, MAX_PATH)) continue;

        // Get the file name length and copy it into a std::wstring
        size_t filename_length = _tcsnlen(filename_buffer, MAX_PATH);
        std::copy(filename_buffer,
                  filename_buffer + filename_length,
                  std::back_inserter(filename));

        // Determine if this is a .aes file or not
        if (HasAESExtension(filename))
        {
            aes_files = true;

            // Do not allow mixing of file types
            if (non_aes_files == true) break;
        }
        else
        {
            non_aes_files = true;

            // Do not allow mixing of file types
            if (aes_files == true) break;
        }

        file_list.emplace_back(std::move(filename));
    }

    // Release resources
    GlobalUnlock(stg.hGlobal);
    ReleaseStgMedium(&stg);

    // Do not show the menu if both a mix of .aes and non-.aes files seen
    if ((aes_files == true) && (non_aes_files == true)) return E_INVALIDARG;

    // If there are no files in the list, do not render a menu
    if (file_list.empty()) return E_INVALIDARG;

    return S_OK;
}

/*
 *  AESCryptShellExtension::QueryContextMenu()
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
 *          First command ID to use for custom menu additons.
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
HRESULT AESCryptShellExtension::QueryContextMenu(HMENU hMenu,
                                                 UINT uMenuIndex,
                                                 UINT uidFirstCmd,
                                                 UINT uidLastCmd,
                                                 UINT uFlags)
{
    // If the flags include CMF_DEFAULTONLY, do nothing
    if (uFlags & CMF_DEFAULTONLY)
    {
        return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);
    }

    // Should not happen, but do nothing if both .aes and non-.aes files seen
    // or if the file list is empty
    if (((aes_files == true) && (non_aes_files == true)) || file_list.empty())
    {
        return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);
    }

    // Insert the menu choice
    if (aes_files == true)
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
 *  AESCryptShellExtension::GetCommandString()
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
 *      puReserved [in]
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
HRESULT AESCryptShellExtension::GetCommandString(UINT_PTR idCmd,
#else
HRESULT AESCryptShellExtension::GetCommandString(UINT idCmd,
#endif
                                                 UINT uType,
                                                 UINT* puReserved,
                                                 LPSTR szName,
                                                 UINT cchMax)
{
    const wchar_t *command_text;

    // There is only one command, so the idCmd should always be 0
    if (idCmd != 0)
    {
        ATLASSERT(0);                           // should never get here
        return E_INVALIDARG;
    }

    // What command was given?
    switch (uType)
    {
        case GCS_HELPTEXT:
            if (aes_files == true)
            {
                command_text = L"Decrypt selected AES file(s)";
            }
            else
            {
                command_text = L"AES Encrypt selected file(s)";
            }

            // Copy the help text into the supplied buffer
            if (!lstrcpynW(reinterpret_cast<PWSTR>(szName),
                           command_text,
                           cchMax))
            {
                return E_FAIL;
            }

            break;

        case GCS_VERB:
            if (aes_files == true)
            {
                command_text = L"AES Decrypt";
            }
            else
            {
                command_text = L"AES Encrypt";
            }

            // Copy the verbtext into the supplied buffer
            if (!lstrcpynW(reinterpret_cast<PWSTR>(szName),
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
 *  AESCryptShellExtension::InvokeCommand()
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
HRESULT AESCryptShellExtension::InvokeCommand(LPCMINVOKECOMMANDINFO pInfo)
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
    Worker_Threads.ProcessFiles(file_list, (non_aes_files == TRUE));

    // Clear the file list
    file_list.clear();

    return S_OK;
}
