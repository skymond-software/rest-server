////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                   (c) Copyright 2012-2024 Skymond, LLC.                    //
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

#include "DbInterface.h"

#ifdef LOGGING_ENABLED
#include "LoggingLib.h"
#else
#undef printLog
#define printLog(...) {}
#define logFile stderr
#define LOG_MALLOC_FAILURE(...) {}
#endif
#include "Scope.h"
#include "SqlClientLib.h"

/// @def dbMSleep
///
/// @brief Convenience macro for database support libraries to sleep for a
/// specified number of milliseconds.
#ifdef _WIN32
#define dbMSleep Sleep
#else
#define dbMSleep(milliseconds) do { \
  struct timespec sleepTime = {0, milliseconds * 1000000}; \
  nanosleep(&sleepTime, NULL); \
} while (0)
#endif

/// @var dbInstance
///
/// @brief Name of a database instance to append to the database names provided
/// to function calls in this library.
char *dbInstance = NULL;

/// @fn void dbFinalizeResult(DbResult *dbResult, Database *database, const char *dbName, const char *tableName)
///
/// @brief Add the metadata to the DbResult before it's returned to the user.
///
/// @param dbResult A pointer to the DbResult to update.
/// @param database A pointer to the Database object representing the database
///   system used to generate dbResult.
/// @param dbName The name of the database the query is for.
/// @param tableName The name of the table the query is for.
///
/// @return This function returns no value.
void dbFinalizeResult(DbResult *dbResult, Database *database,
  const char *dbName, const char *tableName
) {
  printLog(TRACE,
    "ENTER dbFinalizeResult(dbResult=%p, database=%p, dbName=\"%s\", "
    "tableName=\"%s\")\n",
    dbResult, database, dbName, tableName);
  
  if ((dbResult == NULL) || (dbResult->numRows == 0)
    || (dbResult->numFields == 0)
  ) {
    // Cannot continue
    printLog(TRACE,
      "EXIT dbFinalizeResult(dbResult=%p, database=%p, dbName=\"%s\", "
      "tableName=\"%s\") = {}\n",
      dbResult, database, dbName, tableName);
    return;
  }
  
  straddstr(&dbResult->dbName, dbName);
  straddstr(&dbResult->tableName, tableName);
  dbResult->database = database;
  
  // We can assume that every element in the results will be retrieved from the
  // data structure by the caller (because, otherwise, why would you have
  // requested the results?).  The most common way to retrieve the results is
  // by name, which has to somehow search for the name to find the index.  We
  // have two options:  Do a linear search each time or build a hash table to
  // facilitate the searches.  If we search for each element exactly once,
  // a linear search will need the  number of comparisons equal to the sum
  // of the numbers 1 through numFields multipled by the number of results, or:
  // numResults * (((numFields * numFields) + numFields) / 2).  For a hash
  // table, if we assume that allocating and copying memory is free (because
  // the copies are only copying single integer or pointer values here), we
  // will need numFields hash operations to build the table and 2 * numFields
  // comparisons to find each field (one for the hash and one for the final
  // comparison to locate the data assuming best case results).  For the full
  // set of results, we need the following number of hash + comparisons:
  // numFields + (numResults * numFields * 2)
  //
  // Whether or not we should build a hash table therefore depends on the
  // number of fields and results we have.  If the total number of operations
  // for a linear search is less than or equal to the number of operations
  // we'd do for a hash table, we should not build the hash table and just do
  // linear searches.  (We give slight preference to linear searches because
  // memory allocations and copies are not quite "free", so we use <= instead
  // of strict <.)  We can reduce the inequality to:
  // (numResults * numFields) + numResults <= 2 + (4 * numResults)
  // numResults and numFields will always be >= 0, so this is the simplest
  // form that is guaranteed to always work with non-negative integers.
  
  //// u64 numFields = dbResult->numFields;
  //// u64 numResults = dbResult->numResults;
  //// u64 linearOperations = (numResults * numFields) + numResults;
  //// u64 hashOperations = 2 + (numResults << 2); // bit shift instead of multiply
  //// if (linearOperations <= hashOperations) {
  ////   // Not enough data to warrant creating the hash table.
  ////   // This is the expected case.
    dbResult->fieldNameIndexMap = NULL;
  //// } else {
  ////   // Create a hash table with names of fields as keys and indexes as values.
  ////   // We're going to use the first row of the results (field names) as the
  ////   // keys, which will exist for the lifetime of the result, so no need to
  ////   // copy and create new strings for the keys.  Use typeStringNoCopy.
  ////   dbResult->fieldNameIndexMap = htCreate(typeString,
  ////     numFields);
  ////   if (dbResult->fieldNameIndexMap == NULL) {
  ////     printLog(ERR, "Hash table allocation failure.\n");
  ////     printLog(TRACE,
  ////       "EXIT dbFinalizeResult(dbResult=%p, database=%p, dbName=\"%s\", "
  ////       "tableName=\"%s\") = {}\n", dbResult, database, dbName, tableName);
  ////     return;
  ////   }
  ////   
  ////   const char **fieldNames = dbGetFieldNames(dbResult);
  ////   HashTable *fieldNameIndexMap = dbResult->fieldNameIndexMap;
  ////   for (u64 i = 0; i < numFields; i++) {
  ////     htAddEntry(fieldNameIndexMap, fieldNames[i], &i, typeU64);
  ////   }
  //// }
  
  printLog(TRACE,
    "EXIT dbFinalizeResult(dbResult=%p, database=%p, dbName=\"%s\", "
    "tableName=\"%s\") = {}\n", dbResult, database, dbName, tableName);
  return;
}

/// @fn DbResult* dbGetValues_(Database *database, const char *dbName, const char *tableName, const char *select, const char *orderBy, ...)
///
/// @brief Get one or more values from the database.
///
/// @details This is a wrapper function that calls getValuesVargs.  This
///   function is itself wrapped by a #define that appends a NULL as the last
///   argument to make the code cleaner and be more intuitive to future code
///   authors.
///
/// @param database A pointer to the Database object representing the database
///   system to query.
/// @param dbName The name of the database to query.
/// @param tableName The name of the table to query.
/// @param select The name of the field to interrotgae.
/// @param orderBy The field(s) to order by, if any.  This parameter may be
///   NULL.
/// @param ... All further parameters are C string pairs.  The first of the
///   two strings is the name of the field to investigate and the next string
///   is the value desired for that field in the query.  The qurey is
///   terminated by a NULL pointer which is automatically provided by the
///   dbGetValues macro that wraps this function.
///
/// @return A DbResult object with the results of the query.
DbResult* dbGetValues_(Database *database,
  const char *dbName, const char *tableName,
  const char *select, const char *orderBy, ...
) {
  printLog(TRACE,
    "ENTER dbGetValues_(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "select=\"%s\", orderBy=\"%s\")\n",
    database, dbName, tableName, select, (orderBy != NULL) ? orderBy : "");
  
  DbResult *returnValue = NULL;
  if (database == NULL) {
    printLog(ERR, "NULL database parameter provided.\n");
    returnValue = (DbResult*) calloc(1, sizeof(DbResult));
    printLog(TRACE,
      "EXIT dbGetValues_(database=%p, dbName=\"%s\", tableName=\"%s\", "
      "select=\"%s\", orderBy=\"%s\") = {%llu results}\n",
      database, dbName, tableName, select, (orderBy != NULL) ? orderBy : "",
      llu(returnValue->numResults));
    return returnValue;
  }
  
  va_list args;
  va_start(args, orderBy);
  returnValue = database->getValuesVargs(database->db, dbName,
    tableName, select, orderBy, args);
  va_end(args);
  dbFinalizeResult(returnValue, database, dbName, tableName);
  
  printLog(TRACE,
    "EXIT dbGetValues_(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "select=\"%s\", orderBy=\"%s\") = {%llu results}\n",
    database, dbName, tableName, select, (orderBy != NULL) ? orderBy : "",
    llu(returnValue->numResults));
  return returnValue;
}

/// @fn DbResult* dbGetRecords_(Database *database, const char *dbName, const char *tableName, const char *orderBy, ...)
///
/// @brief Get whole records that match a query (as opposed to only a single
///   field from the matching records).
///
/// @details This is a wrapper function around getValuesVargs.  The only
///   difference between this function and dbGetValues_ is that "*" is provided
///   for the field name in the select parameter of the call to
///   getValuesVargs.  This will make the results contain all fields of the
///   matching records instead of just one field of interest.
///
/// @param database A pointer to the Database object representing the database
///   system to query.
/// @param dbName The name of the database to query.
/// @param tableName The name of the table to query.
/// @param orderBy The field(s) to order by, if any.  This parameter may be
///   NULL.
/// @param ... All further parameters are C string pairs.  The first of the
///   two strings is the name of the field to investigate and the next string
///   is the value desired for that field in the query.  The qurey is
///   terminated by a NULL pointer which is automatically provided by the
///   dbGetRecords macro that wraps this function.
///
/// @return A DbResult object with the results of the query.
DbResult* dbGetRecords_(Database *database,
  const char *dbName, const char *tableName,
  const char *orderBy, ...
) {
  printLog(TRACE,
    "ENTER dbGetRecords_(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "orderBy=\"%s\")\n",
    database, dbName, tableName, (orderBy != NULL) ? orderBy : "");
  
  va_list args;
  va_start(args, orderBy);
  DbResult *returnValue = database->getValuesVargs(database->db, dbName,
    tableName, "*", orderBy, args);
  va_end(args);
  dbFinalizeResult(returnValue, database, dbName, tableName);
  
  printLog(TRACE,
    "EXIT dbGetRecords_(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "orderBy=\"%s\") = {%llu results}\n",
    database, dbName, tableName, (orderBy != NULL) ? orderBy : "",
    llu(returnValue->numResults));
  return returnValue;
}

/// @fn bool dbAddRecord_(Database *database, const char *dbName, const char *tableName, ...)
///
/// @brief Add a new record to a table in a database.
///
/// @param database A pointer to the Database object representing the database
///   system to query.
/// @param dbName The name of the database the table is in.
/// @param tableName The name of the table to add a record to.
/// @param ... All further parameters are C strings.  The values are position
///   dependent and must match the full field definitions of the specified
///   table.  The parameters are terminated by a NULL pointer which is
///   automatically provided by the dbAddRecord macro that wraps this
///   function.
///
/// @return True on success, false on failure.
bool dbAddRecord_(Database *database,
  const char *dbName, const char *tableName, ...
) {
  printLog(TRACE,
    "ENTER dbAddRecord_(database=%p, dbName=\"%s\", tableName=\"%s\")\n",
    database, dbName, tableName);
  
  if ((database == NULL) || (dbName == NULL) || (tableName == NULL)) {
    printLog(ERR, "One or more NULL parameters.\n");
    printLog(TRACE,
      "EXIT dbAddRecord_(database=%p, dbName=\"%s\", tableName=\"%s\") "
      "= {record NOT added}\n", database, dbName, tableName);
    return false;
  }
  
  // Variable argument variables
  va_list args;
  va_start(args, tableName);
  dbWaitForTableUnlocked(database, dbName, tableName);
  bool returnValue
    = database->addRecordVargs(database->db, dbName, tableName, args);
  va_end(args);
  
  printLog(TRACE,
    "EXIT dbAddRecord_(database=%p, dbName=\"%s\", tableName=\"%s\") "
    "= {record %sadded}\n", database, dbName, tableName,
    (returnValue == true) ? "" : "NOT ");
  return returnValue;
}

/// @fn bool dbCheckExists(Database *database, const char *dbName)
///
/// @brief Check whether a specified database is present in the database
///   management system.
///
/// @param database A pointer to the Database object representing the database
///   system to query.
/// @param dbName The name of the database to search for.
///
/// @return Returns true if the database is present, false if not.
bool dbCheckExists(Database *database, const char *dbName) {
  printLog(TRACE,
    "ENTER dbCheckExists(database=%p, dbName=\"%s\"))\n",
    database, dbName);
  
  if ((database == NULL) || (dbName == NULL)) {
    printLog(ERR, "database or dbName parameter is NULL.\n");
    printLog(TRACE,
      "EXIT dbCheckExists(database=%p, dbName=\"%s\") = {false}\n",
      database, dbName);
    return false;
  }
  
  char *dbString = NULL;
  straddstr(&dbString, dbName);
  straddstr(&dbString, dbInstance);
  if (dbString == NULL) {
    // Can't use printLog.
    LOG_MALLOC_FAILURE();
    return false;
  }
  
  bool returnValue = false;
  if (*dbName != '\0') {
    DbResult *queryResult = database->getDatabaseNames(database->db);
    dbFinalizeResult(queryResult, database, NULL, NULL);
    
    for (u64 i = 0; i < queryResult->numResults; i++) {
      if (strcmpci(dbString, dbGetStringByIndex(queryResult, i, 0)) == 0) {
        returnValue = true;
        break;
      }
    }
    queryResult = dbFreeResult(queryResult);
  }
  dbString = stringDestroy(dbString);
  
  printLog(TRACE,
    "EXIT dbCheckExists(database=%p, dbName=\"%s\") = {%s}\n",
    database, dbName, (returnValue == true) ? "true" : "false");
  return returnValue;
}

/// @fn bool dbCheckTableExists(Database *database, const char *dbName, const char *tableName)
///
/// @brief Check whether a specified table is present in a given database.
///
/// @param database A pointer to the Database object representing the database
///   system to query.
/// @param dbName The name of the database to inquisition.
/// @param tableName The name of the table to search for.
///
/// @return Returns true if the database is present, false if not.
bool dbCheckTableExists(Database *database,
  const char *dbName, const char *tableName
) {
  printLog(TRACE,
    "ENTER dbCheckTableExists(database=%p, dbName=\"%s\"), tableName=\"%s\")\n",
    database, dbName, tableName);
  
  if ((database == NULL) || (dbName == NULL) || (tableName == NULL)) {
    printLog(ERR, "database, dbName, or tableName parameter is NULL.\n");
    printLog(TRACE,
      "EXIT dbCheckTableExists(database=%p, dbName=\"%s\", tableName=\"%s\")"
      " = {false}\n", database, dbName, tableName);
    return false;
  }
  
  bool returnValue = false;
  if (*dbName != '\0') {
    DbResult *tableNames = database->getTableNames(database->db, dbName);
    dbFinalizeResult(tableNames, database, NULL, NULL);
    
    for (u64 i = 0; i < tableNames->numResults; i++) {
      if (strcmpci(tableName, dbGetStringByIndex(tableNames, i, 0)) == 0) {
        returnValue = true;
        break;
      }
    }
    tableNames = dbFreeResult(tableNames);
  }
  
  printLog(TRACE,
    "EXIT dbCheckTableExists(database=%p, dbName=\"%s\", tableName=\"%s\")"
    " = {%s}\n", database, dbName, tableName, boolNames[returnValue]);
  return returnValue;
}

/// @fn bool dbCheckFieldExists(Database *database, const char *dbName, const char *tableName, const char *fieldName)
///
/// @brief Determine whether or not a field in a table exists.
///
/// @param database A pointer to the Database object that connects to the
///   underlying database.
/// @param dbName The name of the database within the database connection.
/// @param tableName The name of the table within the named database.
/// @param fieldName The name of the field in the table to check for.
///
/// @return Returns true if the field exists in the table, false if not.
bool dbCheckFieldExists(Database *database,
  const char *dbName, const char *tableName, const char *fieldName
) {
  printLog(TRACE,
    "ENTER dbCheckFieldExists(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "fieldName=\"%s\")\n", database, dbName, tableName, fieldName);
  
  bool returnValue = false;
  if ((database == NULL) || (dbName == NULL)
    || (tableName == NULL) || (fieldName == NULL)
  ) {
    printLog(ERR,
      "database, dbName, tableName, or fieldName parameter is NULL.\n");
    printLog(TRACE,
      "EXIT dbCheckFieldExists(database=%p, dbName=\"%s\", tableName=\"%s\", "
      "fieldName=\"%s\") = {%s}\n", database, dbName, tableName, fieldName,
      boolNames[returnValue]);
    return returnValue; // false
  }
  
  if ((*dbName != '\0') && (*tableName != '\0')) {
    DbResult *tableDescription = dbDescribeTable(database, dbName, tableName);
    if ((tableDescription != NULL) && (tableDescription->numResults > 0)) {
      for (u64 ii = 0; ii < tableDescription->numResults; ii++) {
        if (strcmp(fieldName,
                   dbGetStringByName(tableDescription, ii, "fieldName")
          ) == 0
        ) {
          returnValue = true;
          break;
        }
      }
    }
    tableDescription = dbFreeResult(tableDescription);
  }
  
  printLog(TRACE,
    "EXIT dbCheckFieldExists(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "fieldName=\"%s\") = {%s}\n", database, dbName, tableName, fieldName,
    boolNames[returnValue]);
  return returnValue;
}

/// @fn bool dbAddTable_(Database *database, const char *dbName, const char *tableName, const char *primaryKey, ...)
///
/// @brief Add a new table to a database.
///
/// @param database A pointer to the Database object representing the database
///   system to query.
/// @param dbName The name of the database to add a table to.
/// @param tableName The name of the table to add.
/// @param primaryKey The name of the primary key, or a comma-separated string
///  of field names to use as the primary key.
/// @param ... All further parameters are C string/type pairs.  The types are
///   either pointers to type descriptors or C strings that hold the number of
///   characters to use for the field.  The values are position dependent and
///   specify the full field definitions of the specified table.  The parameters
///   are terminated by a NULL pointer which is automatically provided by the
///   dbAddTable macro that wraps this function.
///
/// @return True on success, false on failure.
bool dbAddTable_(Database *database, const char *dbName, const char *tableName,
  const char *primaryKey, ...
) {
  printLog(TRACE,
    "ENTER dbAddTable_(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "primaryKey=\"%s\")\n", database, dbName, tableName, primaryKey);
  
  bool returnValue = false;
  if (database == NULL) {
    printLog(ERR, "NULL database parameter provided.\n");
    printLog(TRACE,
      "EXIT dbAddTable_(database=%p, dbName=\"%s\", tableName=\"%s\", "
      "primaryKey=\"%s\") = {table %sadded to database}\n",
      database, dbName, tableName, primaryKey,
      (returnValue == true) ? "" : "NOT ");
    return returnValue;
  }
  
  va_list args;
  va_start(args, primaryKey);
  returnValue
    = database->addTableVargs(database->db, dbName, tableName, primaryKey,
    args);
  va_end(args);
  
  printLog(TRACE,
    "EXIT dbAddTable_(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "primaryKey=\"%s\") = {table %sadded to database}\n",
    database, dbName, tableName, primaryKey,
    (returnValue == true) ? "" : "NOT ");
  return returnValue;
}

/// @fn DbResult* dbFreeResult(DbResult *dbResult)
///
/// @brief Free a DbResult structure that was previously allocated in a db call.
///
/// @param dbResult The dbResult to free.
///
/// @return Returns a freed DbResult on success, some other value on failure.
DbResult* dbFreeResult(DbResult *dbResult) {
  printLog(TRACE, "ENTER dbFreeResult()\n");
  
  if (dbResult == NULL) {
    // Not an error, but nothing to do.
    printLog(TRACE, "EXIT dbFreeResult() = {%p}\n", dbResult);
    return dbResult;
  }
  
  if (dbResult->rows != NULL) {
    // Header (row 0) is always an array of Bytes objects.
    for (u64 j = 0; j < dbResult->numFields; j++) {
      printLog(DEBUG, "Freeing dbResult[0][%llu]\n", llu(j));
      printLog(DEBUG, "dbResult[0][%llu] = \"%s\"\n", llu(j),
        (char*) dbResult->rows[0][j]);
      dbResult->rows[0][j] = (void*) bytesDestroy((Bytes) dbResult->rows[0][j]);
    }
    dbResult->rows[0] = (void**) pointerDestroy(dbResult->rows[0]);
    i64 minStringTypeIndex = getIndexFromTypeDescriptor(typeString);
    i64 maxStringTypeIndex = getIndexFromTypeDescriptor(typeStringCiNoCopy);
    for (u64 i = 1; i < dbResult->numRows; i++) {
      for (u64 j = 0; j < dbResult->numFields; j++) {
        printLog(DEBUG, "Freeing dbResult[%llu][%llu]\n", llu(i), llu(j));
        if ((dbResult->fieldTypes[j] == typeString)
          || (dbResult->fieldTypes[j] == typeBytes)
        ) {
          i64 typeIndex = getIndexFromTypeDescriptor(dbResult->fieldTypes[j]);
          if ((typeIndex >= minStringTypeIndex)
            && (typeIndex <= maxStringTypeIndex)
          ) {
            printLog(DEBUG, "dbResult[%llu][%llu] = \"%s\"\n", llu(i), llu(j),
              (char*) dbResult->rows[i][j]);
          }
        }
        if (dbResult->fieldTypes[j] != NULL) {
          if (dbResult->fieldTypes[j] != typeString) {
            dbResult->rows[i][j]
              = dbResult->fieldTypes[j]->destroy(dbResult->rows[i][j]);
          } else {
            // All strings are actually encoded as Bytes objects, so destroy
            // them properly.
            dbResult->rows[i][j]
              = (void*) bytesDestroy((Bytes) dbResult->rows[i][j]);
          }
        }
      }
      dbResult->rows[i] = (void**) pointerDestroy(dbResult->rows[i]);
    }
    dbResult->rows = (void***) pointerDestroy(dbResult->rows);
  }
  dbResult->fieldTypes = (TypeDescriptor**) pointerDestroy(dbResult->fieldTypes);
  dbResult->dbName = stringDestroy(dbResult->dbName);
  dbResult->tableName = stringDestroy(dbResult->tableName);
  dbResult->numRows = 0;
  dbResult->numFields = 0;
  if (dbResult->fieldNameIndexMap != NULL) {
    dbResult->fieldNameIndexMap = htDestroy(dbResult->fieldNameIndexMap);
  }
  dbResult = (DbResult*) pointerDestroy(dbResult);
  
  printLog(TRACE, "EXIT dbFreeResult() = {%p}\n", dbResult);
  return dbResult;
}

