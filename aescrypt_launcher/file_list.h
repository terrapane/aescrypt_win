/*
 *  file_list.h
 *
 *  Copyright (C) 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This file defines a FileList type, which holds the list of Unicode
 *      filenames.
 *
 *  Portability Issues:
 *      None.
 */

#pragma once

#include <string>
#include <deque>

using FileList = std::deque<std::wstring>;
