///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              10.17.2021
///
/// @file              SqlClientLib.h
///
///                    These functions are for all SQL database implementations
///                    and are abstractions for the database in use.
///
/// @details
///
/// @copyright
///                    Copyright (c) 2012-2024 Skymond, LLC.
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
///                                Skymond, LLC
///                             https://skymond.io
///
///////////////////////////////////////////////////////////////////////////////

#ifndef SQL_CLIENT_LIB_H
#define SQL_CLIENT_LIB_H

#include <stdarg.h>
#include "DbInterface.h"

#ifdef __cplusplus
extern "C"
{
#endif

/// @enum SqlDatabaseType
///
/// @brief Enumeration of the types of SQL databases supported by this library.
typedef enum SqlDatabaseType {
  MARIADB,
  SQLITE,
  NUM_SQL_DATABASE_TYPES
} SqlDatabaseType;

/// @struct SqlDatabase
///
/// @brief Definition of the object representing a database to the application.
///
/// @param connection A pointer to whatever the underlying database object is.
/// @param bytesQuery A function pointer to the connection-specific Bytes query
///   function.
/// @param stringQuery A function pointer to the connection-specific string
///   query function.
/// @param makeBytesLiteral A function pointer to the connection-specific
///   function for converting a Bytes object to a blob literal.
/// @param makeStringLiteral A function pointer to the connection-specific
///   function for converting a string to a string literal.
/// @param describeTable A function pointer to the connection-specific function
///   for describing the fields of a table.
/// @param compare A function pointer to the connection-specific function to
///   compare two connections of the same slqDbType.
/// @param tableDescriptions A HashTable that contains subordinate HashTables
///   that describe each of the tables queried.
typedef struct SqlDatabase {
  void *connection;
  SqlDatabaseType sqlDbType;
  DbResult* (*bytesQuery)(void *connection, const Bytes query);
  DbResult* (*stringQuery)(void *connection, const char *query);
  Bytes (*makeBytesLiteral)(const Bytes input);
  Bytes (*makeStringLiteral)(const char *input);
  DbResult* (*describeTable)(void *connection, const char *dbName, const char *tableName);
  int (*compare)(void *db1, void *db2);
  HashTable* tableDescriptions;
} SqlDatabase;

DbResult* sqlGetValuesVargs(SqlDatabase *database,
  const char *dbString, const char *tableName,
  const char *select, const char *orderBy, va_list args);
DbResult* sqlGetValues_(SqlDatabase *database,
  const char *dbName, const char *tableName,
  const char *select, const char *orderBy, ...);
/// @def sqlGetValues
/// Convenience macro that calls sqlGetValues_ with a trailing NULL.
#define sqlGetValues(database, dbName, tableName, select, ...) \
  sqlGetValues_(database, dbName, tableName, select, NULL, ##__VA_ARGS__, NULL)
/// @def sqlGetValuesOrderBy
/// Convenience macro that calls sqlGetValues_ with a trailing NULL.
/// This macro utilizes the orderBy parameter.
#define sqlGetValuesOrderBy(database, dbName, tableName, select, orderBy, ...) \
  sqlGetValues_( \
    database, dbName, tableName, select, orderBy, ##__VA_ARGS__, NULL)
/// @def sqlGetRecords
/// Convenience macro that calls sqlGetValues_ with * select and a trailing NULL.
#define sqlGetRecords(database, dbName, tableName, ...) \
  sqlGetValues_(database, dbName, tableName, "*", NULL, ##__VA_ARGS__, NULL)
/// @def sqlGetRecordsOrderBy
/// Convenience macro that calls sqlGetValues_ with * select and a trailing NULL.
/// This macro utilizes the orderBy parameter.
#define sqlGetRecordsOrderBy(database, dbName, tableName, orderBy, ...) \
  sqlGetValues_(database, dbName, tableName, "*", orderBy, ##__VA_ARGS__, NULL)
DbResult* sqlGetValuesDict(SqlDatabase *database,
  const char *dbString, const char *tableName,
  const char *select, const char *orderBy, Dictionary *args);
bool sqlAddRecordVargs(SqlDatabase *database,
  const char *dbString, const char *tableName, va_list args);
bool sqlAddTableVargs(SqlDatabase *database,
  const char *dbName, const char *tableName, const char *primaryKey,
  va_list args);
bool sqlDeleteRecordsVargs(SqlDatabase *database,
  const char *dbString, const char *tableName, va_list args);
bool sqlUpdateRecordDict(SqlDatabase *database,
  const char *dbString, const char *tableName, Dictionary *dict);
bool sqlAddRecordDict(SqlDatabase *database,
  const char *dbString, const char *tableName,
  Dictionary *dict);
DbResult* sqlGetValuesLikeVargs(SqlDatabase *database,
  const char *dbName, const char *tableName,
  const char *select, const char *orderBy, va_list args);
bool sqlAddTableList(SqlDatabase *database,
  const char *dbName, const char *tableName, const char *primaryKey,
  List *args);
bool sqlDeleteTable(SqlDatabase *database,
  const char *dbString, const char *tableName);
bool sqlDeleteRecordsLikeVargs(SqlDatabase *database,
  const char *dbString, const char *tableName, va_list args);
bool sqlUpdateResultVargs(const DbResult *dbResult, u64 resultIndex, va_list args);
DbResult* sqlQuery(SqlDatabase *database, const char *query);
bool sqlLockTablesDict(SqlDatabase *database, const Dictionary *tablesToLock);
bool sqlUnlockTables(SqlDatabase *database, const Dictionary *tableLock);
bool sqlStartTransaction(SqlDatabase *database);
bool sqlCommitTransaction(SqlDatabase *database);
bool sqlRollbackTransaction(SqlDatabase *database);
bool sqlDeleteField(SqlDatabase *database, const char *dbString,
  const char *tableName, const char *fieldName
);
bool sqlAddField(SqlDatabase *database, const char *dbString,
  const char *tableName, const char *afterField, const char *newField,
  void *type);
bool sqlChangeFieldName(SqlDatabase *database, const char *dbString,
  const char *tableName, const char *oldName, const char *newName);
DbResult* sqlDescribeTable(SqlDatabase *sqlDatabase, const char *dbName,
  const char *tableName);
bool sqlAddRecords(void *database,
  const char *dbName, const char *tableName, const DbResult *dbResult);
bool sqlRenameTable(SqlDatabase *database, const char *dbName,
  const char *oldTableName, const char *newTableName);
TypeDescriptor* sqlTypeNameToTypeDescriptor(const char *typeInfo);
int sqlCompare(void *db1, void *db2);
bool sqlUpdateFieldVargs(void *db, const char *dbName,
  const char *tableName, const char *fieldName, const void *newValue,
  va_list args);
TypeDescriptor* sqlGetFieldTypeByName(void *db,
  const char *dbName, const char *tableName, const char *fieldName);
TypeDescriptor* sqlGetFieldTypeByIndex(void *db,
  const char *dbName, const char *tableName, u64 fieldIndex);
i64 sqlGetNumRecords(void *db, const char *dbString, const char *tableName);
const char *typeDescriptorToSqlTypeName(TypeDescriptor *type);
DbResult* sqlGetOrValuesDict(void *database,
  const char *dbString, const char *tableName,
  const char *select, const char *orderBy, Dictionary *args);


#ifdef __cplusplus
} // extern "C"
#endif

#endif // SQL_CLIENT_LIB_H

