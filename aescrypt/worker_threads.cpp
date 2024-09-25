/*
 *  worker_threads.cpp
 *
 *  Copyright (C) 2006, 2007, 2008, 2013, 2015, 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This file implements the WorkerThreads class, which is responsible for
 *      all background encryption and decryption operations.
 *
 *  Portability Issues:
 *      None.
 */

#include "pch.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <limits>
#include <chrono>
#include <atomic>
#include <stdexcept>
#include <terra/aescrypt/engine/encryptor.h>
#include <terra/aescrypt/engine/decryptor.h>
#include <terra/charutil/character_utilities.h>
#include <terra/bitutil/byte_order.h>
#include "worker_threads.h"
#include "password_dialog.h"
#include "report_error.h"
#include "progress_dialog.h"
#include "password_convert.h"
#include "has_aes_extension.h"
#include "version.h"

// Minimum frequency with which to update the progress meter
constexpr std::chrono::milliseconds Progress_Update_Minimum(250);

// Minimum progress meter update interval
constexpr std::size_t Minimal_Interval = 16 * 100;

namespace
{

std::wstring Application_Name = L"AES Crypt";

/*
 *  ThreadEntry()
 *
 *  Description:
 *      This is a C function call used as the initial starting point when
 *      a new thread is created.  It receives a pointer to the WorkerThreads
 *      object and then calls the ThreadEntry() function so that it can then
 *      get the data needed to continue file processing.
 *
 *  Parameters:
 *      lpParameter [in]
 *          A pointer to a WorkerThreads object.
 *
 *  Returns:
 *      Always returns 0.
 *
 *  Comments:
 *      None.
 */
DWORD WINAPI ThreadEntry(LPVOID lpParameter)
{
    try
    {
        WorkerThreads *aes_crypt_worker_threads =
            reinterpret_cast<WorkerThreads *>(lpParameter);

        aes_crypt_worker_threads->ThreadEntry();
    }
    catch (const std::exception &e)
    {
        ::ReportError(Application_Name + L" Error",
                      L"Unhandled exception in worker thread: ",
                      e.what());
    }
    catch (...)
    {
        ::ReportError(Application_Name + L" Error",
                      L"Unhandled exception in worker thread");
    }

    return 0;
}

} // namespace

/*
 *  WorkerThreads::WorkerThreads()
 *
 *  Description:
 *      Constructor for the WorkerThreads object.
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
WorkerThreads::WorkerThreads() : thread_count{0}
{
    // Load the application name
    HMODULE hModule{};
    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                          reinterpret_cast<const LPWSTR>(this),
                          &hModule))
    {
        // Adjust the application name to something reasonably large
        application_name.resize(256, L'\0');

        auto title_length =
            LoadString(hModule,
                       IDS_APP_TITLE,
                       application_name.data(),
                       static_cast<int>(application_name.size()));
        application_name.resize(title_length);
    }
    else
    {
        // Perhaps redundant, use this if the resource string does not load
        application_name = Application_Name;
    }

    application_error = application_name + L" Error";

    InitializeCriticalSection(&critical_section);
}

/*
 *  WorkerThreads::~WorkerThreads()
 *
 *  Description:
 *      Destructor for the WorkerThreads object.
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
WorkerThreads::~WorkerThreads()
{
    HANDLE handle;

    // Wait for any active threads to complete
    // In theory, this block of code should never run, since
    // the IsBusy() call should be made and the class should
    // not be destroyed.  But, just in case...
    while(1)
    {
        EnterCriticalSection(&critical_section);
        if (thread_count == 0)
        {
            LeaveCriticalSection(&critical_section);
            break;
        }
        LeaveCriticalSection(&critical_section);
        Sleep(200);
    }

    // Release any thread handles not already released
    while (!terminated_threads.empty())
    {
        handle = terminated_threads.front();
        WaitForSingleObject(&handle, INFINITE);
        CloseHandle(handle);
        terminated_threads.pop_front();
    }

    // Destroy the critical section object
    DeleteCriticalSection(&critical_section);
}

/*
 *  WorkerThreads::IsBusy()
 *
 *  Description:
 *      Returns true if there are active threads.  This doesn't mean that the
 *      threads have fully exited, but it is indicative that any running thread
 *      is nearing completion (and it would be safe to wait for that thread).
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      True if there are actively working threads, false otherwise.
 *
 *  Comments:
 *      None.
 */
