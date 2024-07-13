////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2024 James Card                     //
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
//                                 James Card                                 //
//                          http://www.jamescard.org                          //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

// Doxygen marker
/// @file

#include "SqliteLib.h"
#include "SqlClientLib.h"
#include <stdio.h>

DbResult* sqliteGetDatabaseNames(SqlDatabase *database);
DbResult* sqliteGetTableNames(SqlDatabase *database, const char *dbString);

#define leaf(path) ((strrchr(path, '/')) ? (strrchr(path, '/') + 1) : path)
int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: %s <database directory> <SQL statement> [options]\n",
      leaf(argv[0]));
    return 1;
  }
  
  // Parse the command line arguments.
  Dictionary *argList = parseCommandLine(argc, argv);
  // We need to do this before we do anything else to ensure we're initializing
  // and verifying the right database.
  dbInstance = (char*) dictionaryGetValue(argList, "dbInstance");
  
  char *dbDirectory = (char*) dictionaryGetValue(argList, "unnamedParameter0");
  char *sql = (char*) dictionaryGetValue(argList, "unnamedParameter1");
  
  Database *database = sqliteInit(dbDirectory);
  if (database == NULL) {
    fprintf(stderr, "sqliteInit failure.\n");
    return 1;
  }
  
  if (sql == NULL) {
    DbResult *queryResult = sqliteGetDatabaseNames((SqlDatabase*) database->db);
    fputs("Databases:\n", stdout);
    for (u64 i = 0; i < queryResult->numResults; i++) {
      fputs(dbGetStringByIndex(queryResult, i, 0), stdout);
      fputs("\n", stdout);
    }
    queryResult = dbFreeResult(queryResult);
    fputs("\n", stdout);
    
    DbResult *databaseNames
      = dbGetValues(database, "main", "Databases", "name", "type", "sqlite");
    for (u64 i = 0; i < databaseNames->numResults; i++) {
      const char *dbName = dbGetStringByName(databaseNames, i, "name");
      DbResult *tableNames = dbGetTableNames(database, dbName);
      fprintf(stdout, "Tables in %s:\n", dbName);
      for (u64 j = 0; j < tableNames->numResults; j++) {
        fputs(dbGetStringByIndex(tableNames, j, 0), stdout);
        fputs("\n", stdout);
      }
      tableNames = dbFreeResult(tableNames);
      fputs("\n", stdout);
    }
    databaseNames = dbFreeResult(databaseNames);
    
    return 0;
  }
  
  int returnValue = 0;
  DbResult *queryResult = sqlQuery((SqlDatabase*) database->db, sql);
  bool querySuccessful = queryResult->successful;
  if (queryResult->numResults > 0) {
    fputs("Query results:\n", stdout);
    for (u64 i = 0; i < queryResult->numRows; i++) {
      for (u64 j = 0; j < queryResult->numFields; j++) {
        if (queryResult->rows[i][j] != NULL) {
          if (i > 0) {
            char *value
              = queryResult->fieldTypes[j]->toString(queryResult->rows[i][j]);
            fputs(value, stdout);
            value = stringDestroy(value);
          } else {
            fputs((char*) queryResult->rows[i][j], stdout);
          }
        }
        if (j < queryResult->numFields - 1) {
          fputs(", ", stdout);
        }
      }
      fputs("\n", stdout);
    }
  } else if (querySuccessful == true) {
    fputs("Query successful.\n", stdout);
  } else {
    fputs("Query failed.\n", stderr);
    returnValue = 1;
  }
  queryResult = dbFreeResult(queryResult);
  
  return returnValue;
}

