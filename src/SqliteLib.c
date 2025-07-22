////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                   Copyright (c) 2012-2025 Skymond, LLC.                    //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included    //
// in all copies or substantial portions of the Software.                     //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//                               Skymond, LLC                                 //
//                            https://skymond.io                              //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

// Doxygen marker
/// @file

#include "SqliteLib.h"
#include "SqlClientLib.h"
#include "StringLib.h"
#include "Scope.h"
#ifdef LOGGING_ENABLED
#include "LoggingLib.h"
#else
#undef printLog
#define printLog(...) {}
#define LOG_MALLOC_FAILURE(...) {}
#define logFile stderr
#endif

// External function prototypes.
bool sqlInvalidateTableDescription(void *db,
  const char *dbName, const char *tableName);

DbResult* sqliteExecQueryString(void *connection, const char *queryString);
DbResult* sqliteExecQueryBytes(void *connection, const Bytes queryBytes);
DbResult* sqliteGetDatabaseNames(void *db);
DbResult* sqliteGetTableNames(void *db, const char *dbString);
bool sqliteAddDatabase(void *db, const char *dbString);
bool sqliteDeleteDatabase(void *db, const char *dbString);
Bytes sqliteMakeStringLiteral(const char *input);
Bytes sqliteMakeBytesLiteral(const Bytes input);
bool sqliteStartTransaction(void *db);
bool sqliteCommitTransaction(void *db);
bool sqliteRollbackTransaction(void *db);
void* sqliteDisconnect(void *db);
DbResult* sqliteDescribeTable(void *connection, const char *dbString,
  const char *tableName);
bool sqliteChangeFieldType(void *db, const char *dbString,
  const char *tableName, const char *fieldName, const void *type);
bool sqliteLockTablesDict(void *db, const Dictionary *tablesToLock);
bool sqliteUnlockTables(void *db, const Dictionary *tableLock);
bool sqliteAddField(void *db, const char *dbString,
  const char *tableName, const char *afterField, const char *newField,
  void *type);
int sqliteCompare(void *db1, void *db2);
bool sqliteRenameTable(void *db, const char *dbName,
  const char *oldTableName, const char *newTableName);
i64 sqliteGetSize(void *db, const char *dbName);
bool sqliteRenameDatabase(void *db,
  const char *oldDbName, const char *newDbName);
bool sqliteEnsureFieldIndexedVargs(void *database,
  const char *dbName, const char *tableName, const char *fieldName,
  va_list args);
bool sqliteAddRecords(void *database,
  const char *dbName, const char *tableName, const DbResult *dbResut);

/// @var _sqliteThreadSetup
///
/// @brief once_flag to ensure that we only initialize the metadata once.
static once_flag _sqliteThreadSetup = ONCE_FLAG_INIT;

/// @fn void setupSqliteMetadata(void)
///
/// @brief Configure anything that's needed to work with SQLite before we issue
/// the first call to a database.  This function may only be called once per
/// run of the program.
///
/// @return This function returns no value.
void setupSqliteMetadata(void) {
  printLog(TRACE, "ENTER setupSqliteMetadata()\n");
#if _MSC_VER || _WIN32
  // On Windows systems, this has to be done BEFORE any database connections
  // are opened.  Unnecessary on Linux.
  char *tmpDir = getenv("TEMP");
  if (tmpDir == NULL)
  {
    tmpDir = getenv("TMP");
  }
  if (tmpDir != NULL) {
    sqlite3_temp_directory = sqlite3_malloc(strlen(tmpDir) + 1);
    if (sqlite3_temp_directory != NULL) {
      strcpy(sqlite3_temp_directory, tmpDir);
    } else {
      LOG_MALLOC_FAILURE();
      goto error;
    }
  } else {
    printLog(WARN, "Could not get temp directory.\n");
    goto error;
  }
#endif
  
  printLog(TRACE, "EXIT setupSqliteMetadata() = {success}\n");
  return;
  
goto error;
error:
  printLog(WARN,
    "SQLite features that require the use of temporary files may fail.\n");
  printLog(TRACE, "EXIT setupSqliteMetadata() = {failure}\n");
  return;
}

/// @fn Database* sqliteInit(const char *databasePath)
///
/// @brief Initialize a connection to a SQLite database instance.  Note that
/// for this library, the path is to the directory that the SQLite databases
/// are in.  The individual databases are managed by this library and need not
/// be directly addressed.  Also note that there is a limit of 124 databases at
/// the provided path because SQLite has a limit of 125 simultaneous attached
/// databases and one (main) is used for the database management metadata.
///
/// @param databasePath The path to the directory that holds the SQLite database
///   files.
///
/// @return Returns a pointer to a fully-initialized Database instance on
/// success, NULL on failure.
Database* sqliteInit(const char *databasePath) {
  printLog(TRACE, "ENTER sqliteInit(databasePath=\"%s\")\n", databasePath);
  
  call_once(&_sqliteThreadSetup, setupSqliteMetadata);
  
  // Variable declarations:
  Bytes mainDatabasePath = NULL;
  int rc = 0;
  DbResult *queryResult = NULL;
  u64 i = 0;
  SqlDatabase *sqlDatabase = NULL;
  Database *database = NULL;
  bool databasesTableFound = false;
  char *query = NULL;
  int status = thrd_success;
  
  // Allocate everything.
  Sqlite *sqlite = (Sqlite*) calloc(1, sizeof(Sqlite));
  if (sqlite == NULL) {
    LOG_MALLOC_FAILURE();
    goto sqliteMallocFailure;
  }
  
  if (bytesAddStr(&sqlite->databasePath, databasePath) == NULL) {
    LOG_MALLOC_FAILURE();
    goto sqliteDatabasePathMallocFailure;
  }
  
  // sqlite->transactionCount is already 0 from the calloc
  
  if (mtx_init(&sqlite->transactionMutex, mtx_plain | mtx_recursive)
    != thrd_success
  ) {
    printLog(ERR, "Could not initialize SQLite transactionMutex.\n");
    goto sqliteTransactionMutexInitFailure;
  }
  
  sqlite->lockedTables = dictionaryCreate(typeString);
  if (sqlite->lockedTables == NULL) {
    goto sqliteLockedTablesMallocFailure;
    LOG_MALLOC_FAILURE();
  }
  
  // Open the main database.
  bytesAddBytes(&mainDatabasePath, sqlite->databasePath);
  if ((databasePath != NULL)
    && (sqlite->databasePath[bytesLength(sqlite->databasePath) - 1] != '/')
  ) {
    bytesAddStr(&mainDatabasePath, "/");
  }
  if (bytesAddStr(&mainDatabasePath, "main") == NULL) {
    LOG_MALLOC_FAILURE();
    mainDatabasePath = bytesDestroy(mainDatabasePath);
    goto sqliteInitFailure;
  }
  if (dbInstance != NULL) {
    if (bytesAddStr(&mainDatabasePath, dbInstance) == NULL) {
      LOG_MALLOC_FAILURE();
      mainDatabasePath = bytesDestroy(mainDatabasePath);
      goto sqliteInitFailure;
    }
  }
  rc = sqlite3_open((char*) mainDatabasePath, &sqlite->db);
  if (rc != SQLITE_OK) {
    printLog(ERR,
      "Cannot open SQLite database: %s\n",
      sqlite3_errmsg(sqlite->db));
    mainDatabasePath = bytesDestroy(mainDatabasePath);
    goto sqliteInitFailure;
  }
  
  // Enable extended result codes.
  sqlite3_extended_result_codes(sqlite->db, 1);
  
  // Change the maximum number of attached databases.
  sqlite3_limit(sqlite->db, SQLITE_LIMIT_ATTACHED, 125);
  if (sqlite3_limit(sqlite->db, SQLITE_LIMIT_ATTACHED, -1) != 125) {
    printLog(ERR, "Attempted to change maximum attached databases to 125.\n");
    printLog(ERR, "%d attached databases allowed.\n",
      sqlite3_limit(sqlite->db, SQLITE_LIMIT_ATTACHED, -1));
    mainDatabasePath = bytesDestroy(mainDatabasePath);
    goto sqliteInitFailure;
  }
  
  if (dbInstance != NULL) {
    // Attach the main mainDatabasePath as main + dbInstance
    if (asprintf(&query, "attach '%s' as main%s;", (char*) mainDatabasePath,
      dbInstance) <= 0
    ) {
      LOG_MALLOC_FAILURE();
      mainDatabasePath = bytesDestroy(mainDatabasePath);
      goto sqliteInitFailure;
    }
    queryResult = sqliteExecQueryString(sqlite, query);
    if (dbQuerySuccessful(queryResult) == false) {
      printLog(ERR, "Could not attach '%s' as main%s.\n",
        (char*) mainDatabasePath, dbInstance);
      queryResult = dbFreeResult(queryResult);
      query = stringDestroy(query);
      mainDatabasePath = bytesDestroy(mainDatabasePath);
      goto sqliteInitFailure;
    }
    queryResult = dbFreeResult(queryResult);
    query = stringDestroy(query);
  }
  mainDatabasePath = bytesDestroy(mainDatabasePath);
  
  // Check to make sure the Databases table is there.
  queryResult = sqliteExecQueryString(sqlite,
    "select name from main.sqlite_schema where type='table';");
  if (dbQuerySuccessful(queryResult) == false) {
    printLog(ERR, "Getting tables from main.sqlite_schema failed.\n");
  }
  for (i = 0; i < dbGetNumResults(queryResult); i++) {
    if (strcmpci(dbGetStringByIndex(queryResult, i, 0), "Databases") == 0) {
      databasesTableFound = true;
      break;
    }
  }
  queryResult = dbFreeResult(queryResult);
  if (databasesTableFound == false) {
    // Create the table.
    queryResult = sqliteExecQueryString(sqlite,
      "create table main.Databases("
        "name varchar(40) primary key, type varchar(40), connection blob);");
    if (dbQuerySuccessful(queryResult) == false) {
      queryResult = dbFreeResult(queryResult);
      printLog(ERR, "Could not create main.Databases metadata table.\n");
      goto sqliteInitFailure;
    }
    queryResult = dbFreeResult(queryResult);
  }
  
  // Attach all the other SQLite databases
  queryResult = sqliteExecQueryString(sqlite,
    "select name, connection from main.Databases where type='sqlite';");
  for (i = 0; i < dbGetNumResults(queryResult); i++) {
    // The connection field holds the path to the database for SQLite databases.
    if (asprintf(&query, "attach '%s' as %s%s;",
      dbGetStringByName(queryResult, i, "connection"),
      dbGetStringByName(queryResult, i, "name"),
      (dbInstance == NULL) ? "" : dbInstance) <= 0
    ) {
      LOG_MALLOC_FAILURE();
      queryResult = dbFreeResult(queryResult);
      goto sqliteInitFailure;
    }
    DbResult* attachResult = sqliteExecQueryString(sqlite, query);
    if (dbQuerySuccessful(attachResult) == false) {
      printLog(ERR, "Could not attach '%s' as %s.\n",
        dbGetStringByName(queryResult, i, "connection"),
        dbGetStringByName(queryResult, i, "name"));
    }
    attachResult = dbFreeResult(attachResult);
    query = stringDestroy(query);
  }
  queryResult = dbFreeResult(queryResult);
  // Sqlite object is set up and configured.  Complete the full object setup.
  
  // SqlDatabase-level object.
  sqlDatabase = (SqlDatabase*) malloc(sizeof(SqlDatabase));
  if (sqlDatabase == NULL) {
    LOG_MALLOC_FAILURE();
    goto sqliteInitFailure;
  }
  sqlDatabase->bytesQuery =sqliteExecQueryBytes;
  sqlDatabase->stringQuery = sqliteExecQueryString;
  sqlDatabase->makeBytesLiteral = sqliteMakeBytesLiteral;
  sqlDatabase->makeStringLiteral = sqliteMakeStringLiteral;
  sqlDatabase->connection = sqlite;
  sqlDatabase->sqlDbType = SQLITE;
  sqlDatabase->describeTable = sqliteDescribeTable;
  sqlDatabase->compare = sqliteCompare;
  sqlDatabase->tableDescriptions = htCreate(typeBytes);
  
  // External Database object.
  // The lock mtx_t member variable has to be zeroed, so use calloc.
  database = (Database*) calloc(1, sizeof(Database));
  if (database == NULL) {
    LOG_MALLOC_FAILURE();
    goto databaseMallocFailure;
  }
  database->getValuesVargs = sqlGetValuesVargs;
  database->getValuesDict = sqlGetValuesDict;
  database->addRecordVargs = sqlAddRecordVargs;
  database->getDatabaseNames = sqliteGetDatabaseNames;
  database->addTableVargs = sqlAddTableVargs;
  database->getTableNames = sqliteGetTableNames;
  database->deleteRecordsVargs = sqlDeleteRecordsVargs;
  database->updateRecordDict = sqlUpdateRecordDict;
  database->addRecordDict = sqlAddRecordDict;
  database->getValuesLikeVargs = sqlGetValuesLikeVargs;
  database->addTableList = sqlAddTableList;
  database->deleteTable = sqlDeleteTable;
  database->deleteRecordsLikeVargs = sqlDeleteRecordsLikeVargs;
  database->updateResultVargs = sqlUpdateResultVargs;
  database->lockTablesDict = sqliteLockTablesDict;
  database->unlockTables = sqliteUnlockTables;
  database->startTransaction = sqliteStartTransaction;
  database->commitTransaction = sqliteCommitTransaction;
  database->rollbackTransaction = sqliteRollbackTransaction;
  database->addDatabase = sqliteAddDatabase;
  database->deleteDatabase = sqliteDeleteDatabase;
  database->deleteField = sqlDeleteField;
  database->addField = sqliteAddField;
  database->changeFieldType = sqliteChangeFieldType;
  database->changeFieldName = sqlChangeFieldName;
  database->disconnect = sqliteDisconnect;
  database->describeTable = sqlDescribeTable;
  database->addRecords = sqliteAddRecords;
  database->renameTable = sqliteRenameTable;
  database->compare = sqlCompare;
  database->getNumRecords = sqlGetNumRecords;
  database->getOrValuesDict = sqlGetOrValuesDict;
  database->getSize = sqliteGetSize;
  database->renameDatabase = sqliteRenameDatabase;
  database->ensureFieldIndexedVargs = sqliteEnsureFieldIndexedVargs;
  database->updateFieldVargs = sqlUpdateFieldVargs;
  database->getFieldTypeByName = sqlGetFieldTypeByName;
  database->getFieldTypeByIndex = sqlGetFieldTypeByIndex;
  database->db = sqlDatabase;
  database->dbType = SQL;
  
  status = tss_create(&database->threadLockedTables,
    (tss_dtor_t) ((void*) dictionaryDestroy)); // Have to double-cast
  if (status != thrd_success) {
    printLog(ERR, "Could not initialize threadLockedTables.\n");
    database = dbDisconnect(database);
    printLog(TRACE,
      "EXIT sqliteInit(databasePath=\"%s\") = {NULl}\n", databasePath);
    return NULL;
  }
  
  status = mtx_init(&database->lockedTablesMutex, mtx_plain);
  if (status != thrd_success) {
    printLog(ERR, "Could not initialize lockedTablesMutex.\n");
    database = dbDisconnect(database);
    printLog(TRACE,
      "EXIT sqliteInit(databasePath=\"%s\") = {NULl}\n", databasePath);
    return NULL;
  }
  
  status = cnd_init(&database->lockedTablesCondition);
  if (status != thrd_success) {
    printLog(ERR, "Could not initialize lockedTablesCondition.\n");
    database = dbDisconnect(database);
    printLog(TRACE,
      "EXIT sqliteInit(databasePath=\"%s\") = {NULl}\n", databasePath);
    return NULL;
  }
  
  database->lockedTables = dictionaryCreate(typeString);
  if (database->lockedTables == NULL) {
    LOG_MALLOC_FAILURE();
    database = dbDisconnect(database);
    printLog(TRACE,
      "EXIT sqliteInit(databasePath=\"%s\") = {NULl}\n", databasePath);
    return NULL;
  }
  
  database->lockedRecords = rbTreeCreate(typeBytes);
  if (database->lockedRecords == NULL) {
    LOG_MALLOC_FAILURE();
    database = dbDisconnect(database);
    printLog(TRACE,
      "EXIT sqliteInit(databasePath=\"%s\") = {NULl}\n", databasePath);
    return NULL;
  }
  
  printLog(TRACE,
    "EXIT sqliteInit(databasePath=\"%s\") = {%p}\n", databasePath, database);
  return database;
  
databaseMallocFailure:
  sqlDatabase = (SqlDatabase*) pointerDestroy(sqlDatabase);
  
sqliteInitFailure:
  sqlite3_close(sqlite->db); sqlite->db = NULL;
  sqlite->lockedTables = dictionaryDestroy(sqlite->lockedTables);
  
sqliteLockedTablesMallocFailure:
  mtx_destroy(&sqlite->transactionMutex);

sqliteTransactionMutexInitFailure:
  sqlite->databasePath = bytesDestroy(sqlite->databasePath);
  
sqliteDatabasePathMallocFailure:
  sqlite = (Sqlite*) pointerDestroy(sqlite);
  
sqliteMallocFailure:
  printLog(TRACE,
    "EXIT sqliteInit(databasePath=\"%s\") = {NULL}\n", databasePath);
  return NULL;
}

