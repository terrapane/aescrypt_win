/*
 *  aescrypt_interface.cpp
 *
 *  Copyright (C) 2026
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This module defines functions that serve as an interface to the
 *      aescrypt.dll library.  This library is loaded and unloaded on demand
 *      and this interface code simplifies access.
 *
 *  Portability Issues:
 *      None.
 */

#include "pch.h"
#include <Windows.h>
#include <mutex>
#include <string_view>
#include "aescrypt_interface.h"
#include <file_list.h>
#include <mode.h>

// Function to return the global AESCryptInterface instance
AESCryptInterface &GetAESCryptInterface()
{
    static AESCryptInterface aescrypt_instance;

    return aescrypt_instance;
}

/*
 *  AESCryptInterface::~AESCryptInterface()
 *
 *  Description:
 *      Destructor for the AESCryptInterface object.  This will ensure the
 *      DLL gets unloaded.
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
AESCryptInterface::~AESCryptInterface()
{
    // Wait for threads to exit (should never have threads, except cleanup),
    // after which the library will be unloaded, too
    while (AESLibraryBusy()) Sleep(250);
}

/*
 *  AESCryptInterface::SetApplicationTitle()
 *
 *  Description:
 *      This function will set the application title used if the interface
 *      needs to render a message to the user.
 *
 *  Parameters:
 *      title [in]
 *          The string that represents that application title.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
void AESCryptInterface::SetApplicationTitle(std::wstring_view title)
{
    std::lock_guard<std::mutex> lock(mutex);
    application_title = title;
}

/*
 *  AESCryptInterface::SetModulePath()
 *
 *  Description:
 *      This function will inform the AES Crypt Interface as to where to find
 *      the aescrypt.dll.  Since the shell extension is loaded into File
 *      Explorer, just trying to load the DLL using a relative path will
 *      fail.  But, the shell extension knows from where it was loaded.
 *
 *  Parameters:
 *      path [in]
 *          The string representing the directory containing the AES Crypt
 *          installation and associated DLLs.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
void AESCryptInterface::SetModulePath(std::wstring_view path)
{
    std::lock_guard<std::mutex> lock(mutex);
    module_path = path;
}

/*
 *  AESCryptInterface::UnloadAESCryptLibrary()
 *
 *  Description:
 *      Unload the aescrypt.dll library
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      This function assumes the caller has already checked to ensure
 *      that there are no threads running in the library by calling IsBusy().
 *      The caller must ensure the mutex is locked, if necessary.  (It is
 *      not necessary in the destructor.)
 */
void AESCryptInterface::UnloadAESCryptLibrary()
{
    if (module_handle)
    {
        FreeLibrary(module_handle);
        module_handle = nullptr;
        process_files = nullptr;
        aes_library_busy = nullptr;
    }
}

/*
 *  AESCryptInterface::LoadAESCryptLibrary()
 *
 *  Description:
 *      Unload the aescrypt.dll library
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      True if successful, false if there was a failure.
 *
 *  Comments:
 *      The caller must ensure the mutex is locked.
 */
bool AESCryptInterface::LoadAESCryptLibrary()
{
    // Already loaded?
    if (module_handle) return true;

    // Ensure the module path was set
    if (module_path.empty())
    {
        ::MessageBox(NULL,
                     L"Cannot determine the path to the AES Crypt library",
                     application_title.c_str(),
                     MB_ICONERROR | MB_OK);
        return false;
    }

    // Attempt to load the library
    module_handle = LoadLibrary(module_path.c_str());
    if (!module_handle)
    {
        ::MessageBox(NULL,
                     L"Failed to load the core encryption library",
                     application_title.c_str(),
                     MB_ICONERROR | MB_OK);
        return false;
    }

    // Get the function pointers
    process_files = reinterpret_cast<ProcessFilesFunc>(
        GetProcAddress(module_handle, "ProcessFiles"));
    aes_library_busy = reinterpret_cast<LibraryBusyFunc>(
        GetProcAddress(module_handle, "AESLibraryBusy"));

    // If we fail to get either function address, this is a failure
    if (!process_files || !aes_library_busy)
    {
        UnloadAESCryptLibrary();
        ::MessageBox(NULL,
                     L"Failed to get locate functions in the AES Crypt library",
                     application_title.c_str(),
                     MB_ICONERROR | MB_OK);
        return false;
    }

    return true;
}

/*
 *  AESCryptInterface::ProcessFiles()
 *
 *  Description:
 *      DESCRIPTION
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
void AESCryptInterface::ProcessFiles(FileList &file_list, AESCryptMode mode)
{
    std::unique_lock<std::mutex> lock(mutex);

    // Attempt to load the library, but fail if it cannot be loaded
    if (!LoadAESCryptLibrary()) return;

    // Increment the thread count
    thread_count++;

    // Unlock the mutex while working
    lock.unlock();

    try
    {
        process_files(file_list, mode);
    }
    catch (...)
    {
        // There is nothing we can do, but we should also never get here
    }

    // Lock the mutex and decrement the thread count
    lock.lock();
    thread_count--;
}

bool AESCryptInterface::AESLibraryBusy()
{
    std::unique_lock<std::mutex> lock(mutex, std::try_to_lock);

    // If we failed to get the lock, the library is busy
    if (!lock.owns_lock()) return true;

    // If we have thread into ProcessFiles, then the library is busy
    if (thread_count > 0) return true;

    // If the library is not loaded, it's not busy
    if (!module_handle) return false;

    // If the library is busy, return true
    if (aes_library_busy()) return true;

    // If not busy, unload the AES Crypt library
    UnloadAESCryptLibrary();

    return false;
}
