///////////////////////////////////////////////////////////////////////////////
///
/// @date              06.15.2025
///
/// @file              DbInterfaceUnitTest.c
///
/// @brief             Unit test for DbInterface.h functionality
///
/// @details           Comprehensive test suite that exercises all functions
///                    in the DbInterface.h header including edge cases and
///                    NULL parameter handling.
///
///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include "DbInterface.h"
#include "Dictionary.h"
#include "HashTable.h"
#include "LoggingLib.h"

// Database and table name constants
#define TEST_DATABASE_NAME "testDatabase"
#define TEST_TABLE_NAME "testTable"
#define SECONDARY_TABLE_NAME "secondaryTable"
#define TEST_FIELD_NAME "testField"
#define TEST_PRIMARY_KEY "id"
#define NEW_DATABASE_NAME "newDatabase"
#define NEW_TABLE_NAME "newTable"
#define NEW_FIELD_NAME "newField"

// Test data constants
#define TEST_STRING_VALUE "testValue"
#define TEST_UPDATED_VALUE "updatedValue"
#define EXPECTED_NUM_FIELDS 3
#define EXPECTED_NUM_RECORDS 2
const i64 TEST_INTEGER_VALUE = 42;

// Forward declarations of helper functions
bool setupTestEnvironment(void);
bool cleanupTestEnvironment(void);
bool testBasicDatabaseOperations(void);
bool testTableOperations(void);
bool testRecordOperations(void);
bool testQueryOperations(void);
bool testTransactionOperations(void);
bool testLockingOperations(void);
bool testUtilityFunctions(void);
bool testDatabaseNullParameterHandling(void);
bool testDbInterfaceCornerCases(void);
bool testDbResult(void);
DbResult* createMockDbResult(void);
Dictionary* createTestDictionary(void);
bool validateDbResult(const DbResult* result, u64 expectedRows);

// External reference to main database
extern Database* mainDatabase;

///
/// @brief Main unit test function for DbInterface
///
/// @return true if all tests pass, false otherwise
///
bool dbInterfaceUnitTest(void) {
  printLog(DEBUG, "Starting DbInterface unit test suite\n");
  
  bool allTestsPassed = true;
  
  // Setup test environment
  if (!setupTestEnvironment()) {
    printLog(ERR, "Failed to setup test environment\n");
    return false;
  }
  
  // Run test categories
  if (!testBasicDatabaseOperations()) {
    printLog(ERR, "Basic database operations test failed\n");
    return false;
  }
  
  if (!testTableOperations()) {
    printLog(ERR, "Table operations test failed\n");
    return false;
  }
  
  if (!testRecordOperations()) {
    printLog(ERR, "Record operations test failed\n");
    return false;
  }
  
  if (!testQueryOperations()) {
    printLog(ERR, "Query operations test failed\n");
    return false;
  }
  
  if (!testTransactionOperations()) {
    printLog(ERR, "Transaction operations test failed\n");
    return false;
  }
  
  if (!testLockingOperations()) {
    printLog(ERR, "Locking operations test failed\n");
    return false;
  }
  
  if (!testUtilityFunctions()) {
    printLog(ERR, "Utility functions test failed\n");
    return false;
  }
  
  if (!testDatabaseNullParameterHandling()) {
    printLog(ERR, "NULL parameter handling test failed\n");
    return false;
  }
  
  if (!testDbInterfaceCornerCases()) {
    printLog(ERR, "Corner cases test failed\n");
    return false;
  }
  
  if (!testDbResult()) {
    printLog(ERR, "DbResult test failed\n");
    return false;
  }
  
  // Cleanup test environment
  if (!cleanupTestEnvironment()) {
    printLog(ERR, "Failed to cleanup test environment\n");
    return false;
  }
  
  printLog(DEBUG, "All DbInterface unit tests passed\n");
  return allTestsPassed;
}

///
/// @brief Sets up the test environment
///
/// @return true if setup successful, false otherwise
///
bool setupTestEnvironment(void) {
  printLog(DEBUG, "Setting up test environment\n");
  
  if (mainDatabase == NULL) {
    printLog(ERR, "mainDatabase is NULL, cannot proceed with tests\n");
    return false;
  }
  
  // We don't care if the delete succeeds or fails, so don't check the return
  // value here.
  dbDeleteDatabase(mainDatabase, TEST_DATABASE_NAME);
  
  // Create test database
  if (!dbAddDatabase(mainDatabase, TEST_DATABASE_NAME)) {
    printLog(DEBUG, "Test database may already exist or creation failed\n");
    return false;
  }
  
  return true;
}

///
/// @brief Cleans up the test environment
///
/// @return true if cleanup successful, false otherwise
///
bool cleanupTestEnvironment(void) {
  printLog(DEBUG, "Cleaning up test environment\n");
  
  // Remove test database and all its contents
  if (!dbDeleteDatabase(mainDatabase, TEST_DATABASE_NAME)) {
    printLog(DEBUG, "Test database may not exist or deletion failed\n");
    return false;
  }
  
  if (!dbDeleteDatabase(mainDatabase, "renamedDatabase")) {
    printLog(DEBUG, "Renamed test database may not exist or deletion failed\n");
    return false;
  }
  
  return true;
}

