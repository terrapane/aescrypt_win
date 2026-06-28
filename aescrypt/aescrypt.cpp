 /*
 *  aescrypt.cpp
 *
 *  Copyright (C) 2026
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This module contains the main entry points for the DLL and the exported
 *      functions to facilitate carrying out the encryption and decryption
 *      of files.
 *
 *  Portability Issues:
 *      Windows specific code.
 */

#include "pch.h"
#include <Windows.h>
#include <atlbase.h>
#include <atlhost.h>
#include "worker_threads.h"
#include "mode.h"
#include "file_list.h"
#include "aescrypt.h"

namespace
{

// Function to wrap the WorkerThreads object, as only one instance is needed
WorkerThreads &GetWorkerThreads()
{
    // Worker Threads class to perform encrytion/decryption in the background
    static WorkerThreads worker_threads;

    return worker_threads;
}

} // namespace

// DLL Entry Point
BOOL WINAPI DllMain([[maybe_unused]] HMODULE module_handle,
                    DWORD reason,
                    [[maybe_unused]] LPVOID reserved)
{
    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
            AtlAxWinInit();
            break;

        case DLL_PROCESS_DETACH:
            // Ensure the worker threads have fully exited; the user of this
            // library should call AESLibraryBusy() so that this routine is not
            // forced to sit in a loop an wait, but is added as a precaution
            while (GetWorkerThreads().IsBusy()) Sleep(250);
            break;
    }

    return TRUE;
}

// Exported function that allows aescrypt_launcher.exe use this library to
// encrypt or decrypt a list of files
void __cdecl ProcessFiles(FileList &file_list, AESCryptMode mode)
{
    GetWorkerThreads().ProcessFiles(file_list, mode);
}

// Exported function that allows the library user to determine if all of the
// active encryption or decryption threads have completed work
bool __cdecl AESLibraryBusy()
{
    return GetWorkerThreads().IsBusy();
}
