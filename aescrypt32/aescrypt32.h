/*
 *  aescrypt32.h
 *
 *  Copyright (C) 2006, 2008, 2013, 2024
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

#pragma once

#include "file_list.h"

// Externals in the aescrypt DLL
bool AESLibraryBusy();
void ProcessFiles(FileList &file_list, bool encrypt);
