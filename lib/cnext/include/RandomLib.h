///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              07.14.2024
///
/// @file              RandomLib.h
///
/// @brief             This is the header for the utilities library which
///                    has manages custom randomization operations.
///
/// @details
///
/// @copyright
///                   Copyright (c) 2012-2025 James Card
///
/// Permission is hereby granted, free of charge, to any person obtaining a
/// copy of this software and associated documentation files (the "Software"),
/// to deal in the Software without restriction, including without limitation
/// the rights to use, copy, modify, merge, publish, distribute, sublicense,
/// and/or sell copies of the Software, and to permit persons to whom the
/// Software is furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included
/// in all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
/// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
/// DEALINGS IN THE SOFTWARE.
///
///                                James Card
///                         http://www.jamescard.org
///
///////////////////////////////////////////////////////////////////////////////

#ifndef RANDOM_LIB_H
#define RANDOM_LIB_H

#include "TypeDefinitions.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Utility functions.
u64 getU48_(u64 upperBound, bool useProvidedSeed, u64 seed, ...);
#define getU48(upperBound, ...) getU48_(upperBound, ##__VA_ARGS__, false, 0)
u32 getU32_(u32 upperBound, bool useProvidedSeed, u64 seed, ...);
#define getU32(upperBound, ...) getU32_(upperBound, ##__VA_ARGS__, false, 0)
u64 startRandom(u64 seed);
char *getRandomString_(u32 length, const char *userAlphabet, ...);
#define getRandomString(length, ...) \
  getRandomString_(length, ##__VA_ARGS__, NULL)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // RANDOM_LIB_H