/// @fn DbResult* sqliteExecQueryString(void *connection, const char *queryString)
///
/// @brief Wrapper around sqlite3_exec that runs the provided query on the
/// SQLite database instance and provides a DbResult.
///
/// @param connection A pointer to a Sqlite instance holding the metadata for
///   the SQLite connection, cast to a void*.
/// @param queryString A string containing the query to run.
///
/// @return Returns a standard DbResult with the results of the query.
DbResult* sqliteExecQueryString(void *connection, const char *queryString) {
  Sqlite *database = (Sqlite*) connection;
  printLog(FLOOD,
    "ENTER sqliteExecQueryString(database=%p, queryString=\"%s\")",
    database, queryString);
  
  Bytes queryBytes = NULL;
  bytesAddStr(&queryBytes, queryString);
  DbResult *queryResult = sqliteExecQueryBytes(database, queryBytes);
  queryBytes = bytesDestroy(queryBytes);
  
  printLog(FLOOD,
    "EXIT sqliteExecQueryString(database=%p, queryString=\"%s\") = {%llu results}",
    database, queryString, llu(dbGetNumResults(queryResult)));
  return queryResult;
}

/// @def NUM_SQLITE_DATA_TYPES
///
/// @brief The number of fundamental data types in SQLite.
#define NUM_SQLITE_DATA_TYPES 5


/// @var sqliteTypes
///
/// @brief Mapping of the funcamental SQLite data types to our internal data
/// types.
TypeDescriptor **sqliteTypes[NUM_SQLITE_DATA_TYPES] = {
  &typeI64,     // SQLITE_INTEGER
  &typeDouble,  // SQLITE_FLOAT
  &typeString,  // SQLITE_TEXT
  &typeBytes,   // SQLITE_BLOB
  &typeBytes,   // SQLITE_NULL
};

/// @fn DbResult* sqliteExecQueryBytes(void *connection, const Bytes queryBytes)
///
/// @brief Wrapper around sqlite3_exec that runs the provided query on the
/// SQLite database instance and provides a DbResult.
///
/// @param connection A pointer to a Sqlite instance holding the metadata for
///   the SQLite connection, cast to a void*.
/// @param queryBytes A string containing the query to run.
///
/// @return Returns a standard DbResult with the results of the query.
DbResult* sqliteExecQueryBytes(void *connection, const Bytes queryBytes) {
  Sqlite *database = (Sqlite*) connection;
  printLog(FLOOD,
    "ENTER sqliteExecQueryBytes(database=%p, queryBytes=\"%s\")",
    database, queryBytes);
  
  DbResult *queryResult = (DbResult*) calloc(1, sizeof(DbResult));
  if (queryResult == NULL) {
    LOG_MALLOC_FAILURE();
    return NULL;
  }
  const char *sqliteErrorMessage = NULL;
  (void) sqliteErrorMessage; // For when logging is disabled.
  sqlite3_stmt *preparedStatement = NULL;
  const char *queryTail = NULL;
  const char *query = (const char*) queryBytes;
  int queryLength = (int) (bytesLength(queryBytes) + 1);
  u64 row = 0;
  void ***check = NULL;
  int dataType = 0;
  int i = 0, j = 0;
  int numRows = 0, numFields = 0;
  i64 intData = 0;
  double doubleData = 0.0;
  const unsigned char *stringData = NULL;
  const void *bytesData = NULL;
  int bytesDataLength = 0;
  
  // Prepare the query.
  int rc = SQLITE_OK;
  while ((queryLength > 1) && (rc == SQLITE_OK)
    && (preparedStatement == NULL)
  ) {
    rc = sqlite3_prepare_v2(
      database->db,       // Database handle
      query,              // SQL statement, UTF-8 encoded
      queryLength,        // Length of query including nul terminator
      &preparedStatement, // OUT: Statement handle
      &queryTail          // OUT: Pointer to unused portion of query
    );
    if (queryTail != NULL) {
      queryLength -= (queryTail - query);
      query = queryTail;
    }
  }
  
  if (rc != SQLITE_OK) {
    printLog(ERR, "Could not prepare statement.\n");
    sqliteErrorMessage = sqlite3_errstr(rc);
    // DO NOT FREE sqliteErrorMessage!!!
    printLog(ERR, "SQLite query error(%d): %s\n", rc, sqliteErrorMessage);
    printLog(ERR, "query: \"%s\"\n", queryBytes);
    printLog(ERR, "queryTail: \"%s\"\n", queryTail);
    printLog(ERR, "bytesLength(queryBytes) = %llu\n", llu(bytesLength(queryBytes)));
    sqlite3_finalize(preparedStatement);
    printLog(FLOOD,
      "EXIT sqliteExecQueryBytes(database=%p, queryBytes=\"%s\") = {NOT successful}",
      database, queryBytes);
    return queryResult; // dbQuerySuccessful(queryResult) is false
  }
  
  // Step through the query results.
  if (preparedStatement != NULL) {
    rc = sqlite3_step(preparedStatement);
  }
  while ((rc == SQLITE_ROW) && (preparedStatement != NULL)) {
    numFields = sqlite3_column_count(preparedStatement);
    if (numFields <= 0) {
      break;
    }
    if (dbGetNumRows(queryResult) == 0) {
      // Uninitialized queryResult->  Populate initial data.
      queryResult->numFields = (u64) numFields;
      
      queryResult->fieldTypes
        = (TypeDescriptor**) calloc(1, numFields * sizeof(TypeDescriptor*));
      if (queryResult->fieldTypes == NULL) {
        LOG_MALLOC_FAILURE();
        goto fieldTypesMallocFailure;
      }
      
      queryResult->rows = (void***) malloc(2 * sizeof(void**));
      if (queryResult->rows == NULL) {
        LOG_MALLOC_FAILURE();
        goto rowsMallocFailure;
      }
      queryResult->rows[0] = (void**) calloc(1, (numFields + 1) * sizeof(void*));
      if (queryResult->rows[0] == NULL) {
        LOG_MALLOC_FAILURE();
        goto rowMallocFailure;
      }
      
      for (i = 0; i < numFields; i++) {
        const char *fieldName = sqlite3_column_name(preparedStatement, i);
        if (bytesAddStr((Bytes*) &queryResult->rows[0][i], fieldName) == NULL) {
          LOG_MALLOC_FAILURE();
          goto fieldMallocFailure;
        }
      }
      queryResult->rows[1] = NULL;
      queryResult->numRows = 1;
    }
    
    row = dbGetNumRows(queryResult);
    check = (void***) realloc(
      queryResult->rows, (row + 2) * sizeof(void**));
    if (check == NULL) {
      LOG_MALLOC_FAILURE();
      goto rowMallocFailure;
    }
    queryResult->rows = check;
    queryResult->rows[row] = (void**) calloc(1, (numFields + 1) * sizeof(void*));
    if (queryResult->rows[row] == NULL) {
      LOG_MALLOC_FAILURE();
      goto rowMallocFailure;
    }
    
    for (i = 0; i < numFields; i++) {
      dataType = sqlite3_column_type(preparedStatement, i);
      if (dataType <= 0) {
        printLog(ERR, "ERROR:  Got data type %d for field %d.\n", dataType, i);
        printLog(ERR, "Query: \"%s\".\n", (char*) queryBytes);
      }
      if (queryResult->fieldTypes[i] == NULL) {
        // Uninitialized data type.
        queryResult->fieldTypes[i] = *sqliteTypes[dataType - 1];
      }
      
      if (dataType == SQLITE_INTEGER) {
        intData = sqlite3_column_int64(preparedStatement, i);
        queryResult->rows[row][i] = typeI64->copy(&intData);
      } else if (dataType == SQLITE_FLOAT) {
        doubleData = sqlite3_column_double(preparedStatement, i);
        queryResult->rows[row][i] = typeDouble->copy(&doubleData);
      } else if (dataType == SQLITE_TEXT) {
        stringData = sqlite3_column_text(preparedStatement, i);
        if ((stringData != (unsigned char*) SQLITE_NULL)
          && (stringData != NULL)
        ) {
          bytesAddStr((Bytes*) &queryResult->rows[row][i],
            (const char*) stringData);
        } else {
          queryResult->rows[row][i] = NULL;
          bytesAllocate((Bytes*) &queryResult->rows[row][i], 0);
        }
      } else if (dataType == SQLITE_BLOB) {
        bytesData = sqlite3_column_blob(preparedStatement, i);
        if ((bytesData != (Bytes) SQLITE_NULL) && (bytesData != NULL)) {
          bytesDataLength = sqlite3_column_bytes(preparedStatement, i);
          queryResult->rows[row][i] = NULL;
          bytesAddData((Bytes*) &queryResult->rows[row][i],
            bytesData, bytesDataLength);
        } else {
          queryResult->rows[row][i] = NULL;
          bytesAllocate((Bytes*) &queryResult->rows[row][i], 0);
        }
      } else if (dataType == SQLITE_NULL) {
        queryResult->rows[row][i] = NULL;
        bytesAllocate((Bytes*) &queryResult->rows[row][i], 0);
      }
    }
    
    queryResult->rows[row + 1] = NULL;
    queryResult->numRows++;
    
    // Get the next step.
    rc = sqlite3_step(preparedStatement);
  }
  
  if (rc == SQLITE_DONE) {
    queryResult->successful = true;
    if (dbGetNumRows(queryResult) > 0) {
      queryResult->numResults = dbGetNumRows(queryResult) - 1;
    }
  } else {
    printLog(ERR, "sqlite3_step failed.\n");
    sqliteErrorMessage = sqlite3_errstr(rc);
    // DO NOT FREE sqliteErrorMessage!!!
    printLog(ERR, "SQLite query error(%d): %s\n", rc, sqliteErrorMessage);
    printLog(ERR, "query: \"%s\"\n", queryBytes);
    printLog(ERR, "queryTail: \"%s\"\n", queryTail);
    printLog(ERR, "bytesLength(queryBytes) = %llu\n", llu(bytesLength(queryBytes)));
    queryResult->successful = false;
  }
  
  rc = sqlite3_finalize(preparedStatement);
  preparedStatement = NULL;
  if (rc != SQLITE_OK) {
    printLog(ERR, "sqlite3_finalize failed.\n");
    sqliteErrorMessage = sqlite3_errstr(rc);
    // DO NOT FREE sqliteErrorMessage!!!
    printLog(ERR, "SQLite query error(%d): %s\n", rc, sqliteErrorMessage);
    printLog(ERR, "query: \"%s\"\n", queryBytes);
    printLog(ERR, "queryTail: \"%s\"\n", queryTail);
    printLog(ERR, "bytesLength(queryBytes) = %llu\n", llu(bytesLength(queryBytes)));
    queryResult->successful = false;
  }
  
  printLog(FLOOD,
    "EXIT sqliteExecQueryBytes(database=%p, queryBytes=\"%s\") = {%llu results}",
    database, queryBytes, llu(dbGetNumResults(queryResult)));
  return queryResult;
  
fieldMallocFailure:
  // The ith malloc failed.  Free everything up to that.
  for (j = 0; j < i; j++) {
    queryResult->rows[row][j]
      = queryResult->fieldTypes[j]->destroy(queryResult->rows[row][j]);
  }
  queryResult->rows[row] = (void**) pointerDestroy(queryResult->rows[row]);
  
rowMallocFailure:
  numRows = (int) dbGetNumRows(queryResult);
  numFields = (int) dbGetNumFields(queryResult);
  for (i = 0; i < numRows; i++) {
    for (j = 0; j < numFields; j++) {
      queryResult->rows[row][j]
        = queryResult->fieldTypes[j]->destroy(queryResult->rows[row][j]);
    }
    queryResult->rows[i] = (void**) pointerDestroy(queryResult->rows[i]);
  }
  queryResult->rows = (void***) pointerDestroy(queryResult->rows);
  queryResult->numRows = 0;
  
rowsMallocFailure:
  queryResult->fieldTypes
    = (TypeDescriptor**) pointerDestroy(queryResult->fieldTypes);
  
fieldTypesMallocFailure:
  queryResult->numFields = 0;
  sqlite3_finalize(preparedStatement);
  // dbQuerySuccessful(queryResult) is false.
  
  printLog(FLOOD,
    "EXIT sqliteExecQueryBytes(database=%p, queryBytes=\"%s\") = {%llu results}",
    database, queryBytes, llu(dbGetNumResults(queryResult)));
  return queryResult;
}

