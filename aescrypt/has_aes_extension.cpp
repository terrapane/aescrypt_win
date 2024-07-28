/*
 *  has_aes_extension.cpp
 *
 *  Copyright (C) 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This file implements a function that will check the given string to
 *      see if it ends with .aes.
 *
 *  Portability Issues:
 *      None.
 */

#pragma once

#include <filesystem>
#include "has_aes_extension.h"

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
bool HasAESExtension(const std::wstring &filename)
{
    try
    {
        // Get the file extension from the filename
        auto extension = std::filesystem::path(filename).extension().wstring();

        // If the extension is not exactly 4 characters (.aes), return false
        if (extension.length() != 4) return false;

        // Compare each of the last 4 characters looking for .aes
        if ((extension[0] == L'.') &&
            ((extension[1] == L'a') || (extension[1] == L'A')) &&
            ((extension[2] == L'e') || (extension[2] == L'E')) &&
            ((extension[3] == L's') || (extension[3] == L'S')))
        {
            return true;
        }
    }
    catch (...)
    {
        // Nothing to do; failure assumes false
    }

    return false;
}