///
/// @brief Tests basic database operations
///
/// @return true if all tests pass, false otherwise
///
bool testBasicDatabaseOperations(void) {
  printLog(DEBUG, "Testing basic database operations\n");
  
  // Test database existence check
  bool exists = dbCheckExists(mainDatabase, TEST_DATABASE_NAME);
  printLog(DEBUG, "Database exists check returned: %s\n", 
           exists ? "true" : "false");
  if (exists == false) {
    return false;
  }
  
  // Test getting database names
  DbResult* dbNames = dbGetDatabaseNames(mainDatabase);
  if (dbNames != NULL) {
    printLog(DEBUG, "Retrieved %llu databases\n", 
             (unsigned long long) dbNames->numResults);
    dbFreeResult(dbNames);
  } else {
    printLog(ERR, "dbGetDatabaseNames failed.\n");
    return false;
  }
  
  // Test adding a new database
  dbDeleteDatabase(mainDatabase, NEW_DATABASE_NAME);
  bool addResult = dbAddDatabase(mainDatabase, NEW_DATABASE_NAME);
  printLog(DEBUG, "Add new database result: %s\n", 
           addResult ? "success" : "failure");
  if (addResult == false) {
    return false;
  }
  
  // Test renaming database
  dbDeleteDatabase(mainDatabase, "renamedDatabase");
  bool renameResult = dbRenameDatabase(mainDatabase, NEW_DATABASE_NAME, 
                                       "renamedDatabase");
  printLog(DEBUG, "Rename database result: %s\n", 
           renameResult ? "success" : "failure");
  if (renameResult == false) {
    return false;
  }
  
  // Test getting database size
  i64 dbSize = dbGetSize(mainDatabase, TEST_DATABASE_NAME);
  printLog(DEBUG, "Database size: %lld bytes\n", (long long) dbSize);
  if (dbSize < 0) {
    return false;
  }
  
  return true;
}

///
/// @brief Tests table operations
///
/// @return true if all tests pass, false otherwise
///
bool testTableOperations(void) {
  printLog(DEBUG, "Testing table operations\n");
  
  // Test adding a table
  bool addTableResult = dbAddTable(mainDatabase, TEST_DATABASE_NAME, 
                                   TEST_TABLE_NAME, TEST_PRIMARY_KEY,
                                   "id", typeI64,
                                   "name", typeString,
                                   "value", typeI64);
  printLog(DEBUG, "Add table result: %s\n", 
           addTableResult ? "success" : "failure");
  if (addTableResult == false) {
    return false;
  }
  
  // Test table existence check
  bool tableExists = dbCheckTableExists(mainDatabase, TEST_DATABASE_NAME, 
                                        TEST_TABLE_NAME);
  printLog(DEBUG, "Table exists: %s\n", tableExists ? "true" : "false");
  if (tableExists == false) {
    return false;
  }
  
  // Test getting table names
  DbResult* tableNames = dbGetTableNames(mainDatabase, TEST_DATABASE_NAME);
  if (tableNames != NULL) {
    printLog(DEBUG, "Retrieved %llu tables\n", 
             (unsigned long long) tableNames->numResults);
    dbFreeResult(tableNames);
  } else {
    printLog(ERR, "dbGetTableNames failed.\n");
    return false;
  }
  
  // Test describing table
  DbResult* tableDescription = dbDescribeTable(mainDatabase, 
                                               TEST_DATABASE_NAME, 
                                               TEST_TABLE_NAME);
  if (tableDescription != NULL) {
    printLog(DEBUG, "Table description has %llu fields\n", 
             (unsigned long long) tableDescription->numFields);
    dbFreeResult(tableDescription);
  } else {
    printLog(ERR, "dbDescribeTable failed.\n");
    return false;
  }
  
  // Test field operations
  bool fieldExists = dbCheckFieldExists(mainDatabase, TEST_DATABASE_NAME, 
                                        TEST_TABLE_NAME, "name");
  printLog(DEBUG, "Field 'name' exists: %s\n", 
           fieldExists ? "true" : "false");
  if (fieldExists == false) {
    return false;
  }
  
  // Test adding a field
  bool addFieldResult = dbAddField(mainDatabase, TEST_DATABASE_NAME, 
                                   TEST_TABLE_NAME, "value", NEW_FIELD_NAME, 
                                   typeString);
  printLog(DEBUG, "Add field result: %s\n", 
           addFieldResult ? "success" : "failure");
  if (addFieldResult == false) {
    return false;
  }
  
  // Test changing field type
  bool changeTypeResult = dbChangeFieldType(mainDatabase, TEST_DATABASE_NAME,
                                            TEST_TABLE_NAME, NEW_FIELD_NAME,
                                            typeBytes);
  printLog(DEBUG, "Change field type result: %s\n", 
           changeTypeResult ? "success" : "failure");
  if (changeTypeResult == false) {
    return false;
  }
  
  // Test changing field name
  bool changeNameResult = dbChangeFieldName(mainDatabase, TEST_DATABASE_NAME,
                                            TEST_TABLE_NAME, NEW_FIELD_NAME,
                                            "renamedField");
  printLog(DEBUG, "Change field name result: %s\n", 
           changeNameResult ? "success" : "failure");
  if (changeNameResult == false) {
    return false;
  }
  
  // Test getting field type by name
  TypeDescriptor* fieldType = dbGetFieldTypeByName(mainDatabase, 
                                                   TEST_DATABASE_NAME,
                                                   TEST_TABLE_NAME, "name");
  if (fieldType != NULL) {
    printLog(DEBUG, "Retrieved field type '%s' for 'name'\n", fieldType->name);
  }
  if (fieldType != typeString) {
    return false;
  }
  
  // Test getting field type by index
  TypeDescriptor* fieldTypeByIndex = dbGetFieldTypeByIndex(mainDatabase,
                                                           TEST_DATABASE_NAME,
                                                           TEST_TABLE_NAME, 0);
  if (fieldTypeByIndex != NULL) {
    printLog(DEBUG, "Retrieved field type '%s' for index 0\n", fieldTypeByIndex->name);
  }
  if (fieldTypeByIndex != typeI64) {
    return false;
  }
  
  // Test renaming table
  bool renameTableResult = dbRenameTable(mainDatabase, TEST_DATABASE_NAME,
                                         TEST_TABLE_NAME, NEW_TABLE_NAME);
  printLog(DEBUG, "Rename table result: %s\n", 
           renameTableResult ? "success" : "failure");
  if (renameTableResult == false) {
    return false;
  }
  
  // Rename back for other tests
  renameTableResult = dbRenameTable(mainDatabase, TEST_DATABASE_NAME,
           NEW_TABLE_NAME, TEST_TABLE_NAME);
  printLog(DEBUG, "Rename table result: %s\n", 
           renameTableResult ? "success" : "failure");
  if (renameTableResult == false) {
    return false;
  }
  // Drop the added field for other tests.
  dbDeleteField(mainDatabase, TEST_DATABASE_NAME, TEST_TABLE_NAME, "renamedField");
  
  return true;
}