bool WorkerThreads::IsBusy()
{
    bool busy = false;

    EnterCriticalSection(&critical_section);
    if (thread_count > 0) busy = true;
    LeaveCriticalSection(&critical_section);

    return busy;
}

/*
 *  WorkerThreads::ProcessFiles()
 *
 *  Description:
 *      This function is called once the user selects the shell extension
 *      menu option or by the aescrypt32.exe (usually when a user double-clicks
 *      on a file having a .aes extension).  This will prompt the user for a
 *      a password and then invoke a thread to handle the encryption or
 *      decryption process.
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
void WorkerThreads::ProcessFiles(const FileList &file_list, bool encrypt)
{
    PasswdDialog password_dialog(application_name);

    // Prompt the user for a password
    if (password_dialog.DoModal(::GetActiveWindow(), (encrypt ? 1 : 0)) == IDOK)
    {
        // Convert the password to UTF-8 as required by the AES Crypt Engine
        SecureU8String password =
            PasswordConvertUTF8(password_dialog.GetPassword(),
                                Terra::BitUtil::IsLittleEndian());

        // Ensure the password converted properly
        if (password.empty())
        {
            ::ReportError(application_error,
                          L"Password could not be converted to UTF-8");
            return;
        }

        StartThread(file_list, password, encrypt);
    }
}

 /*
  *  WorkerThreads::StartThread()
  *
  *  Description:
  *      This function is called after the user provides a password to start
  *      a new thread to process the file list.
  *
  *  Parameters:
  *      file_list [in]
  *         The list of files to encrypt or decrypt.
  *
  *      password [in]
  *         The password to use for encrypting or decrypting.
  *
  *      encrypt [in]
  *         True if encrypting, false if decrypting.
  *
  *  Returns:
  *      Nothing.
  *
  *  Comments:
  *      None.
  */
void WorkerThreads::StartThread(const FileList &file_list,
                                const SecureU8String &password,
                                bool encrypt)
{
    DWORD thread_id;

    // Enter the critical section so that the thread will be held up until
    // the worker_data
    EnterCriticalSection(&critical_section);

    // Create the thread
    HANDLE thread_handle =
        CreateThread(NULL, 0, ::ThreadEntry, this, 0, &thread_id);

    // Did we succeed to create the thread?
    if (thread_handle != NULL)
    {
        // Make a copy of the file list and password, as those will be invalid
        // upon return from this function and as another thread processes
        // this data in the background
        requests.emplace_back(file_list,
                              password,
                              encrypt,
                              thread_id,
                              thread_handle);

        // Increase the internal thread counter
        thread_count++;

        // Let the thread in
        LeaveCriticalSection(&critical_section);
    }
    else
    {
        // Leave the critical section
        LeaveCriticalSection(&critical_section);

        ::ReportError(application_error, L"Thread creation failed");
    }
}