/// @fn DbResult* sqliteGetDatabaseNames(void *db)
///
/// @brief Get the names of the databases managed by the SQL system.
///
/// @param db A pointer to the SqlDatabase object representing the database
///   system to query, cast to a void*.
///
/// @return Returns a DbResult object with the names of the databases.
DbResult* sqliteGetDatabaseNames(void *db) {
  SqlDatabase *database = (SqlDatabase*) db;
  
  printLog(TRACE,
    "ENTER sqliteGetDatabaseNames(database=%p))\n",
    database);
  
  DbResult *queryResult
    = sqliteExecQueryString(database->connection,
      "pragma database_list;");
  if ((dbQuerySuccessful(queryResult) == true) && (dbGetNumFields(queryResult) == 3)) {
    // Format of results:
    // seq, name, file
    // We're only interested in column 2.  We could create a new DbResult and
    // copy, but this is inefficient, especially since we have everything we
    // want, we just have more than we want.  A better way is to just shrink
    // what we already have.
    // Header row (row 0) is always an array of Bytes objects.
    queryResult->rows[0][0] = (void*) bytesDestroy((Bytes) queryResult->rows[0][0]);
    queryResult->rows[0][0] = queryResult->rows[0][1];
    queryResult->rows[0][2] = (void*) bytesDestroy((Bytes) queryResult->rows[0][2]);
    queryResult->rows[0] = (void**) realloc(queryResult->rows[0], sizeof(void*));
    for (u64 i = 1; i < dbGetNumRows(queryResult); i++) {
      queryResult->rows[i][0] = queryResult->fieldTypes[0]->destroy(queryResult->rows[i][0]);
      queryResult->rows[i][0] = queryResult->rows[i][1];
      // All strings are encoded as Bytes objects, so destroy them properly.
      queryResult->rows[i][2] = (void*) bytesDestroy((Bytes) queryResult->rows[i][2]);
      // There should be no reason for this to fail since we're going from
      // three indexes to one:
      queryResult->rows[i] = (void**) realloc(queryResult->rows[i], sizeof(void*));
    }
    queryResult->numFields = 1;
    queryResult->fieldTypes[0] = typeBytes;
  }
  
  printLog(TRACE,
    "EXIT sqliteGetDatabaseNames(database=%p) = {%s}\n",
    database, (dbQuerySuccessful(queryResult)) ? "successful" : "NOT successful");
  return queryResult;
}

/// @fn DbResult* sqliteGetTableNames(void *db, const char *dbString)
///
/// @brief Get the names of the tables in a database.
///
/// @param db A pointer to the SqlDatabase object representing the database
///   system to query, cast to a void*.
/// @param dbString The name of the database to query.
///
/// @return Returns a DbResult object with the names of the tables.
DbResult* sqliteGetTableNames(void *db, const char *dbString) {
  SqlDatabase *database = (SqlDatabase*) db;
  
  char *dbName = NULL;
  straddstr(&dbName, dbString);
  straddstr(&dbName, dbInstance);
  
  printLog(TRACE,
    "ENTER sqliteGetTableNames(database=%p, dbName=\"%s\")\n",
    database, dbName);
  
  char *query = NULL;
  DbResult *queryResult = NULL;
  
  if (asprintf(&query,
    "select name from %s.sqlite_schema where type='table';", dbName) <= 0
  ) {
    LOG_MALLOC_FAILURE();
    printLog(TRACE,
      "EXIT sqliteGetTableNames(database=%p, dbName=\"%s\") = {0 results}\n",
      database, dbName);
    dbName = stringDestroy(dbName);
    return queryResult;
  }
  queryResult = sqliteExecQueryString(database->connection, query);
  query = stringDestroy(query);
  
  printLog(TRACE,
    "EXIT sqliteGetTableNames(database=%p, dbName=\"%s\") = {%llu results}\n",
    database, dbName, llu(dbGetNumRows(queryResult)));
  dbName = stringDestroy(dbName);
  return queryResult;
}

/// @fn bool sqliteAddDatabase(void *db, const char *dbString)
///
/// @brief Add a new database to a SQLite instance.
///
/// @param db A pointer to the SqlDatabase object representing the database
///   system to add the database to, cast to a void*.
/// @param dbString The name of the database to add.
///
/// @return Returns true on success, false on failure.
bool sqliteAddDatabase(void *db, const char *dbString) {
  SqlDatabase *database = (SqlDatabase*) db;
  
  char *dbName = NULL;
  straddstr(&dbName, dbString);
  straddstr(&dbName, dbInstance);
  
  printLog(TRACE,
    "ENTER sqliteAddDatabase(database=%p, dbName=\"%s\")\n",
    database, dbName);
  
  char *query = NULL;
  DbResult *queryResult = NULL;
  Sqlite *sqlite = (Sqlite*) database->connection;
  Bytes databasePath = NULL;
  bytesAddBytes(&databasePath, sqlite->databasePath);
  bytesAddStr(&databasePath, "/");
  bytesAddStr(&databasePath, dbName);
  Bytes databasePathLiteral = sqliteMakeBytesLiteral(databasePath);
  
  if (asprintf(&query, "attach database '%s' as %s;",
    (char*) databasePath, dbName) <= 0
  ) {
    printLog(TRACE,
      "EXIT sqliteAddDatabase(database=%p, dbName=\"%s\") = {NOT successful}\n",
      database, dbName);
    dbName = stringDestroy(dbName);
    return false;
  }
  queryResult = sqliteExecQueryString(sqlite, query);
  query = stringDestroy(query);
  bool returnValue = dbQuerySuccessful(queryResult);
  queryResult = dbFreeResult(queryResult);
  if (returnValue == true) {
    if (asprintf(&query, "insert into main%s.Databases (name, type, connection) "
        "values('%s', 'sqlite', %s);", (dbInstance == NULL) ? "" : dbInstance,
        dbString, str(databasePathLiteral)) <= 0
    ) {
      printLog(TRACE,
        "EXIT sqliteAddDatabase(database=%p, dbName=\"%s\") = {NOT successful}\n",
        database, dbName);
      databasePathLiteral = bytesDestroy(databasePathLiteral);
      dbName = stringDestroy(dbName);
      return false;
    }
    queryResult = sqliteExecQueryString(sqlite, query);
    query = stringDestroy(query);
    if (!dbQuerySuccessful(queryResult)) {
      returnValue = false;
      printLog(ERR, "Could not add database \"%s\" to main.Databases table.\n",
        dbName);
    }
    queryResult = dbFreeResult(queryResult);
  } else {
    printLog(ERR,
      "Could not attach database \"%s\" to main database connection.\n",
      dbName);
  }
  databasePathLiteral = bytesDestroy(databasePathLiteral);
  databasePath = bytesDestroy(databasePath);
  
  printLog(TRACE,
    "EXIT sqliteAddDatabase(database=%p, dbName=\"%s\") = {%s}\n",
    database, dbName, (returnValue) ? "successful" : "NOT successful");
  dbName = stringDestroy(dbName);
  return returnValue;
}

/// @fn bool sqliteDeleteDatabase(void *db, const char *dbString)
///
/// @brief Add a new database to a SQLite instance.
///
/// @param db A pointer to the SqlDatabase object representing the database
///   system to add the database to, cast to a void*.
/// @param dbString The name of the database to add.
///
/// @return Returns true on success, false on failure.
bool sqliteDeleteDatabase(void *db, const char *dbString) {
  SqlDatabase *database = (SqlDatabase*) db;
  
  char *dbName = NULL;
  straddstr(&dbName, dbString);
  straddstr(&dbName, dbInstance);
  
  printLog(TRACE,
    "ENTER sqliteDeleteDatabase(database=%p, dbName=\"%s\")\n",
    database, dbName);
  
  // These commands are best-effort, so not checking return values.
  char *query = NULL;
  DbResult *queryResult = NULL;
  Sqlite *sqlite = (Sqlite*) database->connection;
  Bytes databasePath = NULL;
  bytesAddBytes(&databasePath, sqlite->databasePath);
  bytesAddStr(&databasePath, "/");
  bytesAddStr(&databasePath, dbName);
  
  if (asprintf(&query, "detach database %s;", dbName) <= 0) {
    LOG_MALLOC_FAILURE();
    printLog(TRACE,
      "EXIT sqliteDeleteDatabase(database=%p, dbName=\"%s\") = {NOT successful}\n",
      database, dbName);
    dbName = stringDestroy(dbName);
    databasePath = bytesDestroy(databasePath);
    return false;
  }
  queryResult = sqliteExecQueryString(sqlite, query);
  query = stringDestroy(query);
  bool returnValue = dbQuerySuccessful(queryResult);
  queryResult = dbFreeResult(queryResult);
  
  // name is the primary key of the database, so only need to specify that.
  if (asprintf(&query, "delete from main.Databases where name='%s';",
    dbString) <= 0
  ) {
    LOG_MALLOC_FAILURE();
    printLog(TRACE,
      "EXIT sqliteDeleteDatabase(database=%p, dbName=\"%s\") = {NOT successful}\n",
      database, dbName);
    dbName = stringDestroy(dbName);
    databasePath = bytesDestroy(databasePath);
    return false;
  }
  queryResult = sqliteExecQueryString(sqlite, query);
  query = stringDestroy(query);
  returnValue &= dbQuerySuccessful(queryResult);
  queryResult = dbFreeResult(queryResult);
  
  remove((char*) databasePath);
  databasePath = bytesDestroy(databasePath);
  
  printLog(TRACE,
    "EXIT sqliteDeleteDatabase(database=%p, dbName=\"%s\") = {%s}\n",
    database, dbName, (returnValue) ? "successful" : "NOT successful");
  dbName = stringDestroy(dbName);
  return returnValue;
}

/// @fn Bytes sqliteMakeBytesLiteral(const Bytes input)
///
/// @brief Converts a Bytes object with unwanted characters to its escaped
///   equivalent.
///
/// @param input The Bytes object to convert.
///
/// @return Returns a newly-allocated Bytes object that has been escaped on
/// success, NULL on failure.
Bytes sqliteMakeBytesLiteral(const Bytes input) {
  Bytes returnValue = NULL;
  
  printLog(FLOOD, "ENTER sqliteMakeBytesLiteral(input=\"%s\")\n",
    (input != NULL) ? (char*) input : "");
  
  if (input != NULL) {
    // Query generator functions use single quotes for values, so use double
    // quotes for this literal.
    u64 length = bytesLength(input);
    // Length is exactly 2x the input length plus two ' characters and an x.
    bytesAllocate(&returnValue, (length << 1) + 3);
    bytesAddStr(&returnValue, "x'");
    ZEROINIT(Byte escapedBuffer[sizeof(BytesHeader) + 3]);
    Bytes escapedValue = escapedBuffer + sizeof(BytesHeader);
    escapedValue[2] = '\0'; // To be compliant with a true Bytes object.
    bytesSetLength(escapedValue, 2);
    for (u64 i = 0; i < length ; i++) {
      charToHex(input[i], false, str(escapedValue));
      bytesAddBytes(&returnValue, escapedValue);
    }
    bytesAddStr(&returnValue, "'");
  } else {
    bytesAddStr(&returnValue, "NULL");
  }
  
  printLog(FLOOD, "EXIT sqliteMakeBytesLiteral(input=\"%s\") = {%s}\n",
    (input != NULL) ? (char*) input : "", returnValue);
  return returnValue;
}

/// @fn Bytes sqliteMakeStringLiteral(const char *input)
///
/// @brief Converts a stirng with unwanted characters to its escaped equivalent.
///
/// @param input The string to convert.
///
/// @return Returns a newly-allocated Bytes object that has been escaped on
/// success, NULL on failure.
Bytes sqliteMakeStringLiteral(const char *input) {
  Bytes returnValue = NULL;
  
  printLog(FLOOD, "ENTER sqliteMakeStringLiteral(input=\"%s\")\n",
    (input != NULL) ? input : "");
  
  if (input != NULL) {
    u64 length = strlen(input);
    // Minimum length is the input length plus to ' characters.
    bytesAllocate(&returnValue, length + 2);
    bytesAddStr(&returnValue, "'");
    ZEROINIT(Byte escapedBuffer[sizeof(BytesHeader) + 2]);
    Bytes escapedValue = escapedBuffer + sizeof(BytesHeader);
    bytesSetLength(escapedValue, 1); // The default and expected case.
    for (u64 i = 0; i < length ; i++) {
      if (input[i] != '\'') {
        escapedValue[0] = input[i];
        // length is already set to 1
        bytesAddBytes(&returnValue, escapedValue);
      } else {
        escapedValue[0] = '\'';
        escapedValue[1] = '\''; // input[i];
        bytesSetLength(escapedValue, 2);
        bytesAddBytes(&returnValue, escapedValue);
        bytesSetLength(escapedValue, 1);
      }
    }
    bytesAddStr(&returnValue, "'");
  } else {
    bytesAddStr(&returnValue, "NULL");
  }
  
  printLog(FLOOD, "EXIT sqliteMakeStringLiteral(input=\"%s\") = {%s}\n",
    (input != NULL) ? input : "", (char*) returnValue);
  return returnValue;
}