///
/// @brief Tests record operations
///
/// @return true if all tests pass, false otherwise
///
bool testRecordOperations(void) {
  printLog(DEBUG, "Testing record operations\n");
  
  // Test adding records
  i64 id = TEST_INTEGER_VALUE;
  i64 value = 100;
  bool addRecord1 = dbAddRecord(mainDatabase, TEST_DATABASE_NAME, 
                                TEST_TABLE_NAME,
                                &id,
                                TEST_STRING_VALUE,
                                &value);
  printLog(DEBUG, "Add first record result: %s\n", 
           addRecord1 ? "success" : "failure");
  if (addRecord1 != true) {
    return false;
  }
  
  id = TEST_INTEGER_VALUE + 1;
  value = 100;
  bool addRecord2 = dbAddRecord(mainDatabase, TEST_DATABASE_NAME, 
                                TEST_TABLE_NAME,
                                &id,
                                "secondRecord",
                                &value);
  printLog(DEBUG, "Add second record result: %s\n", 
           addRecord2 ? "success" : "failure");
  if (addRecord2 != true) {
    return false;
  }
  
  // Test deleting records
  bool deleteResult = dbDeleteRecords(mainDatabase, TEST_DATABASE_NAME,
                                      TEST_TABLE_NAME,
                                      "id", &TEST_INTEGER_VALUE);
  printLog(DEBUG, "Delete records result: %s\n", 
           deleteResult ? "success" : "failure");
  if (deleteResult == false) {
    return false;
  }
  
  // Test adding record with dictionary
  Dictionary* recordDict = createTestDictionary();
  bool addRecordDict = dbAddRecordDict(mainDatabase, TEST_DATABASE_NAME,
                                       TEST_TABLE_NAME, recordDict);
  printLog(DEBUG, "Add record with dictionary result: %s\n", 
           addRecordDict ? "success" : "failure");
  if (addRecordDict == false) {
    return false;
  }
  recordDict = dictionaryDestroy(recordDict);
  
  id = 100;
  value = 9999;
  recordDict = dictionaryCreate(typeString);
  dictionaryAddEntry(recordDict, "id", &id, typeI64);
  dictionaryAddEntry(recordDict, "name", "my_name", typeString);
  dictionaryAddEntry(recordDict, "value", &value, typeI64);
  addRecordDict = dbAddRecordDict(mainDatabase, TEST_DATABASE_NAME,
                                       TEST_TABLE_NAME, recordDict);
  printLog(DEBUG, "Second add record with dictionary result: %s\n", 
           addRecordDict ? "success" : "failure");
  if (addRecordDict == false) {
    return false;
  }
  
  dictionaryDestroy(recordDict);
  
  // Test getting number of records
  i64 numRecords = dbGetNumRecords(mainDatabase, TEST_DATABASE_NAME, 
                                   TEST_TABLE_NAME);
  printLog(DEBUG, "Number of records: %lld\n", (long long) numRecords);
  if (numRecords != 3) {
    return false;
  }
  
  // Test updating record
  Dictionary* updateDict = createTestDictionary();
  bool updateResult = dbUpdateRecordDict(mainDatabase, TEST_DATABASE_NAME,
                                         TEST_TABLE_NAME, updateDict);
  printLog(DEBUG, "Update record result: %s\n", 
           updateResult ? "success" : "failure");
  if (updateResult == false) {
    return false;
  }
  dictionaryDestroy(updateDict);
  
  value = 10000;
  updateDict = dictionaryCreate(typeString);
  dictionaryAddEntry(updateDict, "id", &id, typeI64);
  dictionaryAddEntry(updateDict, "name", "my_name", typeString);
  dictionaryAddEntry(updateDict, "value", &value, typeI64);
  updateResult = dbUpdateRecordDict(mainDatabase, TEST_DATABASE_NAME,
                                         TEST_TABLE_NAME, updateDict);
  printLog(DEBUG, "Second update record result: %s\n", 
           updateResult ? "success" : "failure");
  if (updateResult == false) {
    return false;
  }
  
  dictionaryDestroy(updateDict);
  
  // Test updating field
  bool updateFieldResult = dbUpdateField(mainDatabase, TEST_DATABASE_NAME,
                                         TEST_TABLE_NAME, "name", 
                                         TEST_UPDATED_VALUE,
                                         "id", &TEST_INTEGER_VALUE);
  printLog(DEBUG, "Update field result: %s\n", 
           updateFieldResult ? "success" : "failure");
  if (updateFieldResult == false) {
    return false;
  }
  
  // Test deleting a record "like" something
  if (dbDeleteRecordsLike(mainDatabase, TEST_DATABASE_NAME,
    TEST_TABLE_NAME, "name", "updated*") == false
  ) {
    printLog(ERR, "dbDeleteRecordsLike failed.\n");
    return false;
  }
  
  return true;
}

