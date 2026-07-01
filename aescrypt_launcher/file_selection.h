/*
 *  file_selection.h
 *
 *  Copyright (C) 2026
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This file defines the public function for the file selection window
 *      that allows a user to select files to encrypt or decrypt.
 *
 *  Portability Issues:
 *      Windows specific code.
 */

#pragma once

#include <deque>
#include <string>
#include <string_view>

/*
 *  SelectFiles()
 *
 *  Description:
 *      Function to open a dialog window to allow the user to select files
 *      for encrypting or decrypting.
 *
 *  Parameters:
 *      application_title [in]
 *          The name of the application to appear at the top of the dialog.
 *
 *      description [in]
 *          Description to appear following the application title.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      None.
 */
std::deque<std::wstring> SelectFiles(std::wstring_view application_title,
                                     std::wstring_view description);