/// @fn bool dbDeleteRecords_(Database *database, const char *dbName, const char *tableName, ...)
///
/// @brief Delete a record from a table in the database.
///
/// @param database A pointer to the Database object representing the database
///   system to query.
/// @param dbName The name of the database in which to update a record.
/// @param tableName The name of the table in which to update a record.
/// @param ... All further parameters are C string pairs.  The first of the
///   two strings is the name of the field to match and the next string
///   is the value desired for that field in the query.  The qurey is
///   terminated by a NULL pointer which is automatically provided by the
///   dbDeleteRecords macro that wraps this function.
///
/// @return True on success or false on failure.
bool dbDeleteRecords_(Database *database, const char *dbName, const char *tableName, ...) {
  printLog(TRACE, "ENTER dbDeleteRecords_(dbName=\"%s\", tableName=\"%s\"))\n",
    dbName, tableName);
  
  bool returnValue = false;
  if (database == NULL) {
    printLog(ERR, "NULL database parameter provided.\n");
    printLog(TRACE,
      "EXIT dbDeleteRecords_(database=%p, dbName=\"%s\", tableName=\"%s\") "
      "= {record %supdated}\n", database, dbName, tableName,
      (returnValue == true) ? "" : "NOT ");
    return returnValue;
  }
  
  va_list args;
  va_start(args, tableName);
  dbWaitForTableUnlocked(database, dbName, tableName);
  returnValue
    = database->deleteRecordsVargs(database->db, dbName, tableName, args);
  va_end(args);
  
  printLog(TRACE,
    "EXIT dbDeleteRecords_(database=%p, dbName=\"%s\", tableName=\"%s\") "
    "= {record %supdated}\n", database, dbName, tableName,
    (returnValue == true) ? "" : "NOT ");
  return returnValue;
}

/// @fn Bytes** dbResultToBytesTable(DbResult *dbResult)
///
/// @brief Construct a byts table from a DbResult.
///
/// @param dbResult is the DbResult instance to convert.
///
/// @return Returns a pointer to a newly-allocated, two-dimensional array of
/// Bytes objects containing copies of the DbResult values on success, NULL on
/// failure.
Bytes** dbResultToBytesTable(DbResult *dbResult) {
  printLog(TRACE, "ENTER dbResultToBytesTable())\n");
  
  if ((dbResult == NULL) || (dbResult->numRows == 0)) {
    // Nothing to do.
    printLog(TRACE, "EXIT dbResultToBytesTable() = {NULL})\n");
    return NULL;
  }
  
  Bytes **returnValue = (Bytes**) calloc(1,
    (dbResult->numRows + 1) * sizeof(Bytes*));
  if (returnValue == NULL) {
    LOG_MALLOC_FAILURE();
    printLog(NEVER, "EXIT dbResultToBytesTable() = {NULL})\n");
    return NULL;
  }
  
  // We're guaranteed that there is at least one row (because of the if at the
  // start of the function) and the first row is always the name of the fields,
  // which are Bytes objects.  Process those first since they don't adhere to
  // the types defined by the fieldTypes array.
  returnValue[0] = (Bytes*) calloc(1,
    (dbResult->numFields + 1) * sizeof(Bytes));
  for (u64 j = 0; j < dbResult->numFields; j++) {
    // returnValue[i][j] is set to NULL by calloc
    bytesAddBytes(&returnValue[0][j], (Bytes) dbResult->rows[0][j]);
    if (returnValue[0][j] == NULL) {
      LOG_MALLOC_FAILURE();
      for (u64 k = 0; k < j; k++) {
        returnValue[0][k] = bytesDestroy(returnValue[0][k]);
      }
      returnValue[0] = (Bytes*) pointerDestroy(returnValue[0]);
      returnValue = (Bytes**) pointerDestroy(returnValue);
      printLog(NEVER, "EXIT dbResultToBytesTable() = {NULL})\n");
      return NULL;
    }
  }
  
  for (u64 i = 1; i < dbResult->numRows; i++) {
    returnValue[i] = (Bytes*) calloc(1,
      (dbResult->numFields + 1) * sizeof(Bytes));
    if (returnValue[i] == NULL) {
      LOG_MALLOC_FAILURE();
      for (u64 j = 0; j < i; j++) {
        for (u64 k = 0; k < dbResult->numFields; k++) {
          returnValue[j][k] = bytesDestroy(returnValue[j][k]);
        }
        returnValue[j] = (Bytes*) pointerDestroy(returnValue[j]);
      }
      returnValue = (Bytes**) pointerDestroy(returnValue);
      printLog(NEVER, "EXIT dbResultToBytesTable() = {NULL})\n");
      return NULL;
    }
    for (u64 j = 0; j < dbResult->numFields; j++) {
      // returnValue[i][j] is set to NULL by calloc
      returnValue[i][j] = dbResult->fieldTypes[j]->toBytes(dbResult->rows[i][j]);
      if (returnValue[i][j] == NULL) {
        LOG_MALLOC_FAILURE();
        for (u64 k = 0; k < j; k++) {
          returnValue[i][k] = bytesDestroy(returnValue[i][k]);
        }
        for (j = 0; j < i; j++) {
          for (u64 k = 0; k < dbResult->numFields; k++) {
            returnValue[j][k] = bytesDestroy(returnValue[j][k]);
          }
          returnValue[j] = (Bytes*) pointerDestroy(returnValue[j]);
        }
        returnValue = (Bytes**) pointerDestroy(returnValue);
        printLog(NEVER, "EXIT dbResultToBytesTable() = {NULL})\n");
        return NULL;
      }
    }
    // returnValue[i][dbResult->numFields] is set to NULL by calloc
  }
  // returnValue[dbResult->numRows] is set to NULL by calloc
  
  printLog(TRACE, "EXIT dbResultToBytesTable() = {%p})\n", returnValue);
  return returnValue;
}

/// const Bytes** dbResultGetBytesTable(DbResult *dbResult)
///
/// @brief Get a constant reference to the the full table of results cast to
/// a constant Bytes table.
///
/// @note This function only works if all the fields are Bytes objects
/// (typeString or typeBytes).  If this is not true, this function will return
/// NULL since mixed types would potentially result in undefined behavior.  If
/// you want a guaranteed Bytes table, use dbResultToBytesTable.  If you know
/// what you are doing and don't care about the possibility that the results may
/// be mixed type, use dbResultGetRows.
///
/// @param dbResult The DbResult instance to get the table of.
///
/// @return Returns a pointer to a newly-allocated, two-dimensional array of
/// Bytes objects containing copies of the DbResult values on success, NULL on
/// failure.
const Bytes** dbResultGetBytesTable(DbResult *dbResult) {
  printLog(TRACE, "ENTER dbResultGetBytesTable())\n");
  
  const Bytes **returnValue = NULL;
  if (dbResult == NULL) {
    printLog(ERR, "Request to get Bytes table of NULL DbResult.\n");
    return returnValue; // NULL
  }
  
  TypeDescriptor **fieldTypes = dbResult->fieldTypes;
  u64 numFields = dbResult->numFields;
  for (u64 fieldIndex = 0; fieldIndex < numFields; fieldIndex++) {
    if ((fieldTypes[fieldIndex] != typeString)
      && (fieldTypes[fieldIndex] != typeBytes)
    ) {
      printLog(ERR, "Request to get Bytes table from DbResult not "
        "fully-composed of Bytes fields.\n");
      return returnValue; // NULL
    }
  }
  
  returnValue = (const Bytes**) dbResult->rows;
  
  printLog(TRACE, "EXIT dbResultGetBytesTable() = {%p})\n", returnValue);
  return returnValue;
}

/// const void*** dbResultGetRows(DbResult *dbResult)
///
/// @brief Get a constant reference to the the full table of results.
///
/// @note This function does no type checking and assumes you know what you are
/// doing.  You have been warned...
///
/// @param dbResult The DbResult instance to get the table of.
///
/// @return Returns a pointer to the two-dimensional array of void pointers
/// on success, NULL on failure.
const void*** dbResultGetRows(DbResult *dbResult) {
  printLog(TRACE, "ENTER dbResultGetRows())\n");
  
  const void ***returnValue = NULL;
  if (dbResult == NULL) {
    printLog(ERR, "Request to get rows of NULL DbResult.\n");
    return returnValue; // NULL
  }
  
  returnValue = (const void***) dbResult->rows;
  
  printLog(TRACE, "EXIT dbResultGetRows() = {%p})\n", returnValue);
  return returnValue;
}

/// @fn DbResult* dbGetValuesLike_(Database *databaze, const char *dbName, const char *tableName, const char *select, const char *orderBy, ...)
///
/// @brief Get one or more values from the database similar to the criteria.
///
/// @details This is a wrapper function that calls dbGetValuesLikeVargs.  This
///   function is itself wrapped by a #define that appends a NULL as the last
///   argument to make the code cleaner and be more intuitive to future code
///   authors.
///
/// @param database A pointer to the Database object representing the database
///   system to query.
/// @param dbName The name of the database to query.
/// @param tableName The name of the table to query.
/// @param select The name of the field to interrotgae.
/// @param orderBy The field(s) to order by, if any.  This parameter may be
///   NULL.
/// @param ... All further parameters are C string pairs.  The first of the
///   two strings is the name of the field to investigate and the next string
///   is the value desired for that field in the query.  The qurey is
///   terminated by a NULL pointer which is automatically provided by the
///   dbGetValues macro that wraps this function.
///
/// @return A DbResult object with the results of the query.
DbResult* dbGetValuesLike_(Database *database,
  const char *dbName, const char *tableName,
  const char *select, const char *orderBy, ...
) {
  printLog(TRACE,
    "ENTER dbGetValuesLike_(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "select=\"%s\", orderBy=\"%s\")\n",
    database, dbName, tableName, select, (orderBy != NULL) ? orderBy : "");
  
  DbResult *returnValue = NULL;
  if (database == NULL) {
    printLog(ERR, "NULL database parameter provided.\n");
    returnValue = (DbResult*) calloc(1, sizeof(DbResult));
    printLog(TRACE,
      "EXIT dbGetValuesLike_(database=%p, dbName=\"%s\", tableName=\"%s\", "
      "select=\"%s\", orderBy=\"%s\") = {%llu results}\n",
      database, dbName, tableName, select, (orderBy != NULL) ? orderBy : "",
      llu(returnValue->numResults));
    return returnValue;
  }
  
  // Variable argument variables
  va_list args;
  
  va_start(args, orderBy);
  returnValue = database->getValuesLikeVargs(database->db, dbName,
    tableName, select, orderBy, args);
  va_end(args);
  dbFinalizeResult(returnValue, database, dbName, tableName);
  
  printLog(TRACE,
    "EXIT dbGetValuesLike_(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "select=\"%s\", orderBy=\"%s\") = {%llu results}\n",
    database, dbName, tableName, select, (orderBy != NULL) ? orderBy : "",
    llu(returnValue->numResults));
  return returnValue;
}

/// @fn DbResult* dbGetRecordsLike_(Database *database, const char *dbName, const char *tableName, const char *orderBy, ...)
///
/// @brief Get whole records that is similar to query criteria (as opposed to
///   only a single field from the similar records).
///
/// @details This is a wrapper function around dbGetValuesLikeVargs.  The only
///   difference between this function and dbGetValuesLike_ is that "*" is
///   provided for the field name in the select parameter of the call to
///   dbGetValuesLikeVargs.  This will make the results contain all fields of
///   the matching records instead of just one field of interest.
///
/// @param database A pointer to the Database object representing the database
///   system to query.
/// @param dbName The name of the database to query.
/// @param tableName The name of the table to query.
/// @param orderBy The field(s) to order by, if any.  This parameter may be
///   NULL.
/// @param ... All further parameters are C string pairs.  The first of the
///   two strings is the name of the field to investigate and the next string
///   is the value desired for that field in the query.  The qurey is
///   terminated by a NULL pointer which is automatically provided by the
///   dbGetRecords macro that wraps this function.
///
/// @return A DbResult object with the results of the query.
DbResult* dbGetRecordsLike_(Database *database,
  const char *dbName, const char *tableName, const char *orderBy, ...
) {
  printLog(TRACE,
    "ENTER dbGetRecordsLike_(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "orderBy=\"%s\")\n",
    database, dbName, tableName, (orderBy != NULL) ? orderBy : "");
  
  // Variable argument variables
  va_list args;
  
  va_start(args, orderBy);
  DbResult *returnValue = database->getValuesLikeVargs(
    database->db, dbName, tableName, "*", orderBy, args);
  va_end(args);
  dbFinalizeResult(returnValue, database, dbName, tableName);
  
  printLog(TRACE,
    "EXIT dbGetRecordsLike_(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "orderBy=\"%s\") = {%llu results}\n",
    database, dbName, tableName, (orderBy != NULL) ? orderBy : "",
    llu(returnValue->numResults));
  return returnValue;
}

/// @fn bool dbDeleteRecordsLike_(Database *database, const char *dbName, const char *tableName, ...)
///
/// @brief Low-level method for retrieving values from a table in a database
/// similar to the criteria.
///
/// @details This is a support function intented to only be called from other
///   parts of this library.
///
/// @param database A pointer to the Database object representing the database
///   system to query.
/// @param dbName The name of the database to qurey.
/// @param tableName The name of the table to query.
/// @param ... All further arguments are key/value paris with the first of the
/// two being the key (name of the column) and the second of the two being the
/// value.
///
/// @return A DbResult with the results of the query.
bool dbDeleteRecordsLike_(Database *database,
  const char *dbName, const char *tableName, ...
) {
  printLog(TRACE,
    "ENTER dbDeleteRecordsLike_(database=%p, dbName=\"%s\", "
    "tableName=\"%s\")\n",
    database, dbName, tableName);
  
  bool returnValue = false;
  if (database == NULL) {
    printLog(ERR, "NULL database parameter provided.\n");
    printLog(TRACE,
      "EXIT dbDeleteRecordsLike_(database=%p, dbName=\"%s\", tableName=\"%s\") "
      "= {%s}\n",
      database, dbName, tableName, (returnValue == true) ? "true" : "false");
    return returnValue; // false
  }
  
  va_list args;
  va_start(args, tableName);
  dbWaitForTableUnlocked(database, dbName, tableName);
  returnValue
    = database->deleteRecordsLikeVargs(database->db, dbName, tableName, args);
  va_end(args);
  
  printLog(TRACE,
    "EXIT dbDeleteRecordsLike_(database=%p, dbName=\"%s\", tableName=\"%s\") "
    "= {%s}\n",
    database, dbName, tableName, (returnValue == true) ? "true" : "false");
  return returnValue;
}

/// @var _emptyDatabaseString
///
/// @brief The function dbGetResultByIndex below does not allocate a new string.
/// In the event that the result is not found, it has to return a static
/// empty string.  This variable provides that vaule.
static const char *_emptyDatabaseString = "";

/// void* dbGetResultByIndex(const DbResult *dbResult, u64 resultIndex, u64 fieldIndex, const TypeDescriptor *type)
///
/// @brief Get a result from a dbResult returned by a previous query.
///
/// @param dbResult The result from a previous query.
/// @param resultIndex The zero-based index of the row to retrieve the
///   information from.  Note that headers are not considered in the search
///   even though they're row 0 of the results table.
/// @param fieldIndex The zero-based index of the field from which to retrieve
///   the information.
/// @param type A pointer to a TypeDescriptor describing the type of
///   information to return (currently either typeString or typeBytes).
///
/// @return Returns a pointer to the value in the results table of the
/// provided DbResult on success, empty string or NULL value on failure.
void* dbGetResultByIndex(const DbResult *dbResult,
  u64 resultIndex, u64 fieldIndex, const TypeDescriptor *type
) {
  printLog(TRACE,
    "ENTER dbGetResultByIndex(dbName=\"%s\", tableName=\"%s\", "
    "resultIndex=%llu, fieldIndex=%llu, type=%s)\n",
    (dbResult == NULL) ? "NULL" : dbResult->dbName,
    (dbResult == NULL) ? "NULL" : dbResult->tableName,
    llu(resultIndex),
    llu(fieldIndex),
    (type != NULL) ? type->name : "(no type)");
  
  resultIndex++; // Convert to match the row of the results table.
  if (dbResult == NULL) {
    printLog(DEBUG, "DbResult is NULL.\n");
    printLog(TRACE,
      "EXIT dbGetResultByIndex(dbName=\"%s\", tableName=\"%s\", "
      "resultIndex=%llu, fieldIndex=%llu, type=%s) = {%s}\n",
      "NULL", "NULL", llu(resultIndex), llu(fieldIndex),
      (type != NULL) ? type->name : "(no type)",
      (type == typeString) ? _emptyDatabaseString : "NULL");
    return (type == typeString) ? (void*) _emptyDatabaseString : (void*) NULL;
  } else if (dbResult->rows == NULL) {
    printLog(DEBUG, "DbResult.rows is NULL.\n");
    printLog(TRACE,
      "EXIT dbGetResultByIndex(dbName=\"%s\", tableName=\"%s\", "
      "resultIndex=%llu, fieldIndex=%llu, type=%s) = {%s}\n",
      dbResult->dbName, dbResult->tableName, llu(resultIndex), llu(fieldIndex),
      (type != NULL) ? type->name : "(no type)",
      (type == typeString) ? _emptyDatabaseString : "NULL");
    return (type == typeString) ? (void*) _emptyDatabaseString : (void*) NULL;
  } else if (resultIndex >= dbResult->numRows) {
    printLog(DEBUG, "resultIndex out of range for DbResult.\n");
    printLog(TRACE,
      "EXIT dbGetResultByIndex(dbName=\"%s\", tableName=\"%s\", "
      "resultIndex=%llu, fieldIndex=%llu, type=%s) = {%s}\n",
      dbResult->dbName, dbResult->tableName, llu(resultIndex), llu(fieldIndex),
      (type != NULL) ? type->name : "(no type)",
      (type == typeString) ? _emptyDatabaseString : "NULL");
    return (type == typeString) ? (void*) _emptyDatabaseString : (void*) NULL;
  } else if (fieldIndex >= dbResult->numFields) {
    printLog(DEBUG, "fieldIndex out of range for DbResult.\n");
    printLog(TRACE,
      "EXIT dbGetResultByIndex(dbName=\"%s\", tableName=\"%s\", "
      "resultIndex=%llu, fieldIndex=%llu, type=%s) = {%s}\n",
      dbResult->dbName, dbResult->tableName, llu(resultIndex), llu(fieldIndex),
      (type != NULL) ? type->name : "(no type)",
      (type == typeString) ? _emptyDatabaseString : "NULL");
    return (type == typeString) ? (void*) _emptyDatabaseString : (void*) NULL;
  }
  
  void *returnValue = dbResult->rows[resultIndex][fieldIndex];
  if ((returnValue == NULL) && (type == typeString)) {
    returnValue = (void*) _emptyDatabaseString;
  }
  
  if ((type == typeString) || (type == typeBytes)) {
    printLog(TRACE,
      "EXIT dbGetResultByIndex(dbName=\"%s\", tableName=\"%s\", "
      "resultIndex=%llu, fieldIndex=%llu, type=%s) = {%s}\n",
      dbResult->dbName, dbResult->tableName, llu(resultIndex), llu(fieldIndex),
      (type != NULL) ? type->name : "(no type)",
      str(returnValue));
  } else {
    printLog(TRACE,
      "EXIT dbGetResultByIndex(dbName=\"%s\", tableName=\"%s\", "
      "resultIndex=%llu, fieldIndex=%llu, type=%s) = {%p}\n",
      dbResult->dbName, dbResult->tableName, llu(resultIndex), llu(fieldIndex),
      (type != NULL) ? type->name : "(no type)",
      returnValue);
  }
  return returnValue;
}

/// @fn i64 dbGetFieldIndexByName(const DbResult *dbResult, const char *fieldName)
///
/// @brief Get the index of a field in a result by its name.
///
/// @param dbResult The DbResult that contains the fields to search.
/// @param fieldName The name of the field to search for.
///
/// @return Returns the index of the field in the result on success,
/// -1 on failure.
i64 dbGetFieldIndexByName(const DbResult *dbResult, const char *fieldName) {
  const char *dbName = (dbResult != NULL) ? dbResult->dbName : "NULL";
  const char *tableName = (dbResult != NULL) ? dbResult->tableName : "NULL";
  // dbName and tableName are only used in logging, so cast them to void so that
  // the complier doesn't complain when logging isn't enabled.
  (void) dbName;
  (void) tableName;
  printLog(TRACE,
    "ENTER dbGetFieldIndexByName(dbName=\"%s\", tableName=\"%s\", "
    "fieldName=\"%s\")\n", dbName, tableName, fieldName);
  
  i64 fieldIndex = -1;
  if ((dbResult == NULL) || (fieldName == NULL)) {
    printLog(ERR, "One or more NULL parameters.\n");
    printLog(TRACE,
      "EXIT dbGetFieldIndexByName(dbName=\"%s\", tableName=\"%s\", "
      "fieldName=\"%s\") = {%lld}\n", dbName, tableName,
      fieldName, lld(fieldIndex));
    return fieldIndex;
  }
  
  if (dbResult->fieldNameIndexMap == NULL) {
    // No hash table.  Do a linear search.  This is the expected case.
    u64 i = 0;
    const char **fieldNames = dbGetFieldNames(dbResult);
    for (i = 0; i < dbResult->numFields; i++) {
      if (strcmp(fieldNames[i], fieldName) == 0) {
        break;
      }
    }
    if (i < dbResult->numFields) {
      fieldIndex = (i64) i;
    }
  } else {
    // There were multiple results and fields.  Search the hash table.
    u64 *fieldIndexPointer
      = (u64*) htGetValue(dbResult->fieldNameIndexMap, fieldName);
    if (fieldIndexPointer != NULL) {
      fieldIndex = (i64) *fieldIndexPointer;
    }
  }
  
  printLog(TRACE,
    "EXIT dbGetFieldIndexByName(dbName=\"%s\", tableName=\"%s\", "
    "fieldName=\"%s\") = {%lld}\n", dbName, tableName,
    fieldName, lld(fieldIndex));
  return fieldIndex;
}