///
/// @brief Tests query operations
///
/// @return true if all tests pass, false otherwise
///
bool testQueryOperations(void) {
  printLog(DEBUG, "Testing query operations\n");
  
  // Test getting all records
  DbResult* allRecords = dbGetRecords(mainDatabase, TEST_DATABASE_NAME, 
                                      TEST_TABLE_NAME);
  if (allRecords != NULL) {
    printLog(DEBUG, "Retrieved %llu records\n", 
             (unsigned long long) allRecords->numResults);
    
    // Test getting values by name and index
    if (allRecords->numResults > 0) {
      const char* nameValue = dbGetStringByName(allRecords, 0, "name");
      if (nameValue != NULL) {
        printLog(DEBUG, "First record name: %s\n", nameValue);
      }
      
      const char* nameByIndex = dbGetStringByIndex(allRecords, 0, 1);
      if (nameByIndex != NULL) {
        printLog(DEBUG, "First record name by index: %s\n", nameByIndex);
      }
      
      // Test getting field index by name
      i64 fieldIndex = dbGetFieldIndexByName(allRecords, "name");
      printLog(DEBUG, "Field index for 'name': %lld\n", 
               (long long) fieldIndex);
      
      // Test updating result
      bool updateResultSuccess = dbUpdateResult(allRecords, 0,
                                                "name", "updatedName");
      printLog(DEBUG, "Update result success: %s\n", 
               updateResultSuccess ? "true" : "false");
    }
    
    dbFreeResult(allRecords);
  }
  
  // Test getting values with criteria
  i64 id = 100;
  DbResult* filteredRecords = dbGetValues(mainDatabase, TEST_DATABASE_NAME,
                                          TEST_TABLE_NAME, "*",
                                          "id", &id);
  if (filteredRecords != NULL) {
    printLog(DEBUG, "Filtered query returned %llu records\n", 
             (unsigned long long) filteredRecords->numResults);
    if (filteredRecords->numResults == 0) {
      return false;
    }
    dbFreeResult(filteredRecords);
  } else {
    printLog(ERR, "dbGetValues failed.\n");
    return false;
  }
  
  // Test getting values with order by
  DbResult* orderedRecords = dbGetValuesOrderBy(mainDatabase, 
                                                TEST_DATABASE_NAME,
                                                TEST_TABLE_NAME, "*", 
                                                "name ASC",
                                                "id", &id);
  if (orderedRecords != NULL) {
    printLog(DEBUG, "Ordered query returned %llu records\n", 
             (unsigned long long) orderedRecords->numResults);
    if (orderedRecords->numResults == 0) {
      return false;
    }
    dbFreeResult(orderedRecords);
  } else {
    printLog(ERR, "dbGetValuesOrderBy failed.\n");
    return false;
  }
  
  // Test getting values with dictionary
  Dictionary* queryDict = dictionaryCreate(typeString);
  i64 temp = 43;
  dictionaryAddEntry(queryDict, "id", &temp, typeI64);
  dictionaryAddEntry(queryDict, "name", "updatedName", typeString);
  temp = 100;
  dictionaryAddEntry(queryDict, "value", &temp, typeI64);
  DbResult* dictRecords = dbGetValuesDict(mainDatabase, TEST_DATABASE_NAME,
                                          TEST_TABLE_NAME, "*", queryDict);
  if (dictRecords != NULL) {
    printLog(DEBUG, "Dictionary query returned %llu records\n", 
             (unsigned long long) dictRecords->numResults);
    if (dictRecords->numResults == 0) {
      return false;
    }
    dbFreeResult(dictRecords);
  } else {
    printLog(ERR, "dbGetValuesDict failed.\n");
  }
  
  dictRecords = dbGetValuesDictOrderBy(mainDatabase, TEST_DATABASE_NAME,
                                       TEST_TABLE_NAME, "*", "name", queryDict);
  if (dictRecords != NULL) {
    printLog(DEBUG, "Dictionary query returned %llu records\n", 
             (unsigned long long) dictRecords->numResults);
    if (dictRecords->numResults == 0) {
      return false;
    }
    dbFreeResult(dictRecords);
  } else {
    printLog(ERR, "dbGetValuesDict failed.\n");
  }
  
  // Test LIKE queries
  DbResult* likeRecords = dbGetValuesLike(mainDatabase, TEST_DATABASE_NAME,
                                          TEST_TABLE_NAME, "name",
                                          "name", "updated*");
  if (likeRecords != NULL) {
    printLog(DEBUG, "LIKE query returned %llu records\n", 
             (unsigned long long) likeRecords->numResults);
    if (likeRecords->numResults == 0) {
      return false;
    }
    dbFreeResult(likeRecords);
  } else {
    printLog(ERR, "dbGetValuesLike failed.\n");
  }
  
  likeRecords = dbGetRecordsLike(mainDatabase, TEST_DATABASE_NAME,
                                 TEST_TABLE_NAME,
                                 "name", "updated*");
  if (likeRecords != NULL) {
    printLog(DEBUG, "LIKE query returned %llu records\n", 
             (unsigned long long) likeRecords->numResults);
    if (likeRecords->numResults == 0) {
      return false;
    }
    dbFreeResult(likeRecords);
  } else {
    printLog(ERR, "dbGetRecordsLike failed.\n");
  }
  
  // Test OR queries with dictionary
  DbResult* orRecords = dbGetOrValuesDict(mainDatabase, TEST_DATABASE_NAME,
                                          TEST_TABLE_NAME, "*", NULL, queryDict);
  if (orRecords != NULL) {
    printLog(DEBUG, "OR query returned %llu records\n", 
             (unsigned long long) orRecords->numResults);
    if (orRecords->numResults == 0) {
      return false;
    }
    dbFreeResult(orRecords);
  } else {
    printLog(ERR, "dbGetOrValuesDict failed.\n");
    return false;
  }
  dictionaryDestroy(queryDict);
  
  return true;
}

