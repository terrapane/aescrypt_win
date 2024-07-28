/*
 *  has_aes_extension.h
 *
 *  Copyright (C) 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This file defines a function that will check the given string to
 *      see if it ends with .aes.
 *
 *  Portability Issues:
 *      None.
 */

#pragma once

#include <string>

/*
 *  HasAESExtension()
 *
 *  Description:
 *      Returns true if the given filename ends with .aes or not.  This will
 *      perform a case insensitive comparison.
 *
 *  Parameters:
 *      filename [in]
 *          The filename to check for a .aes extension.  This may be a complete
 *          pathname.
 *
 *  Returns:
 *      True if the file ends in .aes and false otherwise.
 *
 *  Comments:
 *      None.
 */
bool HasAESExtension(const std::wstring &filename);
