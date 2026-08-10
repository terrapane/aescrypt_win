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
 *      It can also accept a single a /pipe= argument from which it reads
 *      a list of filenames.
 *
 *      The launcher loads the core aescrypt.dll to then perform encryption
 *      or decryption on the list of files.
 *
 *  Portability Issues:
 *      Windows specific code.
 */

#include "pch.h"
#include <Windows.h>
#include <ShObjIdl_core.h>
#include <shellapi.h>
#include <string>
#include <string_view>
#include <deque>
#include <array>
#include <vector>
#include <optional>
#include <algorithm>
#include <iterator>
#include <span>
#include "mode.h"
#include "file_list.h"
#include "has_aes_extension.h"
#include "aescrypt_launcher.h"
#include "file_selection.h"
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

/*
 *  ParsePipeID()
 *
 *  Description:
 *      Function to parse the argument string looking for "/pipe=nnnn".
 *
 *  Parameters:
 *      argument [in]
 *          String argument to parse.
 *
 *  Returns:
 *      An optional value containing the parsed integer value or std::nullopt
 *      if there was an error parsing the string.
 *
 *  Comments:
 *      None.
 */
std::optional<unsigned long long> ParsePipeID(const std::wstring_view argument)
{
    constexpr std::wstring_view prefix = L"/pipe=";

    // See if the argument starts with "/pipe="
    if (!argument.starts_with(prefix)) return std::nullopt;

    // Extract substring after prefix
    std::wstring number_part(argument.substr(prefix.length()));

    try
    {
        std::size_t position = 0;
        unsigned long long value = std::stoull(number_part, &position);

        // Ensure the entire remaining string was converted
        if (position == number_part.length()) return value;
    }
    catch (...)
    {
        // Parsing error means we ignore the argument
    }

    return std::nullopt;
}

/*
 *  ReadFileListFromPipe()
 *
 *  Description:
 *      This function will read the file list from the specified pipe, closing
 *      the pipe when finished.
 *
 *  Parameters:
 *      pipe_id [in]
 *          The integer value of the pipe file descriptor / handle from which
 *          to read filenames.
 *
 *  Returns:
 *      A FileList containing the list of read files or an empty list if there
 *      was any error.
 *
 *  Comments:
 *      None.
 */
FileList ReadFileListFromPipe(unsigned long long pipe_fd)
{
    FileList file_list;
    std::array<char, 1024 * sizeof(wchar_t)> buffer{};
    std::vector<char> accumulator;
    DWORD bytes_read = 0;

    HANDLE handle = reinterpret_cast<HANDLE>(pipe_fd);

    while (ReadFile(handle,
                    buffer.data(),
                    static_cast<DWORD>(buffer.size()),
                    &bytes_read,
                    nullptr))
    {
        if (bytes_read == 0) break;

        // Append new bytes
        accumulator.insert(accumulator.end(),
                           buffer.begin(),
                           std::next(buffer.begin(), bytes_read));

        // Look for filenames in the accumulator vector
        while (accumulator.size() >= sizeof(wchar_t))
        {
            // Create a type-safe view of whole wchar_t elements
            std::span<const wchar_t> wchar_span{
                reinterpret_cast<const wchar_t *>(accumulator.data()),
                accumulator.size() / sizeof(wchar_t)};

            // Search for the wide null terminator
            auto it = std::ranges::find(wchar_span, L'\0');
            if (it == wchar_span.end()) break;

            // Create a subspan representing strictly the filename
            std::span<const wchar_t> filename_span =
                wchar_span.first(std::distance(wchar_span.begin(), it));

            // Construct std::wstring directly from the span view
            file_list.emplace_back(filename_span.begin(), filename_span.end());

            // Erase processed bytes + the L'\0' character from the accumulator
            const size_t bytes_to_erase =
                (filename_span.size() + 1) * sizeof(wchar_t);
            accumulator.erase(accumulator.begin(),
                              std::next(accumulator.begin(), bytes_to_erase));
        }
    }

    // Capture the exit error state immediately after ReadFile returns false
    const DWORD last_error = GetLastError();

    CloseHandle(handle);

    // ERROR_SUCCESS (0) and ERROR_BROKEN_PIPE (109) are valid EOF states; any
    // other error or any partial bytes in accumulator indicates failure
    if ((last_error != ERROR_SUCCESS && last_error != ERROR_BROKEN_PIPE) ||
        !accumulator.empty())
    {
        return {};
    }

    return file_list;
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
    std::optional<unsigned long long> pipe_fd;
    std::wstring application_name(256, L'\0');

    // Load the application name
    HMODULE hModule = GetModuleHandle(NULL);
    auto title_length = LoadString(hModule,
                                   IDS_APP_TITLE,
                                   application_name.data(),
                                   static_cast<int>(application_name.size()));
    application_name.resize(title_length);

    // Get the command-line argument string, exiting on failure
    int nArgs;
    LPWSTR *szArglist = CommandLineToArgvW(GetCommandLine(), &nArgs);
    if (szArglist == nullptr) return 1;

    // Initialize COM (used in SelectFiles())
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        if (szArglist) LocalFree(szArglist);

        return 1;
    }

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
    HWND hwnd = CreateWindow(application_name.c_str(),
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

    ShowWindow(hwnd, SW_HIDE);
    UpdateWindow(hwnd);

    // If no filenames given on the command-line, then open File Explorer
    if (nArgs <= 1)
    {
        // Allow the user to select files to encrypt or decrypt
        file_list = SelectFiles(hwnd,
                                application_name,
                                L"Select File(s) to Encrypt or Decrypt");
    }
    else
    {
        // Given two arguments (the command and one parameter), see if
        // /pipe=nnn was provided
        if (nArgs == 2) pipe_fd = ParsePipeID(szArglist[1]);

        // If there is a valid /pipe argument, read the list from the pipe
        if (pipe_fd)
        {
            file_list = ReadFileListFromPipe(*pipe_fd);
        }
        else
        {
            // Put the given filename parameters in a list
            for (std::size_t i = 1; i < nArgs; i++)
            {
                file_list.emplace_back(szArglist[i]);
            }
        }
    }

    // If the mode parameter was not given, use the file extension
    AESCryptMode mode = DetermineMode(file_list);

    // Handle the file list (or lack of a list)
    if (file_list.empty())
    {
        // Signal the application to exit when no files were selected
        SendMessage(hwnd, WM_DESTROY, 0, 0);
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
        SendMessage(hwnd, WM_DESTROY, 0, 0);
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
                SendMessage(hwnd, WM_DESTROY, 0, 0);
            }
        }
    }

    // Uninitialize COM
    CoUninitialize();

    // Free allocated memory
    if (szArglist) LocalFree(szArglist);

    return static_cast<int>(msg.wParam);
}