/// @fn bool sqliteStartTransaction(void *db)
///
/// @brief Start a transaction for this thread in the database.
///
/// @note Transactions are managed by connection on the SQL server side and
/// connections are managed by thread on our side, so this transaction will
/// only apply to this thread.
///
/// @param db A pointer to the SqlDatabase object representing the database
///   system to query, cast to a void*.
///
/// @return Returns true if starting the transaction was successful, false
/// if not.
bool sqliteStartTransaction(void *db) {
  SqlDatabase *database = (SqlDatabase*) db;
  
  printLog(TRACE, "ENTER sqliteStartTransaction(database=%p)\n", database);
  
  if (database == NULL) {
    // Nothing to do.
    printLog(TRACE,
      "EXIT sqliteStartTransaction(database=%p) = {NOT successful}\n",
      database);
    return false;
  }
  Sqlite *sqlite = (Sqlite*) database->connection;
  if (sqlite == NULL) {
    // Nothing to do.
    printLog(TRACE,
      "EXIT sqliteStartTransaction(database=%p) = {NOT successful}\n",
      database);
    return false;
  }
  
  mtx_lock(&sqlite->transactionMutex);
  
  bool startSuccessful = true;
  // We can only start a transaction if one isn't already in progress.
  if (sqlite->transactionCount == 0) {
    DbResult *queryResult
      = database->stringQuery(database->connection, "begin transaction;");
    startSuccessful = dbQuerySuccessful(queryResult);
    queryResult = dbFreeResult(queryResult);
  }
  sqlite->transactionCount++;
  
  mtx_unlock(&sqlite->transactionMutex);
  
  printLog(TRACE, "EXIT sqliteStartTransaction(database=%p) = {%s}\n",
    database, (startSuccessful) ? "successful" : "NOT successful");
  return startSuccessful;
}

/// @fn bool sqliteCommitTransaction(void *db)
///
/// @brief Commit an in-progress transaction for this thread in the database.
///
/// @note Transactions are managed by connection on the SQL server side and
/// connections are managed by thread on our side, so this commit will
/// only apply to transactions in progress on this thread.
///
/// @param db A pointer to the SqlDatabase object representing the database
///   system to query, cast to a void*.
///
/// @return Returns true if committing the transaction was successful, false
/// if not.
bool sqliteCommitTransaction(void *db) {
  SqlDatabase *database = (SqlDatabase*) db;
  
  printLog(TRACE, "ENTER sqliteCommitTransaction(database=%p)\n", database);
  
  if (database == NULL) {
    // Nothing to do.
    printLog(TRACE,
      "EXIT sqliteCommitTransaction(database=%p) = {NOT successful}\n",
      database);
    return false;
  }
  Sqlite *sqlite = (Sqlite*) database->connection;
  if (sqlite == NULL) {
    // Nothing to do.
    printLog(TRACE,
      "EXIT sqliteCommitTransaction(database=%p) = {NOT successful}\n",
      database);
    return false;
  }
  
  mtx_lock(&sqlite->transactionMutex);
  if (sqlite->transactionCount == 0) {
    mtx_unlock(&sqlite->transactionMutex);
    printLog(ERR, "No transaction in progress.  Cannot commit.\n");
    printLog(TRACE,
      "EXIT sqliteCommitTransaction(database=%p) = {NOT successful}\n",
      database);
    return false;
  } else if (sqlite->transactionCount > 1) {
    // We don't get to decide what to do here.  Decrement the count and move on.
    // Report success in this case.
    sqlite->transactionCount--;
    mtx_unlock(&sqlite->transactionMutex);
    printLog(TRACE,
      "EXIT sqliteCommitTransaction(database=%p) = {successful}\n",
      database);
    return true;
  }
  
  // sqlite->transactionCount == 1
  DbResult *queryResult
    = database->stringQuery(database->connection, "commit;");
  bool querySuccessful = dbQuerySuccessful(queryResult);
  queryResult = dbFreeResult(queryResult);
  sqlite->transactionCount--; // This sets transactionCount to 0 in this case.
  mtx_unlock(&sqlite->transactionMutex);
  
  printLog(TRACE, "EXIT sqliteCommitTransaction(database=%p) = {%s}\n",
    database, (querySuccessful) ? "successful" : "NOT successful");
  return querySuccessful;
}

/// @fn bool sqliteRollbackTransaction(void *db)
///
/// @brief Rollback an in-progress transaction for this thread in the database.
///
/// @note Transactions are managed by connection on the SQL server side and
/// connections are managed by threads on our side, so this rollback will
/// only apply to transactions in progress on this thread.
///
/// @param db A pointer to the SqlDatabase object representing the database
///   system to query, cast to a void*.
///
/// @return Returns true if rolling back the transaction was successful, false
/// if not.
bool sqliteRollbackTransaction(void *db) {
  SqlDatabase *database = (SqlDatabase*) db;
  
  printLog(TRACE, "ENTER sqliteRollbackTransaction(database=%p)\n", database);
  
  if (database == NULL) {
    // Nothing to do.
    printLog(TRACE,
      "EXIT sqliteRollbackTransaction(database=%p) = {NOT successful}\n",
      database);
    return false;
  }
  Sqlite *sqlite = (Sqlite*) database->connection;
  if (sqlite == NULL) {
    // Nothing to do.
    printLog(TRACE,
      "EXIT sqliteRollbackTransaction(database=%p) = {NOT successful}\n",
      database);
    return false;
  }
  
  mtx_lock(&sqlite->transactionMutex);
  if (sqlite->transactionCount == 0) {
    mtx_unlock(&sqlite->transactionMutex);
    printLog(ERR, "No transaction in progress.  Cannot rollback.\n");
    printLog(TRACE,
      "EXIT sqliteRollbackTransaction(database=%p) = {NOT successful}\n",
      database);
    return false;
  } else if (sqlite->transactionCount > 1) {
    // We don't get to decide what to do here.  Decrement the count and move on.
    // Report success in this case.
    sqlite->transactionCount--;
    mtx_unlock(&sqlite->transactionMutex);
    printLog(TRACE,
      "EXIT sqliteRollbackTransaction(database=%p) = {successful}\n",
      database);
    return true;
  }
  
  // sqlite->transactionCount == 1
  DbResult *queryResult
    = database->stringQuery(database->connection, "rollback;");
  bool querySuccessful = dbQuerySuccessful(queryResult);
  queryResult = dbFreeResult(queryResult);
  sqlite->transactionCount--; // This sets transactionCount to 0 in this case.
  mtx_unlock(&sqlite->transactionMutex);
  
  printLog(TRACE, "EXIT sqliteRollbackTransaction(database=%p) = {%s}\n",
    database, (querySuccessful) ? "successful" : "NOT successful");
  return querySuccessful;
}

/// @fn void* sqliteDisconnect(void *db)
///
/// @brief Disconnect from a MariaDB connection.
///
/// @param db A pointer to a SqlDatabase object that hosts a pointer to
///   a Sqlite object, cast to a void*.
///
/// @return Returns NULL on success, the original unmodified sqlDatabase pointer
/// on failure.
void* sqliteDisconnect(void *db) {
  SqlDatabase *sqlDatabase = (SqlDatabase*) db;
  
  printLog(TRACE, "ENTER sqliteDisconnect(sqlDatabase=%p)\n", sqlDatabase);
  
  if (sqlDatabase == NULL) {
    // No-op.
    printLog(TRACE,
      "EXIT sqliteDisconnect(sqlDatabase=%p) = {successful}\n",
      sqlDatabase);
    return sqlDatabase;
  } else if (sqlDatabase->sqlDbType != SQLITE) {
    printLog(ERR,
      "sqlDatabase->connection member is not a SQLite connection.\n");
    printLog(TRACE,
      "EXIT sqliteDisconnect(sqlDatabase=%p) = {NOT successful}\n",
      sqlDatabase);
    return sqlDatabase;
  }
  
  Sqlite *sqlite = (Sqlite*) sqlDatabase->connection;
  if (sqlite != NULL) {
    // Disconnect the databases.
    DbResult *queryResult = sqliteGetDatabaseNames(sqlDatabase);
    Bytes query = NULL;
    for (u64 i = 0; i < dbGetNumResults(queryResult); i++) {
      const char *dbName = dbGetStringByName(queryResult, i, "name");
      
      // Optimize the database before detaching it.
      if (abprintf(&query,
        "PRAGMA analysis_limit=1000; PRAGMA %s.optimize", dbName
        ) <= 0
      ) {
        LOG_MALLOC_FAILURE();
        break;
      }
      DbResult *optimizeResult = sqliteExecQueryBytes(sqlite, query);
      if (!dbQuerySuccessful(optimizeResult)) {
        printLog(WARN, "Could not optimize database \"%s\".\n", dbName);
      }
      optimizeResult = dbFreeResult(optimizeResult);
      query = bytesDestroy(query);
      
      // Detach the database (if it's not main).
      if (strcmp(dbName, "main") == 0) {
        // Don't detach main.
        continue;
      }
      if (abprintf(&query, "detach database %s;", dbName) <= 0) {
        LOG_MALLOC_FAILURE();
        break;
      }
      DbResult *detachResult = sqliteExecQueryBytes(sqlite, query);
      if (!dbQuerySuccessful(detachResult)) {
        printLog(ERR, "Could not detach database \"%s\".\n", dbName);
        printLog(TRACE,
          "EXIT sqliteDisconnect(sqlDatabase=%p) = {NOT successful}\n",
          sqlDatabase);
        return sqlDatabase;
      }
      detachResult = dbFreeResult(detachResult);
      query = bytesDestroy(query);
    }
    queryResult = dbFreeResult(queryResult);
    
    // Free the Sqlite object.
    int rc = sqlite3_close(sqlite->db);
    if (rc != SQLITE_OK) {
      printLog(ERR,
        "Cannot close SQLite database: %s\n",
        sqlite3_errmsg(sqlite->db));
      printLog(TRACE,
        "EXIT sqliteDisconnect(sqlDatabase=%p) = {NOT successful}\n",
        sqlDatabase);
      return sqlDatabase;
    }
    sqlite->databasePath = bytesDestroy(sqlite->databasePath);
    mtx_destroy(&sqlite->transactionMutex);
    // Closing the SQLite database above would have effectively rolled back any
    // transactions we had in progress, so all table locks are already void.
    // Just destroy the dictionary.
    sqlite->lockedTables = dictionaryDestroy(sqlite->lockedTables);
    sqlite = (Sqlite*) pointerDestroy(sqlite);
  }
  
  sqlDatabase->tableDescriptions = htDestroy(sqlDatabase->tableDescriptions);
  
  // Free the SqlDatabase object.
  sqlDatabase = (SqlDatabase*) pointerDestroy(sqlDatabase);
  
  printLog(TRACE,
    "EXIT sqliteDisconnect(sqlDatabase=%p) = {successful}\n",
    sqlDatabase);
  return sqlDatabase;
}

