///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              06.06.2012
///
/// @file              DbInterface.h
///
///                    These functions are for all database implementations and
///                    are abstractions for the database in use.
///
/// @details
///
/// @copyright
///                    Copyright (c) 2012-2025 Skymond, LLC.
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

#ifndef DB_INTERFACE_H
#define DB_INTERFACE_H

#include <stdarg.h>
#include "Dictionary.h"
#include "HashTable.h"

// Forward declaration of DbResult to avoid cirucular dependencies.
typedef struct DbResult DbResult;

/// @enum DatabaseType
///
/// @brief Enumeration of the types of databases supported by this library.
typedef enum DatabaseType {
  SQL,
  NUM_DATABASE_TYPES
} DatabaseType;

/// @struct Database
///
/// @brief Definition of the object representing a database to the application.
/// @param db A pointer to whatever the underlying database object is.
/// @param dbType A DatabaseType value specifying the type of object the db
///   pointer refers to.
/// @param getValuesVargs A function pointer to the implementation-specific
///   function to get database results matching criteria from a variable
///   argument list.
/// @param getValuesDict A function pointer to the implementation-specific
///   function to get database results with criteria from a Dictionary.
/// @param addRecordVargs A function pointer to the implementation-specific
///   function add a record with values from a variable argument list.
/// @param getDatabaseNames A function pointer to the implementation-specific
///   function to get the names of the databases managed.
/// @param addTableVargs A function pointer to the implementation-specific
///   function to add a table with values from a variable argument list.
/// @param getTableNames A function pointer to the implementation-specific
///   function to get the names of the tables in the specified database.
/// @param deleteRecordsVargs A function pointer to the implementation-specific
///   function to delete records given criteria from a variable argument list.
/// @param updateRecordDict A function pointer to the implementation-specific
///   function that will update a record given a DbResult and new information
///   from a Dictionary.
/// @param addRecordDict A function pointer to the implementation-specific
///   function that will add a record with values provided with a Dictionary.
/// @param getValuesLikeVargs A function pointer to the implementation-specific
///   function to get database results similar to criteria from a variable
///   argument list.
/// @param addTableList A function pointer to the implementation-specific
///   function to add a table with values from a List.
/// @param deleteTable A function pointer to the implementation-specific
///   function to delete a table.
/// @param deleteRecordsLikeVargs A function pointer to the implementation-
///   specific function to delete a record with criteria simlar to what's
///   provided in a variable argument list.
/// @param updateResultVargs A function pointer to the implementation-specific
///   function to update a record given a DbResult and new information from a
///   variable argument list.
/// @param lockTablesDict A function pointer to the implementation-specific
///   funciton to lock a group of tables given a Dictionary describing them.
/// @param unlockTables A function pointer to the implementation-specific
///   function to unlock a group of tables given a Dictionary that describes
///   the previously-locked tables.
/// @param startTransaction A function pointer to the implementation-specific
///   function to start a transaction in the database system.
/// @param commitTransaction A function pointer to the implementation-specific
///   function to commit a transaction in the database system.
/// @param rollbackTransaction A function pointer to the implementation-specific
///   function to rollback a transaction in the database system.
/// @param addDatabase A function pointer to the implementation-specific
///   function to add a database to the database system.
/// @param deleteDatabase A function pointer to the implementation-specific
///   function to delete a database from the database system.
/// @param deleteField A function pointer to the implementation-specific
///   function to delete a field from a table.
/// @param addField A function pointer to the implementation-specific function
///   to add a new field to a table.
/// @param changeFieldType A function pointer to the implementation-specific
///   function to change the type of an existing field in a table.
/// @param changeFieldName
///   function to change the name of an existing field in a talbe.
/// @param disconnect A function pointer to the implementation-specific function
///  to disconnect from the underlying database system.
/// @param describeTable A function pointer to the implementation-specific
///   function to get a DbResult with a description of the fields of a table.
/// @param addRecords A function pointer to the implementation-specific function
///   to add a set of results to a database given a previous DbResult.
/// @param renameTable A function pointer to the implementation-specific
///   function to change the name of an existing table in the database.
/// @param compare A function pointer to the db-specific function to
///   compare two connections of the same dbType.
/// @param updateFieldVargs A function pointer to the db-specific function to
///   update a field in the database.
/// @param getNumRecords A function pointer to the db-specific function to get
///   the number of records in a table.
/// @param getSize A function pointer to the db-specific function to get the
///   size, in bytes, the database consumes.
/// @pararm getFieldTypeByName A function pointer to the db-specific function to
///   get the type of a field given its field name within a table.
/// @pararm getFieldTypeByIndex A function pointer to the db-specific function to
///   get the type of a field given its field index within a table.
/// @param renameDatabase A function pointer to the db-specific function to
///   rename an existing database.
/// @param threadLockedTables Thread-specific storage for database tables this
///   thread has locked.
/// @param lockedTablesMutex Mutex to ensure only one thread attempts to lock
///   tables at a time.
/// @param lockedTables HashTable of locked tables.  Keys are the names of
///   tables, values are the thread IDs of the threads that have the tables
///   locked.
/// @param lockedRecords RedBlaackTree of locked records.  Keys are Bytes
///   objects that are combinations of the database name, table name, and
///   selection criteria, values are the thread IDs of the threads that have
///   the records locked.
typedef struct Database {
  void *db;
  DatabaseType dbType;
  DbResult* (*getValuesVargs)(void *database,
    const char *dbName, const char *tableName,
    const char *select, const char *orderBy, va_list args);
  DbResult* (*getValuesDict)(void *database,
    const char *dbString, const char *tableName,
    const char *select, const char *orderBy, Dictionary *args);
  bool (*addRecordVargs)(void *database,
    const char *dbName, const char *tableName, va_list args);
  DbResult* (*getDatabaseNames)(void *database);
  bool (*addTableVargs)(void *database,
    const char *dbName, const char *tableName, const char *primaryKey,
    va_list args);
  DbResult* (*getTableNames)(void *database, const char *dbName);
  bool (*deleteRecordsVargs)(void *database,
    const char *dbName, const char *tableName, va_list args);
  bool (*updateRecordDict)(void *database,
    const char *dbName, const char *tableName, Dictionary *dict);
  bool (*addRecordDict)(void *database,
    const char *dbName, const char *tableName,
    Dictionary *dict);
  DbResult* (*getValuesLikeVargs)(void *database,
    const char *dbName, const char *tableName,
    const char *select, const char *orderBy, va_list args);
  bool (*addTableList)(void *database,
    const char *dbName, const char *tableName, const char *primaryKey,
    List *args);
  bool (*deleteTable)(void *database,
    const char *dbName, const char *tableName);
  bool (*deleteRecordsLikeVargs)(void *database,
    const char *dbName, const char *tableName, va_list args);
  bool (*updateResultVargs)(const DbResult *dbResult, u64 resultIndex,
    va_list args);
  bool (*lockTablesDict)(void *database, const Dictionary *tablesToLock);
  bool (*unlockTables)(void *database, const Dictionary *tableLock);
  bool (*startTransaction)(void *database);
  bool (*commitTransaction)(void *database);
  bool (*rollbackTransaction)(void *database);
  bool (*addDatabase)(void *database, const char *dbName);
  bool (*deleteDatabase)(void *database, const char *dbName);
  bool (*deleteField)(void *database, const char *dbString,
    const char *tableName, const char *fieldName);
  bool (*addField)(void *database, const char *dbString,
    const char *tableName, const char *afterField, const char *newField,
    void *type);
  bool (*changeFieldType)(void *database, const char *dbString,
    const char *tableName, const char *fieldName, const void *type);
  bool (*changeFieldName)(void *database, const char *dbString,
    const char *tableName, const char *oldName, const char *newName);
  void* (*disconnect)(void *database);
  DbResult* (*describeTable)(void *database, const char *dbName,
    const char *tableName);
  bool (*addRecords)(void *database,
    const char *dbName, const char *tableName, const DbResult *dbResult);
  bool (*renameTable)(void *database, const char *dbName,
    const char *oldTableName, const char *newTableName);
  int (*compare)(void *db1, void *db2);
  bool (*updateFieldVargs)(void *db, const char *dbName,
    const char *tableName, const char *fieldName, const void *newValue,
    va_list args);
  i64 (*getNumRecords)(void *db, const char *dbString, const char *tableName);
  i64 (*getSize)(void *db, const char *dbName);
  DbResult* (*getOrValuesDict)(void *database,
    const char *dbString, const char *tableName,
    const char *select, const char *orderBy, Dictionary *args);
  TypeDescriptor* (*getFieldTypeByName)(void *db,
    const char *dbName, const char *tableName, const char *fieldName);
  TypeDescriptor* (*getFieldTypeByIndex)(void *db,
    const char *dbName, const char *tableName, u64 fieldIndex);
  bool (*renameDatabase)(void *db,
    const char *oldDbName, const char *newDbName);
  bool (*ensureFieldIndexedVargs)(void *db,
    const char *dbName, const char *tableName, const char *fieldName,
    va_list args);
  tss_t threadLockedTables;
  mtx_t lockedTablesMutex;
  cnd_t lockedTablesCondition;
  Dictionary *lockedTables;
  RedBlackTree *lockedRecords;
} Database;