/*
 *  WorkerThreads::ThreadEntry()
 *
 *  Description:
 *      This is the entry point where the worker thread calls to actually
 *      begin processing files.  It will get the information it needs from
 *      the member variables and then begin processing.
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
void WorkerThreads::ThreadEntry()
{
    // Determine the thread ID of this thread
    DWORD thread_id = GetCurrentThreadId();

    // Try to enter the critical section
    EnterCriticalSection(&critical_section);

    // Release any thread handles not already released
    while (!terminated_threads.empty())
    {
        HANDLE handle = terminated_threads.front();
        terminated_threads.pop_front();
        LeaveCriticalSection(&critical_section);
        WaitForSingleObject(&handle, INFINITE);
        CloseHandle(handle);
        EnterCriticalSection(&critical_section);
    }

    // Locate the data to be processed
    auto it = std::find_if(requests.begin(),
                           requests.end(),
                           [thread_id](const RequestData &request)
                           {
                               return request.thread_id == thread_id;
                           });

    // If unable to find the thread ID, that's a problem!
    if (it == requests.end())
    {
        // Leave the critical section while reporting the issue
        LeaveCriticalSection(&critical_section);

        ::ReportError(application_error, L"Thread rendezvous failed");

        // Reduce the thread count and return
        EnterCriticalSection(&critical_section);
        thread_count--;
        LeaveCriticalSection(&critical_section);
        return;
    }

    // Move the data from the deque to the local variable
    RequestData request = std::move(*it);

    // Remove this element from the deque
    requests.erase(it);

    // Leave the critical section since necessary data is now local
    LeaveCriticalSection(&critical_section);

    try
    {
        // Encrypt or decrypt files based on the request
        if (request.encrypt)
        {
            EncryptFiles(request.file_list, request.password);
        }
        else
        {
            DecryptFiles(request.file_list, request.password);
        }
    }
    catch (const std::exception &e)
    {
        ::ReportError(application_error,
                      L"Unhandled exception processing file(s): ",
                      e.what());
    }
    catch (...)
    {
        ::ReportError(application_error,
                      L"Unhandled exception processing file(s)");
    }

    // Once done, decrement the thread count, clean up memory, etc.
    EnterCriticalSection(&critical_section);
    thread_count--;
    terminated_threads.push_back(request.thread_handle);
    LeaveCriticalSection(&critical_section);
}

/*
 *  WorkerThreads::EncryptFiles()
 *
 *  Description:
 *      This function will iterate over the list of files and encrypt each one
 *      given the provided password.
 *
 *  Parameters:
 *      file_list [in]
 *          The list of files to encrypt.
 *
 *      password [in]
 *          The password to use for encryption.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
void WorkerThreads::EncryptFiles(const FileList &file_list,
                                 const SecureU8String &password)
{
    std::condition_variable cv;
    std::mutex mutex;

    // Secure buffer for file I/O
    SecureVector<char> read_buffer(Buffered_IO_Size, 0);
    SecureVector<char> write_buffer(Buffered_IO_Size, 0);

    // If the file list is empty, just return
    if (file_list.empty()) return;

    // Create a progress dialog that will notify the waiting thread
    ProgressDialog progress_dialog(
        [&]()
        {
            std::lock_guard<std::mutex> lock(mutex);
            cv.notify_all();
        });

    // Define the extensions to insert into the header
    const std::vector<std::pair<std::string, std::string>> extensions =
    {
        {"CREATED_BY", Program_Name + " " + Program_Version}
    };

    // Create an event used to indicate the progress dialog is ready
    HANDLE event_handle = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Create a thread to service the windows message loop for the dialog
    std::thread progress_thread(
        [&]()
        {
            try
            {
                // Non-zero LPARAM displays "Encrypting"
                progress_dialog.Create(GetDesktopWindow(), LPARAM(1));
                progress_dialog.ShowWindow(SW_SHOWNORMAL);

                // Signal that the progress dialog is ready
                SetEvent(event_handle);

                // Process messages
                WindowsMessageLoop();

                // Destroy the progress window
                progress_dialog.DestroyWindow();
            }
            catch (const std::exception &e)
            {
                ::ReportError(application_error,
                              L"Unexpected error in progress dialog thread",
                              e.what());
            }
            catch (...)
            {
                ::ReportError(application_error,
                              L"Unexpected error in progress dialog thread");
            }
        });

    // Wait for the progress window to open
    WaitForSingleObject(event_handle, INFINITE);
    CloseHandle(event_handle);

    // Iterate over the list of files
    for (const auto &in_file : file_list)
    {
        std::size_t file_size{};
        std::ifstream ifs;
        std::ofstream ofs;
        bool remove_on_fail{};

        // Reset the progress bar (default range is 0..100)
        progress_dialog.SendDlgItemMessage(IDC_PROGRESSBAR, PBM_SETPOS, 0, 0);

        // Display the file name
        progress_dialog.SetDlgItemText(IDC_FILENAME, in_file.c_str());

        try
        {
            // Attempt to get the file size
            file_size =
                std::filesystem::file_size(std::filesystem::path(in_file));
        }
        catch (...)
        {
            // Nothing we can do, nor is this critical
        }

        try
        {
            // Open the input file for reading
            ifs.open(std::filesystem::path(in_file),
                     std::ios::in | std::ios::binary);
        }
        catch (...)
        {
            // Nothing we can do, but an error will be presented below
        }

        if (!ifs.good() || !ifs.is_open())
        {
            DWORD error_code = ERROR_SUCCESS;
            if (!ifs.good()) error_code = GetLastError();

            // Report an error opening the file
            std::wstring message = L"Unable to open the input file " + in_file;
            ::ReportError(application_error, message, error_code);

            break;
        }

        // Set the buffer to use for reading
        ifs.rdbuf()->pubsetbuf(read_buffer.data(), read_buffer.size());

        // Define the output filename
        std::wstring out_file = in_file + L".aes";

        try
        {
            // Get the file status of the output file
            std::filesystem::file_status file_status =
                std::filesystem::status(std::filesystem::path(out_file));

            // If the output file does not exist, attempt to remove later
            // (Do not remove by default so as to not attempt to remove things
            // like character special devices.)
            if (!std::filesystem::exists(file_status)) remove_on_fail = true;

            // Does a regular file having this output file name exist?
            if (std::filesystem::is_regular_file(file_status))
            {
                // Report an error opening the file
                ::ReportError(application_error,
                              std::wstring(L"Output file already exists: ") +
                                  out_file);

                break;
            }
        }
        catch (const std::exception &e)
        {
            ::ReportError(application_error,
                          std::wstring(L"Unexpected error processing ") +
                              in_file,
                          e.what());
            break;
        }
        catch (...)
        {
            // Report an error opening the file
            ::ReportError(application_error,
                          std::wstring(L"Unexpected error processing ") +
                              in_file);
            break;
        }

        try
        {
            // Open the output file for writing
            ofs.open(std::filesystem::path(out_file),
                     std::ios::out | std::ios::binary);
        }
        catch (...)
        {
            // Nothing we can do, but an error will be presented below
        }
        if (!ofs.good() || !ofs.is_open())
        {
            DWORD error_code = ERROR_SUCCESS;
            if (!ofs.good()) error_code = GetLastError();

            // Report an error opening the file
            std::wstring message =
                L"Unable to open the output file " + out_file;
            ::ReportError(application_error, message, error_code);

            break;
        }

        // Set the buffer to use for writing
        ofs.rdbuf()->pubsetbuf(write_buffer.data(), write_buffer.size());

        // Encrypt the input stream
        bool result = EncryptStream(cv,
                                    mutex,
                                    progress_dialog,
                                    in_file,
                                    password,
                                    KDF_Iterations,
                                    extensions,
                                    file_size,
                                    ifs,
                                    ofs);

        // Close any open files; there may be delay in closing the output
        // file if it is large and transmission is over a network
        if (ifs.is_open()) ifs.close();
        if (ofs.is_open())
        {
            ofs.flush();
            ofs.close();
        }

        // Did the encryption process fail?
        if (!result)
        {
            // Remove the partial output file if it's not stdout
            if (remove_on_fail)
            {
                try
                {
                    std::filesystem::remove(std::filesystem::path(out_file));
                }
                catch (...)
                {
                    // Nothing we can do
                }
            }
            break;
        }

        // If the user clicked cancel or closed the dialog, stop processing
        if (progress_dialog.WasCancelPressed()) break;
    }

    // Instruct the progress window to terminate
    DWORD progress_thread_id = GetThreadId(progress_thread.native_handle());
    PostThreadMessage(progress_thread_id, WM_QUIT, 0, 0);

    // Wait for the progress window thread to complete
    progress_thread.join();
}

/*
 *  WorkerThreads::EncryptStream()
 *
 *  Description:
 *      This function will encrypt the given input stream to the given output
 *      stream using the specified password.
 *
 *  Parameters:
 *      cv [in]
 *          Condition variable used for thread syncronization
 *
 *      mutex [in]
 *          Mutex used in association with the above condition variable.
 *
 *      progress_dialog [in]
 *          A reference to the progress dialog that shows encryption progress.
 *
 *      filename [in]
 *          The name of the file to encrypt.
 *
 *      password [in]
 *          The password (in UTF-16 format) to use for encryption.
 *
 *      iterations [in]
 *          The number of Key Derivation Function (KDF) iterations to perform.
 *
 *      extensions [in]
 *          Plaintext extension data to insert into the AES stream header.
 *
 *      input_size [in]
 *          The size of the input stream.
 *
 *      istream [in]
 *          A reference to the input stream.
 *
 *      ostream [in]
 *          A reference to the output stream.
 *
 *  Returns:
 *      True if successful, false if not.
 *
 *  Comments:
 *      None.
 */
