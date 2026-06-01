/*
 *  globals.h
 *
 *  Copyright (C) 2024, 2026
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This file defines global constant values.
 *
 *  Portability Issues:
 *      None.
 */

#pragma once

#include <cstdint>

// Number of KDF iterations to perform when deriving key from password
constexpr std::uint32_t KDF_Iterations = 600'000;

// Size in octets of buffer for file I/O
constexpr std::size_t Buffered_IO_Size = 131'072;