/// void* dbGetResultByName(const DbResult *dbResult, u64 resultIndex, const char *fieldName, const TypeDescriptor *type)
///
/// @brief Get a result from a dbResult returned by a previous query.
///
/// @param dbResult The result from a previous query.
/// @param resultIndex The zero-based index of the row to retrieve the
///   information from.  Note that headers are not considered in the search
///   even though they're row 0 of the results table.
/// @param fieldName The name of the field from which to retrieve the
///   information.
/// @param type A pointer to a TypeDescriptor describing the type of
///   information to return (currently either typeString or typeBytes).
///
/// @return Returns a pointer to the value in the results table of the
/// provided DbResult on success, empty string or NULL value on failure.
void* dbGetResultByName(const DbResult *dbResult,
  u64 resultIndex, const char *fieldName, const TypeDescriptor *type
) {
  printLog(TRACE,
    "ENTER dbGetResultByName(dbName=\"%s\", tableName=\"%s\", resultIndex=%llu, "
    "fieldName=\"%s\", type=%s)\n",
    dbResult->dbName, dbResult->tableName, llu(resultIndex), fieldName,
    (type != NULL) ? type->name : "(no type)");
  
  i64 fieldIndex = dbGetFieldIndexByName(dbResult, fieldName);
  if (fieldIndex < 0) {
    printLog(TRACE,
      "EXIT dbGetResultByName(dbName=\"%s\", tableName=\"%s\", resultIndex=%llu, "
      "fieldName=\"%s\", type=%s) = {%s}\n",
      dbResult->dbName, dbResult->tableName, llu(resultIndex), fieldName,
      (type != NULL) ? type->name : "(no type)",
      (type == typeBytes) ? "NULL" : _emptyDatabaseString);
    return (type == typeBytes) ? NULL : (void*) _emptyDatabaseString;
  }
  
  void *returnValue
    = dbGetResultByIndex(dbResult, resultIndex, (u64) fieldIndex, type);
  
  if ((type == typeString) || (type == typeBytes)) {
    printLog(TRACE,
      "EXIT dbGetResultByName(dbName=\"%s\", tableName=\"%s\", resultIndex=%llu, "
      "fieldName=\"%s\", type=%s) = {%s}\n",
      dbResult->dbName, dbResult->tableName, llu(resultIndex), fieldName,
      (type != NULL) ? type->name : "(no type)",
      str(returnValue));
  } else {
    printLog(TRACE,
      "EXIT dbGetResultByName(dbName=\"%s\", tableName=\"%s\", resultIndex=%llu, "
      "fieldName=\"%s\", type=%s) = {%p}\n",
      dbResult->dbName, dbResult->tableName, llu(resultIndex), fieldName,
      (type != NULL) ? type->name : "(no type)",
      returnValue);
  }
  return returnValue;
}

/// @fn i64 dbGetResultIndexByLookupDict(const DbResult *dbResult, Dictionary *lookupDict)
///
/// @brief Look up a resultIndex in a DbResult according to key-value pairs
/// in a Dictionary that define criteria for field values that must be met.
///
/// @param dbResult A pointer to a DbResult to search.
/// @param lookupDict A pointer to a Dictionary containing key-value pairs with
///   the search criteria.  The keys are the names of the fields and the values
///   are the required values for those fields in the result.
///
/// @return Returns the index of the result on success, -1 on failure.
i64 dbGetResultIndexByLookupDict(const DbResult *dbResult, Dictionary *lookupDict) {
  scopeBegin();
  printLog(TRACE,
    "ENTER dbGetResultIndexByLookupDict(dbName=\"%s\", tableName=\"%s\", "
    "lookupDict=%p)\n",
    (dbResult != NULL) ? dbResult->dbName : "NULL",
    (dbResult != NULL) ? dbResult->tableName : "NULL",
    lookupDict);
  
  i64 resultIndex = -1;
  
  if (dbResult == NULL) {
    printLog(ERR, "dbResult is NULL.\n");
    printLog(TRACE,
      "EXIT dbGetResultIndexByLookupDict(dbName=\"%s\", tableName=\"%s\", "
      "lookupDict=%p) = {%lld}\n", "NULL", "NULL", lookupDict,
      lld(resultIndex));
    scopeEnd();
    return resultIndex; // -1, bad status
  } else if (lookupDict == NULL) {
    printLog(ERR, "lookupDict is NULL.\n");
    printLog(TRACE,
      "EXIT dbGetResultIndexByLookupDict(dbName=\"%s\", tableName=\"%s\", "
      "lookupDict=NULL) = {%lld}\n",
      dbResult->dbName, dbResult->tableName, lld(resultIndex));
    scopeEnd();
    return resultIndex; // -1, bad status
  }
  
  for (u64 ri = 0; ri < dbResult->numResults; ri++) {
    bool match = true;
    
    for (DictionaryEntry *cur = lookupDict->head;
      cur != NULL;
      cur = cur->next
    ) {
      const char *fieldName = (char*) cur->key;
      const char *value = (char*) cur->value;
      if (strcmp(dbGetStringByName(dbResult, ri, fieldName), value) != 0) {
        match = false;
        break;
      }
    }
    
    if (match == true) {
      resultIndex = (i64) ri;
      break;
    }
  }
  
  if (resultIndex < 0) {
    printLog(ERR, "Could not find result index matching provided criteria.\n");
  }
  
  printLog(TRACE,
    "EXIT dbGetResultIndexByLookupDict(dbName=\"%s\", tableName=\"%s\", "
    "lookupDict=%p) = {%lld}\n",
    dbResult->dbName, dbResult->tableName, lookupDict, lld(resultIndex));
  scopeEnd();
  return resultIndex;
}

/// @fn i64 dbGetResultIndexByLookupVargs(const DbResult *dbResult, va_list args)
///
/// @brief Look up a resultIndex in a DbResult according to key-value pairs
/// in a va_list that define criteria for field values that must be met.
///
/// @param dbResult A pointer to a DbResult to search.
/// @param args A va_list containing key-value pairs with the search criteria.
///   The even indexes are the names of the fields and the odd indexes are the
///   required values for those fields in the result.  The list is terminated
///   when a NULL even index is reached.
///
/// @return Returns the index of the result on success, -1 on failure.
i64 dbGetResultIndexByLookupVargs(const DbResult *dbResult, va_list args) {
  printLog(TRACE,
    "ENTER dbGetResultIndexByLookupVargs(dbName=\"%s\", tableName=\"%s\")\n",
    (dbResult != NULL) ? dbResult->dbName : "NULL",
    (dbResult != NULL) ? dbResult->tableName : "NULL");
  
  Dictionary *lookupDict = NULL;
  i64 resultIndex = -1;
  if (dbResult == NULL) {
    printLog(ERR, "dbResult parameter is NULL.\n");
    printLog(TRACE,
      "EXIT dbGetResultIndexByLookupVargs(dbName=\"%s\", tableName=\"%s\") "
      "= {%lld}\n", "NULL", "NULL", lld(resultIndex));
    return resultIndex; // -1, bad status
  }
  
  for (const char *key = va_arg(args, char*);
    key != NULL;
    key = va_arg(args, char*)
  ) {
    const char *value = va_arg(args, char*);
    dictionaryAddEntry(&lookupDict, key, value);
  }
  
  if (lookupDict == NULL) {
    printLog(ERR, "No lookup criteria provided.\n");
    printLog(TRACE,
      "EXIT dbGetResultIndexByLookupVargs(dbName=\"%s\", tableName=\"%s\") "
      "= {%lld}\n", "NULL", "NULL", lld(resultIndex));
    return resultIndex; // -1, bad status
  }
  
  resultIndex = dbGetResultIndexByLookupDict(dbResult, lookupDict);
  lookupDict = dictionaryDestroy(lookupDict);
  
  printLog(TRACE,
    "EXIT dbGetResultIndexByLookupVargs(dbName=\"%s\", tableName=\"%s\") = {%lld}\n",
    (dbResult != NULL) ? dbResult->dbName : "NULL",
    (dbResult != NULL) ? dbResult->tableName : "NULL",
    lld(resultIndex));
  return resultIndex;
}

/// @fn void *dbGetResultByLookup_(const DbResult *dbResult, const char *fieldName, const TypeDescriptor *type, ...)
///
/// @brief Get a field value from a DbResult given key-value pairs of field
/// names and values required.
///
/// @param dbResult A pointer to a DbResult to search.
/// @param fieldName The name of the field to get the value of if the search
///   criteria is met.
/// @param type A pointer to a TypeDescriptor defining what kind of result to
///   return (typeString or typeBytes).
/// @param ... A NULL-terminated list of key-value pairs where the even indexes
///   are field names and the odd indexes are the required values for those
///   fields.
///
/// @return Returns the found value on success, a default value appropriate for
///   the provided type on failure.
void *dbGetResultByLookup_(const DbResult *dbResult, const char *fieldName,
  const TypeDescriptor *type, ...
) {
  void *returnValue = (type == typeBytes) ? NULL : (void*) _emptyDatabaseString;
  
  printLog(TRACE, "ENTER dbGetResultByLookup(dbName=\"%s\", tableName=\"%s\", "
    "fieldName=\"%s\", type=\"%s\")\n",
    (dbResult != NULL) ? dbResult->dbName : "NULL",
    (dbResult != NULL) ? dbResult->tableName : "NULL",
    (fieldName != NULL) ? fieldName : "NULL",
    (type != NULL) ? type->name : "(no type)");
  
  if (fieldName == NULL) {
    printLog(ERR, "NULL fieldName provided.\n");
    printLog(TRACE, "EXIT dbGetResultByLookup(dbName=\"%s\", "
      "tableName=\"%s\", fieldName=\"%s\", type=\"%s\") = {%s}\n",
      (dbResult != NULL) ? dbResult->dbName : "NULL",
      (dbResult != NULL) ? dbResult->tableName : "NULL",
      (fieldName != NULL) ? fieldName : "NULL",
      (type != NULL) ? type->name : "(no type)",
      (returnValue == NULL) ? "NULL" : str(returnValue)
      );
    return returnValue; // Default empty value.
  }
  
  va_list args;
  va_start(args, type);
  i64 resultIndex = dbGetResultIndexByLookupVargs(dbResult, args);
  va_end(args);
  
  if (resultIndex < 0) {
    // Error message was already logged.  Just exit.
    printLog(TRACE, "EXIT dbGetResultByLookup(dbName=\"%s\", "
      "tableName=\"%s\", fieldName=\"%s\", type=\"%s\") = {%s}\n",
      (dbResult != NULL) ? dbResult->dbName : "NULL",
      (dbResult != NULL) ? dbResult->tableName : "NULL",
      (fieldName != NULL) ? fieldName : "NULL",
      (type != NULL) ? type->name : "(no type)",
      (returnValue == NULL) ? "NULL" : str(returnValue)
      );
    return returnValue; // Default empty value.
  }
  
  returnValue = dbGetResultByName(dbResult, (u64) resultIndex, fieldName, type);
  
  printLog(TRACE, "EXIT dbGetResultByLookup(dbName=\"%s\", "
    "tableName=\"%s\", fieldName=\"%s\", type=\"%s\") = {%s}\n",
    (dbResult != NULL) ? dbResult->dbName : "NULL",
    (dbResult != NULL) ? dbResult->tableName : "NULL",
    (fieldName != NULL) ? fieldName : "NULL",
    (type != NULL) ? type->name : "(no type)",
    (returnValue == NULL) ? "NULL" : str(returnValue)
    );
  return returnValue;
}

/// @fn bool dbUpdateResult_(const DbResult *dbResult, u64 resultIndex, ...)
///
/// @brief Update a record in the database given a previous DbResult and the
/// new parameters and values desired.
///
/// @param dbResult The result from a previous query.
/// @param resultIndex The zero-based result index of dbResult to update.
/// @param ... All further parameters are C string pairs.  The first of the
///   two strings is the name of the field to update and the next string
///   is the new value desired for that field.  The qurey is terminated by a
///   NULL pointer which is automatically provided by the dbUpdateResult macro
///   that wraps this function.
///
/// @return Returns true if the generated query was successful, false if not.
bool dbUpdateResult_(const DbResult *dbResult, u64 resultIndex, ...) {
  printLog(TRACE,
    "ENTER dbUpdateResult_(dbName=\"%s\", tableName=\"%s\", resultIndex=%llu)\n",
    dbResult->dbName, dbResult->tableName, llu(resultIndex));
  
  if (resultIndex >= dbResult->numResults) {
    printLog(WARN, "resultIndex out of range for DbResult.\n");
    printLog(TRACE,
      "EXIT dbUpdateResult_(dbName=\"%s\", tableName=\"%s\", resultIndex=%llu) "
      "= {%s}\n", dbResult->dbName, dbResult->tableName, llu(resultIndex), "false");
    return false;
  } else if (dbResult->numFields == 0) {
    printLog(WARN, "Invalid DbResult provided.\n");
    printLog(TRACE,
      "EXIT dbUpdateResult_(dbName=\"%s\", tableName=\"%s\", resultIndex=%llu) "
      "= {%s}\n", dbResult->dbName, dbResult->tableName, llu(resultIndex), "false");
    return false;
  } else if ((dbResult->dbName == NULL) || (dbResult->tableName == NULL)) {
    printLog(WARN, "NULL dbName or tableName.  Cannot update.\n");
    printLog(TRACE,
      "EXIT dbUpdateResult_(dbName=\"%s\", tableName=\"%s\", resultIndex=%llu) "
      "= {%s}\n", dbResult->dbName, dbResult->tableName, llu(resultIndex), "false");
    return false;
  }
  
  va_list args;
  va_start(args, resultIndex);
  Database *database = dbResult->database;
  bool returnValue = false;
  if (database != NULL) {
    dbWaitForTableUnlocked(database, dbResult->dbName, dbResult->tableName);
    returnValue = database->updateResultVargs(dbResult, resultIndex, args);
  }
  va_end(args);
  
  printLog(TRACE,
    "EXIT dbUpdateResult_(dbName=\"%s\", tableName=\"%s\", resultIndex=%llu) "
    "= {%s}\n", dbResult->dbName, dbResult->tableName, llu(resultIndex),
    (returnValue == true) ? "true" : "false");
  return returnValue;
}

/// @fn Dictionary* dbLockTablesDict(Database *database, const Dictionary *tablesToLock)
///
/// @brief Get a write lock in the database on the specified tables.
///
/// @param database A pointer to the Database object representing the database
///   system to query.
/// @param tablesToLock A Dictionary (HashTable) where the keys are the names of
///   the tables to lock in "dbName(dbInstance).tableName" format.  Values are
///   ignored.  This function does *NOT* take ownership of this data structure.
///   The caller must destroy it.
///
/// @return Returns the a pointer to a copy of the dictionary of locked tables
/// on successful lock of the provided tables, NULL otherwise.  The Dictionary
/// returned on success will have metadata in in beyond the original table names
/// provided (the value of fnPointer).
Dictionary* dbLockTablesDict(Database *database,
  const Dictionary *tablesToLock
) {
  printLog(TRACE,
    "ENTER dbLockTablesDict(database=%p, tablesToLock=%p)\n",
    database, tablesToLock);
  
  // Argument check.
  if (database == NULL) {
    printLog(ERR, "NULL database parameter provided.\n");
    printLog(TRACE,
      "EXIT dbLockTablesDict(database=%p, tablesToLock=%p) "
      "= {unsuccessful}\n",
      database, tablesToLock);
    return NULL;
  } else if (tablesToLock == NULL) {
    // Can't work with a NULL argument.
    printLog(TRACE,
      "EXIT dbLockTablesDict(database=%p, tablesToLock=%p) "
      "= {unsuccessful}\n",
      database, tablesToLock);
    return NULL;
  } else if (tablesToLock->size == 0) {
    // Not an error per se, but nothing to do.
    printLog(WARN, "Zero-length Dictionary provided to dbLockTablesDict.\n");
    printLog(TRACE,
      "EXIT dbLockTablesDict(database=%p, tablesToLock=%p) = {successful}\n",
      database, tablesToLock);
    return NULL;
  }
  
  // Only one thread can be in this funciton at a time per Database.
  mtx_lock(&database->lockedTablesMutex);
  
  u64 threadId = (u64) thrd_current(); // Needed for the locks
  
  Dictionary *lockedTables
    = (Dictionary*) tss_get(database->threadLockedTables);
  if (lockedTables != NULL) {
    // This is a potential problem.  We're being asked to lock tables when
    // tables are already locked by this thread.  Calling lock tables again
    // will first unlock all the tables that are currently locked and then lock
    // the new ones.  If all the tables we're being asked to lock are already
    // locked then the right thing to do is to just return.  This may happen in
    // a situation where a parent function locks all the tables that a child
    // call will later use.  If there are tables we're being asked to lock that
    // are not already locked then the user has made a mistake.  We will honor
    // the request and lock the new set of tables but the results are undefined.
    bool allTablesFound = true;
    for (DictionaryEntry *cur = tablesToLock->head;
      cur != NULL;
      cur = cur->next
    ) {
      if (!dictionaryGetEntry(lockedTables, str(cur->key))) {
        allTablesFound = false;
        break;
      }
    }
    
    if (allTablesFound == true) {
      // All tables to lock are already locked.  Nothing to do.
      // We cannot return a new Dictionary in this case since the calling
      // function was not successful in getting a new lock.  We need to return
      // a copy of the real lock so that when we go to unlock later, we don't
      // do the real unlock.  We do, however, need to mark the lock copy as not
      // being the original.
      Dictionary *lockCopy = dictionaryCopy(lockedTables);
      bool *originalLock
        = (bool*) dictionaryGetValue(lockCopy, "@originalLock");
      if (originalLock != NULL) {
        *originalLock = false;
      } else {
        // dbUnlockTables will detect this as an invalid lock and will not
        // unlock the tables in the database, so we can still return the copy,
        // although this may mean that the system is in a bad state.
        printLog(ERR, "Could not obtain and update originalLock of lock.\n");
      }
      // Log this situation and return a copy of the existing lock.
      printLog(DEBUG,
        "Requested tables already locked.  Returning copy of existing lock.\n");
      mtx_unlock(&database->lockedTablesMutex);
      printLog(TRACE,
        "EXIT dbLockTablesDict(database=%p, tablesToLock=%p) = {%p}\n",
        database, tablesToLock, lockCopy);
      return lockCopy;
    }
    // else the user has made a very bad decision and this call will result
    // in undefined behavior.  lockedTables will be *NOT* be freed by this
    // function.  If the user calls dbUnlockTables with the lock in the future,
    // it will be freed at that point.
    for (DictionaryEntry *cur = database->lockedTables->head;
      cur != NULL;
      cur = cur->next
    ) {
      u64 *lockOwner = (u64*) cur->value;
      if ((lockOwner != NULL) && (*lockOwner == threadId)) {
        // We are the owner of the lock.  Release it.
        dictionaryRemove(database->lockedTables, str(cur->key));
      }
    }
    printLog(WARN, "Locking new set of tables when tables already locked.\n");
    printLog(WARN, "Undefined behavior will result.\n");
  }
  
  bool allTablesLocked = false;
  // This function is "all or nothing".  Either we lock all the tables requested
  // or none of them.  It's possible that some other thread has a superset of
  // the requested tables locked and is also trying to lock a subset of those
  // tables.  If we don't unlock the mutex and allow some time for the other
  // thread to finish and unlock, we'll wind up in a deadlock here.  Briefly
  // unlock and then relock as needed.
  while (allTablesLocked == false) {
    // Make sure each table is unlocked and then lock it by this thread.  We have
    // a lock on the mutex for this function, so nothing can lock a table while
    // we're in the middle of checking and we therefore only need to check each
    // table once.  Other threads can unlock tables, but not lock them.
    mtx_lock(database->lockedTables->lock);
    allTablesLocked = true;
    for (DictionaryEntry *cur = tablesToLock->head;
      cur != NULL;
      cur = cur->next
    ) {
      // Make sure the table isn't locked by any other thread.
      if (dictionaryGetValue(database->lockedTables, str(cur->key))) {
        for (DictionaryEntry *innerCur = database->lockedTables->head;
          innerCur != NULL;
        ) {
          DictionaryEntry *next = innerCur->next;
          if (*((u64*) innerCur->value) == threadId) {
            dictionaryDestroyNode(database->lockedTables, innerCur);
          }
          innerCur = next;
        }
        mtx_unlock(database->lockedTables->lock);
        cnd_wait(&database->lockedTablesCondition,
          &database->lockedTablesMutex);
        mtx_lock(database->lockedTables->lock);
        allTablesLocked = false;
        break;
      }
      
      // Table isn't locked by any other thread.  Mark it locked by this one.
      if (!dictionaryAddEntry(&database->lockedTables,
        str(cur->key), &threadId, typeU64)
      ) {
        printLog(ERR, "Cannot add table %s to lockedTables hash table.\n",
          str(cur->key));
      }
    }
    mtx_unlock(database->lockedTables->lock);
  }
  
  if (!database->lockTablesDict(database->db, tablesToLock)) {
    printLog(ERR, "Could not lock tables in database.  Failing.\n");
    mtx_lock(database->lockedTables->lock);
    for (DictionaryEntry *cur = tablesToLock->head;
      cur != NULL;
      cur = cur->next
    ) {
      dictionaryRemove(database->lockedTables, str(cur->key));
    }
    mtx_unlock(database->lockedTables->lock);
    mtx_unlock(&database->lockedTablesMutex);
    printLog(TRACE,
      "EXIT dbLockTablesDict(database=%p, tablesToLock=%p) = {unsuccessful}\n",
      database, tablesToLock);
    return NULL;
  }
  
  // Copy the input tablesToLock into the tableLock that we'll store and return.
  Dictionary *tableLock = dictionaryCopy(tablesToLock);
  if (tableLock == NULL) {
    // System is in a bad state and we cannot return a successful lock to the
    // caller even though the lock succeeded.  We have to back out the lock
    // and then fail.
    database->unlockTables(database->db, tablesToLock);
    // Since the system is in a bad state, the above call is best effort and
    // we don't care about the return value.
    printLog(ERR, "Could not allocate new tableLock Dictionary.  Failing.\n");
    mtx_unlock(&database->lockedTablesMutex);
    printLog(TRACE,
      "EXIT dbLockTablesDict(database=%p, tablesToLock=%p) = {unsuccessful}\n",
      database, tablesToLock);
    return NULL;
  }
  // Initialize the originalLock value for this lock.
  bool originalLock = true;
  DictionaryEntry *entry = 
    dictionaryAddEntry(&tableLock, "@originalLock", &originalLock, typeBool);
  if (entry == NULL) {
    // Memory allocation failure.  Bail.
    LOG_MALLOC_FAILURE();
    database->unlockTables(database->db, tablesToLock);
    tableLock = dictionaryDestroy(tableLock);
    mtx_unlock(&database->lockedTablesMutex);
    printLog(TRACE,
      "EXIT dbLockTablesDict(database=%p, tablesToLock=%p) = {unsuccessful}\n",
      database, tablesToLock);
    return NULL;
  }
  // Initialize the databasea value for this lock.
  entry
    = dictionaryAddEntry(&tableLock, "@database", database, typePointerNoCopy);
  if (entry == NULL) {
    // Memory allocation failure.  Bail.
    LOG_MALLOC_FAILURE();
    database->unlockTables(database->db, tablesToLock);
    tableLock = dictionaryDestroy(tableLock);
    mtx_unlock(&database->lockedTablesMutex);
    printLog(TRACE,
      "EXIT dbLockTablesDict(database=%p, tablesToLock=%p) = {unsuccessful}\n",
      database, tablesToLock);
    return NULL;
  }
  
  // We need to store the current tableLock as database->threadLockedTables so
  // that future calls know what's locked.  We could check the return value of
  // tss_set here, but there's really no point since we're calling tss_get
  // below.  That's more definitive.
  tss_set(database->threadLockedTables, tableLock);
  if (tss_get(database->threadLockedTables) != tableLock) {
    // Something is wrong with the system.  We can't detect locked tables on
    // a future call to this function even though the lock tables call
    // succeeded.  Print out an error and return the table lock below.
    printLog(ERR,
      "tableLock metadata from thread memory did not match what was set.\n");
  }
  
  mtx_unlock(&database->lockedTablesMutex);
  
  printLog(TRACE,
    "EXIT dbLockTablesDict(database=%p, tablesToLock=%p) = {%p}\n",
    database, tablesToLock, tableLock);
  return tableLock;
}

