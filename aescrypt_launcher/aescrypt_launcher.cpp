/*
 *  aescrypt_launcher.cpp
 *
 *  Copyright (C) 2006-2026
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This is the Win32 application that accepts a list of filenames
 *      and calls the encryption code that resides in the aescrypt.dll file.
 *      This program is relatively simple and relies entirely on the DLL
 *      to perform processing in the background.
 *
 *      The reason this program exists is to serve as a launcher that gets
 *      invoked when the user double-clicks on a .aes file or launches
 *      AES Crypt from the Start menu.  It is not intended to be used from
 *      the command-line, though it will work.  The tool aescrypt.exe file
 *      exists for use from the command-line.
 *
 *  Portability Issues:
 *      Windows specific code.
 */

#include "pch.h"
#include <Windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <string>
#include "mode.h"
#include "file_list.h"
#include "has_aes_extension.h"
#include "aescrypt_launcher.h"
#include "resource.h"
#include "aescrypt.h"

namespace
{

// Windows Callback Procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_PAINT:
        case WM_CREATE:
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Function to open a dialog window to select files
std::deque<std::wstring> SelectFiles(std::wstring_view application_title)
{
    std::deque<std::wstring> files;

    IFileOpenDialog *dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog,
                                  nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dialog));
    if (FAILED(hr)) return {};

    // Allow selection of multiple files
    DWORD options;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_ALLOWMULTISELECT);

    // Custom title
    std::wstring file_selection_title = std::wstring(application_title) +
        std::wstring(L" - Select File(s) to Encrypt or Decrypt");
    dialog->SetTitle(file_selection_title.c_str());

    // Show the file selection dialog
    hr = dialog->Show(nullptr);
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

// Function to determine whether the operation mode (based on file extension)
AESCryptMode DetermineMode(const std::deque<std::wstring> &file_list)
{
    bool aes_files{};
    bool non_aes_files{};

    // All of the files must be either AES or non-AES files
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

    // Mixing file types is not allowed, so returned "Undefined"
    if (aes_files && non_aes_files) return AESCryptMode::Undefined;

    // If there are only .aes files, mode set to decrypt
    if (aes_files) return AESCryptMode::Decrypt;

    return AESCryptMode::Encrypt;
}

} // namespace

// Main Procedure for Windows
int WINAPI wWinMain(_In_ HINSTANCE hInstance,
                    _In_opt_ HINSTANCE hPrevInstance,
                    _In_ [[maybe_unused]] LPWSTR lpCmdLine,
                    _In_ [[maybe_unused]] int nShowCmd)
{
    FileList file_list;
    std::wstring application_name(256, L'\0');

    // Load the application name
    HMODULE hModule = GetModuleHandle(NULL);
    auto title_length = LoadString(hModule,
                                   IDS_APP_TITLE,
                                   application_name.data(),
                                   static_cast<int>(application_name.size()));
    application_name.resize(title_length);

    // Initialize COM (used in SelectFiles())
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 1;

    // Get the command-line argument string, exiting on failure
    int nArgs;
    LPWSTR *szArglist = CommandLineToArgvW(GetCommandLine(), &nArgs);
    if (szArglist == nullptr) return 1;

    // Create the window class for the hidden application window
    if (!hPrevInstance)
    {
        WNDCLASS wndclass{};
        wndclass.style          = CS_HREDRAW | CS_VREDRAW;
        wndclass.lpfnWndProc    = WindowProc;
        wndclass.cbClsExtra     = 0;
        wndclass.cbWndExtra     = 0;
        wndclass.hInstance      = hInstance;
        wndclass.hIcon          = LoadIcon(hInstance,
                                           MAKEINTRESOURCE(IDI_AESCRYPT_LOCK));
        wndclass.hCursor        = LoadCursor(NULL, IDC_ARROW);
        wndclass.hbrBackground  = static_cast<HBRUSH>(
                                                GetStockObject(WHITE_BRUSH));
        wndclass.lpszMenuName   = NULL;
        wndclass.lpszClassName  = application_name.c_str();

        RegisterClass(&wndclass);
    }

    // Create the main application window for event control
    HWND hWnd = CreateWindow(application_name.c_str(),
                             application_name.c_str(),
                             WS_OVERLAPPED,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             NULL,
                             NULL,
                             hInstance,
                             NULL);

    ShowWindow(hWnd, SW_HIDE);
    UpdateWindow(hWnd);

    // If no filenames given on the command-line, then open File Explorer
    if (nArgs <= 1)
    {
        // Allow the user to select files to encrypt or decrypt
        file_list = SelectFiles(application_name);
    }
    else
    {
        // Put the filenames given into the file_list
        for (std::size_t i = 1; i < nArgs; i++)
        {
            file_list.emplace_back(szArglist[i]);
        }
    }

    // If the mode parameter was not given, use the file extension
    AESCryptMode mode = DetermineMode(file_list);

    // Handle the file list (or lack of a list)
    if (file_list.empty())
    {
        // Signal the application to exit when no files were selected
        SendMessage(hWnd, WM_DESTROY, 0, 0);
    }
    else if (mode == AESCryptMode::Undefined)
    {
        // If there was a mixture of file types, report the error
        ::MessageBox(NULL,
                     L"Please select either .aes or non-.aes files, but not "
                     L"both at the same time",
                     application_name.c_str(),
                     MB_ICONERROR | MB_OK);

        // Signal the application to exit
        SendMessage(hWnd, WM_DESTROY, 0, 0);
    }
    else
    {
        // Initiate file processing
        ProcessFiles(file_list, mode);
    }

    // Sit in a loop waiting for the AES Crypt Library to indicate it is no
    // longer busy and message processing completes on WM_DESTROY
    MSG msg;
    while (true)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
        {
            if (GetMessage(&msg, NULL, 0, 0))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else
            {
                // GetMessage() returns false when WM_QUIT is posted
                break;
            }
        }
        else
        {
            // If the AES library is busy, sleep.  Otherwise send a WM_DESTROY
            // message to terminate the main window processing loop
            if (AESLibraryBusy())
            {
                Sleep(250);
            }
            else
            {
                SendMessage(hWnd, WM_DESTROY, 0, 0);
            }
        }
    }

    // Uninitialize COM
    CoUninitialize();

    // Free allocated memory
    if (szArglist) LocalFree(szArglist);

    return static_cast<int>(msg.wParam);
}