///
/// @brief Tests transaction operations
///
/// @return true if all tests pass, false otherwise
///
bool testTransactionOperations(void) {
  printLog(DEBUG, "Testing transaction operations\n");
  
  // Test starting transaction
  Database* transactionDb = dbStartTransaction(mainDatabase);
  if (transactionDb != NULL) {
    printLog(DEBUG, "Transaction started successfully\n");
    
    // Test committing transaction
    Database* committedDb = dbCommitTransaction(transactionDb);
    if (committedDb != NULL) {
      printLog(DEBUG, "Transaction committed successfully\n");
    } else {
      printLog(ERR, "dbCommitTransaction failed.\n");
      return false;
    }
  } else {
    printLog(ERR, "dbStartTransaction failed.\n");
    return false;
  }
  
  // Test rollback transaction
  Database* rollbackDb = dbStartTransaction(mainDatabase);
  if (rollbackDb != NULL) {
    printLog(DEBUG, "Transaction for rollback started\n");
    
    Database* rolledBackDb = dbRollbackTransaction(rollbackDb);
    if (rolledBackDb != NULL) {
      printLog(DEBUG, "Transaction rolled back successfully\n");
    } else {
      printLog(ERR, "dbRollbackTransaction failed.\n");
      return false;
    }
  } else {
    printLog(ERR, "dbStartTransaction failed.\n");
    return false;
  }
  
  return true;
}