/// @fn Dictionary* dbLockTablesVargs(Database *database, va_list args)
///
/// @brief Wrapper around dbLockTablesDict that processes a va_list to generate
/// the dictionary to use in the lock tables statement.
///
/// @param database A pointer to the Database object representing the database
///   system to query.
/// @param args A va_list with arguments (database name, table name pairs) from
///   another function's arguments.  The last parameter must be NULL.
///
/// @return Returns the value of dbLockTablesDict (pointer to the Dictionary
/// created by this function defining what to lock on success, NULL on failure).
Dictionary* dbLockTablesVargs(Database *database, va_list args) {
  printLog(TRACE, "ENTER_dbLockTablesVargs(database=%p)\n", database);
  
  Dictionary *tablesDict = NULL;
  
  char *dbName = va_arg(args, char*);
  while (dbName != NULL) {
    char *tableName = va_arg(args, char*);
    if (tableName == NULL) {
      printLog(ERR, "Prematurely terminated call to dbLockTablesVargs.\n");
      printLog(ERR, "Cannot execute.\n");
      tablesDict = dictionaryDestroy(tablesDict);
      printLog(TRACE,
        "EXIT dbLockTablesVargs(database=%p) = {unsuccessful}\n",
        database);
      return NULL;
    }
    char *fullTableName = NULL;
    if (straddstr(&fullTableName, dbName) == NULL) {
      // Memory allocation failure.  Bail.  Cannot use printLog.
      tablesDict = dictionaryDestroy(tablesDict);
      return NULL;
    }
    if (straddstr(&fullTableName, dbInstance) == NULL) {
      // Memory allocation failure.  Bail.  Cannot use printLog.
      fullTableName = stringDestroy(fullTableName);
      tablesDict = dictionaryDestroy(tablesDict);
      return NULL;
    }
    if (straddstr(&fullTableName, ".") == NULL) {
      // Memory allocation failure.  Bail.  Cannot use printLog.
      fullTableName = stringDestroy(fullTableName);
      tablesDict = dictionaryDestroy(tablesDict);
      return NULL;
    }
    if (straddstr(&fullTableName, tableName) == NULL) {
      // Memory allocation failure.  Bail.  Cannot use printLog.
      fullTableName = stringDestroy(fullTableName);
      tablesDict = dictionaryDestroy(tablesDict);
      return NULL;
    }
    if (dictionaryAddEntry(&tablesDict, fullTableName, NULL, typePointerNoCopy)
      == NULL
    ) {
      // Memory allocation failure.  Bail.  Cannot use printLog.
      fullTableName = stringDestroy(fullTableName);
      tablesDict = dictionaryDestroy(tablesDict);
      return NULL;
    }
    fullTableName = stringDestroy(fullTableName);
    dbName = va_arg(args, char*);
  }
  
  Dictionary* returnValue = dbLockTablesDict(database, tablesDict);
  tablesDict = dictionaryDestroy(tablesDict);
  
  printLog(TRACE,
    "EXIT dbLockTablesVargs(database=%p) = {%s}\n",
    database,
    (returnValue != NULL) ? "successful" : "unsuccessful");
  return returnValue;
}

/// @fn Dictionary* dbLockTables_(Database *database, ...)
///
/// @brief Wrapper around dbLockTableVargs that converts the variable list of
/// arguments after the ignored parameter to a va_list and calls
/// dbLockTableVargs.
///
/// @note This function is wrapped by the dbLockTables macro which provides NULL
/// as the last parameter.
///
/// @param database A pointer to the Database object representing the database
///   system to query.
/// @param ... All further parameters are const char* database name, table name
///   pairs of the tables to lock.  The last parameter must be NULL.
///
/// @return Returns the value of dbLockTablesVargs (pointer to the Dictionary
/// created by dbLockTablesVargs defining what to lock on success, NULL on
/// failure).
Dictionary* dbLockTables_(Database *database, ...) {
  printLog(TRACE, "ENTER dbLockTables(database=%p)\n", database);
  
  // Variable argument variables
  va_list args;
  
  va_start(args, database);
  Dictionary* returnValue = dbLockTablesVargs(database, args);
  va_end(args);
  
  printLog(TRACE, "EXIT dbLockTables(database=%p) = {%s}\n",
    database,
    (returnValue != NULL) ? "successful" : "unsuccessful");
  return returnValue;
}

/// @fn Dictionary* dbUnlockTables(Dictionary *tableLock)
///
/// @brief Unlock previously-locked tables and clear the cache of locked tables
/// for this thread.
///
/// @param tableLock A pointer to a Dictionary returned by a previous call to
///   one of the dbLockTables functions.
///
/// @return This function always returns NULL.
Dictionary* dbUnlockTables(Dictionary *tableLock) {
  printLog(TRACE, "ENTER dbUnlockTables(tableLock=%p)\n", tableLock);
  
  if (tableLock == NULL) {
    // Nothing to do.
    printLog(TRACE, "EXIT dbUnlockTables(tableLock=%p) = {NULL}\n", tableLock);
    return NULL;
  }
  
  Database *database = (Database*) dictionaryGetValue(tableLock, "@database");
  if (database == NULL) {
    // This is an invalid state.
    printLog(ERR, "No database in tableLock.\n");
    printLog(TRACE, "EXIT dbUnlockTables(tableLock=%p) = {NULL}\n", tableLock);
    tableLock = dictionaryDestroy(tableLock);
    return NULL;
  }
  
  Dictionary *lockedTables
    = (Dictionary*) tss_get(database->threadLockedTables);
  if (lockedTables == NULL) {
    // What happened in this case is that the caller got a table lock that was
    // later overridden by a subordinate function that got a new lock and then
    // released it.  Someone isn't taking care of their code, but that's not
    // our problem.  (We printed out warnings in the log file during the lock
    // process.)  We need to destroy the dictionary provided.
    printLog(DEBUG, "No tables locked.\n");
    printLog(TRACE, "EXIT dbUnlockTables(tableLock=%p) = {NULL}\n", tableLock);
    tableLock = dictionaryDestroy(tableLock);
    return NULL;
  }
  
  // NOTE:  It's posible that the caller was provided a lock by the dbLockTables*
  // function but that the caller was not the original locker.  That would
  // indicate that the tables were already locked by a parent function at the
  // time the caller made the call to lock the tables.  Due to this possibility,
  // we need to verify that the caller owns the tableLock provided.  We need
  // to check tableLock and not lockedTables for the reason described in the
  // note below.
  bool *originalLock = (bool*) dictionaryGetValue(tableLock, "@originalLock");
  if ((originalLock != NULL) && (*originalLock == false)) {
    // The caller does not own the lock provided.  We need to free the tableLock
    // provided and return NULL.
    printLog(TRACE, "EXIT dbUnlockTables(tableLock=%p) = {NULL}\n", tableLock);
    tableLock = dictionaryDestroy(tableLock);
    return NULL;
  } else if (originalLock == NULL) {
    // This is not a valid lock.  All locks have this parameter initialized at
    // creation.  The system may be in a bad state.
    printLog(ERR, "Lock provided is missing originalLock metadata.\n");
    printLog(TRACE, "EXIT dbUnlockTables(tableLock=%p) = {NULL}\n", tableLock);
    tableLock = dictionaryDestroy(tableLock);
    return NULL;
  }
  
  // NOTE:  It's possible that the caller does in fact have a valid table lock
  // but that lockedTables and tableLock are not the same dictionary.  That
  // would indicate that the caller got a table lock that was overridden by a
  // later child call and the child did not release the lock.  That would be
  // very poor programming, but the flow below covers this case.  In such a
  // case, the lock will be broken below and the provided lock will be freed.
  
  mtx_lock(&database->lockedTablesMutex);
  if (database->unlockTables(database->db, tableLock)) {
    for (DictionaryEntry *cur = tableLock->head; cur != NULL; cur = cur->next) {
      dictionaryRemove(database->lockedTables, str(cur->key));
    }
    tss_set(database->threadLockedTables, dictionaryDestroy(lockedTables));
    
    if (tableLock == lockedTables) {
      tableLock = NULL;
    } else {
      // Destroy the tableLock provided.
      tableLock = dictionaryDestroy(tableLock);
    }
  }
  cnd_broadcast(&database->lockedTablesCondition);
  mtx_unlock(&database->lockedTablesMutex);
  
  printLog(TRACE, "EXIT dbUnlockTables(tableLock=%p) = {%p}\n",
    tableLock, tableLock);
  return tableLock;
}

/// @fn bool dbIsFieldNameValid(const char *name)
///
/// @brief Determine whether or not a given string is valid as a field name in
/// a table.
///
/// @param name The string to evaluate.
///
/// @return Returns true if the name provided is valid as a field name, false
/// otherwise.
bool dbIsFieldNameValid(const char *name) {
  printLog(FLOOD, "ENTER dbIsFieldNameValid(name=\"%s\")\n", name);
  
  bool returnValue = true;
  
  if ((name != NULL) && (*name != '\0')) {
    char c = '\0';
    for (; *name != '\0'; name++) {
      c = *name;
      if ((c <= 32) || (c >= 127)) {
        returnValue = false;
        break;
      }
    }
  } else {
    returnValue = false;
  }
  
  printLog(FLOOD, "EXIT dbIsFieldNameValid(name=\"%s\") = {%s}\n", name,
    (returnValue == true) ? "true" : "false");
  return returnValue;
}

/// @fn Bytes dbResultToCsv(const DbResult *dbResult)
///
/// @brief Convert a DbResult to its corresponding CSV value.
///
/// @param dbResult The DbResult to convert.
///
/// @return Returns a new Bytes object with the CSV data on success,
/// NULL on failure.
Bytes dbResultToCsv(const DbResult *dbResult) {
  printLog(TRACE, "ENTER dbResultToCsv()\n");
  
  Bytes returnValue = NULL;
  u64 firstRowLength = 0;
  // Go through the first row and get the number of bytes the output would
  // contain.
  u64 numFields = dbResult->numFields;
  for (u64 fieldIndex = 0; fieldIndex < numFields; ++fieldIndex) {
    if ((dbResult->fieldTypes[fieldIndex] == typeString)
      || (dbResult->fieldTypes[fieldIndex] == typeBytes)
    ) {
      // Field is encodes as a Bytes object.
      firstRowLength += bytesLength(dbGetBytesByIndex(dbResult, 0, fieldIndex));
    } else if ((dbResult->fieldTypes[fieldIndex] == typeI64)) {
      // Assume 5 digits.
      firstRowLength += 5;
    } else if ((dbResult->fieldTypes[fieldIndex] == typeDouble)) {
      // Assume 10 digits.
      firstRowLength += 10;
    }
  }
  // Assume every field will be quoted.  There will be a comma between each
  // field and two bytes at the end for the newline.  Add (3 * numFields) + 2
  firstRowLength += ((numFields << 1) + numFields) + 2;
  
  // Estimate the return value's size.
  bytesAllocate(&returnValue, dbResult->numRows * firstRowLength);
  if (returnValue == NULL) {
    // malloc failure weas already logged.  Just return.
    return returnValue; // NULL
  }
  Bytes doubleQuote = NULL;
  bytesAddStr(&doubleQuote, "\"");
  Bytes comma = NULL;
  bytesAddStr(&comma, ",");
  Bytes newline = NULL;
  bytesAddStr(&newline, "\r\n");
  
  // First, add in the headers.
  // All field names are Bytes values.
  for (u64 field = 0; field < numFields; ++field) {
    Bytes fieldName = (Bytes) dbResult->rows[0][field];
    bytesAddBytes(&returnValue, doubleQuote);
    bytesAddBytes(&returnValue, fieldName);
    bytesAddBytes(&returnValue, doubleQuote);
    
    if (field < dbResult->numFields - 1) {
      bytesAddBytes(&returnValue, comma);
    }
  }
  bytesAddBytes(&returnValue, newline);
  
  Bytes fieldValue = NULL;
  int typeStringIndex = getIndexFromTypeDescriptor(typeString);
  int typeBytesNoCopyIndex = getIndexFromTypeDescriptor(typeBytesNoCopy);
  for (u64 row = 0; row < dbResult->numResults; row++) {
    for (u64 field = 0; field < dbResult->numFields; field++) {
      int typeIndex = getIndexFromTypeDescriptor(dbResult->fieldTypes[field]);
      if ((typeIndex >= typeStringIndex)
        && (typeIndex <= typeBytesNoCopyIndex)
      ) {
        // This is the expected case.
        bytesAddBytes(&returnValue, doubleQuote);
        fieldValue
          = bytesReplaceStr(
          dbGetBytesByIndex(dbResult, row, field), "\"", "\"\"");
        bytesAddBytes(&returnValue, fieldValue);
        bytesAddBytes(&returnValue, doubleQuote);
      } else if (typeIndex < typeStringIndex) {
        // Don't quote non-string data.  Some spreadsheet applications have
        // issues working with quoted numeric data.
        fieldValue = dbResult->fieldTypes[field]->toBytes(
          dbGetResultByIndex(dbResult, row, field, NULL));
        bytesAddBytes(&returnValue, fieldValue);
      }
      fieldValue = bytesDestroy(fieldValue);
      
      if (field < dbResult->numFields - 1) {
        bytesAddBytes(&returnValue, comma);
      }
    }
    
    if (row < dbResult->numResults) {
      bytesAddBytes(&returnValue, newline);
    }
  }
  
  newline = bytesDestroy(newline);
  comma = bytesDestroy(comma);
  doubleQuote = bytesDestroy(doubleQuote);
  
  printLog(TRACE, "EXIT dbResultToCsv() = {%s}\n", str(returnValue));
  return returnValue;
}

/*
 * DbResult* csvToDbResult(const char *csv) {
 *   printLog(TRACE, "ENTER csvToDbResult(csv=\"%s\")\n", csv);
 *   DbResult *returnValue = (DbResult*) calloc(1, sizeof(DbResult));
 *   if (returnValue == NULL) {
 *     LOG_MALLOC_FAILURE();
 *     exit(1);
 *   }
 *   
 *   Bytes **rows = stringToBytesTable(csv, "\r\n", ",");
 *   if ((rows == NULL) || (rows[0] == NULL)) {
 *     // Nothing to do.
 *     rows = freeBytesTable(rows);
 *     printLog(TRACE, "EXIT csvToDbResult(csv=\"%s\") = {NOT successful}\n", csv);
 *     return returnValue; // returnValue->successful is false
 *   } else if (rows[1] == NULL) {
 *     // Everything is on one row.  This may be valid or we may have mis-parsed.
 *     // Check for POSIX-formated line endings.
 *     Bytes **rows2 = stringToBytesTable(csv, "\n", ",");
 *     // We know that rows2 and rows2[0] will be non-NULL in this case.
 *     if (rows2[1] != NULL) {
 *       rows = freeBytesTable(rows);
 *       rows = rows2;
 *     } else {
 *       rows2 = freeBytesTable(rows2);
 *       // Check for Mac-formatted line endings.
 *       rows2 = stringToBytesTable(csv, "\r", ",");
 *       if (rows2[1] != NULL) {
 *         rows = freeBytesTable(rows);
 *         rows = rows2;
 *       } else {
 *         // All formats are single row.  Original is valid.
 *         rows2 = freeBytesTable(rows2);
 *       }
 *     }
 *   }
 *   
 *   // Adjust the Bytes table.
 *   u64 numRows = 0;
 *   u64 numFields = 0;
 *   for (u64 row = 0; rows[row] != NULL; row++) {
 *     numRows++;
 *     if (rows[row][0] == NULL) {
 *       // Row with no fields.  Combine rows.
 *     }
 *     
 *     for (u64 field = 0; rows[row][field] != NULL; field++) {
 *       numFields++;
 *       Bytes fieldValue = rows[row][field];
 *       if ((fieldValue[0] == '"')
 *         && ((bytesLength(fieldValue) == 1)
 *           || ((bytesLength(fieldValue) > 0) && (fieldValue[bytesLength(fieldValue) - 1] != '"'))
 *           || ((bytesLength(fieldValue) > 1) && (fieldValue[bytesLength(fieldValue) - 2] == '"'))
 *         )
 *       ) {
 *         // Combine fields.
 *       }
 *     }
 *   }
 *   
 *   printLog(TRACE, "EXIT csvToDbResult(csv=\"%s\") = {%s}\n", csv,
 *     (returnValue->successful == true) ? "successful" : "NOT successful");
 *   return returnValue;
 * }
 */

