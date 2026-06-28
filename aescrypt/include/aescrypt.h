 /*
 *  aescrypt.h
 *
 *  Copyright (C) 2026
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This defines the public C API user of this library can use.
 *
 *  Portability Issues:
 *      Windows specific code.
 */

#pragma once

#include "mode.h"
#include "file_list.h"

#ifdef AESCRYPT_EXPORTS
#define AESCRYPT_API __declspec(dllexport)
#else
#define AESCRYPT_API __declspec(dllimport)
#endif

extern "C"
{

// Exported function that allows aescrypt_launcher.exe use this library to
// encrypt or decrypt a list of files
AESCRYPT_API void ProcessFiles(FileList &file_list, AESCryptMode mode);

// Exported function that allows the library user to determine if all of the
// active encryption or decryption threads have completed work
AESCRYPT_API bool AESLibraryBusy();

} // extern "C"