///
/// @brief Tests locking operations
///
/// @return true if all tests pass, false otherwise
///
bool testLockingOperations(void) {
  printLog(DEBUG, "Testing locking operations\n");
  
  // Test table locking
  Dictionary* lockDict = dbLockTables(mainDatabase, 
                                      TEST_DATABASE_NAME, TEST_TABLE_NAME);
  if (lockDict != NULL) {
    printLog(DEBUG, "Tables locked successfully\n");
    
    // Test checking if table is locked by another thread
    bool isLocked = dbIsTableLocked(mainDatabase, TEST_DATABASE_NAME, 
                                    TEST_TABLE_NAME);
    printLog(DEBUG, "Table lock status: %s\n", 
             isLocked ? "locked" : "unlocked");
    if (isLocked == true) {
      return false;
    }
    
    // Test unlocking tables
    lockDict = dbUnlockTables(lockDict);
    if (lockDict == NULL) {
      printLog(DEBUG, "Tables unlocked successfully\n");
    } else {
      printLog(ERR, "dbUnlockTables failed.\n");
      return false;
    }
  } else {
    printLog(ERR, "dbLockTables failed.\n");
    return false;
  }
  
  // Test record locking
  Bytes recordLock = dbLockRecords(mainDatabase, TEST_DATABASE_NAME,
                                   TEST_TABLE_NAME, "id", 
                                   &TEST_INTEGER_VALUE);
  if (recordLock != NULL) {
    printLog(DEBUG, "Records locked successfully\n");
    
    // Test unlocking records
    recordLock = dbUnlockRecords(mainDatabase, recordLock);
    if (recordLock == NULL) {
      printLog(DEBUG, "Records unlocked successfully\n");
    } else {
      printLog(ERR, "dbUnlockRecords failed.\n");
      return false;
    }
  } else {
    printLog(ERR, "dbLockRecords failed.\n");
    return false;
  }
  
  // Test ensuring field is indexed
  bool indexResult = dbEnsureFieldIndexed(mainDatabase, TEST_DATABASE_NAME,
                                          TEST_TABLE_NAME, "name");
  printLog(DEBUG, "Ensure field indexed result: %s\n", 
           indexResult ? "success" : "failure");
  
  return true;
}

///
/// @brief Tests utility functions
///
/// @return true if all tests pass, false otherwise
///
bool testUtilityFunctions(void) {
  printLog(DEBUG, "Testing utility functions\n");
  
  // Create a mock result for testing
  DbResult* mockResult = createMockDbResult();
  if (mockResult == NULL) {
    printLog(ERR, "Failed to create mock result\n");
    return false;
  }
  
  // Test field name validation
  bool validName = dbIsFieldNameValid("validFieldName");
  bool invalidName = dbIsFieldNameValid("invalid field name");
  printLog(DEBUG, "Field name validation - valid: %s, invalid: %s\n",
           validName ? "true" : "false", invalidName ? "true" : "false");
  if ((validName == false) || (invalidName == true)) {
    return false;
  }
  
  // Test converting result to CSV
  Bytes csvData = dbResultToCsv(mockResult);
  if (csvData != NULL) {
    printLog(DEBUG, "Converted result to CSV successfully\n");
    bytesDestroy(csvData);
  } else {
    printLog(ERR, "dbResultToCsv falied.\n");
    return false;
  }
  
  // Test converting result to bytes
  Bytes bytesData = dbResultToBytes(mockResult);
  if (bytesData != NULL) {
    printLog(DEBUG, "Converted result to bytes successfully\n");
    bytesDestroy(bytesData);
  } else {
    printLog(ERR, "dbResultToBytes falied.\n");
    return false;
  }
  
  // Test result comparison
  DbResult* mockResult2 = createMockDbResult();
  if (mockResult2 != NULL) {
    int compareResult = dbResultCompare(mockResult, mockResult2);
    printLog(DEBUG, "Result comparison returned: %d\n", compareResult);
    if (compareResult != 0) {
      return false;
    }
    dbFreeResult(mockResult2);
  } else {
    printLog(ERR, "createMockDbResult falied.\n");
    return false;
  }
  
  // Test result copy
  DbResult* copiedResult = dbResultCopy(mockResult);
  if (copiedResult != NULL) {
    printLog(DEBUG, "Result copied successfully\n");
    int compareResult = dbResultCompare(mockResult, copiedResult);
    printLog(DEBUG, "Result comparison returned: %d\n", compareResult);
    if (compareResult != 0) {
      return false;
    }
    dbFreeResult(copiedResult);
  } else {
    printLog(ERR, "dbResultCopy falied.\n");
    return false;
  }
  
  // Test getting result range
  DbResult* rangeResult = dbResultGetRange(mockResult, 0, 1);
  if (rangeResult != NULL) {
    printLog(DEBUG, "Got result range successfully\n");
    dbFreeResult(rangeResult);
  } else {
    printLog(ERR, "dbResultGetRange falied.\n");
    return false;
  }
  
  // Test database comparison
  int dbCompareResult = dbCompare(mainDatabase, mainDatabase);
  printLog(DEBUG, "Database self-comparison returned: %d\n", dbCompareResult);
  if (dbCompareResult != 0) {
    return false;
  }
  
  dbFreeResult(mockResult);
  return true;
}

