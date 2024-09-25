/*
 *  report_error.cpp
 *
 *  Copyright (C) 2007, 2008, 2013, 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This file implements functions for reporting errors to the user.
 *
 *  Portability Issues:
 *      None.
 */

#include "pch.h"
#include <terra/charutil/character_utilities.h>
#include <terra/bitutil/byte_order.h>
#include "report_error.h"

/*
 *  ReportError()
 *
 *  Description:
 *      This function will report an error to the user by displaying a
 *      message box.  The message to render is provided as the first parameter.
 *      The second parameter contains either ERROR_SUCCESS or some error
 *      code from GetLastError().  If an error code is provided, the message
 *      will be formatted for user consumption.
 *
 *  Parameters:
 *      window_title [in]
 *          The text of the title in the message box.
 *
 *      message [in]
 *         The message to show the user (encoded as UTF-8).
 *
 *      reason [in]
 *         The reason for the error (generally, the value from GetLastError()).
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
void ReportError(const std::wstring &window_title,
                 const std::string &message,
                 DWORD reason)
{
    std::wstring unicode_message(message.size(), L'\0');

    // This function assumes wchar_t is two octets in length
    static_assert(sizeof(wchar_t) == 2);

    // Convert the string from UTF-8 to UTF-16
    auto [convert_success, length] = Terra::CharUtil::ConvertUTF8ToUTF16(
        {reinterpret_cast<const std::uint8_t *>(message.data()),
         message.size()},
        {reinterpret_cast<std::uint8_t *>(unicode_message.data()),
         unicode_message.size() * sizeof(wchar_t)},
        Terra::BitUtil::IsLittleEndian());

    // If conversion was successful, render the message
    if (convert_success)
    {
        // The length is in octets, resize to two-octet characters
        unicode_message.resize(length / 2);
    }
    else
    {
        unicode_message = L"Error occurred, as did a UTF-8 conversion error";
    }

    ReportError(window_title, unicode_message, reason);
}

/*
 *  ReportError()
 *
 *  Description:
 *      This function will report an error to the user by displaying a
 *      message box.  The message to render is provided as the first parameter.
 *      The second parameter contains either ERROR_SUCCESS or some error
 *      code from GetLastError().  If an error code is provided, the message
 *      will be formatted for user consumption.
 *
 *  Parameters:
 *      window_title [in]
 *          The text of the title in the message box.
 *
 *      message [in]
 *         The message to show the user.
 *
 *      error_string [in]
 *          A UTF-8 error string that will be appended to the above message.
 *
 *      reason [in]
 *         The reason for the error (generally, the value from GetLastError()).
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
void ReportError(const std::wstring &window_title,
                 const std::wstring &message,
                 const std::string &error_string,
                 DWORD reason)
{
    std::wstring unicode_error(error_string.size(), L'\0');

    // This function assumes wchar_t is two octets in length
    static_assert(sizeof(wchar_t) == 2);

    // Convert the string from UTF-8 to UTF-16
    auto [convert_success, length] = Terra::CharUtil::ConvertUTF8ToUTF16(
        {reinterpret_cast<const std::uint8_t *>(error_string.data()),
         error_string.size()},
        {reinterpret_cast<std::uint8_t *>(unicode_error.data()),
         unicode_error.size() * sizeof(wchar_t)},
        Terra::BitUtil::IsLittleEndian());

    // If conversion was successful, render the message
    if (convert_success)
    {
        // The length is in octets, resize to two-octet characters
        unicode_error.resize(length / 2);
    }
    else
    {
        unicode_error = L"Error occurred, as did a UTF-8 conversion error";
    }

    std::wstring error_text = message + L": " + unicode_error;

    ReportError(window_title, error_text, reason);
}

/*
 *  ReportError()
 *
 *  Description:
 *      This function will report an error to the user by displaying a
 *      message box.  The message to render is provided as the first parameter.
 *      The second parameter contains either ERROR_SUCCESS or some error
 *      code from GetLastError().  If an error code is provided, the message
 *      will be formatted for user consumption.
 *
 *  Parameters:
 *      window_title [in]
 *          The text of the title in the message box.
 *
 *      message [in]
 *         The message to show the user.
 *
 *      reason [in]
 *         The reason for the error (generally, the value from GetLastError()).
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
void ReportError(const std::wstring &window_title,
                 const std::wstring &message,
                 DWORD reason)
{
    // Copy the message to report (which may be revised below)
    std::wstring reported_message = message;

    if (reason != ERROR_SUCCESS)
    {
        LPTSTR error_string{};

        if (::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                FORMAT_MESSAGE_FROM_SYSTEM,
                            NULL,
                            reason,
                            0,
                            (LPTSTR) &error_string,
                            0,
                            NULL) != 0)
        {
            LPTSTR p = _tcschr(error_string, L'\r');
            if (p != NULL) { *p = L'\0'; }

            reported_message += L":\n";
            reported_message += error_string;

            ::LocalFree(error_string);
        }
    }

    ::MessageBox(NULL, reported_message.c_str(), window_title.c_str(), MB_OK);
}
