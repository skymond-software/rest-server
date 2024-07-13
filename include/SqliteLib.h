///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              11.07.2021
///
/// @file              SqliteLib.h
///
/// @brief             This is the library of functions specific to SQLite.
///                    Generic database functions are built on top of this
///                    library.
///
/// @details
///
/// @copyright
///                   Copyright (c) 2012-2024 James Card
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

#ifndef SQLITE_LIB_H
#define SQLITE_LIB_H


#include "DbInterface.h"
#include "sqlite3.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Support data structures.
/// @struct Sqlite
///
/// @brief Object containing the things needed to maintain attached SQLite
/// databases.
///
/// @param databasePath The path to the directory containing the SQLite database
///   files.  This is the same value passed into the sqliteInit function.
/// @param db A pointer to a sqlite3 instance that maintains the SQLite metadata
///   for interacting with the databases.
/// @param transactionCount The number of times a transaction start has been
///   requested without an accompanying rollback or commit.
/// @param transactionMutex The mutex that guards transaction and table lock
///   operations.
/// @param lockedTables The dictionary of locked tables for this SQLite
///   connection.
typedef struct Sqlite {
  Bytes databasePath;
  sqlite3 *db;
  u64 transactionCount;
  mtx_t transactionMutex;
  Dictionary *lockedTables;
} Sqlite;

// External API.
Database* sqliteInit(const char *databasePath);


#ifdef __cplusplus
} // extern "C"
#endif

#endif // SQLITE_LIB_H