///
/// @brief Tests NULL parameter handling
///
/// @return true if all tests pass, false otherwise
///
bool testDatabaseNullParameterHandling(void) {
  printLog(DEBUG, "Testing NULL parameter handling\n");
  
  // Test functions with NULL database parameter
  bool nullDbResult = dbCheckExists(NULL, TEST_DATABASE_NAME);
  printLog(DEBUG, "NULL database check result: %s\n", 
           nullDbResult ? "true" : "false");
  if (nullDbResult == true) {
    printLog(ERR, "dbCheckExists with NULL database returned true.\n");
    return false;
  }
  
  DbResult* nullDbQuery = dbGetRecords(NULL, TEST_DATABASE_NAME, 
                                       TEST_TABLE_NAME);
  if (nullDbQuery == NULL) {
    printLog(DEBUG, "NULL database query correctly returned NULL\n");
  } else {
    printLog(ERR, "dbGetRecords with NULL database returned garbage.\n");
    return false;
  }
  
  // Test functions with NULL string parameters
  bool nullStringResult = dbCheckExists(mainDatabase, NULL);
  printLog(DEBUG, "NULL database name check result: %s\n", 
           nullStringResult ? "true" : "false");
  if (nullStringResult == true) {
    printLog(ERR, "dbCheckExists with NULL database name returned true.\n");
    return false;
  }
  
  // Test functions with NULL result parameter
  printLog(DEBUG, "Freeing NULL DbResult\n");
  DbResult* nullResult = dbFreeResult(NULL);
  if (nullResult == NULL) {
    printLog(DEBUG, "NULL result free correctly returned NULL\n");
  }
  
  // Test field name validation with NULL
  bool nullNameValid = dbIsFieldNameValid(NULL);
  printLog(DEBUG, "NULL field name validation result: %s\n", 
           nullNameValid ? "true" : "false");
  if (nullNameValid == true) {
    return false;
  }
  
  // Test getting field index with NULL result
  i64 nullFieldIndex = dbGetFieldIndexByName(NULL, "fieldName");
  printLog(DEBUG, "NULL result field index: %lld\n", 
           (long long) nullFieldIndex);
  if (nullFieldIndex >= 0) {
    return false;
  }
  
  return true;
}

///
/// @brief Tests corner cases and edge conditions
///
/// @return true if all tests pass, false otherwise
///
bool testDbInterfaceCornerCases(void) {
  printLog(DEBUG, "Testing corner cases\n");
  
  // Test with empty strings
  bool emptyDbName = dbCheckExists(mainDatabase, "");
  printLog(DEBUG, "Empty database name check: %s\n", 
           emptyDbName ? "true" : "false");
  if (emptyDbName == true) {
    return false;
  }
  
  // Test field name with special characters
  bool specialCharValid = dbIsFieldNameValid("field@name#test");
  printLog(DEBUG, "Special character field name validation: %s\n", 
           specialCharValid ? "true" : "false");
  if (specialCharValid == true) {
    return false;
  }
  
  // Test with zero values
  i64 zeroRecords = dbGetNumRecords(mainDatabase, TEST_DATABASE_NAME, 
                                    "nonexistentTable");
  printLog(DEBUG, "Zero records count: %lld\n", (long long) zeroRecords);
  if (zeroRecords >= 0) {
    return false;
  }
  
  return true;
}

bool testDbResult(void) {
  DbResult* allRecords = dbGetRecords(mainDatabase, TEST_DATABASE_NAME, 
                                      TEST_TABLE_NAME);
  
  i64 eightyFour = 84;
  if (dbSetResultByName(allRecords, 0, "value", &eightyFour) == NULL) {
    printLog(DEBUG, "dbSetResultByName failed.");
    return false;
  }
  
  if (dbGetResultIndexByLookup(allRecords, "value", &eightyFour) != 0) {
    printLog(DEBUG, "dbGetResultIndexByLookup failed.");
    return false;
  }
  
  Bytes **bytesTable = dbResultToBytesTable(allRecords);
  if (bytesTable == NULL) {
    printLog(DEBUG, "dbResultToBytesTable failed.");
    return false;
  }
  bytesTable = freeBytesTable(bytesTable);
  
  // Our table is mixed-type, so dbResultGetBytesTable should fail since not all
  // values are Bytes values.
  if (dbResultGetBytesTable(allRecords) != NULL) {
    printLog(DEBUG, "dbResultGetBytesTable failed.");
    return false;
  }
  
  // dbResultGetRows does no type checking, so it should succeed.
  if (dbResultGetRows(allRecords) == NULL) {
    printLog(DEBUG, "dbResultGetRows failed.");
    return false;
  }
  
  allRecords = dbFreeResult(allRecords);
  return true;
}