/// @fn bool dbAddRecords(Database *database, const char *dbName, const char *tableName, const DbResult *dbResut)
///
/// @brief Add a group of records to a table in a database.
///
/// @param database A pointer to the Database object representing the database
///   system to query.
/// @param dbName The name of the database the table is in.
/// @param tableName The name of the table to add a record to.
/// @param dbResult A DbResult that contains the records to add.
///
/// @return Returns true on success, false on failure.
bool dbAddRecords(Database *database,
  const char *dbName, const char *tableName, const DbResult *dbResult
) {
  printLog(TRACE,
    "ENTER dbAddRecords(database=%p, dbName=\"%s\", tableName=\"%s\")\n",
    database, dbName, tableName);
  
  if (database == NULL) {
    printLog(ERR, "NULL database parameter.\n");
    printLog(TRACE,
      "EXIT dbAddRecords(database=%p, dbName=\"%s\", tableName=\"%s\") "
      "= {NOT successful}\n", database, dbName, tableName);
    return false;
  }
  
  // The DbResult we've been provided may not have been produced by a query to
  // the database.  It may have, for instance, been created by a call to
  // bytesTableToDbResult.  In such a case, the TypeDescriptors in the
  // fieldTypes may not be correct.  We need to guarantee that the field types
  // are correct before proceeding further.
  //
  // The algorithm for this is:
  // 1. Get the description for the table.
  // 2. For each row in the description:
  //    a. Find the field index of the corresponding field name in the provided
  //       dbResult.
  //    b. Set the TypeDescriptor in the dbResult's fieldTypes array to the
  //       value of the typeInfo field for that row in the description.
  DbResult *tableDescription = dbDescribeTable(database, dbName, tableName);
  if ((tableDescription != NULL) && (tableDescription->numResults > 0)) {
    for (u64 ii = 0; ii < tableDescription->numResults; ii++) {
      const char *fieldName
        = dbGetStringByName(tableDescription, ii, "fieldName");
      i64 fieldIndex = dbGetFieldIndexByName(dbResult, fieldName);
      if (fieldIndex >= 0) {
        dbResult->fieldTypes[fieldIndex] = (TypeDescriptor*) dbGetResultByName(
          tableDescription, ii, "typeInfo", typePointer);
      }
    }
  }
  tableDescription = dbFreeResult(tableDescription);
  
  bool returnValue = database->addRecords(database->db,
    dbName, tableName, dbResult);
  
  printLog(TRACE,
    "EXIT dbAddRecords(database=%p, dbName=\"%s\", tableName=\"%s\") "
    "= {%s}\n", database, dbName, tableName,
    (returnValue == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn bool dbAddRecordDict(Database *database, const char *dbName, const char *tableName, Dictionary *dict)
///
/// @brief Add a record to a specified table using fields and values provided
/// in a dictionary.
///
/// @param database A pointer to a Database object that the record will be
///   added to.
/// @param dbName The name of the database the record will be added to.
/// @param tableName The name of the table the record will be added to.
/// @param dict A pointer to a Dictionary object that contains the field names
///   as keys and the values to add for the record.
///
/// @return Returns true on success, false on failure.
bool dbAddRecordDict(Database *database, const char *dbName,
  const char *tableName, Dictionary *dict
) {
  printLog(TRACE,
    "ENTER dbAddRecordDict(database=%p, dbName=\"%s\", tableName=\"%s\" "
    "dict=%p)\n", database, dbName, tableName, dict);
  
  if ((database == NULL) || (dbName == NULL)
      || (tableName == NULL) || (dict == NULL)
  ) {
    printLog(ERR, "One or more NULL parameters.\n");
    printLog(TRACE,
      "EXIT dbAddRecordDict(database=%p, dbName=\"%s\", tableName=\"%s\" "
      "dict=%p) = {NOT successful}\n", database, dbName, tableName, dict);
    return false;
  }
  
  dbWaitForTableUnlocked(database, dbName, tableName);
  bool returnValue
    = database->addRecordDict(database->db, dbName, tableName, dict);
  
  printLog(TRACE,
    "EXIT dbAddRecordDict(database=%p, dbName=\"%s\", tableName=\"%s\" "
    "dict=%p) = {%s}\n", database, dbName, tableName, dict,
    (returnValue == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn bool dbUpdateRecordDict(Database *database, const char *dbName, const char *tableName, Dictionary *dict)
///
/// @brief Update a record in a specified table using fields and values provided
/// in a dictionary.
///
/// @param database A pointer to a Database object that the record will be
///   updated in.
/// @param dbName The name of the database the record will be updated in.
/// @param tableName The name of the table the record will be updated in.
/// @param dict A pointer to a Dictionary object that contains the field names
///   as keys and the values to update for the record.
///
/// @return Returns true on success, false on failure.
bool dbUpdateRecordDict(Database *database, const char *dbName,
  const char *tableName, Dictionary *dict
) {
  printLog(TRACE,
    "ENTER dbUpdateRecordDict(database=%p, dbName=\"%s\", tableName=\"%s\" "
    "dict=%p)\n", database, dbName, tableName, dict);
  
  if ((database == NULL) || (dbName == NULL)
      || (tableName == NULL) || (dict == NULL)
  ) {
    printLog(ERR, "One or more NULL parameters.\n");
    printLog(TRACE,
      "EXIT dbUpdateRecordDict(database=%p, dbName=\"%s\", tableName=\"%s\" "
      "dict=%p) = {NOT successful}\n", database, dbName, tableName, dict);
    return false;
  }
  
  dbWaitForTableUnlocked(database, dbName, tableName);
  bool returnValue
    = database->updateRecordDict(database->db, dbName, tableName, dict);
  
  printLog(TRACE,
    "EXIT dbUpdateRecordDict(database=%p, dbName=\"%s\", tableName=\"%s\" "
    "dict=%p) = {%s}\n", database, dbName, tableName, dict,
    (returnValue == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn bool dbAddTableList(Database *database, const char *dbName, const char *tableName, const char *primaryKey, List *args)
///
/// @brief Add a new table to a database given a List describing the fields.
///
/// @param database A pointer to a Database object that manages the database.
/// @param dbName The name of the database to add the table to.
/// @param tableName The name of the table to add.
/// @param primaryKey The name of the primary key, or a comma-separated string
///  of field names to use as the primary key.
/// @param args The list that contains the fields to add to the table.
///
/// @return true on success or false on failure.
bool dbAddTableList(Database *database, const char *dbName,
  const char *tableName, const char *primaryKey, List *args
) {
  printLog(TRACE,
    "ENTER dbAddTableList(database=%p, dbName=\"%s\", tableName=\"%s\" "
    "primaryKey=\"%s\", args=%p)\n",
    database, dbName, tableName, primaryKey, args);
  
  if ((database == NULL) || (dbName == NULL) || (tableName == NULL)
      || (primaryKey == NULL) || (args == NULL)
  ) {
    printLog(ERR, "One or more NULL parameters.\n");
    printLog(TRACE,
      "EXIT dbAddTableList(database=%p, dbName=\"%s\", tableName=\"%s\" "
      "primaryKey=\"%s\", args=%p) = {NOT successful}\n",
      database, dbName, tableName,primaryKey, args);
    return false;
  }
  
  bool returnValue
    = database->addTableList(database->db, dbName, tableName, primaryKey, args);
  
  printLog(TRACE,
    "EXIT dbAddTableList(database=%p, dbName=\"%s\", tableName=\"%s\" "
    "primaryKey=\"%s\", args=%p) = {%s}\n",
    database, dbName, tableName, primaryKey, args,
    (returnValue == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn bool dbDeleteTable(Database *database, const char *dbName, const char *tableName)
///
/// @brief Delete a table in a database.
///
/// @param database A pointer to a Database object that manages the database.
/// @param dbName The name of the database the table is in.
/// @param tableName The name of the table to delete.
///
/// @return Returns true on success, false on failure.
bool dbDeleteTable(Database *database, const char *dbName,
  const char *tableName
) {
  printLog(TRACE,
    "ENTER dbDeleteTable(database=%p, dbName=\"%s\", tableName=\"%s\")",
    database, dbName, tableName);
  
  if ((database == NULL) || (dbName == NULL) || (tableName == NULL)) {
    printLog(ERR, "One or more NULL parameters.\n");
    printLog(TRACE,
      "EXIT dbDeleteTable(database=%p, dbName=\"%s\", tableName=\"%s\") = "
      "{NOT successful}",
      database, dbName, tableName);
    return false;
  }
  
  dbWaitForTableUnlocked(database, dbName, tableName);
  bool returnValue
    = database->deleteTable(database->db, dbName, tableName);
  
  printLog(TRACE,
    "EXIT dbDeleteTable(database=%p, dbName=\"%s\", tableName=\"%s\") = {%s}",
    database, dbName, tableName,
    (returnValue == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn Database* dbStartTransaction(Database *database)
///
/// @brief Start a transaction in the database management system.
///
/// @param database A pointer to a Database object that manages the system.
///
/// @return Returns the provided database parameter on success, NULL on failure.
Database* dbStartTransaction(Database *database) {
  printLog(TRACE, "ENTER dbStartTransaction(database=%p)", database);
  
  if (database == NULL) {
    printLog(ERR, "database parameter is NULL.\n");
    printLog(TRACE, "EXIT dbStartTransaction(database=%p) = {NOT successful}",
      database);
    return NULL;
  }
  
  Database *returnValue = database;
  if (!database->startTransaction(database->db)) {
    returnValue = NULL;
  }
  
  printLog(TRACE, "EXIT dbStartTransaction(database=%p) = {%s}", database,
    (returnValue != NULL) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn Database* dbCommitTransaction(Database *database)
///
/// @brief Commit a transaction in the database management system.
///
/// @param database A pointer to a Database object that manages the system.
///
/// @return Returns the pointer to the Database provided on success,
/// NULL on failure.
Database* dbCommitTransaction(Database *database) {
  printLog(TRACE, "ENTER dbCommitTransaction(database=%p)", database);
  
  if (database == NULL) {
    printLog(ERR, "database parameter is NULL.\n");
    printLog(TRACE, "EXIT dbCommitTransaction(database=%p) = {NOT successful}",
      database);
    return NULL;
  }
  
  Database *returnValue = NULL;
  if (database->commitTransaction(database->db)) {
    returnValue = database;
  }
  
  printLog(TRACE, "EXIT dbCommitTransaction(database=%p) = {%s}", database,
    (returnValue) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn Database* dbRollbackTransaction(Database *database)
///
/// @brief Rollback a transaction in the database management system.
///
/// @param database A pointer to a Database object that manages the system.
///
/// @return Returns the pointer to the Database provided on success,
/// NULL on failure.
Database* dbRollbackTransaction(Database *database) {
  printLog(TRACE, "ENTER dbRollbackTransaction(database=%p)", database);
  
  if (database == NULL) {
    printLog(TRACE,
      "EXIT dbRollbackTransaction(database=%p) = {NOT successful}",
      database);
    return NULL;
  }
  
  Database *returnValue = NULL;
  if (database->rollbackTransaction(database->db)) {
    returnValue = database;
  }
  
  printLog(TRACE, "EXIT dbRollbackTransaction(database=%p) = {%s}", database,
    (returnValue) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn bool dbAddDatabase(Database *database, const char *dbName)
///
/// @brief Add a new database to the database management system.
///
/// @param database A pointer to a Database object that manages the system.
/// @param dbName The name of the database to add.
///
/// @return Returns true on success, false on failure.
bool dbAddDatabase(Database *database, const char *dbName) {
  printLog(TRACE, "ENTER dbAddDatabase(database=%p, dbName=\"%s\")",
    database, dbName);
  
  if ((database == NULL) || (dbName == NULL)) {
    printLog(ERR, "NULL parameter.\n");
    printLog(TRACE,
      "EXIT dbAddDatabase(database=%p, dbName=\"%s\") = {NOT successful}",
      database, dbName);
    return false;
  }
  
  bool returnValue = database->addDatabase(database->db, dbName);
  
  printLog(TRACE, "EXIT dbAddDatabase(database=%p, dbName=\"%s\") = {%s}",
    database, dbName, (returnValue == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn bool dbDeleteDatabase(Database *database, const char *dbName)
///
/// @brief Delete an existing database from the database management system.
///
/// @param database A pointer to a Database object that manages the system.
/// @param dbName The name of the database to delete.
///
/// @return Returns true on success, false on failure.
bool dbDeleteDatabase(Database *database, const char *dbName) {
  printLog(TRACE, "ENTER dbDeleteDatabase(database=%p, dbName=\"%s\")",
    database, dbName);
  
  if ((database == NULL) || (dbName == NULL)) {
    printLog(ERR, "NULL parameter.\n");
    printLog(TRACE,
      "EXIT dbDeleteDatabase(database=%p, dbName=\"%s\") = {NOT successful}",
      database, dbName);
    return false;
  }
  
  bool returnValue = database->deleteDatabase(database->db, dbName);
  
  printLog(TRACE, "EXIT dbDeleteDatabase(database=%p, dbName=\"%s\") = {%s}",
    database, dbName, (returnValue == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn bool dbDeleteField(Database *database, const char *dbName, const char *tableName, const char *fieldName)
///
/// @brief Delete a field in a database table.
///
/// @param database A pointer to a Database object that manages the system.
/// @param dbName The name of the database the table is in.
/// @param tableName The name of the table the field is in.
/// @param fieldName The name of the field to delete.
///
/// @return Returns true on success, false on failure.
bool dbDeleteField(Database *database, const char *dbName,
  const char *tableName, const char *fieldName
) {
  printLog(TRACE, "ENTER dbDeleteField(database=%p, dbName=\"%s\", "
    "tableName=\"%s\", fieldName=\"%s\")",
    database, dbName, tableName, fieldName);
  
  if ((database == NULL) || (dbName == NULL)
    || (tableName == NULL) || (fieldName == NULL)
  ) {
    printLog(ERR, "NULL parameter.\n");
    printLog(TRACE, "EXIT dbDeleteField(database=%p, dbName=\"%s\", "
      "tableName=\"%s\", fieldName=\"%s\") = {NOT successful}",
      database, dbName, tableName, fieldName);
    return false;
  }
  
  dbWaitForTableUnlocked(database, dbName, tableName);
  bool returnValue
    = database->deleteField(database->db, dbName, tableName, fieldName);
  
  printLog(TRACE, "EXIT dbDeleteField(database=%p, dbName=\"%s\", "
    "tableName=\"%s\", fieldName=\"%s\") = {%s}",
    database, dbName, tableName, fieldName,
    (returnValue == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn bool dbAddField(Database *database, const char *dbName, const char *tableName, const char *afterField, const char *newField, TypeDescriptor *type)
///
/// @brief Add a new field to an existing table in a database.
///
/// @param database A pointer to a Database object that manages the system.
/// @param dbName The name of the database the table is in.
/// @param tableName The name of the table the field is in.
/// @param afterField The name of the field in the existing table to place the
///   new field after.  If this value is an empty string, the new field will be
///   added at the beginning of the table.
/// @param newField The name of the new field to add.
/// @param type A TypeDescriptor defining the type of the new field
///   -OR- a character pointer holding the number of characters for the field.
///
/// @return Returns true if adding the field to the table was successful,
/// false if not.
bool dbAddField(Database *database, const char *dbName, const char *tableName,
  const char *afterField, const char *newField, TypeDescriptor *type
) {
  printLog(TRACE,
    "ENTER dbAddField(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "afterField=\"%s\", newField=\"%s\", type=%p)",
    database, dbName, tableName, afterField, newField, type);
  
  if ((database == NULL) || (dbName == NULL)
    || (tableName == NULL) || (afterField == NULL)
    || (newField == NULL) || (type == NULL)
  ) {
    printLog(ERR, "NULL parameter.\n");
    printLog(TRACE,
      "EXIT dbAddField(database=%p, dbName=\"%s\", tableName=\"%s\", "
      "afterField=\"%s\", newField=\"%s\", type=%p) = {NOT successful}",
      database, dbName, tableName, afterField, newField, type);
    return false;
  }
  
  dbWaitForTableUnlocked(database, dbName, tableName);
  bool returnValue = database->addField(
    database->db, dbName, tableName, afterField, newField, type);
  
  printLog(TRACE,
    "EXIT dbAddField(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "afterField=\"%s\", newField=\"%s\", type=%p) = {%s}",
    database, dbName, tableName, afterField, newField, type,
    (returnValue == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn bool dbChangeFieldType(Database *database, const char *dbName, const char *tableName, const char *fieldName, const void *type)
///
/// @brief Change the data type associated with a field in a table.
///
/// @param database A pointer to a Database object that manages the system.
/// @param dbName The name of the database the table is in.
/// @param tableName The name of the table the field is in.
/// @param fieldName The name of the field to delete.
/// @param type A TypeDescriptor defining the type of the new field
///   -OR- a character pointer holding the number of characters for the field.
///
/// @return Returns true on success, false on failure.
bool dbChangeFieldType(Database *database, const char *dbName,
  const char *tableName, const char *fieldName, const void *type
) {
  printLog(TRACE,
    "ENTER dbChangeFieldType(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "fieldName=\"%s\", type=%p)",
    database, dbName, tableName, fieldName, type);
  
  if ((database == NULL) || (dbName == NULL)
    || (tableName == NULL) || (fieldName == NULL)
    || (type == NULL)
  ) {
    printLog(ERR, "NULL parameter.\n");
    printLog(TRACE,
      "EXIT dbChangeFieldType(database=%p, dbName=\"%s\", tableName=\"%s\", "
      "fieldName=\"%s\", type=%p) = {NOT successful}",
      database, dbName, tableName, fieldName, type);
    return false;
  }
  
  dbWaitForTableUnlocked(database, dbName, tableName);
  bool returnValue = database->changeFieldType(
    database->db, dbName, tableName, fieldName, type);
  
  printLog(TRACE,
    "EXIT dbChangeFieldType(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "fieldName=\"%s\", type=%p) = {%s}",
    database, dbName, tableName, fieldName, type,
    (returnValue == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn bool dbChangeFieldName(Database *database, const char *dbName, const char *tableName, const char *oldName, const char *newName)
///
/// @brief Change the name of a field in a table.
///
/// @param database A pointer to a Database object that manages the system.
/// @param dbName The name of the database the table is in.
/// @param tableName The name of the table the field is in.
/// @param oldName The current name of the field in the table.
/// @param newName The desired new name for the field.
///
/// @return Returns true on success, false on failure.
bool dbChangeFieldName(Database *database, const char *dbName,
  const char *tableName, const char *oldName, const char *newName
) {
  printLog(TRACE,
    "ENTER dbChangeFieldName(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "oldName=\"%s\", newName=\"%s\")",
    database, dbName, tableName, oldName, newName);
  
  if ((database == NULL) || (dbName == NULL)
    || (tableName == NULL) || (oldName == NULL)
    || (newName == NULL)
  ) {
    printLog(ERR, "NULL parameter.\n");
    printLog(TRACE,
      "EXIT dbChangeFieldName(database=%p, dbName=\"%s\", tableName=\"%s\", "
      "oldName=\"%s\", newName=\"%s\") = {NOT successful}",
      database, dbName, tableName, oldName, newName);
    return false;
  }
  
  dbWaitForTableUnlocked(database, dbName, tableName);
  bool returnValue = database->changeFieldName(
    database->db, dbName, tableName, oldName, newName);
  
  printLog(TRACE,
    "EXIT dbChangeFieldName(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "oldName=\"%s\", newName=\"%s\") = {%s}",
    database, dbName, tableName, oldName, newName,
    (returnValue == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn DbResult* dbGetValuesDict(Database *database, const char *dbName, const char *tableName, const char *select, Dictionary *dict)
///
/// @brief Get values from a table given a Dictionary of field name/value
/// criteria.
///
/// @param database A pointer to a Database object that manages the system.
/// @param dbName The name of the database the table is in.
/// @param tableName The name of the table the values are in.
/// @param select A comma-separated list of fields to select from the table.
/// @param dict A pointer to a Dictionary containing the selection criteria with
///   field names as keys and values as values.
///
/// @return Returns a populated DbResult on success, empty one on failure.
DbResult* dbGetValuesDict(Database *database, const char *dbName,
  const char *tableName, const char *select, Dictionary *dict
) {
  printLog(TRACE,
    "ENTER dbGetValuesDict(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "select=\"%s\", dict=%p)",
    database, dbName, tableName, select, dict);
  
  DbResult *returnValue = NULL;
  
  if ((database == NULL) || (dbName == NULL)
    || (tableName == NULL) || (select == NULL)
    || (dict == NULL)
  ) {
    printLog(ERR, "NULL parameter.\n");
    printLog(TRACE,
      "EXIT dbGetValuesDict(database=%p, dbName=\"%s\", tableName=\"%s\", "
      "select=\"%s\", dict=%p) = {NOT successful}",
      database, dbName, tableName, select, dict);
    returnValue = (DbResult*) calloc(1, sizeof(DbResult));
    if (returnValue == NULL) {
      LOG_MALLOC_FAILURE();
      exit(1);
    }
    return returnValue;
  }
  
  returnValue = database->getValuesDict(
      database->db, dbName, tableName, select, NULL, dict);
  
  printLog(TRACE,
    "EXIT dbGetValuesDict(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "select=\"%s\", dict=%p) = {%s}",
    database, dbName, tableName, select, dict,
    (returnValue->successful == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn DbResult* dbGetValuesDictOrderBy(Database *database, const char *dbName, const char *tableName, const char *select, const char *orderBy, Dictionary *dict)
///
/// @brief Get values from a table given a Dictionary of field name/value
/// criteria, ordered by specified criteria.
///
/// @param database A pointer to a Database object that manages the system.
/// @param dbName The name of the database the table is in.
/// @param tableName The name of the table the values are in.
/// @param select A comma-separated list of fields to select from the table.
/// @param orderBy The field(s) to order by, if any.  This parameter may be
///   NULL.
/// @param dict A pointer to a Dictionary containing the selection criteria with
///   field names as keys and values as values.
///
/// @return Returns a populated DbResult on success, empty one on failure.
DbResult* dbGetValuesDictOrderBy(Database *database, const char *dbName,
  const char *tableName, const char *select, const char *orderBy,
  Dictionary *dict
) {
  printLog(TRACE,
    "ENTER dbGetValuesDictOrderBy(database=%p, dbName=\"%s\", "
    "tableName=\"%s\", select=\"%s\", orderBy=\"%s\", dict=%p)",
    database, dbName, tableName, select, orderBy, dict);
  
  DbResult *returnValue = NULL;
  
  if ((database == NULL) || (dbName == NULL)
    || (tableName == NULL) || (select == NULL)
  ) {
    printLog(ERR, "NULL parameter.\n");
    printLog(TRACE,
      "EXIT dbGetValuesDictOrderBy(database=%p, dbName=\"%s\", "
      "tableName=\"%s\", select=\"%s\", orderBy=\"%s\", dict=%p) = "
      "{NOT successful}",
      database, dbName, tableName, select, orderBy, dict);
    returnValue = (DbResult*) calloc(1, sizeof(DbResult));
    if (returnValue == NULL) {
      LOG_MALLOC_FAILURE();
      exit(1);
    }
    return returnValue;
  }
  
  returnValue = database->getValuesDict(
    database->db, dbName, tableName, select, orderBy, dict);
  
  printLog(TRACE,
    "EXIT dbGetValuesDictOrderBy(database=%p, dbName=\"%s\", tableName=\"%s\", "
    "select=\"%s\", orderBy=\"%s\", dict=%p) = {%s}",
    database, dbName, tableName, select, orderBy, dict,
    (returnValue->successful == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn DbResult* dbGetTableNames(Database *database, const char *dbName)
///
/// @brief Get a table containing the table names in a specified database.
///
/// @param database A pointer to a Database object that manages the system.
/// @param dbName The name of the database to query.
///
/// @return Returns a populated DbResult on success, empty one on failure.
DbResult* dbGetTableNames(Database *database, const char *dbName) {
  printLog(TRACE,
    "ENTER dbGetTableNames(database=%p, dbName=\"%s\")", database, dbName);
  
  DbResult *returnValue = NULL;
  
  if ((database == NULL) || (dbName == NULL)) {
    printLog(ERR, "NULL parameter.\n");
    printLog(TRACE,
      "EXIT dbGetTableNames(database=%p, dbName=\"%s\") = {NOT successful}",
      database, dbName);
    returnValue = (DbResult*) calloc(1, sizeof(DbResult));
    if (returnValue == NULL) {
      LOG_MALLOC_FAILURE();
      exit(1);
    }
    return returnValue;
  }
  
  returnValue = database->getTableNames(database->db, dbName);
  
  printLog(TRACE,
    "EXIT dbGetTableNames(database=%p, dbName=\"%s\") = {%s}", database, dbName,
    (returnValue->successful == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn Database* dbDisconnect(Database *database)
///
/// @brief Disconnect from a database management system.
///
/// @param database A pointer to a Database object that manages the system.
///
/// @return Returns NULL on success, the original pointer on failure.
Database* dbDisconnect(Database *database) {
  printLog(TRACE, "ENTER dbDisconnect(database=%p)\n", database);
  
  if (database == NULL) {
    // No-op.
    printLog(TRACE,
      "EXIT dbDisconnect(database=%p) = {successful}\n",
      database);
    return database;
  }
  
  if (database != NULL) {
    database->db = database->disconnect(database->db);
    if (database->db != NULL) {
      printLog(ERR, "Could not disconnect database.\n");
      printLog(TRACE,
        "EXIT dbDisconnect(database=%p) = {NOT successful}\n",
        database);
      return database;
    }
  }
  
  tss_delete(database->threadLockedTables);
  
  mtx_destroy(&database->lockedTablesMutex);
  cnd_destroy(&database->lockedTablesCondition);
  
  database->lockedTables = dictionaryDestroy(database->lockedTables);
  database->lockedRecords = rbTreeDestroy(database->lockedRecords);
  
  // Free the Database object.
  database = (Database*) pointerDestroy(database);
  
  printLog(TRACE,
    "EXIT dbDisconnect(database=%p) = {successful}\n",
    database);
  return database;
}

/// @fn DbResult* dbDescribeTable(Database *database, const char *dbName, const char *tableName)
///
/// @brief Get a description of the fields of a table.
///
/// @param database A pointer to a Database object that manages the system.
/// @param dbName The name of the database in the database management system.
/// @param tableName The name of the table to describe.
///
/// @return On success, returns a two-column DbResult in the format
/// "fieldName", "typeInfo", where fieldName is a Bytes object containing the
/// name of a field and typeInfo is a pointer to a TypeDescriptor describing
/// the type of data represented by the field.  On failure, returns
/// EMPTY_DB_RESULT.
DbResult* dbDescribeTable(Database *database, const char *dbName,
  const char *tableName
) {
  printLog(TRACE,
    "ENTER dbDescribeTable(database=%p, dbName=\"%s\", tableName=\"%s\")\n",
    database, dbName, tableName);
  
  DbResult *returnValue = NULL;
  
  if ((database == NULL) || (dbName == NULL) || (tableName == NULL)) {
    printLog(ERR, "Parameter is NULL.\n");
    printLog(TRACE,
      "EXIT dbDescribeTable(database=%p, dbName=\"%s\", tableName=\"%s\") "
      "= {NOT successful}\n", database, dbName, tableName);
    returnValue = (DbResult*) calloc(1, sizeof(DbResult));
    if (returnValue == NULL) {
      LOG_MALLOC_FAILURE();
      exit(1);
    }
    return returnValue;
  }
  
  dbWaitForTableUnlocked(database, dbName, tableName);
  returnValue = database->describeTable(database->db, dbName, tableName);
  
  if (returnValue->successful == true) {
    if (database->dbType == SQL) {
      for (u64 row = 1; row < returnValue->numRows; row++) {
        // Determine the TypeDescriptor we should have.
        char *typeInfo = (char*) returnValue->rows[row][1];
        TypeDescriptor *type = sqlTypeNameToTypeDescriptor(typeInfo);
        returnValue->rows[row][1]
          = (void*) bytesDestroy((Bytes) returnValue->rows[row][1]);
        returnValue->rows[row][1] = type;
      }
    }
    
    returnValue->fieldTypes[1] = NULL; // No type for TypeDescriptor field.
  }
  
  printLog(TRACE,
    "EXIT dbDescribeTable(database=%p, dbName=\"%s\", tableName=\"%s\") "
    "= {%s}\n", database, dbName, tableName,
    (returnValue->successful == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn bool dbIsTableLocked(Database *database, const char *dbName, const char *tableName)
///
/// @brief Determine if a table in a database is locked.
///
/// @param database A pointer to a Database object that manages the system.
/// @param dbName The name of the database in the database management system.
/// @param tableName The name of the table to check.
///
/// @return Returns true if the table is locked by another thread, false if not.
bool dbIsTableLocked(Database *database, const char *dbName,
  const char *tableName
) {
  printLog(FLOOD,
    "ENTER dbIsTableLocked(database=%p, dbName=\"%s\", tableName=\"%s\")\n",
    database, dbName, tableName);
  
  bool lockedByOtherThread = false;
  if (database == NULL) {
    printLog(ERR, "NULL database parameter provided.\n");
    printLog(FLOOD,
      "EXIT dbIsTableLocked(database=%p, dbName=\"%s\", tableName=\"%s\") "
      "= {%s}\n", database, dbName, tableName,
      (lockedByOtherThread == true) ? "true" : "false");
    return lockedByOtherThread; // false
  }
  
  if (database->lockedTables->size == 0) {
    printLog(FLOOD,
      "EXIT dbIsTableLocked(database=%p, dbName=\"%s\", tableName=\"%s\") "
      "= {false}\n", database, dbName, tableName);
    return false;
  }
  
  Bytes fullTableName = NULL;
  bytesAddStr(&fullTableName, dbName);
  bytesAddStr(&fullTableName, dbInstance);
  bytesAddStr(&fullTableName, ".");
  bytesAddStr(&fullTableName, tableName);
  
  lockedByOtherThread = true;
  u64 threadId = (u64) thrd_current();
  mtx_lock(database->lockedTables->lock);
  TypeDescriptor *keyType = database->lockedTables->keyType;
  DictionaryEntry *cur = dictionaryGetEntry(database->lockedTables, fullTableName);
  if (cur == NULL) {
    lockedByOtherThread = false;
  } else {
    for (;
      (cur != NULL) && (keyType->compare(cur->key, fullTableName) == 0);
      cur = cur->next
    ) {
      u64 *lockingThread = (u64*) cur->value;
      if (*lockingThread == threadId) {
        lockedByOtherThread = false;
        break;
      }
    }
  }
  mtx_unlock(database->lockedTables->lock);
  
  fullTableName = bytesDestroy(fullTableName);
  
  printLog(FLOOD,
    "EXIT dbIsTableLocked(database=%p, dbName=\"%s\", tableName=\"%s\") "
    "= {%s}\n", database, dbName, tableName,
    (lockedByOtherThread == true) ? "true" : "false");
  return lockedByOtherThread;
}

/// @fn void dbWaitForTableUnlocked(Database *database, const char *dbName, const char *tableName)
///
/// @brief Block until the specified table is released by the thread with a
/// lock.
///
/// @param database A pointer to a Database object that manages the system.
/// @param dbName The name of the database in the database management system.
/// @param tableName The name of the table to wait on.
///
/// @return This function returns no value.
void dbWaitForTableUnlocked(Database *database, const char *dbName,
  const char *tableName
) {
  printLog(TRACE,
    "ENTER dbWaitForTableUnlocked(database=%p, dbName=\"%s\", "
    "tableName=\"%s\")\n", database, dbName, tableName);
  
  mtx_lock(&database->lockedTablesMutex);
  while (dbIsTableLocked(database, dbName, tableName)) {
    cnd_wait(&database->lockedTablesCondition,
      &database->lockedTablesMutex);
  }
  mtx_unlock(&database->lockedTablesMutex);
  
  printLog(TRACE,
    "EXIT dbWaitForTableUnlocked(database=%p, dbName=\"%s\", "
    "tableName=\"%s\") = {}\n", database, dbName, tableName);
  return;
}

/// @fn bool dbRenameTable(Database *database, const char *dbName, const char *oldTableName, const char *newTableName)
///
/// @brief Rename a table in a database.
///
/// @param database A pointer to a Database object that manages the system.
/// @param dbName The name of the database in the database management system.
/// @param oldTableName The name of the table as it exists in the database at
///   the time this call is made.
/// @param newTableName The deisred new name of the table.
///
/// @return Returns true on success, false on failure.
bool dbRenameTable(Database *database, const char *dbName,
  const char *oldTableName, const char *newTableName
) {
  printLog(TRACE, "ENTER dbRenameTable(database=%p, dbName=\"%s\", "
    "oldTableName=\"%s\", newTableName=\"%s\")\n",
    database, dbName, oldTableName, newTableName);
  
  bool returnValue = false;
  if (database == NULL) {
    printLog(ERR, "NULL database parameter provided.\n");
    printLog(TRACE, "EXIT dbRenameTable(database=%p, dbName=\"%s\", "
      "oldTableName=\"%s\", newTableName=\"%s\") = {%s}\n",
      database, dbName, oldTableName, newTableName,
      (returnValue == true) ? "successful" : "NOT successful");
    return returnValue; // false
  }
  
  dbWaitForTableUnlocked(database, dbName, oldTableName);
  returnValue
    = database->renameTable(database->db, dbName, oldTableName, newTableName);
  
  printLog(TRACE, "EXIT dbRenameTable(database=%p, dbName=\"%s\", "
    "oldTableName=\"%s\", newTableName=\"%s\") = {%s}\n",
    database, dbName, oldTableName, newTableName,
    (returnValue == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// void* dbSetResultByIndex(DbResult *dbResult, u64 resultIndex, u64 fieldIndex, const volatile void *value)
///
/// @brief Set a result in a dbResult returned by a previous query.
///
/// @param dbResult The result from a previous query.
/// @param resultIndex The zero-based index of the row to set the
///   information int.  Note that headers are not considered in the search
///   even though they're row 0 of the results table.
/// @param fieldIndex The zero-based index of the field to update.
/// @param value The new value to set.
///
/// @return Returns a pointer to the new value in the results table of the
/// provided DbResult on success, NULL value on failure.
void* dbSetResultByIndex(DbResult *dbResult,
  u64 resultIndex, u64 fieldIndex, const volatile void *value
) {
  if (dbResult == NULL) {
    printLog(TRACE,
      "ENTER dbSetResultByIndex(dbName=\"%s\", tableName=\"%s\", "
      "resultIndex=%llu, fieldIndex=%llu, value=%p)\n",
      "NULL", "NULL", llu(resultIndex), llu(fieldIndex),
      value);
    printLog(ERR, "dbResult is NULL\n");
    printLog(TRACE,
      "EXIT dbSetResultByIndex(dbName=\"%s\", tableName=\"%s\", "
      "resultIndex=%llu, fieldIndex=%llu, value=%p) = {NULL}\n",
      "NULL", "NULL", llu(resultIndex), llu(fieldIndex),
      value);
    return NULL;
  }
  
  printLog(TRACE,
    "ENTER dbSetResultByIndex(dbName=\"%s\", tableName=\"%s\", "
    "resultIndex=%llu, fieldIndex=%llu, value=%p)\n",
    dbResult->dbName, dbResult->tableName, llu(resultIndex), llu(fieldIndex),
    value);
  
  resultIndex++; // Convert to match the row of the results table.
  if (dbResult->rows == NULL) {
    printLog(ERR, "dbResult->rows is NULL.\n");
    printLog(TRACE,
      "EXIT dbSetResultByIndex(dbName=\"%s\", tableName=\"%s\", "
      "resultIndex=%llu, fieldIndex=%llu, value=%p) = {NULL}\n",
      dbResult->dbName, dbResult->tableName, llu(resultIndex), llu(fieldIndex),
      value);
    return NULL;
  } else if (resultIndex >= dbResult->numRows) {
    printLog(WARN, "resultIndex out of range for DbResult.\n");
    printLog(TRACE,
      "EXIT dbSetResultByIndex(dbName=\"%s\", tableName=\"%s\", "
      "resultIndex=%llu, fieldIndex=%llu, value=%p) = {NULL}\n",
      dbResult->dbName, dbResult->tableName, llu(resultIndex), llu(fieldIndex),
      value);
    return NULL;
  } else if (fieldIndex >= dbResult->numFields) {
    printLog(WARN, "fieldIndex out of range for DbResult.\n");
    printLog(TRACE,
      "EXIT dbSetResultByIndex(dbName=\"%s\", tableName=\"%s\", "
      "resultIndex=%llu, fieldIndex=%llu, value=%p) = {NULL}\n",
      dbResult->dbName, dbResult->tableName, llu(resultIndex), llu(fieldIndex),
      value);
    return NULL;
  }
  
  void *returnValue = NULL;
  if (dbResult->fieldTypes[fieldIndex] == typeString) {
    // Strings are actually Bytes objects.  This is the usual case.
    bytesDestroy((Bytes) dbResult->rows[resultIndex][fieldIndex]);
    returnValue = typeBytes->copy(value);
  } else {
    dbResult->fieldTypes[fieldIndex]->destroy(
      dbResult->rows[resultIndex][fieldIndex]);
    returnValue = dbResult->fieldTypes[fieldIndex]->copy(value);
  }
  // NULL is a valid input and return value, so we can't check for that.
  dbResult->rows[resultIndex][fieldIndex] = returnValue;
  
  printLog(TRACE,
    "EXIT dbSetResultByIndex(dbName=\"%s\", tableName=\"%s\", "
    "resultIndex=%llu, fieldIndex=%llu, value=%p) = {%p}\n",
    dbResult->dbName, dbResult->tableName, llu(resultIndex), llu(fieldIndex),
    value, returnValue);
  return returnValue;
}

/// void* dbSetResultByName(DbResult *dbResult, u64 resultIndex, const char *fieldName, const volatile void *value)
///
/// @brief Set a result from a dbResult returned by a previous query.
///
/// @param dbResult The result from a previous query.
/// @param resultIndex The zero-based index of the row to set the
///   information int.  Note that headers are not considered in the search
///   even though they're row 0 of the results table.
/// @param fieldName The name of the field to update.
/// @param value The new value to set.
///
/// @return Returns a pointer to the new value in the results table of the
/// provided DbResult on success, NULL value on failure.
void* dbSetResultByName(DbResult *dbResult,
  u64 resultIndex, const char *fieldName, const volatile void *value
) {
  if (dbResult == NULL) {
    printLog(TRACE,
      "ENTER dbSetResultByName(dbName=\"%s\", tableName=\"%s\", resultIndex=%llu, "
      "fieldName=\"%s\", value=%p)\n",
      "NULL", "NULL", llu(resultIndex), fieldName, value);
    printLog(ERR, "dbResult is NULL.\n");
    printLog(TRACE,
      "EXIT dbSetResultByName(dbName=\"%s\", tableName=\"%s\", resultIndex=%llu, "
      "fieldName=\"%s\", value=%p) = {NULL}\n",
      "NULL", "NULL", llu(resultIndex), fieldName,
      value);
    return NULL;
  }
  
  printLog(TRACE,
    "ENTER dbSetResultByName(dbName=\"%s\", tableName=\"%s\", resultIndex=%llu, "
    "fieldName=\"%s\", value=%p)\n",
    dbResult->dbName, dbResult->tableName, llu(resultIndex), fieldName, value);
  
  i64 fieldIndex = dbGetFieldIndexByName(dbResult, fieldName);
  if (fieldIndex < 0) {
    printLog(TRACE,
      "EXIT dbSetResultByName(dbName=\"%s\", tableName=\"%s\", resultIndex=%llu, "
      "fieldName=\"%s\", value=%p) = {NULL}\n",
      dbResult->dbName, dbResult->tableName, llu(resultIndex), fieldName,
      value);
    return NULL;
  }
  
  void *returnValue
    = dbSetResultByIndex(dbResult, resultIndex, (u64) fieldIndex, value);
  
  printLog(TRACE,
    "EXIT dbSetResultByName(dbName=\"%s\", tableName=\"%s\", resultIndex=%llu, "
    "fieldName=\"%s\", value=%p) = {%p}\n",
    dbResult->dbName, dbResult->tableName, llu(resultIndex), fieldName,
    value, returnValue);
  return returnValue;
}

/// @fn bool dbCreateTableFromResult(Database *database, const char *dbName, const char *tableName, DbResult *dbResult)
///
/// @brief Create a table based off of information pulled from a previous
/// query.  If the table already exists in the target database as specified
/// in the dbResult, this function takes no action.  If the table exists in
/// the target database with a different schema, it is first deleted and then
/// recreated.
///
/// @param database A pointer to the Database object to create the new table
///   within.
/// @param dbName The name of the database in the Database object to create
///   the new table in.
/// @param tableName The desired name of the new table.  The name of the table
///   in the DbResult will be used if this parameter is NULL.
/// @param dbResult A pointer to a previously-returned DbResult.  The result
///   can be from any database.  Only the table metadata will be used.
///
/// @return Returns true on success, false on failure.
bool dbCreateTableFromResult(Database *database, const char *dbName,
  const char *tableName, DbResult *dbResult
) {
  SCOPE_ENTER("database=%p, dbName=\"%s\", tableName=\"%s\", dbResult=%p",
    database, dbName, tableName, dbResult);
  
  bool returnValue = true;
  
  if ((database == NULL) || (dbName == NULL)
    || (*dbName == '\0') || (dbResult == NULL)
  ) {
    printLog(ERR, "One or more NULL parameters.\n");
    returnValue = false;
    SCOPE_EXIT(
      "database=%p, dbName=\"%s\", tableName=\"%s\", dbResult=%p", "%s",
      database, dbName, tableName, dbResult,
      (returnValue == true) ? "successful" : "NOT successful");
    return returnValue;
  }
  
  if (tableName == NULL) {
    // Use the one in the DbResult.
    tableName = dbResult->tableName;
  }
  
  bool tableMatches = false; // until proven true
  DbResult *tableDescription = (DbResult*) scopeAdd(
    dbDescribeTable(database, dbName, tableName), dbFreeResult);
  const char **fieldNames = dbGetFieldNames(dbResult);
  TypeDescriptor **fieldTypes = dbResult->fieldTypes;
  if (tableDescription->numResults == dbResult->numFields) {
    tableMatches = true; // until proven false
    for (u64 index = 0; index < dbResult->numFields; index++) {
      if (strcmp(dbGetStringByIndex(tableDescription, index, 0),
        fieldNames[index]) != 0
      ) {
        printLog(DEBUG,
          "Expected field name \"%s\" at index %llu, found \"%s\".\n",
          fieldNames[index], llu(index),
          dbGetStringByIndex(tableDescription, index, 0));
        tableMatches = false;
        break;
      } else if (dbGetResultByIndex(tableDescription, index, 1, NULL)
        != fieldTypes[index]
      ) {
        printLog(DEBUG, "Expected type %s for field %s, found %s.\n",
          fieldTypes[index]->name, fieldNames[index],
          ((TypeDescriptor*) dbGetResultByIndex(tableDescription, index, 1, NULL))->name);
        tableMatches = false;
        break;
      }
    }
  }
  
  if (tableMatches == true) {
    // Table already exists as it is needed.  No need to do anything else.
    SCOPE_EXIT("database=%p, dbName=\"%s\", tableName=\"%s\", dbResult=%p", "%s",
      database, dbName, tableName, dbResult,
      (returnValue == true) ? "successful" : "NOT successful");
    return returnValue;
  }
  
  // If we got this far, the either the tables don't match or there is no such
  // table in the destination database.  Delete it if it exists.
  if (tableDescription->numResults > 0) {
    returnValue = dbDeleteTable(database, dbName, tableName);
  }
  scopeDestroy(tableDescription);
  
  if (dbResult->database == NULL) {
    returnValue = false;
    SCOPE_EXIT("database=%p, dbName=\"%s\", tableName=\"%s\", dbResult=%p", "%s",
      database, dbName, tableName, dbResult,
      (returnValue == true) ? "successful" : "NOT successful");
    return returnValue;
  }
  tableDescription = (DbResult*) scopeAdd(
    dbDescribeTable(dbResult->database, dbResult->dbName, dbResult->tableName),
    dbFreeResult);
  // We need the second-level details for constructing the table list.
  DbResult *tableDetails = (DbResult*) scopeAdd(
    dbResult->database->describeTable(
      dbResult->database->db, dbResult->dbName, dbResult->tableName),
    dbFreeResult);
  
  List *tableList = (List*) scopeAdd(listCreate(typeString), listDestroy);
  if (tableList == NULL) {
    LOG_MALLOC_FAILURE();
    returnValue = false;
    SCOPE_EXIT("database=%p, dbName=\"%s\", tableName=\"%s\", dbResult=%p", "%s",
      database, dbName, tableName, dbResult,
      (returnValue == true) ? "successful" : "NOT successful");
    return returnValue;
  }
  char *primaryKey = NULL;
  scopeAdd(straddstr(&primaryKey, ""), pointerDestroyFunction);
  for (u64 index = 0; index < dbResult->numFields; index++) {
    listAddBackEntry(tableList, fieldNames[index], fieldTypes[index],
      typePointerNoOwn);
    if (strcmp(dbGetStringByName(tableDescription, index, "primaryKey"),
      "true") == 0
    ) {
      if (*primaryKey != '\0') {
        scopeUpdate(primaryKey, straddstr(&primaryKey, ", "));
      }
      scopeUpdate(primaryKey, straddstr(&primaryKey, fieldNames[index]));
      
      // We may need the second-level information here.
      if (strstrci(dbGetStringByName(tableDetails, index, "typeInfo"),
        "char")
      ) {
        // Replace the type with the VARCHAR information.
        listRemoveBack(tableList);
        Bytes fieldLength
          = getBytesBetween(
            dbGetStringByName(tableDetails, index, "typeInfo"), "(", ")");
        listAddBackEntry(tableList, fieldNames[index], fieldLength, typeString);
        fieldLength = bytesDestroy(fieldLength);
      }
    } else if ((strstrci(dbGetStringByName(tableDetails, index, "typeInfo"),
      "text")) && (fieldTypes[index] != typeString)
    ) {
      // This may have come in as a blob when it should have come in as text.
      // Fix it.
      listRemoveBack(tableList);
      listAddBackEntry(tableList, fieldNames[index], typeString,
        typePointerNoOwn);
    }
  }
  
  if (returnValue == true) {
    returnValue = dbAddTableList(database, dbName,
      tableName, primaryKey, tableList);
  }
  
  SCOPE_EXIT("database=%p, dbName=\"%s\", tableName=\"%s\", dbResult=%p", "%s",
    database, dbName, tableName, dbResult,
    (returnValue == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn int dbCompare(Database *db1, Database *db2)
///
/// @brief Compare two Databases to see if they point to the same underlying
/// instance.
///
/// @param db1 A pointer to the first Database instance.
/// @param db2 A pointer to the second Database instance.
///
/// @return Returns 0 if the two underlying databases identical, non-zero
/// otherwise.
int dbCompare(Database *db1, Database *db2) {
  SCOPE_ENTER("db1=%p, db2=%p", db1, db2);
  
  int returnValue = (db1 != NULL) - (db2 != NULL);
  if (returnValue != 0) {
    SCOPE_EXIT("db1=%p, db2=%p", "%d", db1, db2, returnValue);
    return returnValue;
  }
  
  returnValue = ((int) db1->dbType) - ((int) db2->dbType);
  if (returnValue != 0) {
    SCOPE_EXIT("db1=%p, db2=%p", "%d", db1, db2, returnValue);
    return returnValue;
  }
  
  // The databases are of the same type, so the compare function is the same
  // on both pointers and we can use either one for the comparison operation.
  returnValue = db1->compare(db1->db, db2->db);
  
  SCOPE_EXIT("db1=%p, db2=%p", "%d", db1, db2, returnValue);
  return returnValue;
}

/// @fn DbResult* bytesTableToDbResult(Database *database, const char *dbName, const char *tableName, Bytes **table)
///
/// @brief Convert a Bytes table to a DbResult.
///
/// @note This will cause the returned DbResult to own the table.  It should
/// be freed with dbFreeResult *NOT* freeBytesTable.
///
/// @param database A pointer to the Database object the DbResult will belong
///   to.
/// @param dbName The name of the database the DbResult is associated with.
/// @param tableName The name of the table the DbResut represents.
/// @param table The two-dimensional array of Bytes objects to use as data.
///   Row 0 must be the field names.
///
/// @return Returns a pointer to a newly-allocated DbResult that owns the input
/// table on success, NULL on failure.
DbResult* bytesTableToDbResult(Database *database, const char *dbName,
  const char *tableName, Bytes **table
) {
  SCOPE_ENTER("table=%p", table);
  
  DbResult *returnValue = NULL;
  if (table == NULL) {
    printLog(ERR, "NULL table provided.\n");
    SCOPE_EXIT("table=%p", "%p", table, returnValue);
    return returnValue;
  }
  
  returnValue = (DbResult*) calloc(1, sizeof(DbResult));
  if (returnValue == NULL) {
    LOG_MALLOC_FAILURE();
    SCOPE_EXIT("table=%p", "%p", table, returnValue);
    return returnValue;
  }
  
  returnValue->rows = (void***) table;
  returnValue->successful = true;
  
  u64 numRows = 0;
  for (; table[numRows] != NULL; numRows++);
  returnValue->numRows = numRows;
  
  u64 numFields = 0;
  if (numRows > 0) {
    returnValue->numResults = numRows - 1;
    for (; table[0][numFields] != NULL; numFields++);
  }
  returnValue->numFields = numFields;
  
  TypeDescriptor **fieldTypes
    = (TypeDescriptor**) calloc(1, sizeof(TypeDescriptor*) * numFields);
  if (fieldTypes == NULL) {
    LOG_MALLOC_FAILURE();
    returnValue = (DbResult*) pointerDestroy(returnValue);
    SCOPE_EXIT("table=%p", "%p", table, returnValue);
    return returnValue;
  }
  for (u64 i = 0; i < numFields; i++) {
    fieldTypes[i] = typeBytes;
  }
  returnValue->fieldTypes = fieldTypes;
  
  dbFinalizeResult(returnValue, database, dbName, tableName);
  
  SCOPE_EXIT("table=%p", "%p", table, returnValue);
  return returnValue;
}

/// @fn bool dbUpdateField_(Database *database, const char *dbName, const char *tableName, const char *fieldName, const void *newValue, ...)
///
/// @brief Update a field in a table with a new value where a set of criteria
/// is met.
///
/// @param database A pointr to a Database object to operate on.
/// @param dbName The name of the database in the SqlDatabase to operate on.
/// @param tableName The name of the table in the database the field is in.
/// @param fieldName The name of the field to update.
/// @param newValue The desired new value for the field when the specified
///   conditions are met.
/// @param ... Key-value pairs where the even indexes are the names of the
///   fields to match and the odd indexes are the values required for those
///   fields.
///
/// @return Returns true if the underlying call was successful, false if not.
bool dbUpdateField_(Database *database, const char *dbName,
  const char *tableName, const char *fieldName, const void *newValue, ...
) {
  SCOPE_ENTER("database=%p, dbName=%s, tableName=%s, fieldName=%s, newValue=%s",
    database, strOrNull(dbName), strOrNull(tableName), strOrNull(fieldName),
    strOrNull(newValue));
  
  bool returnValue = false;
  if (database == NULL) {
    printLog(ERR, "NULL database parameter provided.\n");
    SCOPE_EXIT("database=%p, dbName=%s, tableName=%s, fieldName=%s, newValue=%s",
      "%s", database, strOrNull(dbName), strOrNull(tableName),
      strOrNull(fieldName), strOrNull(newValue), boolNames[returnValue]);
    return returnValue;
  }
  
  va_list args;
  va_start(args, newValue);
  dbWaitForTableUnlocked(database, dbName, tableName);
  returnValue = database->updateFieldVargs(database->db, dbName,
    tableName, fieldName, newValue, args);
  va_end(args);
  
  SCOPE_EXIT("database=%p, dbName=%s, tableName=%s, fieldName=%s, newValue=%s",
    "%s", database, strOrNull(dbName), strOrNull(tableName),
    strOrNull(fieldName), strOrNull(newValue), boolNames[returnValue]);
  return returnValue;
}

/// @fn i64 dbGetNumRecords(Database *database, const char *dbName, const char *tableName)
///
/// @brief Get the number of records in an existing table.
///
/// @param database A pointer to the Database object representing the database
///   system to query.
/// @param dbName The name of the database the table is in.
/// @param tableName The name of the table to query.
///
/// @return Returns the number of records in the table which may be zero on
/// success, -1 on failure.
i64 dbGetNumRecords(Database *database,
  const char *dbName, const char *tableName
) {
  printLog(TRACE,
    "ENTER dbGetNumRecords(database=%p, dbName=\"%s\", tableName=\"%s\")\n",
    database, dbName, tableName);
  
  i64 returnValue = -1;
  if ((database == NULL) || (dbName == NULL) || (tableName == NULL)) {
    printLog(ERR, "One or more NULL parameters.\n");
    printLog(TRACE,
      "EXIT dbGetNumRecords(database=%p, dbName=\"%s\", tableName=\"%s\") "
      "= {%lld}\n", database, dbName, tableName, lld(returnValue));
    return returnValue; // -1
  }
  
  // Variable argument variables
  returnValue
    = database->getNumRecords(database->db, dbName, tableName);
  
  printLog(TRACE,
    "EXIT dbGetNumRecords(database=%p, dbName=\"%s\", tableName=\"%s\") "
    "= {%lld}\n", database, dbName, tableName, lld(returnValue));
  return returnValue;
}

/// @fn Bytes dbResultToBytes_(const DbResult *dbResult, const char *recordDelimiter, const char *fieldDelimiter, ...)
///
/// @brief Convert a DbResult to its corresponding bytes value.
///
/// @param dbResult The DbResult to convert.
/// @param recordDelimiter The record delimiter string to use.  If this
///   parameter is NULL, the default value of recordDelim will be used.
/// @param fieldDelimiter The field delimiter string to use.  If this
///   parameter is NULL, the default value of fieldDelim will be used.
/// @param ... All further parameters are ignored.
///
/// @note This function is wrapped by a macro of the same name (minus the
/// trailing underscore) that automatically provides NULL for the
/// recordDelimiter and fieldDelimiter parameters.
///
/// @return Returns a new Bytes object with the Bytes data on success,
/// NULL on failure.
Bytes dbResultToBytes_(const DbResult *dbResult,
  const char *recordDelimiter, const char *fieldDelimiter, ...
) {
  printLog(TRACE, "ENTER dbResultToBytes(dbResult=%p)\n", dbResult);
  
  if (dbResult == NULL) {
    printLog(TRACE, "EXIT dbResultToBytes(dbResult=NULL) = {NULL}\n");
    return NULL;
  }
  
  if (recordDelimiter == NULL) {
    recordDelimiter = recordDelim;
  }
  if (fieldDelimiter == NULL) {
    fieldDelimiter = fieldDelim;
  }
  
  // Convert the delimiters to Bytes objects for faster access.
  Bytes fdelim = NULL;
  bytesAddStr(&fdelim, fieldDelimiter);
  Bytes rdelim = NULL;
  bytesAddStr(&rdelim, recordDelimiter);
  
  Bytes returnValue = NULL;
  u64 firstRowLength = 0;
  // Go through the first row and get the number of bytes the output would
  // contain.
  u64 numFields = dbResult->numFields;
  for (u64 fieldIndex = 0; fieldIndex < numFields; ++fieldIndex) {
    if ((dbResult->fieldTypes[fieldIndex] == typeString)
      || (dbResult->fieldTypes[fieldIndex] == typeBytes)
    ) {
      // Field is encodes as a Bytes object.
      firstRowLength += bytesLength(dbGetBytesByIndex(dbResult, 0, fieldIndex));
    } else if ((dbResult->fieldTypes[fieldIndex] == typeI64)) {
      // Assume 5 digits.
      firstRowLength += 5;
    } else if ((dbResult->fieldTypes[fieldIndex] == typeDouble)) {
      // Assume 10 digits.
      firstRowLength += 10;
    }
  }
  // There will be a field delimiter between each field and a record delimiter
  // at the end.
  // Add (bytesLength(fdelim) * numFields) + bytesLength(rdelim).
  firstRowLength += (bytesLength(fdelim) * numFields) + bytesLength(rdelim);
  
  // Estimate the return value's size.
  bytesAllocate(&returnValue, dbResult->numRows * firstRowLength);
  if (returnValue == NULL) {
    // malloc failure weas already logged.  Just return.
    return returnValue; // NULL
  }
  
  // First, add in the headers.
  // All field names are Bytes values.
  for (u64 field = 0; field < numFields; ++field) {
    Bytes fieldName = (Bytes) dbResult->rows[0][field];
    bytesAddBytes(&returnValue, fieldName);
    
    if (field < numFields - 1) {
      bytesAddBytes(&returnValue, fdelim);
    }
  }
  u64 numResults = dbResult->numResults;
  if (numResults > 0) {
    bytesAddBytes(&returnValue, rdelim);
  }
  
  Bytes fieldValue = NULL;
  int typeStringIndex = getIndexFromTypeDescriptor(typeString);
  int typeBytesNoCopyIndex = getIndexFromTypeDescriptor(typeBytesNoCopy);
  TypeDescriptor **fieldTypes = dbResult->fieldTypes;
  for (u64 row = 0; row < numResults; row++) {
    for (u64 field = 0; field < numFields; field++) {
      int typeIndex = getIndexFromTypeDescriptor(fieldTypes[field]);
      if ((typeIndex >= typeStringIndex)
        && (typeIndex <= typeBytesNoCopyIndex)
      ) {
        // This is the expected case.
        fieldValue = dbGetBytesByIndex(dbResult, row, field);
        bytesAddBytes(&returnValue, fieldValue);
      } else if (typeIndex < typeStringIndex) {
        // Don't quote non-string data.  Some spreadsheet applications have
        // issues working with quoted numeric data.
        fieldValue = fieldTypes[field]->toBytes(
          dbGetResultByIndex(dbResult, row, field, NULL));
        bytesAddBytes(&returnValue, fieldValue);
        fieldValue = bytesDestroy(fieldValue);
      }
      
      if (field < numFields - 1) {
        bytesAddBytes(&returnValue, fdelim);
      }
    }
    
    if (row < numResults - 1) {
      bytesAddBytes(&returnValue, rdelim);
    }
  }
  
  fdelim = bytesDestroy(fdelim);
  rdelim = bytesDestroy(rdelim);
  
  printLog(TRACE, "EXIT dbResultToBytes(dbResult=%p) = {%s}\n",
    dbResult, str(returnValue));
  return returnValue;
}

/// @fn int dbResultCompare(DbResult *dbResultA, DbResult *dbResultB)
///
/// @brief Compare two DbResult values.
///
/// @param dbResultA A pointer to the first DbResult to compare.
/// @param dbResultB A pointer to the second DbResult to compare.
///
/// @return Returns 0 if the two values are equal, <0 if dbResultA evaluates
/// as being less than dbResultB, >0 if dbResultA evaluates as being greater
/// than dbResultB.
int dbResultCompare(DbResult *dbResultA, DbResult *dbResultB) {
  SCOPE_ENTER("dbResultA=%p, dbResultB=%p", dbResultA, dbResultB);
  
  int returnValue = 0;
  
  if (dbResultA == NULL) {
    printLog(WARN, "dbResultA is NULL.\n");
    returnValue--;
  }
  if (dbResultB == NULL) {
    printLog(WARN, "dbResultB is NULL.\n");
    returnValue++;
  }
  if ((dbResultA == NULL) || (dbResultB == NULL)) {
    SCOPE_EXIT("dbResultA=%p, dbResultB=%p", "%d",
      dbResultA, dbResultB, returnValue);
    return returnValue;
  }
  
  if (dbResultA->numFields != dbResultB->numFields) {
    if (dbResultA->numFields < dbResultB->numFields) {
      returnValue--;
    } else {
      returnValue++;
    }
    SCOPE_EXIT("dbResultA=%p, dbResultB=%p", "%d",
      dbResultA, dbResultB, returnValue);
    return returnValue;
  }
  
  u64 numFields = dbResultA->numFields;
  const char **fieldNamesA = dbGetFieldNames(dbResultA);
  const char **fieldNamesB = dbGetFieldNames(dbResultB);
  int (*stringCompare)(const volatile void*, const volatile void*)
    = typeString->compare;
  TypeDescriptor **fieldTypesA = dbResultA->fieldTypes;
  TypeDescriptor **fieldTypesB = dbResultB->fieldTypes;
  for (u64 i = 0; i < numFields; i++) {
    returnValue = stringCompare(fieldNamesA[i], fieldNamesB[i]);
    if (returnValue != 0) {
      SCOPE_EXIT("dbResultA=%p, dbResultB=%p", "%d",
        dbResultA, dbResultB, returnValue);
      return returnValue;
    }
    
    returnValue
      = (int) (((intptr_t) fieldTypesA[i]) - ((intptr_t) fieldTypesB[i]));
    if (returnValue != 0) {
      printLog(DEBUG, "fieldTypeA is %s, fieldTypeB is %s\n",
        fieldTypesA[i]->name, fieldTypesB[i]->name);
      SCOPE_EXIT("dbResultA=%p, dbResultB=%p", "%d",
        dbResultA, dbResultB, returnValue);
      return returnValue;
    }
  }
  // Field names and field types are identical.
  
  if (dbResultA->numResults != dbResultB->numResults) {
    if (dbResultA->numResults < dbResultB->numResults) {
      returnValue--;
    } else {
      returnValue++;
    }
    SCOPE_EXIT("dbResultA=%p, dbResultB=%p", "%d",
      dbResultA, dbResultB, returnValue);
    return returnValue;
  }
  // Number of results is identical.
  
  u64 numResults = dbResultA->numResults;
  int (*bytesCompare)(const volatile void*, const volatile void*)
    = typeBytes->compare;
  int (*valueCompare)(const volatile void*, const volatile void*) = NULL;
  for (u64 i = 0; i < numResults; i++) {
    for (u64 j = 0; j < numFields; j++) {
      void *valueA = dbGetResultByIndex(dbResultA, i, j, NULL);
      void *valueB = dbGetResultByIndex(dbResultB, i, j, NULL);
      valueCompare = bytesCompare;
      if ((fieldTypesA[j] != typeString) && (fieldTypesA[j] != typeBytes)) {
        valueCompare = fieldTypesA[j]->compare;
      }
      returnValue = valueCompare(valueA, valueB);
      if (returnValue != 0) {
        SCOPE_EXIT("dbResultA=%p, dbResultB=%p", "%d",
          dbResultA, dbResultB, returnValue);
        return returnValue;
      }
    }
  }
  
  SCOPE_EXIT("dbResultA=%p, dbResultB=%p", "%d",
    dbResultA, dbResultB, returnValue);
  return returnValue;
}

/// @fn DbResult* dbResultCopy(DbResult *dbResult)
///
/// @brief Copy an instance of a DbResult to a new one.
///
/// @param dbResult A pointer to the DbResult to copy.
///
/// @return Returns a copy of the input dbResult on success, NULL on failure.
DbResult* dbResultCopy(DbResult *dbResult) {
  SCOPE_ENTER("dbResult=%p", dbResult);
  
  DbResult *returnValue = NULL;
  
  if (dbResult == NULL) {
    printLog(DEBUG, "No input DbResult provided.\n");
    SCOPE_EXIT("dbResult=NULL", "%p", returnValue);
    return returnValue;
  }
  
  returnValue = (DbResult*) calloc(1, sizeof(DbResult));
  if (returnValue == NULL) {
    LOG_MALLOC_FAILURE();
    return returnValue; // NULL
  }
  
  returnValue->numRows = dbResult->numRows;
  u64 numRows = dbResult->numRows;
  returnValue->numResults = dbResult->numResults;
  returnValue->numFields = dbResult->numFields;
  u64 numFields = dbResult->numFields;
  
  TypeDescriptor **fieldTypes = dbResult->fieldTypes;
  returnValue->fieldTypes
    = (TypeDescriptor**) calloc(1, numFields * sizeof(TypeDescriptor));
  if (returnValue->fieldTypes == NULL) {
    LOG_MALLOC_FAILURE();
    returnValue = dbFreeResult(returnValue);
    return returnValue; // NULL
  }
  for (u64 j = 0; j < numFields; j++) {
    returnValue->fieldTypes[j] = fieldTypes[j];
  }
  
  returnValue->rows = (void***) calloc(1, (numRows + 1) * sizeof(void**));
  if (returnValue->rows == NULL) {
    LOG_MALLOC_FAILURE();
    returnValue = dbFreeResult(returnValue);
    return returnValue; // NULL
  }
  
  returnValue->rows[0]
    = (void**) calloc(1, (numFields + 1) * sizeof(void**));
  for (u64 j = 0; j < numFields; j++) {
    bytesAddBytes((Bytes*) &returnValue->rows[0][j],
      (Bytes) dbResult->rows[0][j]);
  }
  
  for (u64 i = 1; i < numRows; i++) {
    returnValue->rows[i]
      = (void**) calloc(1, (numFields + 1) * sizeof(void**));
    if (returnValue->rows[i] == NULL) {
      LOG_MALLOC_FAILURE();
      returnValue = dbFreeResult(returnValue);
      return returnValue; // NULL
    }
    
    for (u64 j = 0; j < numFields; j++) {
      if (fieldTypes[j] == typeString) {
        // Strings are encoded as Bytes objects.  This is the majority of the
        // data we have, so this is the expected case.
        bytesAddBytes((Bytes*) &returnValue->rows[i][j],
          (Bytes) dbResult->rows[i][j]);
      } else {
        returnValue->rows[i][j] = fieldTypes[j]->copy(dbResult->rows[i][j]);
      }
    }
  }
  
  straddstr(&returnValue->dbName, dbResult->dbName);
  straddstr(&returnValue->tableName, dbResult->tableName);
  returnValue->successful = dbResult->successful;
  returnValue->fieldNameIndexMap = htCopy(dbResult->fieldNameIndexMap);
  returnValue->database = dbResult->database;
  
  SCOPE_EXIT("dbResult=%p", "%p", dbResult, returnValue);
  return returnValue;
}

/// @fn i64 dbGetSize(Database *database, const char *dbName)
///
/// @brief Get the size, in bytes, of a particular databsae.
///
/// @param database A pointer to a Database object used for the connection.
/// @param dbName The name of the database to inquisition.
///
/// @return Returns the number of bytes the database is consuming on success,
/// -1 on failure.
i64 dbGetSize(Database *database, const char *dbName) {
  SCOPE_ENTER("database=%p, dbName=\"%s\"", database, strOrNull(dbName));
  
  i64 returnValue = database->getSize(database->db, dbName);
  
  SCOPE_EXIT("database=%p, dbName=\"%s\"", "%lld", database, strOrNull(dbName),
    lld(returnValue));
  return returnValue;
}

/// @fn DbResult* dbGetOrValuesDict(Database *database, const char *dbString, const char *tableName, const char *select, const char *orderBy, Dictionary *args)
///
/// @brief Low-level method for retrieving values from a table in a database.
///
/// @details This function performs an "OR" on the key-value pairs in the
/// dictionary instead of an "AND" like the other functions do.
///
/// @param database A pointer to the Database object representing the database
///   system to query.
/// @param dbString The name of the database to qurey.
/// @param tableName The name of the table to query.
/// @param select The name of the field to examine.
/// @param orderBy The field(s) to order by, if any.  This parameter may be
///   NULL.
/// @param args The key-value pairs to OR together in the query.
///
/// @return A DbResult with the results of the query.
DbResult* dbGetOrValuesDict(Database *database,
  const char *dbString, const char *tableName,
  const char *select, const char *orderBy, Dictionary *args
) {
  SCOPE_ENTER("database=%p, dbString=\"%s\", tableName=\"%s\", select=\"%s\", "
    "orderBy=\"%s\", args=%p", database, strOrEmpty(dbString),
    strOrEmpty(tableName), strOrEmpty(select), strOrEmpty(orderBy), args);
  
  DbResult *queryResult = database->getOrValuesDict(database->db, dbString,
    tableName, select, orderBy, args);
  u64 numResults = 0;
  if (queryResult != NULL) {
    numResults = queryResult->numResults;
  }
  // numResults is only used in logging, so cast it to a void to keep the
  // compmiler from complaining when logging is disabled.
  (void) numResults;
  
  SCOPE_EXIT("database=%p, dbString=\"%s\", tableName=\"%s\", select=\"%s\", "
    "orderBy=\"%s\", args=%p", "%llu results", database, strOrEmpty(dbString),
    strOrEmpty(tableName), strOrEmpty(select), strOrEmpty(orderBy), args,
    llu(numResults));
  return queryResult;
}

/// @fn Bytes dbLockRecords_(Database *database, const char *dbName, const char *tableName, const char *fieldName, const volatile void *fieldValue, ...)
///
/// @brief Lock a set of records in a database table.
///
/// @note This function is wrapped by a macro of the same name (minus the
/// trailing underscore) that automatically provides NULL as the last argument.
///
/// @param database A pointer to the Database object representing the database
///   system to lock records in.
/// @param dbName The name of the database the records are in.
/// @param tableName The name of the table the records are in.
/// @param fieldName The name of the first field to match for the lock.
/// @param filedValue The value of the first field to match for the lock.
/// @param ... All further parameters are fieldName/fieldValue pairs that are
///   terminated with a NULL fieldName.
///
/// @return Returns a Bytes object containing the locked record criteria on
/// success, NULL on failure.
Bytes dbLockRecords_(Database *database,
  const char *dbName, const char *tableName,
  const char *fieldName, const volatile void *fieldValue, ...
) {
  SCOPE_ENTER(
    "database=%p, dbName=%s, tableName=%s, fieldName=%s, fieldValue=%p",
    database, strOrNull(dbName), strOrNull(tableName),
    strOrNull(fieldName), fieldValue);
  
  Bytes returnValue = NULL;
  
  // Argument check.
  if ((dbName == NULL) || (tableName == NULL)
    || (fieldName == NULL) || (fieldValue == NULL)
  ) {
    printLog(ERR, "One or more NULL parameters.\n");
    SCOPE_EXIT(
      "database=%p, dbName=%s, tableName=%s, fieldName=%s, fieldValue=%p", "%s",
      database, strOrNull(dbName), strOrNull(tableName),
      strOrNull(fieldName), fieldValue, strOrNull(returnValue));
    return returnValue;
  }
  
  u64 threadId = (u64) thrd_current(); // Needed for the lock.
  
  bytesAddStr(&returnValue, dbName);
  bytesAddStr(&returnValue, tableName);
  bytesAddStr(&returnValue, fieldName);
  TypeDescriptor *fieldType
    = dbGetFieldTypeByName(database, dbName, tableName, fieldName);
  Bytes fieldBytes = NULL;
  if (fieldType != NULL) {
    fieldBytes = fieldType->toBytes(fieldValue);
  }
  bytesAddBytes(&returnValue, fieldBytes);
  fieldBytes = bytesDestroy(fieldBytes);
  
  va_list args;
  va_start(args, fieldValue);
  for (fieldName = va_arg(args, char*);
    fieldName != NULL;
    fieldName = va_arg(args, char*)
  ) {
    fieldValue = va_arg(args, void*);
    bytesAddStr(&returnValue, fieldName);
    fieldType = dbGetFieldTypeByName(database, dbName, tableName, fieldName);
    if (fieldType != NULL) {
      fieldBytes = fieldType->toBytes(fieldValue);
    }
    bytesAddBytes(&returnValue, fieldBytes);
    fieldBytes = bytesDestroy(fieldBytes);
  }
  va_end(args);
  
  mtx_lock(database->lockedRecords->lock);
  u64 *lockingThreadId
    = (u64*) rbTreeGetValue(database->lockedRecords, returnValue);
  if ((lockingThreadId != NULL) && (*lockingThreadId == threadId)) {
    // These records are already locked by this thread.  Just return a copy of
    // the key to signify that the lock has been obtained.
    mtx_unlock(database->lockedRecords->lock);
    SCOPE_EXIT(
      "database=%p, dbName=%s, tableName=%s, fieldName=%s, fieldValue=%p", "%s",
      database, strOrNull(dbName), strOrNull(tableName),
      strOrNull(fieldName), fieldValue, strOrNull(returnValue));
    return returnValue;
  } else if (lockingThreadId != NULL) {
    do {
      mtx_unlock(database->lockedRecords->lock);
      dbMSleep(1);
      mtx_lock(database->lockedRecords->lock);
    } while (rbTreeGetValue(database->lockedRecords, returnValue));
  }
  
  // At this point, the records are unlocked and we have the mutex, so add
  // our lock.
  rbTreeAddEntry(database->lockedRecords, returnValue, &threadId, typeU64);
  
  mtx_unlock(database->lockedRecords->lock);
  
  SCOPE_EXIT(
    "database=%p, dbName=%s, tableName=%s, fieldName=%s, fieldValue=%p", "%s",
    database, strOrNull(dbName), strOrNull(tableName),
    strOrNull(fieldName), fieldValue, strOrNull(returnValue));
  return returnValue;
}

/// @fn Bytes dbUnlockRecords(Database *database, Bytes recordLock)
///
/// @brief Unlock a previously-locked set of records in a database.
///
/// @param database A pointer to a Database object that has locked records in
///   it.
/// @param recordLock A Bytes object returned from a previous call to
///   dbLockRecords.
///
/// @return This function always returns NULL.
Bytes dbUnlockRecords(Database *database, Bytes recordLock) {
  SCOPE_ENTER("database=%p, recordLock=%s", database, strOrNull(recordLock));
  
  if (recordLock == NULL) {
    // Not an error, strictly speaking, but nothing to do.
    SCOPE_EXIT("database=%p, recordLock=%s", "NULL",
      database, strOrNull(recordLock));
    return NULL;
  } else if (database == NULL) {
    printLog(ERR, "database parameter is NULL.\n");
    SCOPE_EXIT("database=%p, recordLock=%s", "NULL",
      database, strOrNull(recordLock));
    return NULL;
  }
  
  rbTreeRemove(database->lockedRecords, recordLock);
  recordLock = bytesDestroy(recordLock);
  
  SCOPE_EXIT("database=%p, recordLock=%s", "NULL",
    database, strOrNull(recordLock));
  return NULL;
}

/// @fn TypeDescriptor* dbGetFieldTypeByName(Database *database, const char *dbName, const char *tableName, const char *fieldName)
///
/// @brief Get the type of a field in a table given its name.
///
/// @param database A pointer to a Database object the database and table
///   are in.
/// @param dbName The name of the database in the database management system.
/// @param tableName The name of the table in the database.
/// @param fieldName The name of the field in the table.
///
/// @return Returns the a pointer to the TypeDescriptor for the field on
/// success, NULL on failure.
TypeDescriptor* dbGetFieldTypeByName(Database *database,
  const char *dbName, const char *tableName, const char *fieldName
) {
  SCOPE_ENTER("database=%p, dbName=%s, tableName=%s, fieldName=%s",
    database, strOrNull(dbName), strOrNull(tableName), strOrNull(fieldName));
  
  TypeDescriptor *returnValue = NULL;
  if ((database == NULL) || (dbName == NULL)
    || (tableName == NULL) || (fieldName == NULL)
  ) {
    printLog(ERR, "One or more NULL parameters.\n");
    SCOPE_EXIT("database=%p, dbName=%s, tableName=%s, fieldName=%s", "%s",
      database, strOrNull(dbName), strOrNull(tableName), strOrNull(fieldName),
      (returnValue != NULL) ? returnValue->name : "NULL");
    return returnValue;
  }
  
  returnValue = database->getFieldTypeByName(database->db,
    dbName, tableName, fieldName);
  
  SCOPE_EXIT("database=%p, dbName=%s, tableName=%s, fieldName=%s", "%s",
    database, strOrNull(dbName), strOrNull(tableName), strOrNull(fieldName),
    (returnValue != NULL) ? returnValue->name : "NULL");
  return returnValue;
}

/// @fn TypeDescriptor* dbGetFieldTypeByIndex(Database *database, const char *dbName, const char *tableName, u64 fieldIndex)
///
/// @brief Get the type of a field in a table given its name.
///
/// @param database A pointer to a Database object the database and table
///   are in.
/// @param dbName The name of the database in the database management system.
/// @param tableName The name of the table in the database.
/// @param fieldIndex The index of the field in the table.
///
/// @return Returns the a pointer to the TypeDescriptor for the field on
/// success, NULL on failure.
TypeDescriptor* dbGetFieldTypeByIndex(Database *database,
  const char *dbName, const char *tableName, u64 fieldIndex
) {
  SCOPE_ENTER("database=%p, dbName=%s, tableName=%s, fieldIndex=%llu",
    database, strOrNull(dbName), strOrNull(tableName), llu(fieldIndex));
  
  TypeDescriptor *returnValue = NULL;
  if ((database == NULL) || (dbName == NULL) || (tableName == NULL)) {
    printLog(ERR, "One or more NULL parameters.\n");
    SCOPE_EXIT("database=%p, dbName=%s, tableName=%s, fieldIndex=%llu", "%s",
      database, strOrNull(dbName), strOrNull(tableName), llu(fieldIndex),
      (returnValue != NULL) ? returnValue->name : "NULL");
    return returnValue;
  }
  
  returnValue = database->getFieldTypeByIndex(database->db,
    dbName, tableName, fieldIndex);
  
  SCOPE_EXIT("database=%p, dbName=%s, tableName=%s, fieldIndex=%llu", "%s",
    database, strOrNull(dbName), strOrNull(tableName), llu(fieldIndex),
    (returnValue != NULL) ? returnValue->name : "NULL");
  return returnValue;
}

/// @fn bool dbRenameDatabase(Database *database, const char *oldDbName, const char *newDbName)
///
/// @brief Rename a database in the database management system.
///
/// @param database A pointer to a Database object the old database is in.
/// @param oldDbName The name of the database as it exists at the time the call
///   to this function as made.
/// @param newDbName The desired name of the database when this call completes.
///
/// @return Returns true on success, false on failure.
bool dbRenameDatabase(Database *database,
  const char *oldDbName, const char *newDbName
) {
  SCOPE_ENTER("database=%p, oldDbName=%s, newDbName=%s",
    database, strOrNull(oldDbName), strOrNull(newDbName));
  
  bool returnValue = false;
  if ((database == NULL) || (oldDbName == NULL) || (newDbName == NULL)) {
    printLog(ERR, "One or more NULL parameters.\n");
    SCOPE_EXIT("database=%p, oldDbName=%s, newDbName=%s", "%s",
      database, strOrNull(oldDbName), strOrNull(newDbName),
      boolNames[returnValue]);
    return returnValue;
  }
  
  returnValue = database->renameDatabase(database->db, oldDbName, newDbName);
  
  SCOPE_EXIT("database=%p, oldDbName=%s, newDbName=%s", "%s",
    database, strOrNull(oldDbName), strOrNull(newDbName),
    boolNames[returnValue]);
  return returnValue;
}

/// @fn i64 dbGetResultIndexByLookup_(const DbResult *dbResult, const char *fieldName, const TypeDescriptor *type, ...)
///
/// @brief Get a result index from a DbResult given key-value pairs of field
/// names and values required.
///
/// @param dbResult A pointer to a DbResult to search.
/// @param ... A NULL-terminated list of key-value pairs where the even indexes
///   are field names and the odd indexes are the required values for those
///   fields.
///
/// @return Returns the found result index on success, -1 on failure.
i64 dbGetResultIndexByLookup_(const DbResult *dbResult, ...) {
  printLog(TRACE,
    "ENTER dbGetResultIndexByLookup(dbName=\"%s\", tableName=\"%s\")\n",
    (dbResult != NULL) ? dbResult->dbName : "NULL",
    (dbResult != NULL) ? dbResult->tableName : "NULL");
  
  va_list args;
  va_start(args, dbResult);
  i64 resultIndex = dbGetResultIndexByLookupVargs(dbResult, args);
  va_end(args);
  
  printLog(TRACE,
    "EXIT dbGetResultIndexByLookup(dbName=\"%s\", tableName=\"%s\") = {%lld}\n",
    (dbResult != NULL) ? dbResult->dbName : "NULL",
    (dbResult != NULL) ? dbResult->tableName : "NULL",
    lld(resultIndex));
  return resultIndex;
}

/*
/// @fn int dbShareTableLock(const Dictionary *tablesToLock)
///
/// @brief Share a previously-acquired table lock with the current thread.
///
/// @param tablesToLock A Dictionary (HashTable) where the keys are the names of
///   the lockedTables in "dbName(dbInstance).tableName" format.
///
/// @return Returns 0 on success, -1 on failure.
int dbShareTableLock(const Dictionary *tablesToLock) {
  printLog(TRACE,
    "ENTER dbShareTableLock(database=%p, tablesToLock=%p)\n",
    database, tablesToLock);
  
  int returnValue = 0;
  
  // Argument check.
  if (tablesToLock == NULL) {
    // Can't work with a NULL argument.
    returnValue = -1;
    printLog(TRACE, "EXIT dbShareTableLock(tablesToLock=%p) = {%d}\n",
        tablesToLock, returnValue);
    return returnValue;
  } else if (tablesToLock->size == 0) {
    // Not an error per se, but nothing to do.
    printLog(WARN, "Zero-length Dictionary provided to dbShareTableLock.\n");
    printLog(TRACE, "EXIT dbShareTableLock(tablesToLock=%p) = {%d}\n",
        tablesToLock, returnValue);
    return returnValue; // 0
  }
  
  Database *database = (Database*) dictionaryGetValue(tablesToLock, "@database");
  if (database == NULL) {
    printLog(ERR, "@database of provided tablesToLock is NULL.\n");
    returnValue = -1;
    printLog(TRACE, "EXIT dbShareTableLock(tablesToLock=%p) = {%d}\n",
        tablesToLock, returnValue);
    return returnValue;
  }
  
  // Only one thread can be in this funciton at a time per Database.
  mtx_lock(&database->lockedTablesMutex);
  
  u64 threadId = (u64) thrd_current(); // Needed for the locks
  
  Dictionary *lockedTables
    = (Dictionary*) tss_get(database->threadLockedTables);
  if (lockedTables != NULL) {
    // This is a potential problem.  We're being asked to lock tables when
    // tables are already locked by this thread.  Calling lock tables again
    // will first unlock all the tables that are currently locked and then lock
    // the new ones.  If all the tables we're being asked to lock are already
    // locked then the right thing to do is to just return.  This may happen in
    // a situation where a parent function locks all the tables that a child
    // call will later use.  If there are tables we're being asked to lock that
    // are not already locked then the user has made a mistake.  We will honor
    // the request and lock the new set of tables but the results are undefined.
    bool allTablesFound = true;
    for (DictionaryEntry *cur = tablesToLock->head;
      cur != NULL;
      cur = cur->next
    ) {
      if (!dictionaryGetEntry(lockedTables, str(cur->key))) {
        allTablesFound = false;
        break;
      }
    }
    
    if (allTablesFound == true) {
      // All tables to lock are already locked.  Nothing to do.
      // We cannot return a new Dictionary in this case since the calling
      // function was not successful in getting a new lock.  We need to return
      // a copy of the real lock so that when we go to unlock later, we don't
      // do the real unlock.  We do, however, need to mark the lock copy as not
      // being the original.
      Dictionary *lockCopy = dictionaryCopy(lockedTables);
      bool *originalLock
        = (bool*) dictionaryGetValue(lockCopy, "@originalLock");
      if (originalLock != NULL) {
        *originalLock = false;
      } else {
        // dbUnlockTables will detect this as an invalid lock and will not
        // unlock the tables in the database, so we can still return the copy,
        // although this may mean that the system is in a bad state.
        printLog(ERR, "Could not obtain and update originalLock of lock.\n");
      }
      // Log this situation and return a copy of the existing lock.
      printLog(DEBUG,
        "Requested tables already locked.  Returning copy of existing lock.\n");
      mtx_unlock(&database->lockedTablesMutex);
      printLog(TRACE,
        "EXIT dbShareTableLock(database=%p, tablesToLock=%p) = {%p}\n",
        database, tablesToLock, lockCopy);
      return lockCopy;
    }
    // else the user has made a very bad decision and this call will result
    // in undefined behavior.  lockedTables will be *NOT* be freed by this
    // function.  If the user calls dbUnlockTables with the lock in the future,
    // it will be freed at that point.
    for (DictionaryEntry *cur = database->lockedTables->head;
      cur != NULL;
      cur = cur->next
    ) {
      u64 *lockOwner = (u64*) cur->value;
      if ((lockOwner != NULL) && (*lockOwner == threadId)) {
        // We are the owner of the lock.  Release it.
        dictionaryRemove(database->lockedTables, str(cur->key));
      }
    }
    printLog(WARN, "Locking new set of tables when tables already locked.\n");
    printLog(WARN, "Undefined behavior will result.\n");
  }
  
  bool allTablesLocked = false;
  // This function is "all or nothing".  Either we lock all the tables requested
  // or none of them.  It's possible that some other thread has a superset of
  // the requested tables locked and is also trying to lock a subset of those
  // tables.  If we don't unlock the mutex and allow some time for the other
  // thread to finish and unlock, we'll wind up in a deadlock here.  Briefly
  // unlock and then relock as needed.
  while (allTablesLocked == false) {
    // Make sure each table is unlocked and then lock it by this thread.  We have
    // a lock on the mutex for this function, so nothing can lock a table while
    // we're in the middle of checking and we therefore only need to check each
    // table once.  Other threads can unlock tables, but not lock them.
    mtx_lock(database->lockedTables->lock);
    allTablesLocked = true;
    for (DictionaryEntry *cur = tablesToLock->head;
      cur != NULL;
      cur = cur->next
    ) {
      // Make sure the table isn't locked by any other thread.
      if (dictionaryGetValue(database->lockedTables, str(cur->key))) {
        for (DictionaryEntry *innerCur = tablesToLock->head;
          innerCur != cur;
          innerCur = innerCur->next
        ) {
          dictionaryRemove(database->lockedTables, str(innerCur->key));
        }
        mtx_unlock(database->lockedTables->lock);
        cnd_wait(&database->lockedTablesCondition,
          &database->lockedTablesMutex);
        mtx_lock(database->lockedTables->lock);
        allTablesLocked = false;
        break;
      }
      
      // Table isn't locked by any other thread.  Mark it locked by this one.
      if (!dictionaryAddEntry(&database->lockedTables,
        str(cur->key), &threadId, typeU64)
      ) {
        printLog(ERR, "Cannot add table %s to lockedTables hash table.\n",
          str(cur->key));
      }
    }
    mtx_unlock(database->lockedTables->lock);
  }
  
  if (!database->lockTablesDict(database->db, tablesToLock)) {
    printLog(ERR, "Could not lock tables in database.  Failing.\n");
    mtx_unlock(&database->lockedTablesMutex);
    printLog(TRACE,
      "EXIT dbShareTableLock(database=%p, tablesToLock=%p) = {unsuccessful}\n",
      database, tablesToLock);
    return NULL;
  }
  
  // Copy the input tablesToLock into the tableLock that we'll store and return.
  Dictionary *tableLock = dictionaryCopy(tablesToLock);
  if (tableLock == NULL) {
    // System is in a bad state and we cannot return a successful lock to the
    // caller even though the lock succeeded.  We have to back out the lock
    // and then fail.
    database->unlockTables(database->db, tablesToLock);
    // Since the system is in a bad state, the above call is best effort and
    // we don't care about the return value.
    printLog(ERR, "Could not allocate new tableLock Dictionary.  Failing.\n");
    mtx_unlock(&database->lockedTablesMutex);
    printLog(TRACE,
      "EXIT dbShareTableLock(database=%p, tablesToLock=%p) = {unsuccessful}\n",
      database, tablesToLock);
    return NULL;
  }
  // Initialize the originalLock value for this lock.
  bool originalLock = true;
  DictionaryEntry *entry = 
    dictionaryAddEntry(&tableLock, "@originalLock", &originalLock, typeBool);
  if (entry == NULL) {
    // Memory allocation failure.  Bail.
    LOG_MALLOC_FAILURE();
    database->unlockTables(database->db, tablesToLock);
    tableLock = dictionaryDestroy(tableLock);
    mtx_unlock(&database->lockedTablesMutex);
    printLog(TRACE,
      "EXIT dbShareTableLock(database=%p, tablesToLock=%p) = {unsuccessful}\n",
      database, tablesToLock);
    return NULL;
  }
  // Initialize the databasea value for this lock.
  entry
    = dictionaryAddEntry(&tableLock, "@database", database, typePointerNoCopy);
  if (entry == NULL) {
    // Memory allocation failure.  Bail.
    LOG_MALLOC_FAILURE();
    database->unlockTables(database->db, tablesToLock);
    tableLock = dictionaryDestroy(tableLock);
    mtx_unlock(&database->lockedTablesMutex);
    printLog(TRACE,
      "EXIT dbShareTableLock(database=%p, tablesToLock=%p) = {unsuccessful}\n",
      database, tablesToLock);
    return NULL;
  }
  
  // We need to store the current tableLock as database->threadLockedTables so
  // that future calls know what's locked.  We could check the return value of
  // tss_set here, but there's really no point since we're calling tss_get
  // below.  That's more definitive.
  tss_set(database->threadLockedTables, tableLock);
  if (tss_get(database->threadLockedTables) != tableLock) {
    // Something is wrong with the system.  We can't detect locked tables on
    // a future call to this function even though the lock tables call
    // succeeded.  Print out an error and return the table lock below.
    printLog(ERR,
      "tableLock metadata from thread memory did not match what was set.\n");
  }
  
  mtx_unlock(&database->lockedTablesMutex);
  
  printLog(TRACE,
    "EXIT dbShareTableLock(database=%p, tablesToLock=%p) = {%p}\n",
    database, tablesToLock, tableLock);
  return tableLock;
}
*/

/// @fn DbResult* dbResultGetRange(DbResult *inputResult, u64 startIndex, u64 endIndex)
///
/// @brief Get a subset range of results from a DbResult returned by a previous
/// query.
///
/// @param inputResult The DbResult returned from an earlier query.
/// @param startIndex The index of the first result within the DbResult to copy
///   from.
/// @param endIndex One greater than the index of the last result within the
///   DbResult to copy from.
///
/// @return Returns a new DbResult with the selected subset of results on
/// success, an empty dbResult on failure.
DbResult* dbResultGetRange(
  DbResult *inputResult, u64 startIndex, u64 endIndex
) {
  SCOPE_ENTER("inputResult=%p, startIndex=%llu, endIndex=%llu",
    inputResult, llu(startIndex), llu(endIndex));
  
  DbResult *outputResult = (DbResult*) calloc(1, sizeof(DbResult));
  
  // Do all the straight copies first.
  outputResult->numFields = inputResult->numFields;
  outputResult->fieldTypes = (TypeDescriptor**) malloc(
    outputResult->numFields * sizeof(TypeDescriptor*));
  memcpy(outputResult->fieldTypes, inputResult->fieldTypes,
    outputResult->numFields * sizeof(TypeDescriptor*));
  // outputResult->numRows is computed below
  // outputResult->numResults is computed below
  // outputResult->rows is computed below
  straddstr(&outputResult->dbName, inputResult->dbName);
  straddstr(&outputResult->tableName, inputResult->tableName);
  outputResult->successful = inputResult->successful;
  outputResult->fieldNameIndexMap = htCopy(inputResult->fieldNameIndexMap);
  outputResult->database = inputResult->database;
  
  // Figure out how much we need to allocate for our subset range.
  if (endIndex >= inputResult->numResults) {
    if (inputResult->numResults > 0) {
      endIndex = inputResult->numResults;
    } else {
      endIndex = 0;
    }
  }
  outputResult->numResults = 0;
  if ((endIndex > startIndex) && (endIndex <= inputResult->numResults)) {
    outputResult->numResults = endIndex - startIndex;
  }
  outputResult->numRows = outputResult->numResults + 1;
  outputResult->rows = (void***) calloc(1,
      (outputResult->numRows + 1) * sizeof(void**));
  for (u64 ii = 0; ii < outputResult->numRows; ii++) {
    outputResult->rows[ii] = (void**) calloc(1,
      (outputResult->numFields + 1) * sizeof(void*));
  }
  
  // Copy over the values.
  Bytes *inputFieldNames = (Bytes*) inputResult->rows[0];
  Bytes *outputFieldNames = (Bytes*) outputResult->rows[0];
  for (u64 jj = 0; jj < inputResult->numFields; jj++) {
    bytesAddBytes(&outputFieldNames[jj], inputFieldNames[jj]);
  }
  u64 outputRow = 1;
  // The rows of the input that we're interested in start at 1, not 0, so we
  // need to increment both the startIndex and endIndex.
  startIndex++;
  endIndex++;
  for (u64 ii = startIndex; ii < endIndex; ii++) {
    for (u64 jj = 0; jj < inputResult->numFields; jj++) {
      if (inputResult->fieldTypes[jj] == typeString) {
        // These are really Bytes objects.  Treat them as such.  This is the
        // expected usual case, so list it first.
        bytesAddBytes((Bytes*) &outputResult->rows[outputRow][jj],
          (Bytes) inputResult->rows[ii][jj]);
      } else {
        outputResult->rows[outputRow][jj]
          = inputResult->fieldTypes[jj]->copy(inputResult->rows[ii][jj]);
      }
    }
    outputRow++;
  }
  
  SCOPE_EXIT("inputResult=%p, startIndex=%llu, endIndex=%llu", "%llu results",
    inputResult, llu(startIndex), llu(endIndex), llu(outputResult->numResults));
  return outputResult;
}

/// @fn bool dbEnsureFieldIndexed_(Database *database, const char *dbName, const char *tableName, const char *fieldName, ...)
///
/// @brief Ensure there is an index for a particular field in a database table.
///
/// @param database A pointer to the Database object representing the database
///   system used to generate dbResult.
/// @param dbName The name of the database that the table is in.
/// @param tableName The name of the table the field is in.
/// @param fieldName The name of the field to make sure is indexed.
///
/// @return Returns true on success, false on failure.
bool dbEnsureFieldIndexed_(Database *database,
  const char *dbName, const char *tableName, const char *fieldName, ...
) {
  SCOPE_ENTER("database=%p, dbName=%s, tableName=%s, fieldName=%s",
    database, dbName, tableName, fieldName);
  
  dbWaitForTableUnlocked(database, dbName, tableName);
  char *dbWithInstance = NULL;
  straddstr(&dbWithInstance, dbName);
  straddstr(&dbWithInstance, dbInstance);
  scopeAdd(dbWithInstance, typeString->destroy);
  
  va_list args;
  va_start(args, fieldName);
  bool returnValue
    = database->ensureFieldIndexedVargs(
      database->db, dbWithInstance, tableName, fieldName, args);
  va_end(args);
  
  SCOPE_EXIT("database=%p, dbName=%s, tableName=%s, fieldName=%s", "%s",
    database, dbName, tableName, fieldName, boolNames[returnValue]);
  return returnValue;
}

