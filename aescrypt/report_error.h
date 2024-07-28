/*
 *  report_error.h
 *
 *  Copyright (C) 2007, 2008, 2013, 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This file defines functions for reporting errors to the user.
 *
 *  Portability Issues:
 *      None.
 */

#pragma once

#include <Windows.h>
#include <string>

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
                 const std::string &message,
                 DWORD reason = ERROR_SUCCESS);

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
                 DWORD reason = ERROR_SUCCESS);