///
/// @brief Creates a mock DbResult for testing
///
/// @return Pointer to created DbResult or NULL on failure
///
DbResult* createMockDbResult(void) {
  DbResult* result = (DbResult*) malloc(sizeof(DbResult));
  if (result == NULL) {
    return NULL;
  }
  
  // Initialize basic fields
  result->numFields = EXPECTED_NUM_FIELDS;
  result->numRows = EXPECTED_NUM_RECORDS + 1; // +1 for header row
  result->numResults = EXPECTED_NUM_RECORDS;
  result->successful = true;
  result->dbName = (char*) typeString->copy(TEST_DATABASE_NAME);
  result->tableName = (char*) typeString->copy(TEST_TABLE_NAME);
  result->database = mainDatabase;
  
  // Create field types array
  result->fieldTypes = (TypeDescriptor**) malloc(
    EXPECTED_NUM_FIELDS * sizeof(TypeDescriptor*));
  if (result->fieldTypes != NULL) {
    result->fieldTypes[0] = typeI64;
    result->fieldTypes[1] = typeString;
    result->fieldTypes[2] = typeI64;
  }
  
  // Create rows array
  result->rows = (void***) calloc(1, (result->numRows + 1) * sizeof(void**));
  if (result->rows != NULL) {
    for (u64 ii = 0; ii < result->numRows; ii++) {
      result->rows[ii] = (void**) calloc(1,
        (EXPECTED_NUM_FIELDS + 1) * sizeof(void*));
      if (result->rows[ii] != NULL) {
        if (ii == 0) {
          // Header row
          bytesAddStr((Bytes*) &result->rows[ii][0], "id");
          bytesAddStr((Bytes*) &result->rows[ii][1], "name");
          bytesAddStr((Bytes*) &result->rows[ii][2], "value");
        } else {
          // Data rows
          result->rows[ii][0] = malloc(sizeof(i64));
          *((i64*) result->rows[ii][0]) = ii;
          bytesAddStr((Bytes*) &result->rows[ii][1], "testData");
          result->rows[ii][2] = malloc(sizeof(i64));
          *((i64*) result->rows[ii][2]) = ii * 100;
        }
      }
    }
  }
  
  // Create field name index map
  result->fieldNameIndexMap = htCreate(typeString);
  if (result->fieldNameIndexMap != NULL) {
    i64 idIndex = 0;
    htAddEntry(result->fieldNameIndexMap, "id", &idIndex, typeI64);
    
    idIndex = 1;
    htAddEntry(result->fieldNameIndexMap, "name", &idIndex, typeI64);
    
    idIndex = 2;
    htAddEntry(result->fieldNameIndexMap, "value", &idIndex, typeI64);
  }
  
  return result;
}

///
/// @brief Creates a test dictionary for testing
///
/// @return Pointer to created Dictionary or NULL on failure
///
Dictionary* createTestDictionary(void) {
  Dictionary* dict = dictionaryCreate(typeString);
  if (dict == NULL) {
    return NULL;
  }
  
  i64 id = TEST_INTEGER_VALUE;
  i64 value = TEST_INTEGER_VALUE;
  dictionaryAddEntry(dict, "id", &id, typeI64);
  dictionaryAddEntry(dict, "name", TEST_STRING_VALUE, typeString);
  dictionaryAddEntry(dict, "value", &value, typeI64);
  
  return dict;
}

//// ///
//// /// @brief Validates a DbResult structure
//// ///
//// /// @param result The DbResult to validate
//// /// @param expectedRows The expected number of rows
//// ///
//// /// @return true if validation passes, false otherwise
//// ///
//// bool validateDbResult(const DbResult* result, u64 expectedRows) {
////   if (result == NULL) {
////     printLog(ERR, "DbResult is NULL\n");
////     return false;
////   }
////   
////   if (result->numRows != expectedRows) {
////     printLog(ERR, "Expected %llu rows, got %llu\n", 
////              (unsigned long long) expectedRows, 
////              (unsigned long long) result->numRows);
////     return false;
////   }
////   
////   if (result->rows == NULL && expectedRows > 0) {
////     printLog(ERR, "Rows array is NULL but expected %llu rows\n", 
////              (unsigned long long) expectedRows);
////     return false;
////   }
////   
////   if (result->fieldTypes == NULL && result->numFields > 0) {
////     printLog(ERR, "Field types array is NULL but expected %llu fields\n", 
////              (unsigned long long) result->numFields);
////     return false;
////   }
////   
////   if (result->fieldNameIndexMap == NULL && result->numFields > 0) {
////     printLog(ERR, "Field name index map is NULL but expected %llu fields\n", 
////              (unsigned long long) result->numFields);
////     return false;
////   }
////   
////   if (!result->successful) {
////     printLog(ERR, "DbResult indicates operation was not successful\n");
////     return false;
////   }
////   
////   printLog(DEBUG, "DbResult validation passed\n");
////   return true;
//// }
