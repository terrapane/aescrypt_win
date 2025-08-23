/*
 *  aescrypt_launcher.cpp
 *
 *  Copyright (C) 2006, 2008, 2013, 2024, 2025
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
 *      invoked when the user double-clicks on a .aes file.  It is not
 *      intended to be used directly by the user or via the command-line.
 *
 *  Portability Issues:
 *      Windows specific code.
 */

#include "pch.h"
#include <Windows.h>
#include <tchar.h>
#include <cstdint>
#include "aescrypt_launcher.h"

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

// Main Procedure for Windows
int WINAPI wWinMain(_In_ HINSTANCE hInstance,
                    _In_opt_ HINSTANCE hPrevInstance,
                    _In_ [[maybe_unused]] LPWSTR lpCmdLine,
                    _In_ [[maybe_unused]] int nShowCmd)
{
    WNDCLASS wndclass{};
    HWND hWnd;
    MSG msg;
    int nArgs;
    bool encrypt = false;
    FileList file_list;
    std::wstring application_name(256, '\0');

    // Load the application name
    HMODULE hModule = GetModuleHandle(NULL);
    auto title_length = LoadString(hModule,
                                   IDS_APP_TITLE,
                                   application_name.data(),
                                   static_cast<int>(application_name.size()));
    application_name.resize(title_length);

    // Get the command-line argument string
    LPWSTR *szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
    if(szArglist == NULL) return 0;

    // Create the window class and application window (hidden)
    if (!hPrevInstance)
    {
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
    hWnd = CreateWindow(application_name.c_str(),
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

    // Process the command-line arguments
    for (std::size_t i = 1; i < nArgs; i++)
    {
        if (i == 1)
        {
            if (!_tcscmp(szArglist[i], L"/d"))
            {
                encrypt = false;
            }
            else if (!_tcscmp(szArglist[i], L"-d"))
            {
                encrypt = false;
            }
            else if (!_tcscmp(szArglist[i], L"/e"))
            {
                encrypt = true;
            }
            else if (!_tcscmp(szArglist[i], L"-e"))
            {
                encrypt = true;
            }
            else
            {
                file_list.push_back(szArglist[i]);
            }
        }
        else
        {
            file_list.push_back(szArglist[i]);
        }
    }

    // Report an error if the file list is empty
    if (file_list.empty())
    {
        ::MessageBox(NULL,
                     L"Usage: aescrypt_launcher [/d|/e] filename ...",
                     application_name.c_str(),
                     MB_ICONERROR | MB_OK);
        SendMessage(hWnd, WM_DESTROY, 0, 0);
    }
    else
    {
        // Initiate file processing
        ProcessFiles(file_list, encrypt);
    }

    // Sit in a loop waiting for the AES Crypt Library to indicate it is no
    // longer busy and message processing completes on WM_DESTROY.
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
            // If the AES library is busy, we will sleep.  Otherwise
            // we will send a WM_DESTROY message to terminate the main
            // window processing loop.
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

    // Free allocated memory
    LocalFree(szArglist);

    return static_cast<int>(msg.wParam);
}
