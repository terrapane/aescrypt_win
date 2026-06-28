/*
 *  aescrypt_interface.h
 *
 *  Copyright (C) 2026
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This header defines functions that serve as an interface to the
 *      aescrypt.dll library.  This library is loaded and unloaded on demand
 *      and this interface code simplifies access.
 *
 *  Portability Issues:
 *      None.
 */

#pragma once

#include <Windows.h>
#include <mutex>
#include <string>
#include <string_view>
#include "mode.h"
#include "file_list.h"

// Defines the AES Crypt interface class
class AESCryptInterface
{
    public:
        AESCryptInterface() :
            application_title{L"AES Crypt"},
            module_handle{nullptr},
            thread_count{},
            process_files{},
            aes_library_busy{}
        {
        }
        ~AESCryptInterface();
        void SetApplicationTitle(std::wstring_view title);
        void SetModulePath(std::wstring_view path);
        void ProcessFiles(FileList &file_list, AESCryptMode mode);
        bool AESLibraryBusy();

    private:
        bool LoadAESCryptLibrary();
        void UnloadAESCryptLibrary();

        std::mutex mutex;
        std::wstring application_title;
        std::wstring module_path;
        HMODULE module_handle;
        std::size_t thread_count;

        // Define function prototype syntax
        using ProcessFilesFunc = void (__cdecl *)(FileList &, AESCryptMode);
        using LibraryBusyFunc = bool (__cdecl *)();

        // Define function pointer members
        ProcessFilesFunc process_files;
        LibraryBusyFunc aes_library_busy;
};

// There should only be one instance of the AES Crypt Interface and this
// function will return a reference to that instance
AESCryptInterface &GetAESCryptInterface();