bool WorkerThreads::EncryptStream(std::condition_variable &cv,
                                  std::mutex &mutex,
                                  ProgressDialog &progress_dialog,
                                  const std::wstring &filename,
                                  const SecureU8String &password,
                                  const std::uint32_t iterations,
                                  const ExtensionList &extensions,
                                  const std::size_t input_size,
                                  std::istream &istream,
                                  std::ostream &ostream)
{
    Terra::AESCrypt::Engine::Encryptor encryptor;
    Terra::AESCrypt::Engine::EncryptResult encrypt_result{};
    bool encryption_complete{};
    bool cancel_encryption{};
    std::atomic<std::size_t> current_meter_position{};
    std::size_t last_meter_position{};

    // Get the current time
    std::chrono::steady_clock::time_point last_update_time =
        std::chrono::steady_clock::now();

    // Define the update interval (progress bar has 100 positions)
    std::size_t update_interval = input_size / 100;

    // If the interval is really tiny, just update once -- but only if the
    // input size is known
    if ((update_interval < Minimal_Interval) && (input_size > 0))
    {
        update_interval = std::numeric_limits<std::size_t>::max();
    }

    // Progress meter update function
    auto progress_updater = [&]([[maybe_unused]]const std::string &instance,
                                std::size_t position)
    {
        std::lock_guard<std::mutex> lock(mutex);

        // Dot not update if the input size is not known
        if (input_size == 0) return;

        // Get the current time; do not update too frequently (but do update
        // on the final position)
        std::chrono::steady_clock::time_point current_time =
            std::chrono::steady_clock::now();
        if ((position == input_size) ||
            (current_time - last_update_time < Progress_Update_Minimum))
        {
            return;
        }
        last_update_time = current_time;

        // Compute the percentage of file completion (aligns with the meters'
        // range of 0..100
        current_meter_position = 100 * position / input_size;

        // Notify thread to update the progress bar
        cv.notify_all();
    };

    // Encrypt the stream via a separate thread
    std::thread encrypt_thread(
        [&]()
        {
            // Encrypt the current input stream
            encrypt_result = encryptor.Encrypt(
                static_cast<std::u8string>(password),
                iterations,
                istream,
                ostream,
                extensions,
                progress_updater,
                update_interval);

            // Lock the mutex to assign result
            std::lock_guard<std::mutex> lock(mutex);
            encryption_complete = true;
            cv.notify_all();
        });

    // Lock the mutex
    std::unique_lock<std::mutex> lock(mutex);

    // Wait for encryption to complete or to be told to terminate
    while (!encryption_complete)
    {
        // If the user clicked cancel (or closed the dialog), stop the
        // encryption thread
        if (progress_dialog.WasCancelPressed())
        {
            lock.unlock();
            encryptor.Cancel();
            lock.lock();
            break;
        }

        // Wait for some event to trigger the thread
        cv.wait(lock,
                [&]() -> bool
                {
                    return encryption_complete ||
                           (current_meter_position != last_meter_position) ||
                           progress_dialog.WasCancelPressed();
                });

        // Service the message loop to ensure an up-to-date display
        lock.unlock();

        auto new_meter_position = current_meter_position.load();
        if (new_meter_position > last_meter_position)
        {
            progress_dialog.SendDlgItemMessage(
                IDC_PROGRESSBAR,
                PBM_SETPOS,
                static_cast<WPARAM>(new_meter_position),
                0);
            last_meter_position = new_meter_position;
        }

        lock.lock();
    }

    // Unlock the mutex
    lock.unlock();

    // Wait for the encryption thread to exit
    encrypt_thread.join();

    // Present a reason to the user in the event of an error
    if ((encrypt_result != Terra::AESCrypt::Engine::EncryptResult::Success) &&
        (encrypt_result !=
         Terra::AESCrypt::Engine::EncryptResult::EncryptionCancelled))
    {
        std::ostringstream oss;

        // Convert the encryption result into a string
        oss << encrypt_result;

        // Report the error to the user
        ::ReportError(application_error,
                      std::string("Failed to encrypt: ") + oss.str());

        return false;
    }

    // Return true only if encryption succeeded
    return encrypt_result == Terra::AESCrypt::Engine::EncryptResult::Success;
}

