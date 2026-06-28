/*
 *  worker_threads.cpp
 *
 *  Copyright (C) 2006-2026
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This file defines the WorkerThreads class, which is responsible for all
 *      background encryption and decryption operations.
 *
 *  Portability Issues:
 *      None.
 */

#pragma once

#include <Windows.h>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <ostream>
#include <istream>
#include "secure_containers.h"
#include "file_list.h"
#include "progress_dialog.h"
#include "mode.h"

// Type to hold extensions to insert into the container header
using ExtensionList = std::vector<std::pair<std::string, std::string>>;

// Type used to hold data associated with an encryption or decryption request
struct RequestData
{
    FileList file_list;
    SecureU8String password;
    AESCryptMode mode;
    DWORD thread_id;
    HANDLE thread_handle;
};

// Class that interfaces between the Windows shell and the AES Crypt Engine
class WorkerThreads
{
    public:
        WorkerThreads();
        ~WorkerThreads();

        // Indicates whether threads are working
        bool IsBusy();

        // Process files for encryption (true) or decryption (false)
        void ProcessFiles(const FileList &file_list, AESCryptMode mode);

        // This should only be called by threads spawned by this class
        void ThreadEntry();

    protected:
        void CloseThreadHandles();

        void StartThread(const FileList &file_list,
                         const SecureU8String &password,
                         AESCryptMode mode);

        void EncryptFiles(const FileList &file_list,
                          const SecureU8String &password);

        void DecryptFiles(const FileList &file_list,
                          const SecureU8String &password);

        std::pair<bool, std::string> EncryptStream(
            std::condition_variable &cv,
            std::mutex &mutex,
            ProgressDialog &progress_dialog,
            const SecureU8String &password,
            const std::uint32_t iterations,
            const ExtensionList &extensions,
            const std::size_t input_size,
            std::istream &istream,
            std::ostream &ostream) const;

        std::pair<bool, std::string> DecryptStream(
            std::condition_variable &cv,
            std::mutex &mutex,
            ProgressDialog &progress_dialog,
            const SecureU8String &password,
            const std::size_t input_size,
            std::istream &istream,
            std::ostream &ostream) const;

        void WindowsMessageLoop();

        std::wstring application_name;
        std::wstring application_error;
        std::size_t thread_count;
        std::deque<HANDLE> terminated_threads;
        std::deque<RequestData> requests;
        std::mutex module_mutex;
};