/// @fn DbResult* sqliteDescribeTable(void *connection, const char *dbString, const char *tableName)
///
/// @brief Describe a table in a SQLite database.
///
/// @param connection A pointer to the Sqlite object representing the database
///   system to query, cast to a void*.
/// @param dbString The name of the database to query.
/// @param tableName The name of the table to describe.
///
/// @return Returns a DbResult object with the names of the table fields and
/// their types.
DbResult* sqliteDescribeTable(void *connection, const char *dbString,
  const char *tableName
) {
  Sqlite *database = (Sqlite*) connection;
  printLog(TRACE,
    "ENTER sqliteDescribeTable(database=%p, dbString=%s, tableName=%s)\n",
    database, dbString, tableName);
  
  char *dbName = NULL;
  straddstr(&dbName, dbString);
  straddstr(&dbName, dbInstance);
  Bytes query = NULL;
  abprintf(&query, "pragma %s.table_info(%s);", dbName, tableName);
  dbName = stringDestroy(dbName);
  DbResult *queryResult = sqliteExecQueryBytes(database, query);
  query = bytesDestroy(query);
  
  DbResult *returnValue = (DbResult*) calloc(1, sizeof(DbResult));
  if (returnValue == NULL) {
    LOG_MALLOC_FAILURE();
    return NULL;
  }
  if (dbQuerySuccessful(queryResult) == true) {
    returnValue->successful = true;
    
    returnValue->numFields = 3;
    returnValue->fieldTypes
      = (TypeDescriptor**) calloc(1, 3 * sizeof(TypeDescriptor*));
    returnValue->fieldTypes[0] = typeBytes;
    returnValue->fieldTypes[1] = typeBytes;
    returnValue->fieldTypes[2] = typeBytes;
    
    returnValue->numRows = dbGetNumRows(queryResult);
    returnValue->numResults = dbGetNumResults(queryResult);
    if (dbGetNumRows(returnValue) > 0) {
      returnValue->rows
        = (void***) calloc(1, (dbGetNumRows(returnValue) + 1) * sizeof(void**));
      returnValue->rows[0]
        = (void**) calloc(1, dbGetNumFields(returnValue) * sizeof(void*));
      bytesAddStr((Bytes*) &returnValue->rows[0][0], "fieldName");
      bytesAddStr((Bytes*) &returnValue->rows[0][1], "typeInfo");
      bytesAddStr((Bytes*) &returnValue->rows[0][2], "primaryKey");
    }
    
    for (u64 row = 1; row < dbGetNumRows(returnValue); row++) {
      returnValue->rows[row]
        = (void**) calloc(1, dbGetNumFields(returnValue) * sizeof(void*));
      bytesAddBytes((Bytes*) &returnValue->rows[row][0],
        dbGetBytesByName(queryResult, row - 1, "name"));
      bytesAddBytes((Bytes*) &returnValue->rows[row][1],
        dbGetBytesByName(queryResult, row - 1, "type"));
      i64 *primaryKey
        = (i64*) dbGetResultByName(queryResult, row - 1, "pk", NULL);
      if (*primaryKey == 0) {
        // The case for most fields.
        bytesAddStr((Bytes*) &returnValue->rows[row][2], "false");
      } else {
        // Primary key.
        bytesAddStr((Bytes*) &returnValue->rows[row][2], "true");
      }
    }
  }
  queryResult = dbFreeResult(queryResult);
  
  printLog(TRACE,
    "EXIT sqliteDescribeTable(database=%p, dbString=%s, tableName=%s) = {%s}\n",
    database, dbString, tableName,
    (dbQuerySuccessful(returnValue) == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn bool sqliteChangeFieldType(void *db, const char *dbName, const char *tableName, const char *fieldName, const void *type)
///
/// @brief Add a new field to an existing table in a database.
///
/// @param database A pointer to the SqlDatabase object representing the database
///   system to query.
/// @param dbString The name of the database in the SqlDatabase to address.
/// @param tableName The name of the table to add a field to.
/// @param fieldName The name of the new field to change.
/// @param type A TypeDescriptor defining the new type of the field
///   -OR- a character pointer holding the number of characters for the field.
///
/// @return Returns true if changing the type of the field was successful,
/// false if not.
bool sqliteChangeFieldType(void *db, const char *dbName,
  const char *tableName, const char *fieldName, const void *type
) {
  SqlDatabase *database = (SqlDatabase*) db;
  
  printLog(TRACE,
    "ENTER sqliteChangeFieldType(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "fieldName=\"%s\", type=%p)\n",
    database, dbName, tableName, fieldName, type);
  
  bool successful = false;
  
  if ((database == NULL) || (dbName == NULL) || (tableName == NULL)
    || (fieldName == NULL) || (type == NULL)
  ) {
    printLog(ERR, "One or more invalid parameters.\n");
    printLog(TRACE,
      "EXIT sqliteChangeFieldType(database=%p, dbName=\"%s\", "
      "tableName=\"%s\", fieldName=\"%s\", type=%p) = {%s}\n",
      database, dbName, tableName, fieldName, type,
      (successful) ? "successful" : "NOT successful");
    return successful; // false
  }
  
  if (!sqliteStartTransaction(database)) {
    printLog(ERR, "Could not start transaction for changing field type.\n");
    printLog(TRACE,
      "EXIT sqliteChangeFieldType(database=%p, dbName=\"%s\", "
      "tableName=\"%s\", fieldName=\"%s\", type=%p) = {%s}\n",
      database, dbName, tableName, fieldName, type,
      (successful) ? "successful" : "NOT successful");
    return successful; // false
  }
  
  DbResult *records = sqlGetRecords(database, dbName, tableName);
  if (dbQuerySuccessful(records) == false) {
    printLog(ERR, "Could not get records from %s.%s.\n", dbName, tableName);
    records = dbFreeResult(records);
    if (!sqlRollbackTransaction(database)) {
      printLog(ERR,
        "Could not rollback transaction for changing field type.\n");
    }
    printLog(TRACE,
      "EXIT sqliteChangeFieldType(database=%p, dbName=\"%s\", "
      "tableName=\"%s\", fieldName=\"%s\", type=%p) = {%s}\n",
      database, dbName, tableName, fieldName, type,
      (successful) ? "successful" : "NOT successful");
    return successful; // false
  }
  
  bool fieldFound = false;
  const char **fieldNames = dbGetFieldNames(records);
  u64 fieldIndex = 0;
  for (; fieldIndex < dbGetNumFields(records); fieldIndex++) {
    if (strcmp(fieldNames[fieldIndex], fieldName) == 0) {
      records->fieldTypes[fieldIndex] = (TypeDescriptor*) type;
      fieldFound = true;
      break;
    }
  }
  if (fieldFound == false) {
    printLog(ERR, "Could not find field \"%s\" in %s.%s.\n",
      fieldName, dbName, tableName);
    records = dbFreeResult(records);
    if (!sqlRollbackTransaction(database)) {
      printLog(ERR,
        "Could not rollback transaction for changing field type.\n");
    }
    printLog(TRACE,
      "EXIT sqliteChangeFieldType(database=%p, dbName=\"%s\", "
      "tableName=\"%s\", fieldName=\"%s\", type=%p) = {%s}\n",
      database, dbName, tableName, fieldName, type,
      (successful) ? "successful" : "NOT successful");
    return successful; // false
  }
  
  List *tableList = listCreate(typeString);
  if (tableList == NULL) {
    LOG_MALLOC_FAILURE();
    records = dbFreeResult(records);
    if (!sqlRollbackTransaction(database)) {
      printLog(ERR,
        "Could not rollback transaction for changing field type.\n");
    }
    printLog(TRACE,
      "EXIT sqliteChangeFieldType(database=%p, dbName=\"%s\", "
      "tableName=\"%s\", fieldName=\"%s\", type=%p) = {%s}\n",
      database, dbName, tableName, fieldName, type,
      (successful) ? "successful" : "NOT successful");
    return successful; // false
  }
  
  for (u64 fieldIndex = 0; fieldIndex < dbGetNumFields(records); fieldIndex++) {
    if (listAddBackEntry(tableList, fieldNames[fieldIndex],
      records->fieldTypes[fieldIndex], typePointerNoCopy) == NULL
    ) {
      printLog(ERR, "Could not add field \"%s\" to new table.\n",
        fieldNames[fieldIndex]);
      records = dbFreeResult(records);
      if (!sqlRollbackTransaction(database)) {
        printLog(ERR,
          "Could not rollback transaction for changing field type.\n");
      }
      printLog(TRACE,
        "EXIT sqliteChangeFieldType(database=%p, dbName=\"%s\", "
        "tableName=\"%s\", fieldName=\"%s\", type=%p) = {%s}\n",
        database, dbName, tableName, fieldName, type,
        (successful) ? "successful" : "NOT successful");
      return successful; // false
    }
  }
  
  Bytes tempTableName = NULL;
  bytesAddStr(&tempTableName, "TEMP");
  bytesAddStr(&tempTableName, tableName);
  bytesAddStr(&tempTableName, "TEMP");
  
  Bytes primaryKey = NULL;
  DbResult *tableDescription = sqlDescribeTable(database, dbName, tableName);
  for (u64 i = 0; i < dbGetNumResults(tableDescription); i++) {
    if (strcmp(dbGetStringByName(tableDescription, i, "primaryKey"),
      boolNames[true]) == 0
    ) {
      if (primaryKey != NULL) {
        bytesAddStr(&primaryKey, ", ");
      }
      bytesAddStr(&primaryKey,
        dbGetStringByName(tableDescription, i, "fieldName"));
    }
  }
  tableDescription = dbFreeResult(tableDescription);
  
  if (sqlAddTableList(database, dbName, str(tempTableName), 
    strOrEmpty(primaryKey), tableList) == false
  ) {
    printLog(ERR, "Could not create new table %s.%s.\n", dbName, tableName);
    primaryKey = bytesDestroy(primaryKey);
    tempTableName = bytesDestroy(tempTableName);
    records = dbFreeResult(records);
    if (!sqlRollbackTransaction(database)) {
      printLog(ERR,
        "Could not rollback transaction for changing field type.\n");
    }
    printLog(TRACE,
      "EXIT sqliteChangeFieldType(database=%p, dbName=\"%s\", "
      "tableName=\"%s\", fieldName=\"%s\", type=%p) = {%s}\n",
      database, dbName, tableName, fieldName, type,
      (successful) ? "successful" : "NOT successful");
    return successful; // false
  }
  primaryKey = bytesDestroy(primaryKey);
  tableList = listDestroy(tableList);
  
  if (getIndexFromTypeDescriptor((TypeDescriptor*) type) < 0) {
    // Type is a varchar.  Set the type descriptor back to typeString so that
    // sqlAddRecords works correctly below.
    records->fieldTypes[fieldIndex] = typeString;
  }
  
  successful = sqlAddRecords(database, dbName, str(tempTableName), records);
  records = dbFreeResult(records);
  
  if (successful == true) {
    if (sqlDeleteTable(database, dbName, tableName) == false) {
      successful = false;
      tempTableName = bytesDestroy(tempTableName);
      printLog(ERR, "Could not delete old table %s.%s.\n", dbName, tableName);
      if (!sqlRollbackTransaction(database)) {
        printLog(ERR,
          "Could not rollback transaction for changing field type.\n");
      }
      printLog(TRACE,
        "EXIT sqliteChangeFieldType(database=%p, dbName=\"%s\", "
        "tableName=\"%s\", fieldName=\"%s\", type=%p) = {%s}\n",
        database, dbName, tableName, fieldName, type,
        (successful) ? "successful" : "NOT successful");
      return successful; // false
    }
    
    if (!sqliteRenameTable(database, dbName, str(tempTableName), tableName)) {
      successful = false;
      printLog(ERR, "Could not rename temporary table %s.%s to %s.%s.\n",
        dbName, str(tempTableName), dbName, tableName);
      tempTableName = bytesDestroy(tempTableName);
      if (!sqlRollbackTransaction(database)) {
        printLog(ERR,
          "Could not rollback transaction for changing field type.\n");
      }
      printLog(TRACE,
        "EXIT sqliteChangeFieldType(database=%p, dbName=\"%s\", "
        "tableName=\"%s\", fieldName=\"%s\", type=%p) = {%s}\n",
        database, dbName, tableName, fieldName, type,
        (successful) ? "successful" : "NOT successful");
      return successful; // false
    }
    
    if (!sqliteCommitTransaction(database)) {
      printLog(ERR, "Could not commit transaction for changing field type.\n");
      if (!sqlRollbackTransaction(database)) {
        printLog(ERR,
          "Could not rollback transaction for changing field type.\n");
      }
      successful = false;
    }
  } else {
    if (!sqlRollbackTransaction(database)) {
      printLog(ERR,
        "Could not rollback transaction for changing field type.\n");
    }
  }
  tempTableName = bytesDestroy(tempTableName);
  
  // Invalidate the cache.
  sqlInvalidateTableDescription(database, dbName, tableName);
  
  printLog(TRACE,
    "EXIT sqliteChangeFieldType(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "fieldName=\"%s\", type=%p) = {%s}\n",
    database, dbName, tableName, fieldName, type,
    (successful) ? "successful" : "NOT successful");
  return successful;
}

/// @fn bool sqliteLockTablesDict(void *db, const Dictionary *tablesToLock)
///
/// @brief Get a write lock in the database on the specified tables.
///
/// @param db A pointer to the SqlDatabase object representing the database
///   system to query, cast to a void*.
/// @param tablesToLock A Dictionary (HashTable) where the keys are the names of
///   the tables to lock.  Values are ignored.  This function does *NOT* take
///   ownership of this data structure.  The caller must destroy it.
///
/// @return Returns true if the tables were locked successfully, false if not.
bool sqliteLockTablesDict(void *db, const Dictionary *tablesToLock) {
  SqlDatabase *database = (SqlDatabase*) db;
  
  printLog(TRACE,
    "ENTER sqliteLockTablesDict(database=%p, tablesToLock=%p)\n",
    database, tablesToLock);
  
  // Argument check.
  if ((database == NULL) || (tablesToLock == NULL)) {
    // Can't work with a NULL argument.
    printLog(TRACE,
      "EXIT sqliteLockTablesDict(database=%p, tablesToLock=%p) "
      "= {NOT successful}\n", database, tablesToLock);
    return false;
  } else if (tablesToLock->size == 0) {
    // Not an error per se, but nothing to do.
    printLog(WARN,
      "Zero-length Dictionary provided to sqliteLockTablesDict.\n");
    printLog(TRACE,
      "EXIT sqliteLockTablesDict(database=%p, tablesToLock=%p) "
      "= {successful}\n", database, tablesToLock);
    return true;
  }
  Sqlite *sqlite = (Sqlite*) database->connection;
  if (sqlite == NULL) {
    printLog(ERR, "database->connection is NULL.\n");
    printLog(TRACE,
      "EXIT sqliteLockTablesDict(database=%p, tablesToLock=%p) "
      "= {NOT successful}\n", database, tablesToLock);
    return false;
  }
  
  u64 threadId = (u64) thrd_current();
  
  mtx_lock(&sqlite->transactionMutex);
  for (DictionaryEntry *cur = tablesToLock->head;
    cur != NULL;
    cur = cur->next
  ) {
    dictionaryAddEntry(sqlite->lockedTables, str(cur->key),
      &threadId, typeU64);
  }
  
  bool querySuccessful = true;
  if (sqlite->transactionCount == 0) {
    DbResult *queryResult
      = database->stringQuery(database->connection, "begin transaction;");
    querySuccessful = dbQuerySuccessful(queryResult);
    queryResult = dbFreeResult(queryResult);
  }
  if (!querySuccessful) {
    for (DictionaryEntry *cur = tablesToLock->head;
      cur != NULL;
      cur = cur->next
    ) {
      dictionaryRemove(sqlite->lockedTables, str(cur->key));
    }
    mtx_unlock(&sqlite->transactionMutex);
    printLog(TRACE,
      "EXIT sqliteLockTablesDict(database=%p, tablesToLock=%p) "
      "= {NOT successful}\n", database, tablesToLock);
    return false;
  }
  sqlite->transactionCount++;
  mtx_unlock(&sqlite->transactionMutex);
  
  printLog(TRACE,
    "EXIT sqliteLockTablesDict(database=%p, tablesToLock=%p) = {%s}\n",
    database, tablesToLock,
    (querySuccessful == true) ? "successful" : "NOT successful");
  return querySuccessful;
}

/// @fn bool sqliteUnlockTables(void *db, const Dictionary *tableLock)
///
/// @brief Unlock previously-locked tables and clear the cache of locked tables
/// for this thread.
///
/// @param db A pointer to the SqlDatabase object representing the
///   database system to query, cast to a void*.
/// @param tableLock A pointer to a Dictionary returned by a previous call to
///   one of the sqliteLockTables functions.
///
/// @return This function always returns NULL.
bool sqliteUnlockTables(void *db, const Dictionary *tableLock) {
  SqlDatabase *database = (SqlDatabase*) db;
  
  printLog(TRACE,
    "ENTER sqliteUnlockTables(database=%p, tableLock=%p)\n",
    database, tableLock);
  
  if ((database == NULL) || (tableLock == NULL)) {
    // Nothing to do.
    printLog(TRACE,
      "EXIT sqliteUnlockTables(database=%p, tableLock=%p) = {NOT successful}\n",
      database, tableLock);
    return false;
  }
  Sqlite *sqlite = (Sqlite*) database->connection;
  if (sqlite == NULL) {
    printLog(ERR, "database->connection is NULL.\n");
    printLog(TRACE,
      "EXIT sqliteUnlockTables(database=%p, tablesToLock=%p) "
      "= {unsuccessful}\n", database, tableLock);
    return false;
  }
  
  u64 threadId = (u64) thrd_current();
  
  mtx_lock(&sqlite->transactionMutex);
  if (sqlite->lockedTables->size == 0) {
    if (sqlite->transactionCount > 1) {
      sqlite->transactionCount--;
    }
    mtx_unlock(&sqlite->transactionMutex);
    printLog(WARN, "No tables locked.  Cannot unlock tables.\n");
    printLog(TRACE,
      "EXIT sqliteUnlockTables(database=%p, tableLock=%p) = {NOT successful}\n",
      database, tableLock);
    return false;
  }
  
  for (DictionaryEntry *cur = tableLock->head;
    cur != NULL;
    cur = cur->next
  ) {
    for (DictionaryEntry *innerCur
      = dictionaryGetEntry(sqlite->lockedTables, cur->key);
      innerCur != NULL;
    ) {
      DictionaryEntry *next = innerCur->next;
      if (*((u64*) innerCur->value) == threadId) {
        dictionaryDestroyNode(sqlite->lockedTables, innerCur);
      }
      innerCur = next;
    }
  }
  if (sqlite->lockedTables->size > 0) {
    // There are still tables locked.  Can't commit the transaction, but not a
    // failure.
    if (sqlite->transactionCount > 1) {
      sqlite->transactionCount--;
    }
    mtx_unlock(&sqlite->transactionMutex);
    printLog(TRACE,
      "EXIT sqliteUnlockTables(database=%p, tableLock=%p) = {successful}\n",
      database, tableLock);
    return true;
  }
  
  bool querySuccessful = sqliteCommitTransaction(database);
  mtx_unlock(&sqlite->transactionMutex);
  
  printLog(TRACE, "EXIT sqliteUnlockTables(database=%p, tableLock=%p) = {%s}\n",
    database, tableLock,
    (querySuccessful == true) ? "successful" : "NOT successful");
  return querySuccessful;
}

/// @fn bool sqliteAddField(void *db, const char *dbString, const char *tableName, const char *afterField, const char *newField, void *type)
///
/// @brief Add a new field to an existing table in a database.
///
/// @param database A pointer to the SqlDatabase object representing the database
///   system to query.
/// @param dbString The name of the database in the SqlDatabase to address.
/// @param tableName The name of the table to add a field to.
/// @param afterField The name of the field in the existing table to place the
///   new field after.  If this value is an empty string, the new field will be
///   added at the beginning of the table.
/// @param newField The name of the new field to add.
/// @param type A TypeDescriptor defining the type of the new field
///   -OR- a character pointer holding the number of characters for the field.
///
/// @return Returns true if adding the field to the table was successful,
/// false if not.
bool sqliteAddField(void *db, const char *dbString,
  const char *tableName, const char *afterField, const char *newField,
  void *type
) {
  SqlDatabase *database = (SqlDatabase*) db;
  
  printLog(TRACE,
    "ENTER sqliteAddField(database=%p, dbString=\"%s\", tableName=\"%s\", "
    "afterField=\"%s\", newField=\"%s\", type=%p)\n",
    database, dbString, tableName, afterField, newField, type);
  
  if ((database == NULL) || (dbString == NULL) || (tableName == NULL)
    || (afterField == NULL) || (newField == NULL) || (*newField == '\0')
    || (type == NULL)
  ) {
    // Nothing to do.
    printLog(TRACE,
      "EXIT sqliteAddField(database=%p, dbString=\"%s\", tableName=\"%s\", "
      "afterField=\"%s\", newField=\"%s\", type=%p) = {NOT successful}\n",
      database, dbString, tableName, afterField, newField, type);
    return false;
  }
  
  scopeBegin(MAX_SCOPE_VARS);
  // SQLite doesn't support adding new column names, so we need to add a new
  // table, add all the records, and then delete the old table and rename the
  // new table to the old name.
  
  char *typeName = NULL;
  scopeAdd(straddstr(&typeName,
    typeDescriptorToSqlTypeName((TypeDescriptor*) type)),
    pointerDestroyFunction);
  if (*typeName == '\0') {
    // type is non-NULL and not a type.  It is a string holding the number of
    // characters for the field.
    typeName = (char*) scopeDestroy(typeName);
    straddstr(&typeName, "VARCHAR(");
    straddstr(&typeName, (char*) type);
    scopeAdd(straddstr(&typeName, ")"));
  }
  
  Bytes dbName = NULL;
  bytesAddStr(&dbName, dbString);
  scopeAdd(bytesAddStr(&dbName, dbInstance), bytesDestroy);
  
  if (!sqliteStartTransaction(database)) {
    printLog(ERR, "Could not start transaction.  Cannot continue.\n");
    if (!sqliteRollbackTransaction(database)) {
      printLog(ERR, "Could not rollback database transaction.\n");
    }
    
    printLog(TRACE,
      "EXIT sqliteAddField(database=%p, dbString=\"%s\", tableName=\"%s\", "
      "afterField=\"%s\", newField=\"%s\", type=%p) = {NOT successful}\n",
      database, dbString, tableName, afterField, newField, type);
    scopeEnd();
    return false;
  }
  
  DbResult *existingTable = (DbResult*) scopeAdd(
    sqlGetRecords(database, str(dbString), tableName), dbFreeResult);
  if (dbQuerySuccessful(existingTable) == false) {
    printLog(ERR, "Could not get existing table %s.%s from the database.\n",
      dbString, tableName);
    if (!sqliteRollbackTransaction(database)) {
      printLog(ERR, "Could not rollback database transaction.\n");
    }
    
    printLog(TRACE,
      "EXIT sqliteAddField(database=%p, dbString=\"%s\", tableName=\"%s\", "
      "afterField=\"%s\", newField=\"%s\", type=%p) = {NOT successful}\n",
      database, dbString, tableName, afterField, newField, type);
    scopeEnd();
    return false;
  }
  
  // OK, we have a small problem with SQLite for this work.  In order to add a
  // table, we have to know its primary key.  SQLite does not report this
  // information when doing a table_info command, which is how we get our table
  // descriptions.  This is despite the fact that there is a column that is
  // supposed to hold this information in the output of the command.  The only
  // thing we have to go off of are the datatypes of the columns.  Anything with
  // a VARCHAR datatype must be part of a primary key because this is what we
  // use for MariaDB (because MariaDB can't use something like a generic TEXT
  // column as a key).  Go with this for now.  NOTE:  We can't use the results
  // of sqlDescribeTable because the output at that level has the type
  // descriptors instead of the raw column types and both TEXT and VARCHAR
  // columns resolve to typeString.
  //
  // FIXME:  If we ever figure out a way to properly record and manage the
  // primary keys of tables in SQLite, update this logic acccordingly.
  DbResult *fieldTypes = (DbResult*) scopeAdd(
    sqliteDescribeTable(database->connection, dbString, tableName),
    dbFreeResult);
  if (dbQuerySuccessful(fieldTypes) == false) {
    printLog(ERR, "Could not get field types for %s.%s from the database.\n",
      dbString, tableName);
    if (!sqliteRollbackTransaction(database)) {
      printLog(ERR, "Could not rollback database transaction.\n");
    }
    
    printLog(TRACE,
      "EXIT sqliteAddField(database=%p, dbString=\"%s\", tableName=\"%s\", "
      "afterField=\"%s\", newField=\"%s\", type=%p) = {NOT successful}\n",
      database, dbString, tableName, afterField, newField, type);
    scopeEnd();
    return false;
  }
  
  List *fields = (List*) scopeAdd(listCreate(typeString), listDestroy);
  
  if (*afterField == '\0') {
    listAddBackEntry(fields, newField, type, typePointerNoCopy);
  }
  
  char *primaryKey = NULL;
  for (u64 fieldTypeIndex = 0;
    fieldTypeIndex < dbGetNumResults(fieldTypes);
    fieldTypeIndex++
  ) {
    Bytes fieldName
      = dbGetBytesByName(fieldTypes, fieldTypeIndex, "fieldName");
    const char *typeInfo
      = dbGetStringByName(fieldTypes, fieldTypeIndex, "typeInfo");
    if (stringIsTrue(
        dbGetStringByName(fieldTypes, fieldTypeIndex, "primaryKey")
      )
    ) {
      if (primaryKey != NULL) {
        straddstr(&primaryKey, ", ");
      }
      straddstr(&primaryKey, str(fieldName));
    }
    
    if (stringStartsWithCi(typeInfo, "VARCHAR") == true) {
      // Add the field to the fields list.
      Bytes varcharLength = getBytesBetween(typeInfo, "(", ")");
      // Use typeBytesNoCopy so that we avoid an extra allocation and copy.
      ListNode *node
        = listAddBackEntry(fields, fieldName, varcharLength, typeBytesNoCopy);
      if (node == NULL) {
        // malloc failure.  Error was already logged as best as possible.  Bail.
        scopeEnd();
        return false;
      }
      // Set the type to typeBytes so that the list takes ownership and the
      // value is destroyed properly when the list is destroyed.
      node->type = typeBytes;
    } else {
      TypeDescriptor *typeDescriptor = sqlTypeNameToTypeDescriptor(typeInfo);
      if (!listAddBackEntry(fields, fieldName, typeDescriptor, typePointerNoCopy)) {
        // malloc failure.  Error was already logged as best as possible.  Bail.
        scopeEnd();
        return false;
      }
    }
    
    if (strcmp(str(fieldName), afterField) == 0) {
      if (!listAddBackEntry(fields, newField, type, typePointerNoCopy)) {
        // malloc failure.  Error was already logged as best as possible.  Bail.
        scopeEnd();
        return false;
      }
    }
  }
  scopeAdd(primaryKey, pointerDestroyFunction);
  
  char *tempTableName = NULL;
  straddstr(&tempTableName, "TEMP");
  straddstr(&tempTableName, tableName);
  straddstr(&tempTableName, "TEMP");
  scopeAdd(tempTableName, pointerDestroyFunction);
  
  if (!sqlAddTableList(database, dbString, tempTableName, primaryKey, fields)) {
    printLog(ERR, "Could not add new table to database.\n");
    if (!sqliteRollbackTransaction(database)) {
      printLog(ERR, "Could not rollback database transaction.\n");
    }
    
    printLog(TRACE,
      "EXIT sqliteAddField(database=%p, dbString=\"%s\", tableName=\"%s\", "
      "afterField=\"%s\", newField=\"%s\", type=%p) = {NOT successful}\n",
      database, dbString, tableName, afterField, newField, type);
    scopeEnd();
    return false;
  }
  
  // Extend the exiting results.
  const char **fieldNames = dbGetFieldNames(existingTable);
  u64 beforeFieldIndex = 0;
  if (*afterField != '\0') {
    for (u64 afterFieldIndex = 0;
      fieldNames[afterFieldIndex] != NULL;
      afterFieldIndex++
    ) {
      if (strcmp(fieldNames[afterFieldIndex], afterField) == 0) {
        beforeFieldIndex = afterFieldIndex + 1;
        break;
      }
    }
  }
  if (!bytesTableAddField((Bytes***) &existingTable->rows, beforeFieldIndex)) {
    printLog(ERR, "Could not extend existing table data in memory.\n");
    if (!sqliteRollbackTransaction(database)) {
      printLog(ERR, "Could not rollback database transaction.\n");
    }
    
    printLog(TRACE,
      "EXIT sqliteAddField(database=%p, dbString=\"%s\", tableName=\"%s\", "
      "afterField=\"%s\", newField=\"%s\", type=%p) = {NOT successful}\n",
      database, dbString, tableName, afterField, newField, type);
    scopeEnd();
    return false;
  }
  bytesAddStr((Bytes*) &existingTable->rows[0][beforeFieldIndex], newField);
  existingTable->numFields++;
  
  // Extend the existing field types.
  TypeDescriptor **check = (TypeDescriptor**) realloc(existingTable->fieldTypes,
    (dbGetNumFields(existingTable) + 1) * sizeof(TypeDescriptor*));
  if (check == NULL) {
    LOG_MALLOC_FAILURE();
    scopeEnd();
    return false;
  }
  existingTable->fieldTypes = check;
  // Move everything down.
  for (u64 fieldIndex = dbGetNumFields(existingTable);
    fieldIndex > beforeFieldIndex;
    fieldIndex--
  ) {
    existingTable->fieldTypes[fieldIndex]
      = existingTable->fieldTypes[fieldIndex - 1];
  }
  if (strncmp(typeName, "VARCHAR", 7) != 0) {
    // The usual case.  type is a TypeDescriptor pointer.
    existingTable->fieldTypes[beforeFieldIndex] = (TypeDescriptor*) type;
    
    if ((type != typeString) && (type != typeBytes)) {
      // Convert the new bytes field to a field of the correct type.
      for (u64 ii = 1; ii < dbGetNumRows(existingTable); ii++) {
        existingTable->rows[ii][beforeFieldIndex]
          = bytesDestroy((Bytes) existingTable->rows[ii][beforeFieldIndex]);
        existingTable->rows[ii][beforeFieldIndex]
          = existingTable->fieldTypes[beforeFieldIndex]->create(NULL);
      }
    }
  } else {
    // type was the number of characters in the VARCHAR.  Use typeStirng.
    existingTable->fieldTypes[beforeFieldIndex] = typeString;
  }
  // Extension of old data is now complete.
  
  // Use the newly-extended data from the old table to popultate the temp table.
  if (!sqlAddRecords(database, dbString, tempTableName, existingTable)) {
    printLog(ERR, "Could not add records from %s to %s.\n",
      tableName, tempTableName);
    if (!sqliteRollbackTransaction(database)) {
      printLog(ERR, "Could not rollback database transaction.\n");
    }
    
    printLog(TRACE,
      "EXIT sqliteAddField(database=%p, dbString=\"%s\", tableName=\"%s\", "
      "afterField=\"%s\", newField=\"%s\", type=%p) = {NOT successful}\n",
      database, dbString, tableName, afterField, newField, type);
    scopeEnd();
    return false;
  }
  
  // Delete the old table.
  if (!sqlDeleteTable(database, dbString, tableName)) {
    printLog(ERR, "Could not delete old table \"%s\".\n", tableName);
    if (!sqliteRollbackTransaction(database)) {
      printLog(ERR, "Could not rollback database transaction.\n");
    }
    
    printLog(TRACE,
      "EXIT sqliteAddField(database=%p, dbString=\"%s\", tableName=\"%s\", "
      "afterField=\"%s\", newField=\"%s\", type=%p) = {NOT successful}\n",
      database, dbString, tableName, afterField, newField, type);
    scopeEnd();
    return false;
  }
  
  // Rename the temporary table to the old table name.
  if (!sqliteRenameTable(database, dbString, tempTableName, tableName)) {
    printLog(ERR, "Could not rename table \"%s\" to \"%s\".\n",
      tempTableName, tableName);
    if (!sqliteRollbackTransaction(database)) {
      printLog(ERR, "Could not rollback database transaction.\n");
    }
    
    printLog(TRACE,
      "EXIT sqliteAddField(database=%p, dbString=\"%s\", tableName=\"%s\", "
      "afterField=\"%s\", newField=\"%s\", type=%p) = {NOT successful}\n",
      database, dbString, tableName, afterField, newField, type);
    scopeEnd();
    return false;
  }
  
  // Finally done!  Commit the transaction.
  if (!sqliteCommitTransaction(database)) {
    printLog(ERR, "Could not commit database transaction.\n");
    if (!sqliteRollbackTransaction(database)) {
      printLog(ERR, "Could not rollback database transaction.\n");
    }
    
    printLog(TRACE,
      "EXIT sqliteAddField(database=%p, dbString=\"%s\", tableName=\"%s\", "
      "afterField=\"%s\", newField=\"%s\", type=%p) = {NOT successful}\n",
      database, dbString, tableName, afterField, newField, type);
    scopeEnd();
    return false;
  }
  
  // Invalidate the cache.
  sqlInvalidateTableDescription(database, dbString, tableName);
  
  scopeEnd();
  return true;
}

/// @fn int sqliteCompare(void *db1, void *db2)
///
/// @brief Compare two MariaDb instances to see if they refer to the same
/// underlying instance.
///
/// @param db1 A pointer to the first MariaDb instance, cast to a void pointer.
/// @param db2 A pointer to the second MariaDb instance, cast to a void pointer.
///
/// @return Returns 0 if the two MariaDb instances refer to the same MariaDb
/// connection, non-zero otherwise.
int sqliteCompare(void *db1, void *db2) {
  SCOPE_ENTER("db1=%p, db2=%p", db1, db2);
  
  int returnValue = 0; // Equal until proven different.
  Sqlite *sqlite1 = (Sqlite*) db1;
  Sqlite *sqlite2 = (Sqlite*) db2;
  
  printLog(DEBUG, "sqlite1->databasePath = \"%s\"\n", sqlite1->databasePath);
  printLog(DEBUG, "sqlite2->databasePath = \"%s\"\n", sqlite2->databasePath);
  
  returnValue
    = bytesCompare(sqlite1->databasePath, sqlite2->databasePath);
  
  SCOPE_EXIT("db1=%p, db2=%p", "%d", db1, db2, returnValue);
  return returnValue;
}

/// @fn bool sqliteRenameTable(void *db, const char *dbName, const char *oldTableName, const char *newTableName)
///
/// @brief Rename a table in a database.
///
/// @param db A pointer to a SqlDatabase object that manages the system, cast
///   to a void*.
/// @param dbName The name of the database in the SQL database.
/// @param oldTableName The name of the table as it exists in the database at
///   the time this call is made.
/// @param newTableName The deisred new name of the table.
///
/// @return Returns true on success, false on failure.
bool sqliteRenameTable(void *db, const char *dbName,
  const char *oldTableName, const char *newTableName
) {
  SqlDatabase *database = (SqlDatabase*) db;
  
  bool returnValue = true;
  
  printLog(TRACE, "ENTER sqliteRenameTable(database=%p, dbName=\"%s\", "
    "oldTableName=\"%s\", newTableName=\"%s\")\n",
    database, dbName, oldTableName, newTableName);
  
  if ((database == NULL) || (dbName == NULL)
    || (oldTableName == NULL) || (newTableName == NULL)
  ) {
    printLog(ERR, "One or more NULL parameters.\n");
    returnValue = false;
    printLog(TRACE, "EXIT sqliteRenameTable(database=%p, dbName=\"%s\", "
      "oldTableName=\"%s\", newTableName=\"%s\") = {%s}\n",
      database, dbName, oldTableName, newTableName,
      (returnValue == true) ? "successful" : "NOT successful");
    return returnValue;
  }
  
  Bytes query = NULL;
  abprintf(&query, "alter table %s%s.%s rename to %s;",
    dbName, (dbInstance != NULL) ? dbInstance : "", oldTableName,
    newTableName);
  DbResult *queryResult = database->bytesQuery(database->connection, query);
  query = bytesDestroy(query);
  returnValue = dbQuerySuccessful(queryResult);
  queryResult = dbFreeResult(queryResult);
  
  printLog(TRACE, "EXIT sqliteRenameTable(database=%p, dbName=\"%s\", "
    "oldTableName=\"%s\", newTableName=\"%s\") = {%s}\n",
    database, dbName, oldTableName, newTableName,
    (returnValue == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn i64 sqliteGetSize(void *db, const char *dbName)
///
/// @brief Get the amount of space, in bytes, that a given database consumes on
/// disk.
///
/// @param db A pointer to a SqlDatabase object, cast to a void*.
/// @param dbName The name of the database to inquisition without the
///   dbInstance suffix.
///
/// @return Returns the number of bytes the database consumes on disk on
/// success, -1 on failure.
i64 sqliteGetSize(void *db, const char *dbName) {
  SCOPE_ENTER("db=%p, dbName=\"%s\"", db, strOrNull(dbName));
  
  SqlDatabase *database = (SqlDatabase*) db;
  Sqlite *sqlite = (Sqlite*) database->connection;
  
  Bytes filePath = NULL;
  bytesAddBytes(&filePath, sqlite->databasePath);
  bytesAddStr(&filePath, "/");
  bytesAddStr(&filePath, dbName);
  bytesAddStr(&filePath, dbInstance);
  scopeAdd(filePath, bytesDestroy);
  
  i64 returnValue = getFileLength(str(filePath));
  
  SCOPE_EXIT("db=%p, dbName=\"%s\"", "%lld", db, strOrNull(dbName),
    lld(returnValue));
  return returnValue;
}

/// @fn bool sqliteRenameDatabase(void *db, const char *oldDbName, const char *newDbName)
///
/// @brief Rename an existing database.
///
/// @param db A pointer to a SqlDatabase object cast to a void*.
/// @param oldDbName The name of the database as it exists at the time the call
///   to this function as made.
/// @param newDbName The desired name of the database when this call completes.
///
/// @return Returns true on success, false on failure.
bool sqliteRenameDatabase(void *db,
  const char *oldDbName, const char *newDbName
) {
  SCOPE_ENTER("db=%p, oldDbName=\"%s\", newDbName=\"%s\"",
    db, strOrNull(oldDbName), strOrNull(newDbName));
  
  bool returnValue = false;
  SqlDatabase *database = (SqlDatabase*) db;
  Sqlite *sqlite = (Sqlite*) database->connection;
  
  if ((database == NULL) || (oldDbName == NULL) || (newDbName == NULL)) {
    printLog(ERR, "One or more NULL parameters.  Cannot rename database.\n");
    SCOPE_EXIT("db=%p, oldDbName=\"%s\", newDbName=\"%s\"", "%s",
      db, strOrNull(oldDbName), strOrNull(newDbName), boolNames[returnValue]);
    return returnValue;
  }
  
  // Construct the real database names to use in the queries.
  char *oldDbNameWithInstance = NULL;
  straddstr(&oldDbNameWithInstance, oldDbName);
  straddstr(&oldDbNameWithInstance, dbInstance);
  scopeAdd(oldDbNameWithInstance, pointerDestroyFunction);
  char *newDbNameWithInstance = NULL;
  straddstr(&newDbNameWithInstance, newDbName);
  straddstr(&newDbNameWithInstance, dbInstance);
  scopeAdd(newDbNameWithInstance, pointerDestroyFunction);
  
  // Construct the real database paths to use with file operations.
  Bytes newDatabasePath = NULL;
  bytesAddBytes(&newDatabasePath, sqlite->databasePath);
  bytesAddStr(&newDatabasePath, "/");
  bytesAddStr(&newDatabasePath, newDbNameWithInstance);
  scopeAdd(newDatabasePath, bytesDestroy);
  Bytes oldDatabasePath = NULL;
  bytesAddBytes(&oldDatabasePath, sqlite->databasePath);
  bytesAddStr(&oldDatabasePath, "/");
  bytesAddStr(&oldDatabasePath, oldDbNameWithInstance);
  scopeAdd(oldDatabasePath, bytesDestroy);
  
  // Make sure the old database exists before doing anything else.
  DbResult *existingDatabases = sqliteGetDatabaseNames(database);
  if (existingDatabases == NULL) {
    printLog(ERR, "Could not get names of existing databases from database.\n");
    SCOPE_EXIT("db=%p, oldDbName=\"%s\", newDbName=\"%s\"", "%s",
      db, strOrNull(oldDbName), strOrNull(newDbName), boolNames[returnValue]);
    return returnValue;
  }
  scopeAdd(existingDatabases, dbFreeResult);
  // returnValue is currently false.
  for (u64 ii = 0; ii < dbGetNumResults(existingDatabases); ii++) {
    if (strcmpci(oldDbNameWithInstance,
      dbGetStringByIndex(existingDatabases, ii, 0)) == 0
    ) {
      returnValue = true;
      break;
    }
  }
  if (returnValue == false) {
    printLog(ERR, "Database \"%s\" does not exist.\n", oldDbName);
    SCOPE_EXIT("db=%p, oldDbName=\"%s\", newDbName=\"%s\"", "%s",
      db, strOrNull(oldDbName), strOrNull(newDbName), boolNames[returnValue]);
    return returnValue;
  }
  
  // Make sure the new database does NOT exist before proceeding.
  // returnValue is currently true.
  for (u64 ii = 0; ii < dbGetNumResults(existingDatabases); ii++) {
    if (strcmpci(newDbNameWithInstance,
      dbGetStringByIndex(existingDatabases, ii, 0)) == 0
    ) {
      returnValue = false;
      break;
    }
  }
  if (returnValue == false) {
    printLog(ERR, "Database \"%s\" already exists.\n", newDbName);
    SCOPE_EXIT("db=%p, oldDbName=\"%s\", newDbName=\"%s\"", "%s",
      db, strOrNull(oldDbName), strOrNull(newDbName), boolNames[returnValue]);
    return returnValue;
  }
  // New database is not being tracked by our metadata, but the file may exist.
  // Check to make sure it doesn't.
  if (getFileLength(str(newDatabasePath)) >= 0) {
    returnValue = false;
    printLog(ERR,
      "Database \"%s\" is not being tracked, but SQLite file exists on disk.\n",
      newDbName);
    printLog(ERR, "Cannot rename \"%s\" database.\n", oldDbName);
    SCOPE_EXIT("db=%p, oldDbName=\"%s\", newDbName=\"%s\"", "%s",
      db, strOrNull(oldDbName), strOrNull(newDbName), boolNames[returnValue]);
    return returnValue;
  }
  
  // Algorithm:
  // 1. Add a new entry to the Databases table of the main database.
  // 2. DETACH the old database.
  // 3. Rename the old database file to the new name.
  // 4. ATTACH the new database.
  // 5. Delete the old entry.
  
  Bytes query = NULL;
  DbResult *queryResult = NULL;
  bool successful = false;
  Bytes databasePathLiteral = NULL;
  
  databasePathLiteral = sqliteMakeBytesLiteral(newDatabasePath);
  abprintf(&query, "insert into main%s.Databases (name, type, connection) "
    "values('%s', 'sqlite', %s);", strOrEmpty(dbInstance),
    newDbName, str(databasePathLiteral));
  databasePathLiteral = bytesDestroy(databasePathLiteral);
  
  // Add the new entry into the main.Databases table.
  queryResult = sqliteExecQueryBytes(sqlite, query);
  query = bytesDestroy(query);
  successful = (queryResult != NULL) ? dbQuerySuccessful(queryResult) : false;
  queryResult = dbFreeResult(queryResult);
  if (!successful) {
    printLog(ERR,
      "Could not add entry for new database \"%s\" to main.Databases table.\n",
      newDbName);
    goto addError;
  }
  
  // DETACH the old database.
  abprintf(&query, "detach database %s;", oldDbNameWithInstance);
  queryResult = sqliteExecQueryBytes(sqlite, query);
  query = bytesDestroy(query);
  successful = (queryResult != NULL) ? dbQuerySuccessful(queryResult) : false;
  queryResult = dbFreeResult(queryResult);
  if (!successful) {
    printLog(ERR, "Could not DETACH old database \"%s\".\n",
      oldDbNameWithInstance);
    goto detachError;
  }
  
  // Rename the old database file to the new name.
  if (rename(str(oldDatabasePath), str(newDatabasePath)) != 0) {
    printLog(ERR, "Could not rename \"%s\" to \"%s\".\n",
      oldDatabasePath, newDatabasePath);
    goto renameError;
  }
  
  // ATTACH the new database.
  abprintf(&query, "attach '%s' as %s;",
    newDatabasePath, newDbNameWithInstance);
  queryResult = sqliteExecQueryBytes(sqlite, query);
  query = bytesDestroy(query);
  successful = (queryResult != NULL) ? dbQuerySuccessful(queryResult) : false;
  queryResult = dbFreeResult(queryResult);
  if (!successful) {
    printLog(ERR, "Could not ATTACH new database \"%s\".\n",
      oldDbNameWithInstance);
    goto attachError;
  }
  
  // Delete the old entry.
  abprintf(&query, "delete from main%s.Databases where name='%s';",
    strOrEmpty(dbInstance), oldDbName);
  queryResult = sqliteExecQueryBytes(sqlite, query);
  query = bytesDestroy(query);
  successful = (queryResult != NULL) ? dbQuerySuccessful(queryResult) : false;
  queryResult = dbFreeResult(queryResult);
  if (!successful) {
    printLog(WARN, "Could not delete entry for \"%s\" in main%s.Databases.\n",
      oldDbName, strOrEmpty(dbInstance));
    printLog(WARN, "Manual database maintenance needed.\n");
    // No error handling here.  The warning is sufficient.
  }
  
  SCOPE_EXIT("db=%p, oldDbName=\"%s\", newDbName=\"%s\"", "%s",
    db, strOrNull(oldDbName), strOrNull(newDbName), boolNames[returnValue]);
  return returnValue;
  
attachError:
  // Rename the new database file back to the old one.
  if (rename(str(newDatabasePath), str(oldDatabasePath)) != 0) {
    printLog(ERR, "Could not rename \"%s\" to \"%s\".\n",
      newDatabasePath, oldDatabasePath);
    printLog(ERR, "Manual file system maintenance needed.\n");
    goto detachError;
  }
  
renameError:
  // Attach the old database again.
  abprintf(&query, "attach '%s' as %s;",
    oldDatabasePath, oldDbNameWithInstance);
  queryResult = sqliteExecQueryBytes(sqlite, query);
  query = bytesDestroy(query);
  successful = (queryResult != NULL) ? dbQuerySuccessful(queryResult) : false;
  queryResult = dbFreeResult(queryResult);
  if (!successful) {
    printLog(ERR, "Could not ATTACH old database \"%s\".\n",
      oldDbNameWithInstance);
    printLog(ERR, "Manual database maintenance needed.\n");
  }
  
detachError:
  // Delete the entry we added for the new database in main.Databases.
  abprintf(&query, "delete from main%s.Databases where name='%s';",
    strOrEmpty(dbInstance), newDbName);
  queryResult = sqliteExecQueryBytes(sqlite, query);
  query = bytesDestroy(query);
  successful = (queryResult != NULL) ? dbQuerySuccessful(queryResult) : false;
  queryResult = dbFreeResult(queryResult);
  if (!successful) {
    printLog(WARN, "Could not delete entry for \"%s\" in main%s.Databases.\n",
      newDbName, strOrEmpty(dbInstance));
    printLog(WARN, "Manual database maintenance needed.\n");
  }
  
addError:
  // Return bad status.
  returnValue = false;
  printLog(ERR, "Cannot rename \"%s\" database.\n", oldDbName);
  SCOPE_EXIT("db=%p, oldDbName=\"%s\", newDbName=\"%s\"", "%s",
    db, strOrNull(oldDbName), strOrNull(newDbName), boolNames[returnValue]);
  return returnValue;
}

/// @fn bool sqliteEnsureFieldIndexedVargs(void *database, const char *dbName, const char *tableName, const char *fieldName, va_list args)
///
/// @brief Ensure that a particular field in a table is indexed by the database.
///
/// @param database A pointer to the SqlDatabase object representing the database
///   system to query.
/// @param dbName The name of the database that the table is in.
/// @param tableName The name of the table the field is in.
/// @param fieldName The name of the field to make sure is indexed.
/// @param args A va_list of all the arguments aferr the fieldName, terminating
///   with a NULL pointer.
///
/// @return Returns true on success, false on failure.
bool sqliteEnsureFieldIndexedVargs(void *database,
  const char *dbName, const char *tableName, const char *fieldName,
  va_list args
) {
  SCOPE_ENTER("database=%p, dbName=%s, tableName=%s, fieldName=%s",
    database, dbName, tableName, fieldName);
  
  bool querySuccessful = false;
  if ((database == NULL) || (dbName == NULL)
    || (tableName == NULL) || (fieldName == NULL)
  ) {
    // Nothing to do.
    printLog(ERR, "One or more NULL parameters.\n");
    SCOPE_EXIT("database=%p, dbName=%s, tableName=%s, fieldName=%s", "%s",
      database, dbName, tableName, fieldName, boolNames[querySuccessful]);
    return querySuccessful;
  }
  
  Bytes fullFieldName = NULL;
  Bytes fullIndexName = NULL;
  bytesAddStr(&fullFieldName, fieldName);
  bytesAddStr(&fullIndexName, fieldName);
  for (char *nextFieldName = va_arg(args, char*);
    nextFieldName != NULL;
    nextFieldName = va_arg(args, char*)
  ) {
    bytesAddStr(&fullFieldName, ", ");
    bytesAddStr(&fullFieldName, nextFieldName);
    bytesAddStr(&fullIndexName, "_");
    bytesAddStr(&fullIndexName, nextFieldName);
  }
  scopeAdd(fullFieldName, bytesDestroy);
  scopeAdd(fullIndexName, bytesDestroy);
  
  SqlDatabase *sqlDatabase = (SqlDatabase*) database;
  Bytes query = NULL;
  (void) abprintf(&query, "CREATE INDEX IF NOT EXISTS %s.%s_%s ON %s(%s);",
    dbName, tableName, fullIndexName, tableName, fullFieldName);
  scopeAdd(query, bytesDestroy);
  
  DbResult *queryResult
    = sqlDatabase->bytesQuery(sqlDatabase->connection, query);
  querySuccessful = dbQuerySuccessful(queryResult);
  queryResult = dbFreeResult(queryResult);
  
  SCOPE_EXIT("database=%p, dbName=%s, tableName=%s, fieldName=%s", "%s",
    database, dbName, tableName, fieldName, boolNames[querySuccessful]);
  return querySuccessful;
}

/// @fn bool sqliteAddRecords(void *database, const char *dbName, const char *tableName, const DbResult *dbResut)
///
/// @brief Add a group of records to a table in a database.
///
/// @param database A pointer to the SqlDatabase object representing the
///   database system to query cast to a void*.
/// @param dbName The name of the database the table is in.
/// @param tableName The name of the table to add a record to.
/// @param dbResult A DbResult that contains the records to add.
///
/// @return Returns true on success, false on failure.
bool sqliteAddRecords(void *database,
  const char *dbName, const char *tableName, const DbResult *dbResult
) {
  printLog(TRACE,
    "ENTER sqliteAddRecords(database=%p, dbName=\"%s\", tableName=\"%s\")\n",
    database, dbName, tableName);
  
  if ((database == NULL) || (dbName == NULL) || (tableName == NULL)) {
    printLog(ERR, "One or more NULL parameters.\n");
    printLog(TRACE,
      "EXIT sqliteAddRecords(database=%p, dbName=\"%s\", tableName=\"%s\") "
      "= {NOT successful}\n", database, dbName, tableName);
    return false;
  } else if ((dbResult == NULL) || (dbGetNumResults(dbResult) == 0)) {
    // Not an error, but nothing to do.
    printLog(TRACE,
      "EXIT sqliteAddRecords(database=%p, dbName=\"%s\", tableName=\"%s\") "
      "= {successful}\n", database, dbName, tableName);
    return true;
  }
  
  SqlDatabase *sqlDatabase = (SqlDatabase*) database;
  bool returnValue = false;
  DbResult *queryResult = NULL;
  
  Bytes query = NULL;
  
  int typeLongDoubleIndex = getIndexFromTypeDescriptor(typeLongDouble);
  int typeStringCiNoCopyIndex = getIndexFromTypeDescriptor(typeStringCiNoCopy);
  int typeBytesNoCopyIndex = getIndexFromTypeDescriptor(typeBytesNoCopy);
  
  returnValue = false;
  
  bytesAddStr(&query, "insert or ignore into ");
  bytesAddStr(&query, dbName);
  bytesAddStr(&query, dbInstance);
  bytesAddStr(&query, ".");
  bytesAddStr(&query, tableName);
  bytesAddStr(&query, " values ");
  
  u64 numResults = dbGetNumResults(dbResult);
  u64 numFields = dbGetNumFields(dbResult);
  TypeDescriptor **fieldTypes = dbResult->fieldTypes;
  for (u64 recordIndex = 0; recordIndex < numResults; recordIndex++) {
    bytesAddStr(&query, "(");
    for (u64 fieldIndex = 0; fieldIndex < numFields; fieldIndex++) {
      void *fieldValue
        = dbGetResultByIndex(dbResult, recordIndex, fieldIndex, NULL);
      TypeDescriptor *fieldType = fieldTypes[fieldIndex];
      printLog(DEBUG, "Field (%s, %s, %llu) is of type \"%s\".\n",
        dbName, tableName, llu(fieldIndex),
        (fieldType != NULL) ? fieldType->name : "NULL");
      int fieldTypeIndex = getIndexFromTypeDescriptor(fieldType);
      if ((fieldTypeIndex < 0) || (fieldTypeIndex > typeBytesNoCopyIndex)) {
        printLog(ERR, "Invalid field type.\n");
        query = bytesDestroy(query);
        returnValue = false;
        printLog(TRACE,
          "EXIT sqliteAddRecords(database=%p, dbName=\"%s\", tableName=\"%s\") "
          "= {%s}\n", database, dbName, tableName,
          (returnValue == true) ? "record added" : "record NOT added");
        return returnValue;
      }
      
      if (fieldValue != NULL) {
        Bytes escapedValue = NULL;
        if (fieldTypeIndex <= typeLongDoubleIndex) {
          // Numeric value.  Convert to string and use the literal.
          char *stringValue = fieldType->toString(fieldValue);
          bytesAddStr(&escapedValue, stringValue);
          stringValue = stringDestroy(stringValue);
        } else if (fieldTypeIndex <= typeStringCiNoCopyIndex) {
          escapedValue = sqlDatabase->makeStringLiteral(str(fieldValue));
        } else { // fieldTypeIndex <= typeBytesNoCopyIndex
          escapedValue = sqlDatabase->makeBytesLiteral((Bytes) fieldValue);
        }
        bytesAddBytes(&query, escapedValue);
        escapedValue = bytesDestroy(escapedValue);
      } else {
        bytesAddStr(&query, "NULL");
      }
      
      if (fieldIndex < numFields - 1) {
        bytesAddStr(&query, ", ");
      }
    }
    bytesAddStr(&query, ")");
    
    if (recordIndex < numResults - 1) {
      bytesAddStr(&query, ", ");
    }
  }
  bytesAddStr(&query, ";");
  
  printLog(DEBUG, "Running query \"%s\"\n", str(query));
  queryResult = sqlDatabase->bytesQuery(sqlDatabase->connection, query);
  query = bytesDestroy(query);
  returnValue = dbQuerySuccessful(queryResult);
  queryResult = dbFreeResult(queryResult);
  
  if (returnValue == false) {
    // Bulk update failed.  Add what we can one-by-one.
    // NOTE:  This does not change the overall return value since we still
    // failed to add all the records, it just does a better job than giving
    // up completely.  Only the caller knows whether a partial success is
    // acceptable or not.
    for (u64 recordIndex = 0; recordIndex < numResults; recordIndex++) {
      bytesAddStr(&query, "insert or ignore into ");
      bytesAddStr(&query, dbName);
      bytesAddStr(&query, dbInstance);
      bytesAddStr(&query, ".");
      bytesAddStr(&query, tableName);
      bytesAddStr(&query, " values ");
      bytesAddStr(&query, "(");
      
      for (u64 fieldIndex = 0; fieldIndex < numFields; fieldIndex++) {
        void *fieldValue
          = dbGetResultByIndex(dbResult, recordIndex, fieldIndex, NULL);
        TypeDescriptor *fieldType = fieldTypes[fieldIndex];
        printLog(DEBUG, "Field (%s, %s, %llu) is of type \"%s\".\n",
          dbName, tableName, llu(fieldIndex),
          (fieldType != NULL) ? fieldType->name : "NULL");
        int fieldTypeIndex = getIndexFromTypeDescriptor(fieldType);
        if ((fieldTypeIndex < 0) || (fieldTypeIndex > typeBytesNoCopyIndex)) {
          printLog(ERR, "Invalid field type.\n");
          query = bytesDestroy(query);
          returnValue = false;
          printLog(TRACE,
            "EXIT sqliteAddRecords(database=%p, dbName=\"%s\", tableName=\"%s\") "
            "= {%s}\n", database, dbName, tableName,
            (returnValue == true) ? "record added" : "record NOT added");
          return returnValue;
        }
        
        if (fieldValue != NULL) {
          Bytes escapedValue = NULL;
          if (fieldTypeIndex <= typeLongDoubleIndex) {
            // Numeric value.  Convert to string and use the literal.
            char *stringValue = fieldType->toString(fieldValue);
            bytesAddStr(&escapedValue, stringValue);
            stringValue = stringDestroy(stringValue);
          } else if (fieldTypeIndex <= typeStringCiNoCopyIndex) {
            escapedValue = sqlDatabase->makeStringLiteral(str(fieldValue));
          } else { // fieldTypeIndex <= typeBytesNoCopyIndex
            escapedValue = sqlDatabase->makeBytesLiteral((Bytes) fieldValue);
          }
          bytesAddBytes(&query, escapedValue);
          escapedValue = bytesDestroy(escapedValue);
        } else {
          bytesAddStr(&query, "NULL");
        }
        
        if (fieldIndex < numFields - 1) {
          bytesAddStr(&query, ", ");
        }
      }
      bytesAddStr(&query, ");");
      
      printLog(DEBUG, "Running query \"%s\"\n", str(query));
      queryResult = sqlDatabase->bytesQuery(sqlDatabase->connection, query);
      query = bytesDestroy(query);
      // We don't care about the success of the operation.  Our return value is
      // going to be false regardless of what was returned here.
      queryResult = dbFreeResult(queryResult);
    }
  }
  
  printLog(TRACE,
    "EXIT sqliteAddRecords(database=%p, dbName=\"%s\", tableName=\"%s\") "
    "= {%s}\n", database, dbName, tableName,
    (returnValue == true) ? "successful" : "NOT successful");
  return returnValue;
}