/*
 *  WorkerThreads::DecryptFiles()
 *
 *  Description:
 *      This function will iterate over the list of files and decrypt each one
 *      given the provided password.
 *
 *  Parameters:
 *      file_list [in]
 *          The list of files to decrypt.
 *
 *      password [in]
 *          The password to use for decryption.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
void WorkerThreads::DecryptFiles(const FileList &file_list,
                                 const SecureU8String &password)
{
    std::condition_variable cv;
    std::mutex mutex;

    // Secure buffer for file I/O
    SecureVector<char> read_buffer(Buffered_IO_Size, 0);
    SecureVector<char> write_buffer(Buffered_IO_Size, 0);

    // If the file list is empty, just return
    if (file_list.empty()) return;

    // Ensure all files end in .aes
    for (const auto &in_file : file_list)
    {
        if (!HasAESExtension(in_file))
        {
            ::ReportError(application_error,
                          L"File to decrypt does not end in .aes: " + in_file);
            return;
        }
    }

    // Create a progress dialog that will notify the waiting thread
    ProgressDialog progress_dialog(
        [&]()
        {
            std::lock_guard<std::mutex> lock(mutex);
            cv.notify_all();
        });

    // Create an event used to indicate the progress dialog is ready
    HANDLE event_handle = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Create a thread to service the windows message loop for the dialog
    std::thread progress_thread(
        [&]()
        {
            try
            {
                // Zero LPARAM displays "Decrypting"
                progress_dialog.Create(GetDesktopWindow(), LPARAM(0));
                progress_dialog.ShowWindow(SW_SHOWNORMAL);

                // Signal that the progress dialog is ready
                SetEvent(event_handle);

                // Process messages
                WindowsMessageLoop();

                // Destroy the progress window
                progress_dialog.DestroyWindow();
            }
            catch (const std::exception &e)
            {
                ::ReportError(application_error,
                              L"Unexpected error in progress dialog thread",
                              e.what());
            }
            catch (...)
            {
                ::ReportError(application_error,
                              L"Unexpected error in progress dialog thread");
            }
        });

    // Wait for the progress window to open
    WaitForSingleObject(event_handle, INFINITE);
    CloseHandle(event_handle);

    // Iterate over the list of files
    for (const auto &in_file : file_list)
    {
        std::size_t file_size{};
        std::ifstream ifs;
        std::ofstream ofs;
        bool remove_on_fail{};

        // Reset the progress bar (default range is 0..100)
        progress_dialog.SendDlgItemMessage(IDC_PROGRESSBAR, PBM_SETPOS, 0, 0);

        // Display the file name
        progress_dialog.SetDlgItemText(IDC_FILENAME, in_file.c_str());

        try
        {
            // Attempt to get the file size
            file_size =
                std::filesystem::file_size(std::filesystem::path(in_file));
        }
        catch (...)
        {
            // Nothing we can do, nor is this critical
        }

        try
        {
            // Open the input file for reading
            ifs.open(std::filesystem::path(in_file),
                     std::ios::in | std::ios::binary);
        }
        catch (...)
        {
            // Nothing we can do, but an error will be presented below
        }

        if (!ifs.good() || !ifs.is_open())
        {
            DWORD error_code = ERROR_SUCCESS;
            if (!ifs.good()) error_code = GetLastError();

            // Report an error opening the file
            std::wstring message = L"Unable to open the input file " + in_file;
            ::ReportError(application_error, message, error_code);

            break;
        }

        // Set the buffer to use for reading
        ifs.rdbuf()->pubsetbuf(read_buffer.data(), read_buffer.size());

        // Define the output filename (same as input file without .aes)
        std::wstring out_file = in_file;
        out_file.resize(out_file.size() - 4);

        try
        {
            // Get the file status of the output file
            std::filesystem::file_status file_status =
                std::filesystem::status(std::filesystem::path(out_file));

            // If the output file does not exist, attempt to remove later
            // (Do not remove by default so as to not attempt to remove things
            // like character special devices.)
            if (!std::filesystem::exists(file_status)) remove_on_fail = true;

            // Does a regular file having this output file name exist?
            if (std::filesystem::is_regular_file(file_status))
            {
                // Report an error opening the file
                ::ReportError(application_error,
                              std::wstring(L"Output file already exists: ") +
                                  out_file);

                break;
            }
        }
        catch (const std::exception &e)
        {
            // Report an error opening the file
            ::ReportError(application_error,
                          std::wstring(L"Unexpected error processing ") +
                              in_file,
                          e.what());
            break;
        }
        catch (...)
        {
            // Report an error opening the file
            ::ReportError(application_error,
                          std::wstring(L"Unexpected error processing ") +
                              in_file);
            break;
        }

        try
        {
            // Open the output file for writing
            ofs.open(std::filesystem::path(out_file),
                     std::ios::out | std::ios::binary);
        }
        catch (...)
        {
            // Nothing we can do, but an error will be presented below
        }

        if (!ofs.good() || !ofs.is_open())
        {
            DWORD error_code = ERROR_SUCCESS;
            if (!ofs.good()) error_code = GetLastError();

            // Report an error opening the file
            std::wstring message =
                L"Unable to open the output file " + out_file;
            ::ReportError(application_error, message, error_code);

            break;
        }

        // Set the buffer to use for writing
        ofs.rdbuf()->pubsetbuf(write_buffer.data(), write_buffer.size());

        // Decrypt the input stream
        bool result = DecryptStream(cv,
                                    mutex,
                                    progress_dialog,
                                    in_file,
                                    password,
                                    file_size,
                                    ifs,
                                    ofs);

        // Close any open files; there may be delay in closing the output
        // file if it is large and transmission is over a network
        if (ifs.is_open()) ifs.close();
        if (ofs.is_open())
        {
            ofs.flush();
            ofs.close();
        }

        // Did the decryption process fail?
        if (!result)
        {
            // Remove the partial output file if it's not stdout
            if (remove_on_fail)
            {
                try
                {
                    std::filesystem::remove(std::filesystem::path(out_file));
                }
                catch (...)
                {
                    // Nothing we can do
                }
            }
            break;
        }

        // If the user clicked cancel or closed the dialog, stop processing
        if (progress_dialog.WasCancelPressed()) break;
    }

    // Instruct the progress window to terminate
    DWORD progress_thread_id = GetThreadId(progress_thread.native_handle());
    PostThreadMessage(progress_thread_id, WM_QUIT, 0, 0);

    // Wait for the progress window thread to complete
    progress_thread.join();
}

/*
 *  WorkerThreads::DecryptStream()
 *
 *  Description:
 *      This function will decrypt the given input stream to the given output
 *      stream using the specified password.
 *
 *  Parameters:
 *      cv [in]
 *          Condition variable used for thread syncronization
 *
 *      mutex [in]
 *          Mutex used in association with the above condition variable.
 *
 *      progress_dialog [in]
 *          A reference to the progress dialog that shows decryption progress.
 *
 *      filename [in]
 *          The name of the file to decrypt.
 *
 *      password [in]
 *          The password (in UTF-16 format) to use for decryption.
 *
 *      input_size [in]
 *          The size of the input stream.
 *
 *      istream [in]
 *          A reference to the input stream.
 *
 *      ostream [in]
 *          A reference to the output stream.
 *
 *  Returns:
 *      True if successful, false if not.
 *
 *  Comments:
 *      None.
 */