/// @struct DbResult
///
/// @brief Container for results coming from the database.
///
/// @param numFields The number of fields in the results table.
/// @param fieldTypes An array of TypeDescriptors that describe the type of
///   each field.
/// @param numRows The number of rows in the rows member.  This includes the
///   field names.
/// @param numResults The number of records returned.  (numRows - 1)
/// @param rows The table of field names and results.
/// @param dbName The name of the databse the query was made to.
/// @param tableName The name of the table the query was made to.
/// @param successful Indicates whether or not the query succeeded.
/// @param fieldNameIndexMap A hash table with the names of fields as keys and
///   their corresponding indexes in the table as values.
/// @param database A pointer to the Database object that connects to the
///   database system that generated this result.
typedef struct DbResult {
  u64 numFields;
  TypeDescriptor **fieldTypes;
  u64 numRows;
  u64 numResults;
  void ***rows;
  char *dbName;
  char *tableName;
  bool successful;
  HashTable *fieldNameIndexMap;
  Database *database;
} DbResult;

#ifdef __cplusplus
extern "C"
{
#endif

/// @def EMPTY_DB_RESULT
///
/// @brief Empty DbResult object to suffice as a default value for some of the
/// macros below.
#define EMPTY_DB_RESULT \
  ((DbResult) {0, NULL, 0, 0, NULL, NULL, NULL, false, NULL, NULL})

/// @def dbGetFieldNames(dbResult)
///
/// @brief Return a pointer to the first row of the result (the field names)
/// if the rows table is not NULL, NULL otherwise.
#define dbGetFieldNames(dbResult) \
  ((const char**) (((dbResult)->rows != NULL) ? (dbResult)->rows[0] : NULL))

extern char *dbInstance;

DbResult* dbGetValues_(Database *database,
  const char *dbName, const char *tableName,
  const char *select, const char *orderBy, ...);
/// @def dbGetValues
/// Convenience macro that calls dbGetValues_ with a trailing NULL.
#define dbGetValues(database, dbName, tableName, select, ...) \
  dbGetValues_(database, dbName, tableName, select, NULL, ##__VA_ARGS__, NULL)
/// @def dbGetValuesOrderBy
/// Convenience macro that calls dbGetValues_ with a trailing NULL.
/// This macro utilizes the orderBy parameter.
#define dbGetValuesOrderBy(database, dbName, tableName, select, orderBy, ...) \
  dbGetValues_( \
    database, dbName, tableName, select, orderBy, ##__VA_ARGS__, NULL)
DbResult* dbGetRecords_(Database *database,
  const char *dbName, const char *tableName,
  const char *orderBy, ...);
/// @def dbGetRecords
/// Convenience macro that calls dbGetRecords_ with a trailing NULL.
#define dbGetRecords(database, dbName, tableName, ...) \
  dbGetRecords_(database, dbName, tableName, NULL, ##__VA_ARGS__, NULL)
/// @def dbGetRecordsOrderBy
/// Convenience macro that calls dbGetRecords_ with a trailing NULL.
/// This macro utilizes the orderBy parameter.
#define dbGetRecordsOrderBy(database, dbName, tableName, orderBy, ...) \
  dbGetRecords_(database, dbName, tableName, orderBy, ##__VA_ARGS__, NULL)
bool dbAddRecord_(Database *database,
  const char *dbName, const char *tableName, ...);
/// @def dbAddRecord
/// Convenience macro that calls dbAddRecord_ with a trailing NULL.
#define dbAddRecord(database, dbName, tableName, ...) \
  dbAddRecord_(database, dbName, tableName, ##__VA_ARGS__, NULL)
bool dbCheckExists(Database *database, const char *dbName);
bool dbCheckTableExists(Database *database,
  const char *dbName, const char *tableName);
bool dbCheckFieldExists(Database *database,
  const char *dbName, const char *tableName, const char *fieldName);
bool dbAddTable_(Database *database,
  const char *dbName, const char *tableName, const char *primaryKey, ...);
/// @def dbAddTable
/// Convenience macro that calls dbAddTable_ with a trailing NULL.
#define dbAddTable(database, dbName, tableName, primaryKey, ...) \
  dbAddTable_(database, dbName, tableName, primaryKey, ##__VA_ARGS__, NULL)
DbResult* dbGetTableNames(Database *database, const char *dbName);
DbResult* dbFreeResult(DbResult *dbResult);
bool dbDeleteRecords_(Database *database,
  const char *dbName, const char *tableName, ...);
#define dbDeleteRecords(database, dbName, tableName, ...) \
  dbDeleteRecords_(database, dbName, tableName, ##__VA_ARGS__, NULL)
Bytes** dbResultToBytesTable(DbResult *dbResult);
const Bytes** dbResultGetBytesTable(DbResult *dbResult);
const void*** dbResultGetRows(DbResult *dbResult);
DbResult* dbGetValuesLike_(Database *database,
  const char *dbName, const char *tableName,
  const char *select, const char *orderBy, ...);
/// @def dbGetValuesLike
/// Convenience macro that calls dbGetValuesLike_ with a NULL for the orderBy
/// parameter and a trailing NULL.
#define dbGetValuesLike(database, dbName, tableName, select, ...) \
  dbGetValuesLike_(database, dbName, tableName, select, NULL, ##__VA_ARGS__, NULL)
/// @def dbGetValuesLikeOrderBy
/// Convenience macro that calls dbGetValuesLike_ with a trailing NULL.
/// This macro utilizes the orderBy parameter.
#define dbGetValuesLikeOrderBy(database, dbName, tableName, select, orderBy, ...) \
  dbGetValuesLike_( \
    database, dbName, tableName, select, orderBy, ##__VA_ARGS__, NULL)
DbResult* dbGetRecordsLike_(Database *database,
  const char *dbName, const char *tableName, 
  const char *orderBy, ...);
/// @def dbGetRecordsLike
/// Convenience macro that calls dbGetRecords_ with a trailing NULL.
#define dbGetRecordsLike(database, dbName, tableName, ...) \
  dbGetRecordsLike_(database, dbName, tableName, NULL, ##__VA_ARGS__, NULL)
/// @def dbGetRecordsLikeOrderBy
/// Convenience macro that calls dbGetRecords_ with a trailing NULL.
/// This macro utilizes the orderBy parameter.
#define dbGetRecordsLikeOrderBy(database, dbName, tableName, orderBy, ...) \
  dbGetRecordsLike_(database, dbName, tableName, orderBy, ##__VA_ARGS__, NULL)
bool dbDeleteRecordsLike_(Database *database,
  const char *dbName, const char *tableName, ...);
/// @def dbDeleteRecordsLike
/// Convenience macro that calls dbDeleteRecordsLike_ with a trailing NULL.
#define dbDeleteRecordsLike(database, dbName, tableName, ...) \
  dbDeleteRecordsLike_(database, dbName, tableName, ##__VA_ARGS__, NULL)
void* dbGetResultByIndex(const DbResult *dbResult, u64 resultIndex,
  u64 fieldIndex, const TypeDescriptor *type);
i64 dbGetFieldIndexByName(const DbResult *dbResult, const char *fieldName);
void* dbGetResultByName(const DbResult *dbResult, u64 resultIndex,
  const char *fieldName, const TypeDescriptor *type);
#define dbGetStringByName(dbResult, resultIndex, fieldName) \
  ((const char*) \
    dbGetResultByName(dbResult, resultIndex, fieldName, typeString))
#define dbGetStringByIndex(dbResult, resultIndex, fieldIndex) \
  ((const char*) \
    dbGetResultByIndex(dbResult, resultIndex, fieldIndex, typeString))
#define dbGetBytesByName(dbResult, resultIndex, fieldName) \
  ((Bytes) dbGetResultByName(dbResult, resultIndex, fieldName, typeBytes))
#define dbGetBytesByIndex(dbResult, resultIndex, fieldIndex) \
  ((Bytes) dbGetResultByIndex(dbResult, resultIndex, fieldIndex, typeBytes))
void *dbGetResultByLookup_(const DbResult *dbResult, const char *fieldName,
  const TypeDescriptor *type, ...);
#define dbGetStringByLookup(dbResult, fieldName, ...) \
  ((const char*) \
    dbGetResultByLookup_(dbResult, fieldName, typeString, ##__VA_ARGS__, NULL))
#define dbGetBytesByLookup(dbResult, fieldName, ...) \
  ((Bytes) \
    dbGetResultByLookup_(dbResult, fieldName, typeBytes, ##__VA_ARGS__, NULL))
i64 dbGetResultIndexByLookup_(const DbResult *dbResult, ...);
#define dbGetResultIndexByLookup(dbResult, ...) \
  dbGetResultIndexByLookup_(dbResult, ##__VA_ARGS__, NULL)
bool dbUpdateResult_(const DbResult *dbResult, u64 resultIndex, ...);
#define dbUpdateResult(dbResult, resultIndex, ...) \
  dbUpdateResult_(dbResult, resultIndex, ##__VA_ARGS__, NULL)
Dictionary* dbLockTablesDict(Database *database,
  const Dictionary *tablesToLock);
Dictionary* dbLockTablesVargs(Database *database, va_list args);
Dictionary* dbLockTables_(Database *database, ...);
#define dbLockTables(database, ...) \
  dbLockTables_(database, ##__VA_ARGS__, NULL)
Dictionary* dbUnlockTables(Dictionary *tableLock);
bool dbIsFieldNameValid(const char *name);
Database* dbDisconnect(Database *database);
Bytes dbResultToCsv(const DbResult *dbResult);
bool dbAddRecords(Database *database,
  const char *dbName, const char *tableName, const DbResult *dbResult);
bool dbAddRecordDict(Database *database, const char *dbName,
  const char *tableName, Dictionary *dict);
bool dbUpdateRecordDict(Database *database, const char *dbName,
  const char *tableName, Dictionary *dict);
bool dbAddTableList(Database *database, const char *dbName,
  const char *tableName, const char *primaryKey, List *args);
bool dbDeleteTable(Database *database, const char *dbName,
  const char *tableName);
Database* dbStartTransaction(Database *database);
Database* dbCommitTransaction(Database *database);
Database* dbRollbackTransaction(Database *database);
bool dbAddDatabase(Database *database, const char *dbName);
bool dbDeleteDatabase(Database *database, const char *dbName);
bool dbDeleteField(Database *database, const char *dbName,
  const char *tableName, const char *fieldName);
bool dbAddField(Database *database, const char *dbName, const char *tableName,
  const char *afterField, const char *newField, TypeDescriptor *type);
bool dbChangeFieldType(Database *database, const char *dbName,
  const char *tableName, const char *fieldName, const void *type);
bool dbChangeFieldName(Database *database, const char *dbName,
  const char *tableName, const char *oldName, const char *newName);
DbResult* dbGetValuesDict(Database *database, const char *dbName,
  const char *tableName, const char *select, Dictionary *dict);
DbResult* dbGetValuesDictOrderBy(Database *database, const char *dbName,
  const char *tableName, const char *select, const char *orderBy,
  Dictionary *dict);
DbResult* dbDescribeTable(Database *database, const char *dbName,
  const char *tableName);
bool dbIsTableLocked(Database *database, const char *dbName,
  const char *tableName);
void dbWaitForTableUnlocked(Database *database, const char *dbName,
  const char *tableName);
void dbFinalizeResult(DbResult *dbResult, Database *database,
  const char *dbName, const char *tableName);
bool dbRenameTable(Database *database, const char *dbName,
  const char *oldTableName, const char *newTableName);
void* dbSetResultByIndex(DbResult *dbResult,
  u64 resultIndex, u64 fieldIndex, const volatile void *value);
void* dbSetResultByName(DbResult *dbResult,
  u64 resultIndex, const char *fieldName, const volatile void *value);
bool dbCreateTableFromResult(Database *database, const char *dbName,
  const char *tableName, DbResult *dbResult);
int dbCompare(Database *db1, Database *db2);
DbResult* bytesTableToDbResult(Database *database, const char *dbName,
  const char *tableName, Bytes **table);
bool dbUpdateField_(Database *database, const char *dbName,
  const char *tableName, const char *fieldName, const void *newValue, ...);
#define dbUpdateField(database, dbName, tableName, fieldName, newValue, ...) \
  dbUpdateField_(database, dbName, tableName, fieldName, newValue, \
    ##__VA_ARGS__, NULL)
i64 dbGetNumRecords(Database *database,
  const char *dbName, const char *tableName);
Bytes dbResultToBytes_(const DbResult *dbResult,
  const char *recordDelimiter, const char *fieldDelimiter, ...);
#define dbResultToBytes(dbResult, ...) \
  dbResultToBytes_(dbResult, ##__VA_ARGS__, NULL, NULL)
int dbResultCompare(DbResult *dbResultA, DbResult *dbResultB);
DbResult* dbResultCopy(DbResult *dbResult);
i64 dbGetSize(Database *database, const char *dbName);
DbResult* dbGetOrValuesDict(Database *database,
  const char *dbString, const char *tableName,
  const char *select, const char *orderBy, Dictionary *args);
Bytes dbLockRecords_(Database *database,
  const char *dbName, const char *tableName,
  const char *fieldName, const volatile void *fieldValue, ...);
#define dbLockRecords(database, dbName, tableName, fieldName, fieldValue, ...) \
  dbLockRecords_(database, dbName, tableName, fieldName, fieldValue, \
    ##__VA_ARGS__, NULL)
Bytes dbUnlockRecords(Database *database, Bytes recordLock);
TypeDescriptor* dbGetFieldTypeByName(Database *database,
  const char *dbName, const char *tableName, const char *fieldName);
TypeDescriptor* dbGetFieldTypeByIndex(Database *database,
  const char *dbName, const char *tableName, u64 fieldIndex);
bool dbRenameDatabase(Database *database,
  const char *oldDbName, const char *newDbName);
DbResult* dbResultGetRange(DbResult *inputResult, u64 startIndex, u64 endIndex);
bool dbEnsureFieldIndexed_(Database *database,
  const char *dbName, const char *tableName, const char *fieldName, ...);
#define dbEnsureFieldIndexed(database, dbName, tableName, fieldName, ...) \
  dbEnsureFieldIndexed_(database, dbName, tableName, fieldName, \
    ##__VA_ARGS__, NULL)


#ifdef __cplusplus
} // extern "C"
#endif

#endif // DB_INTERFACE_H