bool WorkerThreads::DecryptStream(std::condition_variable &cv,
                                  std::mutex &mutex,
                                  ProgressDialog &progress_dialog,
                                  const std::wstring &filename,
                                  const SecureU8String &password,
                                  const std::size_t input_size,
                                  std::istream &istream,
                                  std::ostream &ostream)
{
    Terra::AESCrypt::Engine::Decryptor decryptor;
    Terra::AESCrypt::Engine::DecryptResult decrypt_result{};
    bool decryption_complete{};
    bool cancel_decryption{};
    std::atomic<std::size_t> current_meter_position{};
    std::size_t last_meter_position{};

    // Get the current time
    std::chrono::steady_clock::time_point last_update_time =
        std::chrono::steady_clock::now();

    // Define the update interval (progress bar has 100 positions)
    std::size_t update_interval = input_size / 100;

    // If the interval is really tiny, just update once -- but only if the
    // input size is known
    if ((update_interval < Minimal_Interval) && (input_size > 0))
    {
        update_interval = std::numeric_limits<std::size_t>::max();
    }

    // Progress meter update function
    auto progress_updater = [&]([[maybe_unused]]const std::string &instance,
                                std::size_t position)
    {
        std::lock_guard<std::mutex> lock(mutex);

        // Dot not update if the input size is not known
        if (input_size == 0) return;

        // Get the current time; do not update too frequently (but do update
        // on the final position)
        std::chrono::steady_clock::time_point current_time =
            std::chrono::steady_clock::now();
        if ((position == input_size) ||
            (current_time - last_update_time < Progress_Update_Minimum))
        {
            return;
        }
        last_update_time = current_time;

        // Compute the percentage of file completion (aligns with the meters'
        // range of 0..100
        current_meter_position = 100 * position / input_size;

        // Notify thread to update the progress bar
        cv.notify_all();
    };

    // Decrypt the stream via a separate thread
    std::thread decrypt_thread(
        [&]()
        {
            // Decrypt the current input stream
            decrypt_result = decryptor.Decrypt(
                static_cast<std::u8string>(password),
                istream,
                ostream,
                progress_updater,
                update_interval);

            // Lock the mutex to assign result
            std::lock_guard<std::mutex> lock(mutex);
            decryption_complete = true;
            cv.notify_all();
        });

    // Lock the mutex
    std::unique_lock<std::mutex> lock(mutex);

    // Wait for decryption to complete or to be told to terminate
    while (!decryption_complete)
    {
        // If the user clicked cancel (or closed the dialog), stop the
        // decryption thread
        if (progress_dialog.WasCancelPressed())
        {
            lock.unlock();
            decryptor.Cancel();
            lock.lock();
            break;
        }

        // Wait for some event to trigger the thread
        cv.wait(lock,
                [&]() -> bool
                {
                    return decryption_complete ||
                           (current_meter_position != last_meter_position) ||
                           progress_dialog.WasCancelPressed();
                });

        // Service the message loop to ensure an up-to-date display
        lock.unlock();

        auto new_meter_position = current_meter_position.load();
        if (new_meter_position > last_meter_position)
        {
            progress_dialog.SendDlgItemMessage(
                IDC_PROGRESSBAR,
                PBM_SETPOS,
                static_cast<WPARAM>(new_meter_position),
                0);
            last_meter_position = new_meter_position;
        }

        lock.lock();
    }

    // Unlock the mutex
    lock.unlock();

    // Wait for the decryption thread to exit
    decrypt_thread.join();

    // Present a reason to the user in the event of an error
    if ((decrypt_result != Terra::AESCrypt::Engine::DecryptResult::Success) &&
        (decrypt_result !=
         Terra::AESCrypt::Engine::DecryptResult::DecryptionCancelled))
    {
        std::ostringstream oss;

        // Convert the decryption result into a string
        oss << decrypt_result;

        // Report the error to the user
        ::ReportError(application_error,
                      std::string("Failed to decrypt: ") + oss.str());

        return false;
    }

    // Return true only if decryption succeeded
    return decrypt_result == Terra::AESCrypt::Engine::DecryptResult::Success;
}

/*
 *  WorkerThreads::WindowsMessageLoop()
 *
 *  Description:
 *      This routine will service the Windows message queues for the running
 *      thread so that messages are delivered and windows are properly updated.
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

void WorkerThreads::WindowsMessageLoop()
{
    MSG msg;

    // Process messages until WM_QUIT is received
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
