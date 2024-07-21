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

#include "MariaDbLib.h"
#include "SqlClientLib.h"
#include "DbInterface.h"
#ifdef LOGGING_ENABLED
#include "LoggingLib.h"
#else
#undef printLog
#define printLog(...) {}
#undef printBinary
#define printBinary(...) {}
#define logFile stderr
#define LOG_MALLOC_FAILURE(...) {}
#endif
#include "Scope.h"
#include "OsApi.h"

// External function prototypes.
bool sqlInvalidateTableDescription(void *db,
  const char *dbName, const char *tableName);

// Support function prototypes.  Doxygen comments above definitions.
int mariaDbConnect(const char *remoteHostAddress,
  const char *username, const char *password,
  MariaDbPasswordHashType passwordHashType,
  char **connectionAddress);
DbResult* mariaDbExecQueryBytes(MariaDb *database, const Bytes query);
DbResult* mariaDbExecQueryString(MariaDb *database, const char *query);
char* binToHex(const unsigned char *bin, long long unsigned int length,
  char *outputString);
DbResult* mariaDbGetDatabaseNames(SqlDatabase *database);
DbResult* mariaDbGetTableNames(SqlDatabase *database, const char *dbString);
bool mariaDbAddDatabase(SqlDatabase *database, const char *dbName);
bool mariaDbDeleteDatabase(SqlDatabase *database, const char *dbName);
Bytes mariaDbMakeStringLiteral(const char *input);
Bytes mariaDbMakeBytesLiteral(const Bytes input);

char *mariaDbDecodeErrorPacket(u8 *response, u32 responseLength,
  u16 *errorCode, char **sqlState);
int mariaDbGetDataFromServer(u8 **response, u32 *bytesReceived);
u64 mariaDbGetLengthEncodedInteger(
  unsigned char **packetData, u64 *index, u32 *bytesReceived);
Bytes mariaDbGetLengthEncodedString(
  unsigned char **packetData, u64 *index, u32 *bytesReceived);
void* mariaDbGetI8(
  unsigned char **packetData, u64 *index, u32 *bytesReceived);
void* mariaDbGetI16(
  unsigned char **packetData, u64 *index, u32 *bytesReceived);
void* mariaDbGetI24(
  unsigned char **packetData, u64 *index, u32 *bytesReceived);
void* mariaDbGetI32(
  unsigned char **packetData, u64 *index, u32 *bytesReceived);
void* mariaDbGetI64(
  unsigned char **packetData, u64 *index, u32 *bytesReceived);
void* mariaDbGetFloat(
  unsigned char **packetData, u64 *index, u32 *bytesReceived);
void* mariaDbGetDouble(
  unsigned char **packetData, u64 *index, u32 *bytesReceived);
bool mariaDbStartTransaction(SqlDatabase *database);
bool mariaDbCommitTransaction(SqlDatabase *database);
bool mariaDbRollbackTransaction(SqlDatabase *database);
bool mariaDbLockTablesDict(SqlDatabase *database, const Dictionary *tablesToLock);
bool mariaDbUnlockTables(SqlDatabase *database, const Dictionary *tableLock);
SqlDatabase* mariaDbDisconnect(SqlDatabase *sqlDatabase);
DbResult* mariaDbDescribeTable(MariaDb *database, const char *dbString,
  const char *tableName);
int mariaDbCompare(void *db1, void *db2);
bool mariaDbChangeFieldType(void *db, const char *dbString,
  const char *tableName, const char *fieldName, const void *type);
i64 mariaDbGetSize(void *db, const char *dbName);
bool mariaDbRenameDatabase(void *db,
  const char *oldDbName, const char *newDbName);
int sha1digest(uint8_t *digest, char *hexdigest,
  const uint8_t *data, size_t databytes);

/// @def SHA_DIGEST_LENGTH
///
/// @brief The number of bytes in a SHA1 hash.  Note that this is the binary
/// version.  The hex version would be twice as long (plus a final NULL byte).
# define SHA_DIGEST_LENGTH 20

/// @def DEFAULT_USERNAME
///
/// @brief The default username to use if the user doesn't specify one when
/// connecting.
#define DEFAULT_USERNAME "root"

/// @def DEFAULT_PASSWORD
///
/// @brief The default password to use if the user doesn't specify one when
/// connecting.
#define DEFAULT_PASSWORD ""

/// @var _defaultUsername
///
/// @brief Global variable.  Username to use for MariaDb connection.
static char *_defaultUsername = NULL;

/// @var _defaultPassword
///
/// @brief Global variable.  Password to use for MariaDb connection.
static char *_defaultPassword = NULL;

/// @var _defaultPasswordHashType
///
/// @brief Global variable.  Tracks the MariaDbPasswordHashType value specifying
///   what kind of value is encoded in _defaultPassword.
static MariaDbPasswordHashType _defaultPasswordHashType
  = MARIADB_PASSWORD_PLAINTEXT;

/// @var _defaultHostAddress
///
/// @brief The host address used to connect to MariaDB.
static char *_defaultHostAddress = NULL;

/// @var _defaultHostPort
///
/// @brief The port used to connect to MariaDB.  This is the MariaDb default.
static int _defaultHostPort = 3306;

/// @var _mariaDbThreadSetup
///
/// @brief once_flag to ensure that we only initialize the below two pieces
/// of metadata once.
static once_flag _mariaDbThreadSetup = ONCE_FLAG_INIT;

/// @var _threadClientSocket
///
/// @brief Thread-specific storage for the connection this thread is using
/// to communicate with the database.
ZEROINIT(static tss_t _threadClientSocket);

/// @var _inUseMariaDbObjects
///
/// @brief Hash table of queues of connections that have established to the
/// databases.  Keys are thrd_t objects and values are pointers to the queues.
static HashTable *_inUseMariaDbObjects = NULL;

/// @var mysqlFieldTypeNames
///
/// @brief Mapping of MysqlFieldType to its corresponding string representation.
/// Get the index into this array by calling getMysqlFieldTypeIndex.
const char* mysqlFieldTypeNames[] = {
  "MYSQL_TYPE_DECIMAL",
  "MYSQL_TYPE_TINY",
  "MYSQL_TYPE_SHORT",
  "MYSQL_TYPE_LONG",
  "MYSQL_TYPE_FLOAT",
  "MYSQL_TYPE_DOUBLE",
  "MYSQL_TYPE_NULL",
  "MYSQL_TYPE_TIMESTAMP",
  "MYSQL_TYPE_LONGLONG",
  "MYSQL_TYPE_INT24",
  "MYSQL_TYPE_DATE",
  "MYSQL_TYPE_TIME",
  "MYSQL_TYPE_DATETIME",
  "MYSQL_TYPE_YEAR",
  "MYSQL_TYPE_NEWDATE",
  "MYSQL_TYPE_VARCHAR",
  "MYSQL_TYPE_BIT",
  "MYSQL_TYPE_TIMESTAMP2",
  "MYSQL_TYPE_DATETIME2",
  "MYSQL_TYPE_TIME2",
  "MYSQL_TYPE_TYPED_ARRAY",
  "MYSQL_TYPE_JSON",
  "MYSQL_TYPE_NEWDECIMAL",
  "MYSQL_TYPE_ENUM",
  "MYSQL_TYPE_SET",
  "MYSQL_TYPE_TINY_BLOB",
  "MYSQL_TYPE_MEDIUM_BLOB",
  "MYSQL_TYPE_LONG_BLOB",
  "MYSQL_TYPE_BLOB",
  "MYSQL_TYPE_VAR_STRING",
  "MYSQL_TYPE_STRING",
  "MYSQL_TYPE_GEOMETRY"
};

/// @var mysqlFieldDataTypes
/// @brief Mapping of MysqlFieldType to its corresponding packet data type.
/// Get the index into this array by calling getMysqlFieldTypeIndex.
MysqlDataType mysqlFieldTypeDataTypes[] = {
  lenStr, // MYSQL_TYPE_DECIMAL
  int1,   // MYSQL_TYPE_TINY
  int2,   // MYSQL_TYPE_SHORT
  int4,   // MYSQL_TYPE_LONG
  float4, // MYSQL_TYPE_FLOAT
  float8, // MYSQL_TYPE_DOUBLE
  int0,   // MYSQL_TYPE_NULL
  int11,  // MYSQL_TYPE_TIMESTAMP
  int8,   // MYSQL_TYPE_LONGLONG
  int4,   // MYSQL_TYPE_INT24
  int11,  // MYSQL_TYPE_DATE
  int11,  // MYSQL_TYPE_TIME
  int11,  // MYSQL_TYPE_DATETIME
  int4,   // MYSQL_TYPE_YEAR
  int0,   // MYSQL_TYPE_NEWDATE - Internal type, not used in protocol
  lenStr, // MYSQL_TYPE_VARCHAR
  lenStr, // MYSQL_TYPE_BIT
  int11,  // MYSQL_TYPE_TIMESTAMP2
  int0,   // MYSQL_TYPE_DATETIME2 - Internal type, not used in protocol
  int0,   // MYSQL_TYPE_TIME2 - Internal type, not used in protocol
  int11,  // MYSQL_TYPE_TYPED_ARRAY - Used for replication only
  lenStr, // MYSQL_TYPE_JSON
  lenStr, // MYSQL_TYPE_NEWDECIMAL
  lenStr, // MYSQL_TYPE_ENUM
  lenStr, // MYSQL_TYPE_SET
  lenStr, // MYSQL_TYPE_TINY_BLOB
  lenStr, // MYSQL_TYPE_MEDIUM_BLOB
  lenStr, // MYSQL_TYPE_LONG_BLOB
  lenStr, // MYSQL_TYPE_BLOB
  lenStr, // MYSQL_TYPE_VAR_STRING
  lenStr, // MYSQL_TYPE_STRING
  lenStr  // MYSQL_TYPE_GEOMETRY
};

/// @var typeI24
///
/// @brief TypeDescriptor for MariaDB's 24-bit integer type.  This is stored
/// and handled exactly the same as an i32 value for this library and typeI24
/// will be initialized to typeI32 upon library initialization.  We need a
/// different pointer value to distinguish the two types in lookups.
ZEROINIT(TypeDescriptor _typeI24);
TypeDescriptor *typeI24 = &_typeI24;

/// @var mysqlFieldDataTypes
/// @brief Mapping of MysqlFieldType to its corresponding packet data type.
/// Get the index into this array by calling getMysqlFieldTypeIndex.
ZEROINIT(TypeDescriptor *mysqlFieldTypeTypeDescriptors[NUM_MYSQL_FIELD_TYPES]);

typedef void* (*MariaDbGetValue)(unsigned char**, u64*, u32*);

/// @var getValueFunctions
///
/// @brief Array of function pointers that holds the getters for individual
/// types.
MariaDbGetValue getValueFunctions[NUM_MYSQL_FIELD_TYPES] = {
  (MariaDbGetValue) mariaDbGetLengthEncodedString, // MYSQL_TYPE_DECIMAL
  mariaDbGetI8,                                    // MYSQL_TYPE_TINY
  mariaDbGetI16,                                   // MYSQL_TYPE_SHORT
  mariaDbGetI32,                                   // MYSQL_TYPE_LONG
  mariaDbGetFloat,                                 // MYSQL_TYPE_FLOAT
  mariaDbGetDouble,                                // MYSQL_TYPE_DOUBLE
  NULL,                                            // MYSQL_TYPE_NULL
  NULL,                                            // MYSQL_TYPE_TIMESTAMP
  mariaDbGetI64,                                   // MYSQL_TYPE_LONGLONG
  mariaDbGetI24,                                   // MYSQL_TYPE_INT24
  NULL,                                            // MYSQL_TYPE_DATE
  NULL,                                            // MYSQL_TYPE_TIME
  NULL,                                            // MYSQL_TYPE_DATETIME
  mariaDbGetI32,                                   // MYSQL_TYPE_YEAR
  NULL,                                            // MYSQL_TYPE_NEWDATE
  (MariaDbGetValue) mariaDbGetLengthEncodedString, // MYSQL_TYPE_VARCHAR
  (MariaDbGetValue) mariaDbGetLengthEncodedString, // MYSQL_TYPE_BIT
  NULL,                                            // MYSQL_TYPE_TIMESTAMP2
  NULL,                                            // MYSQL_TYPE_DATETIME2
  NULL,                                            // MYSQL_TYPE_TIME2
  NULL,                                            // MYSQL_TYPE_TYPED_ARRAY
  (MariaDbGetValue) mariaDbGetLengthEncodedString, // MYSQL_TYPE_JSON
  (MariaDbGetValue) mariaDbGetLengthEncodedString, // MYSQL_TYPE_NEWDECIMAL
  (MariaDbGetValue) mariaDbGetLengthEncodedString, // MYSQL_TYPE_ENUM
  (MariaDbGetValue) mariaDbGetLengthEncodedString, // MYSQL_TYPE_SET
  (MariaDbGetValue) mariaDbGetLengthEncodedString, // MYSQL_TYPE_TINY_BLOB
  (MariaDbGetValue) mariaDbGetLengthEncodedString, // MYSQL_TYPE_MEDIUM_BLOB
  (MariaDbGetValue) mariaDbGetLengthEncodedString, // MYSQL_TYPE_LONG_BLOB
  (MariaDbGetValue) mariaDbGetLengthEncodedString, // MYSQL_TYPE_BLOB
  (MariaDbGetValue) mariaDbGetLengthEncodedString, // MYSQL_TYPE_VAR_STRING
  (MariaDbGetValue) mariaDbGetLengthEncodedString, // MYSQL_TYPE_STRING
  (MariaDbGetValue) mariaDbGetLengthEncodedString, // MYSQL_TYPE_GEOMETRY
};

/// @var mariaDbPasswordHashTypeNames
///
/// @brief String values that correspond to the enum values in
/// MariaDbPasswordHashType.
const char *mariaDbPasswordHashTypeNames[] = {
  "PLAINTEXT",
  "SHA1",
};

/// @fn int getMysqlFieldTypeIndex(MysqlFieldType type)
///
/// @brief Get the index into one of the arrays above given a MysqlFieldType.
///
/// @param type The MysqlFieldType returned in the packet.
///
/// @return Returns the index of the array.  This function always succeeds.
int getMysqlFieldTypeIndex(MysqlFieldType type) {
  int index = (int) type;
  if (type >= MYSQL_TYPE_JSON) {
    index = type - MYSQL_TYPE_JSON + MYSQL_TYPE_TYPED_ARRAY + MYSQL_TYPE_TINY;
  }
  
  return index;
}

/// @fn const char* getMysqlFieldTypeName(MysqlFieldType type)
///
/// @brief Get the name of the MysqlFieldType given the value of its type as
/// returned in a MySQL response packet.
///
/// @param type The MysqlFieldType.
///
/// @return Returns a pointer from the mysqlFieldTypeNames array above.
const char* getMysqlFieldTypeName(MysqlFieldType type) {
  int index = getMysqlFieldTypeIndex(type);
  
  return mysqlFieldTypeNames[index];
}

/// @fn MysqlDataType getMysqlDataType(MysqlFieldType type)
///
/// @brief Get the Mysql data type given the field type as returned in a MySQL
/// response packet.
///
/// @param type The MysqlFieldType.
///
/// @return Returns the MysqlDataType corresponding to the provided field type.
MysqlDataType getMysqlDataType(MysqlFieldType type) {
  int index = getMysqlFieldTypeIndex(type);
  
  return mysqlFieldTypeDataTypes[index];
}

/// @fn MariaDbGetValue getterFromType(TypeDescriptor *type)
///
/// @brief Get the appropriate getter based on the TypeDescriptor for a field.
///
/// @param type The TypeDescriptor for the field.
///
/// @return Returns a pointer to the appropriate getter on success, NULL on
/// failure.  Please note that as some field types are not supported by this
/// library, some of the MySQL types do map to NULL in the getValueFunctions
/// array.  Therefore, NULL is also a valid value from this function as well.
MariaDbGetValue getterFromType(TypeDescriptor *type) {
  MariaDbGetValue returnValue = NULL;
  
  for (i32 i = 0; i < NUM_MYSQL_FIELD_TYPES; i++) {
    if (mysqlFieldTypeTypeDescriptors[i] == type) {
      returnValue = getValueFunctions[i];
      break;
    }
  }
  
  return returnValue;
}

/// @fn void freeDbClientSocket(void *socketPointer)
///
/// @brief Push an unused dbClientSocket back on to the queue of available
/// sockets for use by a future thread.
///
/// @details Due to the requirements set by the C threading library, a
///   destructor for thread specific storage (which is what dbClientSocket is)
///   is only allowed one parameter.  This is not enough information for us
///   because the appropriate queue to push back to is on a specific database
///   object.  When the connection is first established, we make a record of
///   the appropriate object in the _inUseMariaDbObjects hash table with
///   the thread ID as the key.  Because this thread is now exiting, we have
///   to retrieve the appropriate object, push our socket onto its queue, and
///   remove the entry in the hash table.
///
/// @param dbClientSocket A pointer to the socket that is no longer being used
///   by the current thread.
///
/// @return This function returns no value.
void freeDbClientSocket(void *dbClientSocket) {
  if (dbClientSocket != NULL) {
    thrd_t threadId = thrd_current();
    MariaDb *database
      = (MariaDb*) htGetValue(_inUseMariaDbObjects, &threadId);
    if (database != NULL) {
      mtx_lock(&database->lock);
      Queue *socketQueue = database->availableDbSockets;
      queuePushEntry(socketQueue, dbClientSocket);
      cnd_signal(&database->socketAvailable);
      mtx_unlock(&database->lock);
    } else {
      // Corresponding database has been disconnected.  Destroy the socket.
      dbClientSocket = socketDestroy((Socket*) dbClientSocket);
    }
    htRemoveEntry(_inUseMariaDbObjects, &threadId);
  }
}

/// @fn void releaseDbClientSocket(Socket *dbClientSocket)
///
/// @brief Disassociate a socket with a thread and return it back to the
/// database's socket queue.
///
/// @param database A pointer to the MariaDb instance whose queue the socket
///   should be added to.
/// @param dbClientSocket A pointer to the socket that is no longer being used
///   by the current thread.
///
/// @return This function returns no value.
void releaseDbClientSocket(MariaDb *database, Socket *dbClientSocket) {
  if ((database != NULL) && (dbClientSocket != NULL)) {
    printLog(DEBUG, "database->transactionInProgress = %s\n",
      boolNames[(intptr_t) tss_get(database->transactionInProgress)]);
    printLog(DEBUG, "database->tablesLocked = %s\n",
      boolNames[(intptr_t) tss_get(database->tablesLocked)]);
    if ((tss_get(database->transactionInProgress) == VOID_POINTER_FALSE)
      && (tss_get(database->tablesLocked) == VOID_POINTER_FALSE)
    ) {
      if (dbClientSocket != NULL) {
        tss_set(_threadClientSocket, NULL);
        mtx_lock(&database->lock);
        Queue *socketQueue = database->availableDbSockets;
        queuePushEntry(socketQueue, dbClientSocket);
        cnd_signal(&database->socketAvailable);
        mtx_unlock(&database->lock);
      }
    }
    // else the socket needs to stay associated with the thread until the
    // transaction is complete and tables are unlocked.
  } else if (dbClientSocket != NULL) {
    // Corresponding database has been disconnected.  Destroy the socket.
    dbClientSocket = socketDestroy((Socket*) dbClientSocket);
  }
}

/// @fn void setupMariaDbThreadMetadata(void)
///
/// @brief Setup the metadata for this library (the thread specific storage
/// for a dbClientSocket pointer and the queue of available dbClientSocket
/// pointers).  This function is and must be called only once.
///
/// @return This function returns no value.
void setupMariaDbThreadMetadata(void) {
  int status = tss_create(&_threadClientSocket, freeDbClientSocket);
  if (status != thrd_success) {
    fprintf(stderr, "Could not initialize _threadClientSocket.\n");
  }
  
  if (sizeof(thrd_t) >= sizeof(u64)) {
    _inUseMariaDbObjects = htCreate(typeU64);
  } else if (sizeof(thrd_t) == sizeof(u32)) {
    _inUseMariaDbObjects = htCreate(typeU32);
  } else if (sizeof(thrd_t) == sizeof(u16)) {
    _inUseMariaDbObjects = htCreate(typeU16);
  } else if (sizeof(thrd_t) == sizeof(u8)) {
    _inUseMariaDbObjects = htCreate(typeU8);
  }
  
  if (_inUseMariaDbObjects == NULL) {
    fprintf(stderr, "Could not initialize _inUseMariaDbObjects.\n");
  }
  
  _typeI24 = *typeI32;
  
  mysqlFieldTypeTypeDescriptors[0] = typeBytes;   // MYSQL_TYPE_DECIMAL
  mysqlFieldTypeTypeDescriptors[1] = typeI8;      // MYSQL_TYPE_TINY
  mysqlFieldTypeTypeDescriptors[2] = typeI16;     // MYSQL_TYPE_SHORT
  mysqlFieldTypeTypeDescriptors[3] = typeI32;     // MYSQL_TYPE_LONG
  mysqlFieldTypeTypeDescriptors[4] = typeFloat;   // MYSQL_TYPE_FLOAT
  mysqlFieldTypeTypeDescriptors[5] = typeDouble;  // MYSQL_TYPE_DOUBLE
  mysqlFieldTypeTypeDescriptors[6] = NULL;        // MYSQL_TYPE_NULL
  mysqlFieldTypeTypeDescriptors[7] = NULL;        // MYSQL_TYPE_TIMESTAMP
  mysqlFieldTypeTypeDescriptors[8] = typeI64;     // MYSQL_TYPE_LONGLONG
  mysqlFieldTypeTypeDescriptors[9] = typeI24;     // MYSQL_TYPE_INT24
  mysqlFieldTypeTypeDescriptors[10] = NULL;       // MYSQL_TYPE_DATE
  mysqlFieldTypeTypeDescriptors[11] = NULL;       // MYSQL_TYPE_TIME
  mysqlFieldTypeTypeDescriptors[12] = NULL;       // MYSQL_TYPE_DATETIME
  mysqlFieldTypeTypeDescriptors[13] = typeI32;    // MYSQL_TYPE_YEAR
  mysqlFieldTypeTypeDescriptors[14] = NULL;       // MYSQL_TYPE_NEWDATE - Internal type, not used in protocol
  mysqlFieldTypeTypeDescriptors[15] = typeString; // MYSQL_TYPE_VARCHAR
  mysqlFieldTypeTypeDescriptors[16] = typeBytes;  // MYSQL_TYPE_BIT
  mysqlFieldTypeTypeDescriptors[17] = NULL;       // MYSQL_TYPE_TIMESTAMP2
  mysqlFieldTypeTypeDescriptors[18] = NULL;       // MYSQL_TYPE_DATETIME2 - Internal type, not used in protocol
  mysqlFieldTypeTypeDescriptors[19] = NULL;       // MYSQL_TYPE_TIME2 - Internal type, not used in protocol
  mysqlFieldTypeTypeDescriptors[20] = NULL;       // MYSQL_TYPE_TYPED_ARRAY - Used for replication only
  mysqlFieldTypeTypeDescriptors[21] = typeBytes;  // MYSQL_TYPE_JSON
  mysqlFieldTypeTypeDescriptors[22] = typeBytes;  // MYSQL_TYPE_NEWDECIMAL
  mysqlFieldTypeTypeDescriptors[23] = typeBytes;  // MYSQL_TYPE_ENUM
  mysqlFieldTypeTypeDescriptors[24] = typeBytes;  // MYSQL_TYPE_SET
  mysqlFieldTypeTypeDescriptors[25] = typeBytes;  // MYSQL_TYPE_TINY_BLOB
  mysqlFieldTypeTypeDescriptors[26] = typeBytes;  // MYSQL_TYPE_MEDIUM_BLOB
  mysqlFieldTypeTypeDescriptors[27] = typeBytes;  // MYSQL_TYPE_LONG_BLOB
  mysqlFieldTypeTypeDescriptors[28] = typeBytes;  // MYSQL_TYPE_BLOB
  mysqlFieldTypeTypeDescriptors[29] = typeString; // MYSQL_TYPE_VAR_STRING
  mysqlFieldTypeTypeDescriptors[30] = typeString; // MYSQL_TYPE_STRING
  mysqlFieldTypeTypeDescriptors[31] = typeBytes;  // MYSQL_TYPE_GEOMETRY
  
  return;
}

/// @fn Database* mariaDbInit_(const char *ignored, const char *remoteHostAddress, const char *username, const char *password, ...)
///
/// @brief Initialize the database.
///
/// @param ignored Ignored by this function.  Present to make the macro work.
/// @param remoteHostAddress A string representing the host and port of the
///   MariaDb server.
/// @param username The username to use to connect to the server.
/// @param password The password for the user account.
/// @param passwordHashType MariaDbPasswordHashType value specifying what kind
///   of value is encoded in the password field.
///
/// @details This is an abstraction function to keep the database-specific
///   details out of the main code.  The code needs only to know that it has
///   to call this function before interacting with any database functions.
///   The mechanics "under the hood" are done here so that we can change the
///   underlying database at a later point in time if we need to.
///
/// @return Returns a pointer to a Database object on success, NULL on failure.
Database* mariaDbInit_(const char *ignored,
  const char *remoteHostAddress,
  const char *username, const char *password,
  int passwordHashType, ...
) {
  (void) ignored;
  
  printLog(TRACE,
    "ENTER mariaDbInit(remoteHostAddress=\"%s\", "
    "username=\"%s\", password=\"%s\", passwordHashType=%s)\n",
    remoteHostAddress, username, password,
    mariaDbPasswordHashTypeNames[passwordHashType]
  );
  
  call_once(&_mariaDbThreadSetup, setupMariaDbThreadMetadata);
  
  char *connectionAddress = NULL;
  int connectStatus
    = mariaDbConnect(remoteHostAddress, username, password,
    (MariaDbPasswordHashType) passwordHashType, &connectionAddress);
  if (connectStatus != 0) {
    printLog(ERR, "mariaDbConnect failed with status %d\n", connectStatus);
    connectionAddress = stringDestroy(connectionAddress);
    printLog(TRACE,
      "EXIT mariaDbInit(remoteHostAddress=\"%s\", username=\"%s\", "
      "password=\"%s\", passwordHashType=%s) = {NULL}\n",
      remoteHostAddress, username, password,
      mariaDbPasswordHashTypeNames[passwordHashType]);
    return NULL;
  }
  
  printLog(DEBUG, "Successfully connected to database.\n");
  MariaDb *mariaDb = (MariaDb*) calloc(1, sizeof(MariaDb));
  if (mariaDb == NULL) {
    LOG_MALLOC_FAILURE();
    connectionAddress = stringDestroy(connectionAddress);
    printLog(TRACE,
      "EXIT mariaDbInit(remoteHostAddress=\"%s\", username=\"%s\", "
      "password=\"%s\", passwordHashType=%s) = {NULL}\n",
      remoteHostAddress, username, password,
      mariaDbPasswordHashTypeNames[passwordHashType]);
    return NULL;
  }
  mariaDb->remoteHostAddress = connectionAddress;
  straddstr(&mariaDb->username, (username == NULL) ? "root" : username);
  mariaDb->passwordHashType = (MariaDbPasswordHashType) passwordHashType;
  if (password != NULL) {
    straddstr(&mariaDb->password, password);
  } // else mariaDb->password will remain NULL
  mariaDb->availableDbSockets = queueCreate(typePointerNoCopy);
  if (mtx_init(&mariaDb->lock, mtx_plain | mtx_recursive) != thrd_success) {
    printLog(ERR, "Could not initialize mariaDb->lock.\n");
    mariaDb->remoteHostAddress = stringDestroy(mariaDb->remoteHostAddress);
    mariaDb->username = stringDestroy(mariaDb->username);
    mariaDb->password = stringDestroy(mariaDb->password);
    mariaDb->availableDbSockets = queueDestroy(mariaDb->availableDbSockets);
    mariaDb = (MariaDb*) pointerDestroy(mariaDb);
    printLog(TRACE,
      "EXIT mariaDbInit(remoteHostAddress=\"%s\", username=\"%s\", "
      "password=\"%s\", passwordHashType=%s) = {NULL}\n",
      remoteHostAddress, username, password,
      mariaDbPasswordHashTypeNames[passwordHashType]);
    return NULL;
  }
  mariaDb->maxConnections = MAX_CONCURRENT_MARIADB_CONNECTIONS;
  // mariaDb->numConnections is initialized to zero through calloc.
  if (cnd_init(&mariaDb->socketAvailable) != thrd_success) {
    printLog(ERR, "Could not initialize mariaDb->socketAvailable.\n");
    mtx_destroy(&mariaDb->lock);
    mariaDb->remoteHostAddress = stringDestroy(mariaDb->remoteHostAddress);
    mariaDb->username = stringDestroy(mariaDb->username);
    mariaDb->password = stringDestroy(mariaDb->password);
    mariaDb->availableDbSockets = queueDestroy(mariaDb->availableDbSockets);
    mariaDb = (MariaDb*) pointerDestroy(mariaDb);
    printLog(TRACE,
      "EXIT mariaDbInit(remoteHostAddress=\"%s\", username=\"%s\", "
      "password=\"%s\", passwordHashType=%s) = {NULL}\n",
      remoteHostAddress, username, password,
      mariaDbPasswordHashTypeNames[passwordHashType]);
    return NULL;
  }
  int status = tss_create(&mariaDb->transactionInProgress, NULL);
  if (status != thrd_success) {
    fprintf(stderr, "Could not initialize mariaDb->transactionInProgress.\n");
    cnd_destroy(&mariaDb->socketAvailable);
    mtx_destroy(&mariaDb->lock);
    mariaDb->remoteHostAddress = stringDestroy(mariaDb->remoteHostAddress);
    mariaDb->username = stringDestroy(mariaDb->username);
    mariaDb->password = stringDestroy(mariaDb->password);
    mariaDb->availableDbSockets = queueDestroy(mariaDb->availableDbSockets);
    mariaDb = (MariaDb*) pointerDestroy(mariaDb);
    printLog(TRACE,
      "EXIT mariaDbInit(remoteHostAddress=\"%s\", username=\"%s\", "
      "password=\"%s\", passwordHashType=%s) = {NULL}\n",
      remoteHostAddress, username, password,
      mariaDbPasswordHashTypeNames[passwordHashType]);
    return NULL;
  }
  status = tss_create(&mariaDb->tablesLocked, NULL);
  if (status != thrd_success) {
    fprintf(stderr, "Could not initialize mariaDb->tablesLocked.\n");
    tss_delete(mariaDb->transactionInProgress);
    cnd_destroy(&mariaDb->socketAvailable);
    mtx_destroy(&mariaDb->lock);
    mariaDb->remoteHostAddress = stringDestroy(mariaDb->remoteHostAddress);
    mariaDb->username = stringDestroy(mariaDb->username);
    mariaDb->password = stringDestroy(mariaDb->password);
    mariaDb->availableDbSockets = queueDestroy(mariaDb->availableDbSockets);
    mariaDb = (MariaDb*) pointerDestroy(mariaDb);
    printLog(TRACE,
      "EXIT mariaDbInit(remoteHostAddress=\"%s\", username=\"%s\", "
      "password=\"%s\", passwordHashType=%s) = {NULL}\n",
      remoteHostAddress, username, password,
      mariaDbPasswordHashTypeNames[passwordHashType]);
    return NULL;
  }
  status = tss_create(&mariaDb->transactionCount, NULL);
  if (status != thrd_success) {
    fprintf(stderr, "Could not initialize mariaDb->tablesLocked.\n");
    tss_delete(mariaDb->tablesLocked);
    tss_delete(mariaDb->transactionInProgress);
    cnd_destroy(&mariaDb->socketAvailable);
    mtx_destroy(&mariaDb->lock);
    mariaDb->remoteHostAddress = stringDestroy(mariaDb->remoteHostAddress);
    mariaDb->username = stringDestroy(mariaDb->username);
    mariaDb->password = stringDestroy(mariaDb->password);
    mariaDb->availableDbSockets = queueDestroy(mariaDb->availableDbSockets);
    mariaDb = (MariaDb*) pointerDestroy(mariaDb);
    printLog(TRACE,
      "EXIT mariaDbInit(remoteHostAddress=\"%s\", username=\"%s\", "
      "password=\"%s\", passwordHashType=%s) = {NULL}\n",
      remoteHostAddress, username, password,
      mariaDbPasswordHashTypeNames[passwordHashType]);
    return NULL;
  }
  
  
  // SqlDatabase-level object.
  SqlDatabase *sqlDatabase = (SqlDatabase*) malloc(sizeof(SqlDatabase));
  if (sqlDatabase == NULL) {
    tss_delete(mariaDb->transactionCount);
    tss_delete(mariaDb->tablesLocked);
    tss_delete(mariaDb->transactionInProgress);
    cnd_destroy(&mariaDb->socketAvailable);
    mtx_destroy(&mariaDb->lock);
    mariaDb->remoteHostAddress = stringDestroy(mariaDb->remoteHostAddress);
    mariaDb->username = stringDestroy(mariaDb->username);
    mariaDb->password = stringDestroy(mariaDb->password);
    mariaDb->availableDbSockets = queueDestroy(mariaDb->availableDbSockets);
    mariaDb = (MariaDb*) pointerDestroy(mariaDb);
    LOG_MALLOC_FAILURE();
    printLog(TRACE,
      "EXIT mariaDbInit(remoteHostAddress=\"%s\", username=\"%s\", "
      "password=\"%s\", passwordHashType=%s) = {NULL}\n",
      remoteHostAddress, username, password,
      mariaDbPasswordHashTypeNames[passwordHashType]);
    return NULL;
  }
  sqlDatabase->bytesQuery =
    (DbResult* (*)(void *database, const Bytes query)) mariaDbExecQueryBytes;
  sqlDatabase->stringQuery = 
    (DbResult* (*)(void *database, const char *query)) mariaDbExecQueryString;
  sqlDatabase->makeBytesLiteral = mariaDbMakeBytesLiteral;
  sqlDatabase->makeStringLiteral = mariaDbMakeStringLiteral;
  sqlDatabase->connection = mariaDb;
  sqlDatabase->sqlDbType = MARIADB;
  sqlDatabase->describeTable = (DbResult* (*)(void *database, const char *dbName,
    const char *tableName)) mariaDbDescribeTable;
  sqlDatabase->compare = mariaDbCompare;
  sqlDatabase->tableDescriptions = htCreate(typeBytes);
  
  // External Database object.
  // The lock member mtx_t variable has to be zeroed, so use calloc.
  Database *database = (Database*) calloc(1, sizeof(Database));
  if (database == NULL) {
    sqlDatabase = mariaDbDisconnect(sqlDatabase);;
    LOG_MALLOC_FAILURE();
    printLog(TRACE,
      "EXIT mariaDbInit(remoteHostAddress=\"%s\", username=\"%s\", "
      "password=\"%s\", passwordHashType=%s) = {NULL}\n",
      remoteHostAddress, username, password,
      mariaDbPasswordHashTypeNames[passwordHashType]);
    return NULL;
  }
  database->getValuesVargs = (DbResult* (*)(void *database,
    const char *dbString, const char *tableName,
    const char *select, const char *orderBy, va_list args)) sqlGetValuesVargs;
  database->getValuesDict = (DbResult* (*)(void *database,
    const char *dbString, const char *tableName,
    const char *select, const char *orderBy,
    Dictionary *args)) sqlGetValuesDict;
  database->addRecordVargs = (bool (*)(void *database,
    const char *dbString, const char *tableName, va_list args))
    sqlAddRecordVargs;
  database->getDatabaseNames = (DbResult* (*)(void *database))
    mariaDbGetDatabaseNames;
  database->addTableVargs = (bool (*)(void *database,
    const char *dbName, const char *tableName, const char *primaryKey,
    va_list args)) sqlAddTableVargs;
  database->getTableNames = (DbResult* (*)(void *database, const char *dbName))
    mariaDbGetTableNames;
  database->deleteRecordsVargs = (bool (*)(void *database,
    const char *dbString, const char *tableName, va_list args))
    sqlDeleteRecordsVargs;
  database->updateRecordDict = (bool (*)(void *database,
    const char *dbString, const char *tableName, Dictionary *dict))
    sqlUpdateRecordDict;
  database->addRecordDict = (bool (*)(void *database,
    const char *dbString, const char *tableName,
    Dictionary *dict)) sqlAddRecordDict;
  database->getValuesLikeVargs = (DbResult* (*)(void *database,
    const char *dbName, const char *tableName,
    const char *select, const char *orderBy, va_list args))
    sqlGetValuesLikeVargs;
  database->addTableList = (bool (*)(void *database,
    const char *dbName, const char *tableName, const char *primaryKey,
    List *args)) sqlAddTableList;
  database->deleteTable = (bool (*)(void *database,
    const char *dbString, const char *tableName)) sqlDeleteTable;
  database->deleteRecordsLikeVargs = (bool (*)(void *database,
    const char *dbString, const char *tableName, va_list args))
    sqlDeleteRecordsLikeVargs;
  database->updateResultVargs
    = (bool (*)(const DbResult *dbResult, u64 resultIndex, va_list args))
    sqlUpdateResultVargs;
  database->lockTablesDict = (bool (*)(void *database,
    const Dictionary *tablesToLock)) mariaDbLockTablesDict;
  database->unlockTables
    = (bool (*)(void *database, const Dictionary *tableLock)) mariaDbUnlockTables;
  database->startTransaction = (bool (*)(void *database)) mariaDbStartTransaction;
  database->commitTransaction = (bool (*)(void *database)) mariaDbCommitTransaction;
  database->rollbackTransaction
    = (bool (*)(void *database)) mariaDbRollbackTransaction;
  database->addDatabase
    = (bool (*)(void *database, const char* dbName)) mariaDbAddDatabase;
  database->deleteDatabase
    = (bool (*)(void *database, const char* dbName)) mariaDbDeleteDatabase;
  database->deleteField
    = (bool (*)(void *database, const char *dbString,
    const char *tableName, const char *fieldName)) sqlDeleteField;
  database->addField
    = (bool (*)(void *database, const char *dbString,
    const char *tableName, const char *afterField, const char *newField,
    void *type)) sqlAddField;
  database->changeFieldType = mariaDbChangeFieldType;
  database->changeFieldName
    = (bool (*)(void *database, const char *dbString,
    const char *tableName, const char *oldName, const char *newName))
    sqlChangeFieldName;
  database->disconnect = (void* (*)(void*)) mariaDbDisconnect;
  database->describeTable = (DbResult* (*)(void *database, const char *dbName,
    const char *tableName)) sqlDescribeTable;
  database->addRecords = sqlAddRecords;
  database->renameTable = (bool (*)(void *database, const char *dbName,
    const char *oldTableName, const char *newTableName)) sqlRenameTable;
  database->compare = sqlCompare;
  database->getNumRecords = sqlGetNumRecords;
  database->getOrValuesDict = sqlGetOrValuesDict;
  database->updateFieldVargs = sqlUpdateFieldVargs;
  database->getSize = mariaDbGetSize;
  database->getFieldTypeByName = sqlGetFieldTypeByName;
  database->getFieldTypeByIndex = sqlGetFieldTypeByIndex;
  database->renameDatabase = mariaDbRenameDatabase;
  database->db = sqlDatabase;
  database->dbType = SQL;
  
  status = tss_create(&database->threadLockedTables,
    (tss_dtor_t) ((void*) dictionaryDestroy)); // Have to double-cast
  if (status != thrd_success) {
    printLog(ERR, "Could not initialize threadLockedTables.\n");
    database = dbDisconnect(database);
    printLog(TRACE,
      "EXIT mariaDbInit(remoteHostAddress=\"%s\", "
      "username=\"%s\", password=\"%s\", passwordHashType=%s) = {NULL}\n",
      remoteHostAddress, username, password,
      mariaDbPasswordHashTypeNames[passwordHashType]
    );
    return NULL;
  }
  
  status = mtx_init(&database->lockedTablesMutex, mtx_plain);
  if (status != thrd_success) {
    printLog(ERR, "Could not initialize lockedTablesMutex.\n");
    database = dbDisconnect(database);
    printLog(TRACE,
      "EXIT mariaDbInit(remoteHostAddress=\"%s\", "
      "username=\"%s\", password=\"%s\", passwordHashType=%s) = {NULL}\n",
      remoteHostAddress, username, password,
      mariaDbPasswordHashTypeNames[passwordHashType]
    );
    return NULL;
  }
  
  status = cnd_init(&database->lockedTablesCondition);
  if (status != thrd_success) {
    printLog(ERR, "Could not initialize lockedTablesCondition.\n");
    database = dbDisconnect(database);
    printLog(TRACE,
      "EXIT mariaDbInit(remoteHostAddress=\"%s\", "
      "username=\"%s\", password=\"%s\", passwordHashType=%s) = {NULL}\n",
      remoteHostAddress, username, password,
      mariaDbPasswordHashTypeNames[passwordHashType]
    );
    return NULL;
  }
  
  database->lockedTables = dictionaryCreate(typeString);
  if (database->lockedTables == NULL) {
    LOG_MALLOC_FAILURE();
    database = dbDisconnect(database);
    printLog(TRACE,
      "EXIT mariaDbInit(remoteHostAddress=\"%s\", "
      "username=\"%s\", password=\"%s\", passwordHashType=%s) = {NULL}\n",
      remoteHostAddress, username, password,
      mariaDbPasswordHashTypeNames[passwordHashType]
    );
    return NULL;
  }
  
  database->lockedRecords = rbTreeCreate(typeBytes);
  if (database->lockedRecords == NULL) {
    LOG_MALLOC_FAILURE();
    database = dbDisconnect(database);
    printLog(TRACE,
      "EXIT mariaDbInit(remoteHostAddress=\"%s\", "
      "username=\"%s\", password=\"%s\", passwordHashType=%s) = {NULL}\n",
      remoteHostAddress, username, password,
      mariaDbPasswordHashTypeNames[passwordHashType]
    );
    return NULL;
  }
  
  printLog(TRACE,
    "EXIT mariaDbInit(remoteHostAddress=\"%s\", "
    "username=\"%s\", password=\"%s\", passwordHashType=%s) = {%p}\n",
    remoteHostAddress, username, password,
    mariaDbPasswordHashTypeNames[passwordHashType], database
  );
  return database;
}

/// @fn Socket* getDbClientSocket(MariaDb *database)
///
/// @brief Get a dbClientSocket pointer to use on this thread for
/// communicating with the database.
///
/// @detail
/// This function will provide the caller with a dbClientSocket to use unless
/// no connection can be established.  Order of precedence for this function
/// is:
/// 1. Get the Socket associated with this thread's _threadClientSocket.
/// 2. If no socket is associated with this thread's _threadClientSocket,
///   retrieve a socket from _availableDbSocektQueue.
/// 3. If no socket is available from _availableDbSocektQueue, create a new
///   socket using the default parameters for mariaDbConnect.
///
/// @param database The MariaDb object with the database metadata.  This
///   function makes use of the availableDbSockets property of the object.
///
/// @return This function returns a pointer to an established dbClientSocket
/// on success, NULL on failure.
Socket* getDbClientSocket(MariaDb *database) {
  Socket *dbClientSocket = (Socket*) tss_get(_threadClientSocket);
  if (dbClientSocket == NULL) {
    printLog(DEBUG,
      "No dbClientSocket set on thread.  Popping queue of length %llu.\n",
      llu(database->availableDbSockets->size));
    if (database != NULL) {
      dbClientSocket = (Socket*) queuePop(database->availableDbSockets);
      thrd_t threadId = thrd_current();
      if (!htGetValue(_inUseMariaDbObjects, &threadId)) {
        htAddEntry(_inUseMariaDbObjects,
          &threadId, database, typePointerNoCopy);
      } else {
        htRemoveEntry(_inUseMariaDbObjects, &threadId);
        htAddEntry(_inUseMariaDbObjects,
          &threadId, database, typePointerNoCopy);
      }
    }
    
    if (dbClientSocket != NULL) {
      printLog(DEBUG, "Got dbClientSocket from queue.  Setting for thread.\n");
      tss_set(_threadClientSocket, dbClientSocket);
    } else if (database != NULL) {
      mtx_lock(&database->lock);
      if (database->numConnections < database->maxConnections) {
        // Create a new connection to the server.
        if (mariaDbConnect(database->remoteHostAddress,
          database->username, database->password, database->passwordHashType,
          &database->remoteHostAddress) == 0
        ) {
          printLog(DEBUG, "No dbClientSocket in queue.  New connection.\n");
          // _threadClientSocket is now set.
          dbClientSocket = (Socket*) tss_get(_threadClientSocket);
          database->numConnections++;
        } // else dbClientSocket == NULL and mariaDbConnect failed; return NULL
      } else {
        // We've reached the maximum number of connections that we can establish
        // to the server.  We'll have to wait for one to free up.
        struct timespec now;
        timespec_get(&now, TIME_UTC);
        now.tv_sec += 60;
        if (cnd_timedwait(&database->socketAvailable, &database->lock, &now)
          == thrd_success
        ) {
          // We've been signaled.  Grab a connection from the queue.
          dbClientSocket = (Socket*) queuePop(database->availableDbSockets);
          if (dbClientSocket != NULL) {
            printLog(DEBUG, "Got dbClientSocket from queue after signal.\n");
            tss_set(_threadClientSocket, dbClientSocket);
          } else if (database->numConnections < database->maxConnections) {
            // A socket has been destroyed.  We can create a new one.
            if (mariaDbConnect(database->remoteHostAddress,
              database->username, database->password,
              database->passwordHashType, &database->remoteHostAddress) == 0
            ) {
              printLog(DEBUG, "New connection after signal.\n");
              // _threadClientSocket is now set.
              dbClientSocket = (Socket*) tss_get(_threadClientSocket);
              database->numConnections++;
            } // else mariaDbConnect failed; return NULL
          } else {
            printLog(ERR,
              "database->socketAvailable was signaled but no socket was "
              "available!\n");
            dbClientSocket = NULL; // Should already be true, but don't risk it.
          }
        } else {
          printLog(ERR, "cnd_wait failed in getDbClientSocket.\n");
          if (database->numConnections < database->maxConnections) {
            // We can create a new socket.
            if (mariaDbConnect(database->remoteHostAddress,
              database->username, database->password,
              database->passwordHashType, &database->remoteHostAddress) == 0
            ) {
              printLog(DEBUG, "New connection after signal.\n");
              // _threadClientSocket is now set.
              dbClientSocket = (Socket*) tss_get(_threadClientSocket);
              database->numConnections++;
            } // else mariaDbConnect failed; return NULL
          } else {
            printLog(ERR,
              "The maximum number of connections has been reached.  "
              "Failing.\n");
            dbClientSocket = NULL; // Should already be true, but don't risk it.
          }
        }
      }
      mtx_unlock(&database->lock);
    }
  }
  
  return dbClientSocket;
}

/// @fn int mariaDbParseServerHandshake(unsigned char *response, int bytesReceived, unsigned char *packetNumber, unsigned char **scramble1stPart, unsigned char **scramble2ndPart, u64 *serverCapabilities, unsigned char *serverDefaultCollation, char **authenticationPluginName, u32 *scrambleLength, unsigned char *pluginDataLength);
///
/// @brief Parse the server response portion of a MySQL handshake exchange.
///
/// @param response An array of unsigned characters containing the response from
///   the server.
/// @param bytesReceived The number of bytes received from the server.  i.e. The
///   number of bytes in response.
/// @param packetNumber A pointer to an unsigned character that will contain the
///   packet number parsed from the response.
/// @param scramble1stPart A pointer to an unsigned character pointer that
///   will contain the first part of the scramble parsed from the response.
/// @param scramble2ndPart A pointer to an unsigned character pointer that
///   will contain the second part of the scramble parsed from the response.
/// @param serverCapabilities A pointer to a u64 that will hold the server
///   capabilities parsed from the response.
/// @param serverDefaultCollation A pointer to an unsigned character that will
///   hold the server's default collation parsed from the response.
/// @param authenticationPluginName A pointer to a character pointer that will
///   hold the name of the authentication plugin parsed from the response.
/// @param scrambleLength A pointer to a u32 that will hold the total length of
///   the scramble in bytes.
/// @param pluginDataLength A pointer to an unsigned character that will hold
///   the length of the plugin data parsed from the response.
int mariaDbParseServerHandshake(unsigned char *response, int bytesReceived,
  unsigned char *packetNumber, unsigned char **scramble1stPart,
  unsigned char **scramble2ndPart, u64 *serverCapabilities,
  unsigned char *serverDefaultCollation, char **authenticationPluginName,
  u32 *scrambleLength, unsigned char *pluginDataLength
) {
  char nullByte[] = { 0x00 };
  
  if (bytesReceived < 4) {
    printLog(ERR, "Invalid response received from server.\n");
    return -1;
  }
  
  u32 packetLength = (*((u32*) &response[0]));
  littleEndianToHost(&packetLength, sizeof(u32));
  packetLength &= 0x00ffffff;
  *packetNumber = response[3];
  
  char *packetString
    = binToHex(response, packetLength + DB_PACKET_HEADER_LENGTH, NULL);
  printLog(DEBUG, "Packet received:  %s\n", packetString);
  packetString = stringDestroy(packetString);
  
  if (bytesReceived < 5) {
    printLog(ERR, "Invalid response received from server.\n");
    return -1;
  }
  char protocolVersion = response[4];
  (void) protocolVersion; // This avoids errors when logging is not enabled.
  
  if (bytesReceived < 6) {
    printLog(ERR, "Invalid response received from server.\n");
    return -1;
  }
  char *serverVersion = NULL;
  straddstr(&serverVersion, (char*) &response[5]);
  
  int i = 0;
  // Skip over the server version we just got.
  for (i = 5; response[i] != '\0'; i++);
  i++;
  
  if (bytesReceived < i + 4) {
    printLog(ERR, "Invalid response received from server.\n");
    printBinary(DEBUG, response, packetLength + DB_PACKET_HEADER_LENGTH);
    return -1;
  }
  u32 connectionId = (u32) *(&response[i]);
  littleEndianToHost(&connectionId, sizeof(u32));
  (void) connectionId; // This avoids errors when logging is not enabled.
  i += sizeof(u32);
  
  if (bytesReceived < i + 8) {
    printLog(ERR, "Invalid response received from server.\n");
    return -1;
  }
  dataAddData((void**) scramble1stPart, 0, (void*) &response[i], 8);
  dataAddData((void**) scramble1stPart, 8, (void*) &nullByte[0], 1);
  i += 8;
  
  if (bytesReceived < i + 1) {
    printLog(ERR, "Invalid response received from server.\n");
    *scramble1stPart = (unsigned char*) pointerDestroy(*scramble1stPart);
    return -1;
  }
  char reservedByte1 = response[i];
  (void) reservedByte1; // This avoids errors when logging is not enabled.
  i++;
  
  if (bytesReceived < i + 2) {
    printLog(ERR, "Invalid response received from server.\n");
    *scramble1stPart = (unsigned char*) pointerDestroy(*scramble1stPart);
    return -1;
  }
  u16 serverCapabilities1stPart = *((u16*) &response[i]);
  littleEndianToHost(&serverCapabilities1stPart, sizeof(u16));
  i += sizeof(u16);
  
  if (bytesReceived < i + 1) {
    printLog(ERR, "Invalid response received from server.\n");
    *scramble1stPart = (unsigned char*) pointerDestroy(*scramble1stPart);
    return -1;
  }
  *serverDefaultCollation = *((char*) &response[i]);
  i++;
  
  if (bytesReceived < i + 2) {
    printLog(ERR, "Invalid response received from server.\n");
    *scramble1stPart = (unsigned char*) pointerDestroy(*scramble1stPart);
    return -1;
  }
  u16 statusFlags = *((u16*) &response[i]);
  littleEndianToHost(&statusFlags, sizeof(u16));
  (void) statusFlags; // This avoids errors when logging is not enabled.
  i += sizeof(u16);
  
  if (bytesReceived < i + 2) {
    printLog(ERR, "Invalid response received from server.\n");
    *scramble1stPart = (unsigned char*) pointerDestroy(*scramble1stPart);
    return -1;
  }
  u16 serverCapabilities2ndPart = *((u16*) &response[i]);
  littleEndianToHost(&serverCapabilities2ndPart, sizeof(u16));
  i += sizeof(u16);
  
  if (bytesReceived < i + 1) {
    printLog(ERR, "Invalid response received from server.\n");
    *scramble1stPart = (unsigned char*) pointerDestroy(*scramble1stPart);
    return -1;
  }
  *pluginDataLength = response[i];
  i++;
  
  if (bytesReceived < i + 6) {
    printLog(ERR, "Invalid response received from server.\n");
    *scramble1stPart = (unsigned char*) pointerDestroy(*scramble1stPart);
    return -1;
  }
  char *filler1 = NULL;
  dataAddData((void**) &filler1, 0, (void*) &response[i], 6);
  dataAddData((void**) &filler1, 6, (void*) &nullByte[0], 1);
  i += 6;
  
  if (bytesReceived < i + 4) {
    printLog(ERR, "Invalid response received from server.\n");
    *scramble1stPart = (unsigned char*) pointerDestroy(*scramble1stPart);
    return -1;
  }
  u32 serverCapabilities3rdPart = *((u32*) &response[i]);
  littleEndianToHost(&serverCapabilities3rdPart, sizeof(u16));
  i += sizeof(u32);
  
  *scrambleLength = 0;
  if (*pluginDataLength - 9 > 12) {
    *scrambleLength = *pluginDataLength - 9;
  } else {
    *scrambleLength = 12;
  }
  if (bytesReceived < i + ((int) *scrambleLength)) {
    printLog(ERR, "Invalid response received from server.\n");
    *scramble1stPart = (unsigned char*) pointerDestroy(*scramble1stPart);
    return -1;
  }
  dataAddData((void**) scramble2ndPart, 0, (void*) &response[i], *scrambleLength);
  dataAddData((void**) scramble2ndPart, *scrambleLength, (void*) &nullByte[0], 1);
  i += *scrambleLength;
  
  if (bytesReceived < i + 1) {
    printLog(ERR, "Invalid response received from server.\n");
    *scramble1stPart = (unsigned char*) pointerDestroy(*scramble1stPart);
    *scramble2ndPart = (unsigned char*) pointerDestroy(*scramble2ndPart);
    return -1;
  }
  char reservedByte2 = response[i];
  (void) reservedByte2; // This avoids errors when logging is not enabled.
  i++;
  
  straddstr(authenticationPluginName, (char*) &response[i]);
  i += strlen(*authenticationPluginName) + 1;
  
  printLog(DEBUG, "Packet Length: %d\n", packetLength);
  printLog(DEBUG, "Packet Number: %d\n", *packetNumber);
  printLog(DEBUG, "Protocol Version: %d\n", protocolVersion);
  printLog(DEBUG, "Server Version: \"%s\"\n", serverVersion);
  printLog(DEBUG, "Connection ID: %d\n", connectionId);
  printLog(DEBUG, "Scramble 1st Part: \"%s\"\n", *scramble1stPart);
  printLog(DEBUG, "Reserved Byte 1: %c\n", reservedByte1);
  printLog(DEBUG, "Server Capabilities 1st Part: %d\n",
    serverCapabilities1stPart);
  printLog(DEBUG, "Server Default Collation: %d\n", *serverDefaultCollation);
  printLog(DEBUG, "Status Flags: %d\n", statusFlags);
  printLog(DEBUG, "Server Capabilities 2nd Part: %d\n",
    serverCapabilities2ndPart);
  printLog(DEBUG, "Plugin Data Length: %u\n", *pluginDataLength);
  printLog(DEBUG, "Filler 1: \"%s\"\n", filler1);
  printLog(DEBUG, "Server Capabilities 3rd Part: %d\n",
    serverCapabilities3rdPart);
  printLog(DEBUG, "Scramble Length: %u\n", *scrambleLength);
  printLog(DEBUG, "Scramble 2nd Part: \"%s\"\n", *scramble2ndPart);
  printLog(DEBUG, "Reserved Byte 2: %c\n", reservedByte2);
  printLog(DEBUG, "Authentication Plugin Name: \"%s\"\n",
    *authenticationPluginName);
  printLog(DEBUG, "strlen(authenticationPluginName): %d\n",
    (int) strlen(*authenticationPluginName));
  printLog(DEBUG, "i: %d\n", i);
  
  filler1 = stringDestroy(filler1);
  serverVersion = stringDestroy(serverVersion);
  
  if (i != bytesReceived) {
    printLog(ERR, "Could not parse server handshake packet.\n");
    *scramble1stPart = (unsigned char*) pointerDestroy(*scramble1stPart);
    *scramble2ndPart = (unsigned char*) pointerDestroy(*scramble2ndPart);
    return -1;
  }
  
  // Construct server capabilities
  *serverCapabilities = (u64) serverCapabilities3rdPart;
  *serverCapabilities <<= 32;
  *serverCapabilities |= (u64) serverCapabilities2ndPart;
  *serverCapabilities <<= 16;
  *serverCapabilities |= (u64) serverCapabilities1stPart;
  
  return 0;
}

/// @fn int mariaDbConnect(const char *remoteHostAddress, const char *username, const char *password, MariaDbPasswordHashType passwordHashType, char **connectionAddress);
///
/// @brief Establish a connection between the QA system db client and the MariaDB
/// server.
///
/// @param remoteHostAddress is the network address (including optional port value)
///   of the MariaDB server.  If no address is provided, 127.0.0.1 will be used
///   as the address.  If no port is provided, 3306 will be used.
/// @param username is the username to use for the connection.  The username
///   "root" will be used if none is provided.
/// @param password is the password to use for the connection.  If none is
///   provided, none will be sent to the MariaDB server when connecting.
/// @param passwordHashType MariaDbPasswordHashType value specifying what kind
///   of value is encoded in the password field.
/// @param connectionAddress A pointer to a string that will hold the real
///   address used to connect to the server.
///
/// @return Returns 0 on success, a negative value on failure.<br />
///   -1 indicates a network connection could not be established<br />
///   -2 indicates that the login attempt failed<br />
int mariaDbConnect(const char *remoteHostAddress,
  const char *username, const char *password,
  MariaDbPasswordHashType passwordHashType,
  char **connectionAddress
) {
  call_once(&_mariaDbThreadSetup, setupMariaDbThreadMetadata);
  
  char nullByte[] = { 0x00 };
  u32 packetLength = 0;
  unsigned char response[2000];
  int port = 0;
  char *address = NULL;
  int returnValue = 0;
  
  printLog(TRACE,
    "ENTER mariaDbConnect(remoteHostAddress=\"%s\", username=\"%s\", "
    "password=\"%s\", passwordHashType=%s, connectionAddress=%p)\n",
    remoteHostAddress, username, password,
    mariaDbPasswordHashTypeNames[passwordHashType], connectionAddress
  );
  
  // Setup the parameters
  if (connectionAddress == NULL) {
    connectionAddress = &_defaultHostAddress;
  }
  
  // Get the port
  straddstr(&address, remoteHostAddress);
  if (strrchr(address, ':') != NULL) {
    char *portStart = strrchr(address, ':');
    *portStart = '\0';
    portStart++;
    port = strtol(portStart, NULL, 10);
  } else {
    // Default value.
    port = _defaultHostPort;
  }
  _defaultHostPort = port;
  
  // Get the address minus the port
  if (*address != '\0') {
    getIpAddress(&address);
  }
  if (*address == '\0') {
    // Default value.
    if (_defaultHostAddress != NULL) {
      if (strrchr(_defaultHostAddress, ':') != NULL) {
        *strrchr(_defaultHostAddress, ':') = '\0';
      }
      straddstr(&address, _defaultHostAddress);
    } else {
      straddstr(&address, "127.0.0.1");
    }
  }
  
  // Construct the default host address with the port
  *connectionAddress = stringDestroy(*connectionAddress);
  _defaultHostAddress = stringDestroy(_defaultHostAddress);
  if (asprintf(connectionAddress, "%s:%d", address, port) < 0) {
    *connectionAddress = NULL;
  }
  straddstr(&_defaultHostAddress, *connectionAddress);
  address = stringDestroy(address);
  
  // Create socket to connect to the remote host.
  Socket *dbClientSocket = socketCreate(CLIENT, TCP, *connectionAddress);
  if (dbClientSocket == NULL) {
    printLog(ERR, "Could not create socket\n");
    printLog(TRACE,
      "EXIT mariaDbConnect(remoteHostAddress=\"%s\", username=\"%s\", "
      "password=\"%s\", connectionAddress=\"%s\") = {%d}\n",
      remoteHostAddress, username, password, *connectionAddress,
      -1);
    return -1;
  }
  
  // Get the server's handshake initialization
  int bytesReceived = 0;
  if ((bytesReceived = socketReceive(dbClientSocket, response, sizeof(response))) < 3) {
    dbClientSocket = socketDestroy(dbClientSocket);
    printLog(ERR, "Could not get server's initial handshake packet.\n");
    printLog(TRACE,
      "EXIT mariaDbConnect(remoteHostAddress=\"%s\", username=\"%s\", "
      "password=\"%s\", connectionAddress=\"%s\") = {%d}\n",
      remoteHostAddress, username, password, *connectionAddress,
      -2);
    return -2;
  }
  
  // Parse the response.
  unsigned char packetNumber;
  unsigned char *scramble1stPart = NULL;
  unsigned char *scramble2ndPart = NULL;
  u64 serverCapabilities = 0;
  unsigned char serverDefaultCollation = 0;
  char *authenticationPluginName = NULL;
  u32 scrambleLength = 0;
  unsigned char pluginDataLength = 0;
  if (mariaDbParseServerHandshake(response, bytesReceived,
    &packetNumber, &scramble1stPart, &scramble2ndPart, &serverCapabilities,
    &serverDefaultCollation, &authenticationPluginName, &scrambleLength,
    &pluginDataLength) != 0
  ) {
    printLog(ERR, "Could not parse server handshake packet.\n");
    dbClientSocket = socketDestroy(dbClientSocket);
    printLog(ERR, "Could not get server's initial handshake packet.\n");
    printLog(TRACE,
      "EXIT mariaDbConnect(remoteHostAddress=\"%s\", username=\"%s\", "
      "password=\"%s\", connectionAddress=\"%s\") = {%d}\n",
      remoteHostAddress, username, password, *connectionAddress,
      -2);
    return -2;
  }
  
  u32 clientCapabilities
    = SECURE_CONNECTION | CLIENT_PROTOCOL_41 | PLUGIN_AUTH;
  u32 maxPacketSize = 0x00ffffff;
  char clientCharacterCollation = serverDefaultCollation;
  u32 extendedClientCapabilities = 0;
  
#ifdef TLS_SOCKETS_ENABLED
  if ((serverCapabilities & ((u64) CLIENT_SSL))
    && (tlsSocketsEnabled() == true)
  ) {
    // Switch to secure tunnel.  Send an SSL Request Packet.
    clientCapabilities |= CLIENT_SSL;
    unsigned char sslRequestPacket[36];
    packetLength = 32;
    packetNumber++;
    
    // MySQL header
    memcpy(&sslRequestPacket[0], &packetLength, sizeof(u32));
    hostToLittleEndian(&sslRequestPacket[0], sizeof(u32));
    sslRequestPacket[3] = packetNumber;
    
    memcpy(&sslRequestPacket[4], &clientCapabilities, sizeof(u32));
    hostToLittleEndian(&sslRequestPacket[4], sizeof(u32));
    
    memcpy(&sslRequestPacket[8], &maxPacketSize, sizeof(u32));
    hostToLittleEndian(&sslRequestPacket[8], sizeof(u32));
    
    sslRequestPacket[12] = clientCharacterCollation;
    
    // The next 19 bytes are reserved.
    memset(&sslRequestPacket[13], 0x00, 19);
    
    memcpy(&sslRequestPacket[32], &extendedClientCapabilities, sizeof(u32));
    hostToLittleEndian(&sslRequestPacket[8], sizeof(u32));
    
    printLog(DEBUG, "Switching to SSL tunnel.\n");
    if (socketSend(dbClientSocket, sslRequestPacket, sizeof(sslRequestPacket))
      != sizeof(sslRequestPacket)
    ) {
      // Communication error.
      printLog(ERR, "Could not send SSL Request Packet to server.\n");
      dbClientSocket = socketDestroy(dbClientSocket);
      printLog(ERR, "Could not configure the socket for TLS.\n");
      printLog(TRACE,
        "EXIT mariaDbConnect(remoteHostAddress=\"%s\", username=\"%s\", "
        "password=\"%s\", connectionAddress=\"%s\") = {%d}\n",
        remoteHostAddress, username, password, *connectionAddress,
        -2);
      return -2;
    }
    
    if (configureTlsClientSocket(dbClientSocket, 30000) != 0) {
      // We're toast.
      dbClientSocket = socketDestroy(dbClientSocket);
      printLog(ERR, "Could not configure the socket for TLS.\n");
      printLog(TRACE,
        "EXIT mariaDbConnect(remoteHostAddress=\"%s\", username=\"%s\", "
        "password=\"%s\", connectionAddress=\"%s\") = {%d}\n",
        remoteHostAddress, username, password, *connectionAddress,
        -2);
      return -2;
    }
  } else if (serverCapabilities & ((u64) CLIENT_SSL)) {
    printLog(DEBUG,
      "Local system does not support SSL.  Using plaintext connection.\n");
  } else {
    printLog(DEBUG,
      "Server does not support SSL.  Using plaintext connection.\n");
  }
#endif
  
  // Get the username
  if (username != NULL) {
    _defaultUsername = stringDestroy(_defaultUsername);
    straddstr(&_defaultUsername, username);
  }
  if ((_defaultUsername == NULL) || (*_defaultUsername == '\0')) {
    // Set the default
    straddstr(&_defaultUsername, DEFAULT_USERNAME);
  }
  
  // Get the password
  if (password != NULL) {
    _defaultPassword = stringDestroy(_defaultPassword);
    straddstr(&_defaultPassword, password);
  }
  if (password == NULL) {
    // Set the default
    straddstr(&_defaultPassword, DEFAULT_PASSWORD);
  }
  
  // Save the inforamtion about the password.
  _defaultPasswordHashType = passwordHashType;
  
  // Construct the password hash
  unsigned char *seed = NULL;
  dataAddData((void**) &seed, 0, (void*) scramble1stPart, 8);
  dataAddData((void**) &seed, 8, (void*) scramble2ndPart, scrambleLength);
  int seedLength = 8 + scrambleLength;
  scramble1stPart = (unsigned char*) pointerDestroy(scramble1stPart);
  scramble2ndPart = (unsigned char*) pointerDestroy(scramble2ndPart);
  
  // password hash = SHA1(passwd) ^ SHA1(seed + SHA1(SHA1(passwd)))
  unsigned char *passwordHash = NULL;
  if (passwordHashType == MARIADB_PASSWORD_PLAINTEXT) {
    // Normal case.
    passwordHash = generateMariaDbPasswordHash(
      (const unsigned char*) _defaultPassword, strlen(_defaultPassword),
      seed, seedLength);
  } else {
    unsigned char *sha = hexStringToSha1(_defaultPassword);
    passwordHash = generateMariaDbPasswordHashFromSha(
      sha, seed, seedLength);
    sha = (unsigned char*) pointerDestroy(sha);
  }
  seed = (unsigned char*) pointerDestroy(seed);
  
  // Construct the handshake response packet
  ZEROINIT(char reserved1[19]);
  if (!(serverCapabilities & CLIENT_MYSQL)) {
    printLog(DEBUG, "Use extended client capabilities.\n");
  } else {
    printLog(DEBUG, "Extended client capabilities reserved.\n");
  }
  if (serverCapabilities & PLUGIN_AUTH_LENENC_CLIENT_DATA) {
    printLog(DEBUG, "Use length-encoded authentication data.\n");
  } else if (serverCapabilities & SECURE_CONNECTION) {
    printLog(DEBUG, "Use fixed-length authentication data.\n");
  } else {
    printLog(DEBUG, "Use no authentication data???\n");
  }
  unsigned char authenticationDataLength = 0;
  if (strlen(_defaultPassword) > 0 ) {
    // The user has supplied a password, so we need to hash it.  If the user
    // does not supply a password, then this field will be zero length.
    authenticationDataLength = SHA_DIGEST_LENGTH;
  }
  if (serverCapabilities & CONNECT_WITH_DB) {
    printLog(DEBUG, "Supply default schema name.\n");
  }
  char defaultSchema = '\0';
  if (serverCapabilities & PLUGIN_AUTH) {
    printLog(DEBUG, "Supply authentication plugin name.\n");
  }
  // use authenticationPluginName
  if (serverCapabilities & CONNECT_ATTRS) {
    printLog(DEBUG, "Supply connection attributes.\n");
  }
  char connectionAttributesSize = 0;
  
  char *handshakeResponsePacket = NULL;
  int i = 0;
  hostToLittleEndian(&clientCapabilities, sizeof(u32));
  dataAddData((void**) &handshakeResponsePacket,
    i, (void*) &clientCapabilities, sizeof(clientCapabilities));
  i += sizeof(clientCapabilities); // 4
  hostToLittleEndian(&maxPacketSize, sizeof(u32));
  dataAddData((void**) &handshakeResponsePacket,
    i, (void*) &maxPacketSize, sizeof(maxPacketSize));
  i += sizeof(maxPacketSize); // 4
  dataAddData((void**) &handshakeResponsePacket,
    i, (void*) &clientCharacterCollation, sizeof(clientCharacterCollation));
  i += sizeof(clientCharacterCollation); // 1
  dataAddData((void**) &handshakeResponsePacket,
    i, (void*) reserved1, sizeof(reserved1));
  i += sizeof(reserved1); // 19
  hostToLittleEndian(&extendedClientCapabilities, sizeof(u32));
  dataAddData((void**) &handshakeResponsePacket,
    i, (void*) &extendedClientCapabilities, sizeof(extendedClientCapabilities));
  i += sizeof(extendedClientCapabilities); // 4
  dataAddData((void**) &handshakeResponsePacket,
    i, (void*) _defaultUsername, strlen(_defaultUsername) + 1);
  i += strlen(_defaultUsername) + 1;
  dataAddData((void**) &handshakeResponsePacket,
    i, (void*) &authenticationDataLength, sizeof(authenticationDataLength));
  i += sizeof(authenticationDataLength); // 1
  dataAddData((void**) &handshakeResponsePacket,
    i, (void*) passwordHash, authenticationDataLength);
  passwordHash = (unsigned char*) pointerDestroy(passwordHash);
  i += authenticationDataLength; // 20
  dataAddData((void**) &handshakeResponsePacket,
    i, (void*) &defaultSchema, sizeof(defaultSchema));
  i += sizeof(defaultSchema); // 1
  dataAddData((void**) &handshakeResponsePacket,
    i, (void*) authenticationPluginName, pluginDataLength);
  // NOTE:  This may not be correct.  This should reflect the length of single
  // plugin name we're using and pluginDataLength may include more than that.
  i += pluginDataLength; // 21
  dataAddData((void**) &handshakeResponsePacket, i, (void*) nullByte, 1);
  i += 1; // 1
  dataAddData((void**) &handshakeResponsePacket,
    i, (void*) &connectionAttributesSize, sizeof(connectionAttributesSize));
  i += sizeof(connectionAttributesSize); // 1
  
  packetNumber++;
  char *totalPacket = NULL;
  u32 littleEndianI = (u32) i;
  hostToLittleEndian(&littleEndianI, sizeof(u32));
  dataAddData((void**) &totalPacket, 0, (void*) &littleEndianI, 3);
  dataAddData((void**) &totalPacket, 3, (void*) &packetNumber, 1);
  dataAddData((void**) &totalPacket, 4, (void*) handshakeResponsePacket, i);
  handshakeResponsePacket = stringDestroy(handshakeResponsePacket);
  
  char *packetString
    = binToHex((const unsigned char*) totalPacket,
    i + DB_PACKET_HEADER_LENGTH, NULL);
  printLog(DEBUG, "Sending packet:  %s\n", packetString);
  packetString = stringDestroy(packetString);
  
  printLog(DEBUG, "Sending handshake response.\n");
  int bytesSent = 0;
  bytesSent
    = socketSend(dbClientSocket, totalPacket, i + DB_PACKET_HEADER_LENGTH);
  printLog(DEBUG, "Sent %d bytes.  Expected to send %d bytes.\n",
    bytesSent, i + DB_PACKET_HEADER_LENGTH);
  (void) bytesSent; // This avoids errors when logging is not enabled.
  totalPacket = stringDestroy(totalPacket);
  
  bytesReceived = socketReceive(dbClientSocket, response, 2000);
  printLog(DEBUG, "Received %d-byte response from server\n", bytesReceived);
  
  if (bytesReceived < 4) {
    printLog(ERR, "Invalid response from server.\n");
    dbClientSocket = socketDestroy(dbClientSocket);
    returnValue = -2;
    // remoteHostAddress may have been freed when we freed *connectionAddress,
    // so use its pointer here.
    printLog(TRACE,
      "EXIT mariaDbConnect(remoteHostAddress=%p, username=\"%s\", "
      "password=\"%s\", passwordHashType=%s, connectionAddress=\"%s\") = {%d}\n",
      remoteHostAddress, username, password,
      mariaDbPasswordHashTypeNames[passwordHashType], *connectionAddress,
      returnValue);
    return returnValue;
  }
  
  packetLength = (*((u32*) &response[0]));
  littleEndianToHost(&packetLength, sizeof(u32));
  packetLength &= 0x00ffffff;
  packetNumber = response[3];
  printLog(DEBUG, "Packet Length: %d\n", packetLength);
  printLog(DEBUG, "Packet Number: %d\n", packetNumber);
  for (int j = 4; j < bytesReceived; j++) {
    printLog(DEBUG, "response[%d] = 0x%x ('%c')\n", j, response[j], response[j]);
  }
  
  if (packetLength < 1) {
    printLog(ERR, "Invalid response from server.\n");
    dbClientSocket = socketDestroy(dbClientSocket);
    returnValue = -2;
    printLog(TRACE,
      "EXIT mariaDbConnect(remoteHostAddress=\"%s\", username=\"%s\", "
      "password=\"%s\", passwordHashType=%s, connectionAddress=\"%s\") = {%d}\n",
      remoteHostAddress, username, password,
      mariaDbPasswordHashTypeNames[passwordHashType], *connectionAddress,
      returnValue);
    return returnValue;
  }
  
  while (response[4] == 0xfe) {
    // Server is re-challenging authentication.  Go another round.
    authenticationPluginName = stringDestroy(authenticationPluginName);
    straddstr(&authenticationPluginName, (char*) &response[5]);
    int packetSize = 0;
    
    printLog(DEBUG, "Server issuing re-challenge.\n");
    if (strcmp(authenticationPluginName, "mysql_native_password") == 0) {
      // Construct the password hash
      int scrambleStart = 5 + strlen("mysql_native_password") + 1;
      dataAddData((void**) &seed,
        0, (void*) &response[scrambleStart], SHA_DIGEST_LENGTH);
      seedLength = SHA_DIGEST_LENGTH;
      
      // Re-using variables decleard above.
      
      // password hash = SHA1(passwd) ^ SHA1(seed + SHA1(SHA1(passwd)))
      if (passwordHashType == MARIADB_PASSWORD_PLAINTEXT) {
        // Normal case.
        passwordHash = generateMariaDbPasswordHash(
          (const unsigned char*) _defaultPassword, strlen(_defaultPassword),
          seed, seedLength);
      } else {
        unsigned char *sha = hexStringToSha1(_defaultPassword);
        passwordHash = generateMariaDbPasswordHashFromSha(
          sha, seed, seedLength);
        sha = (unsigned char*) pointerDestroy(sha);
      }
      seed = (unsigned char*) pointerDestroy(seed);
      
      packetNumber++;
      totalPacket = stringDestroy(totalPacket);
      packetSize = authenticationDataLength; // SHA_DIGEST_LENGTH or 0
      hostToLittleEndian(&packetSize, sizeof(u32));
      dataAddData((void**) &totalPacket, 0, (void*) &packetSize, 3);
      dataAddData((void**) &totalPacket, 3, (void*) &packetNumber, 1);
      dataAddData((void**) &totalPacket, 4, (void*) passwordHash, packetSize);
      passwordHash = (unsigned char*) pointerDestroy(passwordHash);
    } else {
      // We don't know how to respond to this challenge.  Fail.
      printLog(ERR, "Unknown plguin type \"%s\" in handshake challenge.\n",
        authenticationPluginName);
      break;
    }
    
    printLog(DEBUG, "Sending challenge response to server.\n");
    packetString = binToHex((const unsigned char*) totalPacket,
      packetSize + DB_PACKET_HEADER_LENGTH, NULL);
    printLog(DEBUG, "Sending packet:  %s\n", packetString);
    packetString = stringDestroy(packetString);
    
    bytesSent = socketSend(dbClientSocket,
      totalPacket, packetSize + DB_PACKET_HEADER_LENGTH);
    printLog(DEBUG, "Sent %d bytes.  Expected to send %d bytes.\n",
      bytesSent, packetSize + DB_PACKET_HEADER_LENGTH);
    totalPacket = stringDestroy(totalPacket);
    if (bytesSent <= 0) {
      printLog(DEBUG, "MariaDB server has closed the socket.  Failing.\n");
      break;
    }
    
    bytesReceived = socketReceive(dbClientSocket, response, 2000);
    printLog(DEBUG, "Received %d-byte response from server\n", bytesReceived);
    if (bytesReceived < 4) {
      printLog(ERR, "Invalid response from server.\n");
      dbClientSocket = socketDestroy(dbClientSocket);
      returnValue = -2;
      printLog(TRACE,
        "EXIT mariaDbConnect(remoteHostAddress=\"%s\", username=\"%s\", "
        "password=\"%s\", passwordHashType=%s, connectionAddress=\"%s\") = {%d}\n",
        remoteHostAddress, username, password,
        mariaDbPasswordHashTypeNames[passwordHashType], *connectionAddress,
        returnValue);
      return returnValue;
    }
    
    packetLength = (*((u32*) &response[0]));
    littleEndianToHost(&packetLength, sizeof(u32));
    packetLength &= 0x00ffffff;
    packetNumber = response[3];
    printLog(DEBUG, "Packet Length: %d\n", packetLength);
    printLog(DEBUG, "Packet Number: %d\n", packetNumber);
    for (int j = 4; j < bytesReceived; j++) {
      printLog(DEBUG, "response[%d] = 0x%x ('%c')\n", j, response[j], response[j]);
    }
    
    if (packetLength < 1) {
      printLog(ERR, "Invalid response from server.\n");
      dbClientSocket = socketDestroy(dbClientSocket);
      returnValue = -2;
      printLog(TRACE,
        "EXIT mariaDbConnect(remoteHostAddress=\"%s\", username=\"%s\", "
        "password=\"%s\", passwordHashType=%s, connectionAddress=\"%s\") = {%d}\n",
        remoteHostAddress, username, password,
        mariaDbPasswordHashTypeNames[passwordHashType], *connectionAddress,
        returnValue);
      return returnValue;
    }
  }
  
  authenticationPluginName = stringDestroy(authenticationPluginName);
  
  if (response[4] != 0x00) {
    if (response[4] == 0xff) {
      unsigned short int errorCode = 0;
      char *sqlState = NULL;
      char *errorMessage = mariaDbDecodeErrorPacket(&response[5],
        bytesReceived - 5, &errorCode, &sqlState);
      printLog(ERR, "Received Error Packet from DB Server.\n");
      printLog(ERR, "Error code: %d\n", errorCode);
      if (sqlState != NULL) {
        printLog(ERR, "SQL State: %c%c%c%c%c\n",
          sqlState[0], sqlState[1], sqlState[2], sqlState[3], sqlState[4]); 
        sqlState = stringDestroy(sqlState);
      }
      printLog(ERR, "Error message: \"%s\"\n", errorMessage);
      errorMessage = stringDestroy(errorMessage);
    }
    
    printLog(ERR, "Could not login to database server at \"%s\"\n",
      remoteHostAddress);
    dbClientSocket = socketDestroy(dbClientSocket);
    returnValue = -2;
  }
  
  totalPacket = stringDestroy(totalPacket);
  tss_set(_threadClientSocket,
    socketDestroy((Socket*) tss_get(_threadClientSocket)));
  tss_set(_threadClientSocket, dbClientSocket);
  
  printLog(TRACE,
    "EXIT mariaDbConnect(remoteHostAddress=\"%s\", username=\"%s\", "
    "password=\"%s\", passwordHashType=%s, connectionAddress=\"%s\") = {%d}\n",
    *connectionAddress, username, password,
    mariaDbPasswordHashTypeNames[passwordHashType], *connectionAddress,
    returnValue);
  return returnValue;
}

/// @fn char *mariaDbGetStatistics(MariaDb *database)
///
/// @brief Get the runtime statistics of the server
///
/// @details This function assumes a connection to the server has been
/// previously established.  This function takes no parameters.
///
/// @param database The MariaDb object with the database metadata.
///
/// @return Returns a string containing the server's statistics summary on
/// success, empty string on failure.
char *mariaDbGetStatistics(MariaDb *database) {
  call_once(&_mariaDbThreadSetup, setupMariaDbThreadMetadata);
  
  char *totalPacket = NULL;
  char *returnValue = NULL;
  char nullByte = '\0';
  char response[2000];
  
  u32 packetLength = 1;
  char commandCode = 0x09;
  char packetNumber = 0;
  
  printLog(TRACE, "ENTER mariaDbGetStatistics()\n");
  
  hostToLittleEndian(&packetLength, sizeof(u32));
  dataAddData((void**) &totalPacket, 0, (void*) &packetLength, 3);
  littleEndianToHost(&packetLength, sizeof(u32));
  dataAddData((void**) &totalPacket, 3, (void*) &packetNumber, 1);
  dataAddData((void**) &totalPacket, 4, (void*) &commandCode, packetLength);
  
  Socket *dbClientSocket = getDbClientSocket(database);
  socketSend(dbClientSocket, totalPacket,
    packetLength + DB_PACKET_HEADER_LENGTH);
  totalPacket = stringDestroy(totalPacket);
  
  int bytesReceived = socketReceive(dbClientSocket, response, 2000);
  if (bytesReceived >= DB_PACKET_HEADER_LENGTH) {
    packetLength = (*((u32*) &response[0]));
    littleEndianToHost(&packetLength, sizeof(u32));
    packetLength &= 0x00ffffff;
    packetNumber = response[3];
  } else {
    // Bad response
    packetLength = 0;
    packetNumber = -1;
  }
  
  dataAddData((void**) &returnValue, 0,
    (void*) &response[DB_PACKET_HEADER_LENGTH], packetLength);
  dataAddData((void**) &returnValue, packetLength, (void*) &nullByte, 1);
  
  printLog(TRACE, "EXIT mariaDbGetStatistics() = {%s}\n", returnValue);
  return returnValue;
}

#define MAX_PACKET_SIZE 0x00ffffff

#define FREE_COLUMN_METADATA() \
  do { \
    catalog = bytesDestroy(catalog); \
    schema = bytesDestroy(schema); \
    table = bytesDestroy(table); \
    orgTable = bytesDestroy(orgTable); \
    columnName = bytesDestroy(columnName); \
    orgColumnName = bytesDestroy(orgColumnName); \
    returnValue->fieldTypes \
      = (TypeDescriptor**) pointerDestroy(returnValue->fieldTypes); \
    for (u64 j = 0; j < columnIndex; j++) { \
      returnValue->rows[0][j] \
        = (void*) bytesDestroy((Bytes) returnValue->rows[0][j]); \
    } \
    returnValue->rows[0] = (void**) pointerDestroy(returnValue->rows[0]); \
    returnValue->rows = (void***) pointerDestroy(returnValue->rows); \
    response = (unsigned char*) pointerDestroy(response); \
  } while (0)

#define FREE_PARTIAL_RESULT() \
  do { \
    for (u64 j = 0; j < returnValue->numFields; j++) { \
      returnValue->rows[0][j] \
        = (void*) bytesDestroy((Bytes) returnValue->rows[0][j]); \
    } \
    returnValue->rows[0] = (void**) pointerDestroy(returnValue->rows[0]); \
    for (u64 i = 1; i < returnValue->numRows; i++) { \
      for (u64 j = 0; j < returnValue->numFields; j++) { \
        void* (*destructor)(volatile void*) \
          = returnValue->fieldTypes[j]->destroy; \
        if (returnValue->fieldTypes[j] == typeString) { \
          destructor = (void* (*)(volatile void*)) bytesDestroy; \
        } \
        returnValue->rows[i][j] \
          = destructor(returnValue->rows[i][j]); \
      } \
      returnValue->rows[i] = (void**) pointerDestroy(returnValue->rows[i]); \
    } \
    returnValue->rows = (void***) pointerDestroy(returnValue->rows); \
    returnValue->fieldTypes \
      = (TypeDescriptor**) pointerDestroy(returnValue->fieldTypes); \
    returnValue->numRows = 0; \
    returnValue->numFields = 0; \
    response = (unsigned char*) pointerDestroy(response); \
  } while (0)

/// @fn DbResult* _mariaDbExecQuery(MariaDb *database, const Bytes query)
///
/// @brief Send a SQL Bytes query to the server.
///
/// @param database The MariaDb object with the database metadata.
/// @param query is the SQL query Bytes object to send.
///
/// @return Returns a DbResult object containing the server's results on
/// success, empty DbResult on failure.
DbResult* _mariaDbExecQuery(MariaDb *database, const Bytes query) {
  printLog(TRACE, "ENTER _mariaDbExecQuery(database=%p, query=%p)\n",
    database, query);
  
  // Zeroize the entire result.  This also initializes successful to false.
  DbResult *returnValue = (DbResult*) calloc(1, sizeof(DbResult));
  if (returnValue == NULL) {
    LOG_MALLOC_FAILURE();
    exit(1);
  }
  
  if (query == NULL) {
    printLog(ERR, "query is NULL.\n");
    printLog(TRACE,
      "EXIT _mariaDbExecQuery(query=NULL) = {successful = false}\n");
    return returnValue; // succesful is false
  }
  
  Socket *dbClientSocket = getDbClientSocket(database);
  if (dbClientSocket == NULL) {
    printLog(ERR, "No connection to the database is available.\n");
    printLog(TRACE,
      "EXIT _mariaDbExecQuery(query=NULL) = {successful = false}\n");
    return returnValue; // succesful is false
  }
  
  printLog(DEBUG, "Starting query \"%s\".\n", str(query));
  u64 queryStartTime = getElapsedMicroseconds(0);
  // This avoids errors when logging is not enabled.
  (void) queryStartTime;
  
  unsigned char mariaDbBuffer[DB_PACKET_HEADER_LENGTH + DB_SMALL_PACKET_SIZE];
  char commandCode = 0x03;
  u64 packetLength = bytesLength(query) + sizeof(commandCode);
  char senderPacketNumber = 0;
  u32 bytesToSend = 0;
  if (packetLength >= (u64) 0x00ffffff) {
    bytesToSend = 0x00ffffff;
  } else {
    bytesToSend = (u32) packetLength;
  }
  u32 littleEndianPacketLength = bytesToSend;
  hostToLittleEndian(&littleEndianPacketLength, sizeof(u32));
  *((u32*) mariaDbBuffer) = littleEndianPacketLength;
  mariaDbBuffer[3] = senderPacketNumber;
  mariaDbBuffer[4] = commandCode;
  
  if (packetLength <= DB_SMALL_PACKET_SIZE) {
    // For whatever reason, MariaDB responds more than twice as fast when it's
    // sent the enteire packet at once vs. splitting into header and payload.
    // Since most queries are relatively small, optimize for that case here.
    memcpy(&mariaDbBuffer[5], (void*) query, bytesLength(query));
    int sendResult = socketSend(dbClientSocket,
      mariaDbBuffer, DB_PACKET_HEADER_LENGTH + bytesToSend);
    while ((sendResult <= 0) && (dbClientSocket != NULL)) {
      // Try a different connection to the database from the connection pool.
      tss_set(_threadClientSocket,
        socketDestroy((Socket*) tss_get(_threadClientSocket)));
      if (mariaDbConnect(database->remoteHostAddress,
        database->username, database->password, database->passwordHashType,
        &database->remoteHostAddress) == 0
      ) {
        printLog(ERR, "Could not reconnect to database.\n");
        dbClientSocket = NULL;
        mtx_lock(&database->lock);
        if (database->numConnections > 0) {
          database->numConnections--;
        }
        cnd_signal(&database->socketAvailable);
        mtx_unlock(&database->lock);
        break;
      }
      // _threadClientSocket is now set.
      dbClientSocket = (Socket*) tss_get(_threadClientSocket);
      sendResult = socketSend(dbClientSocket,
        mariaDbBuffer, DB_PACKET_HEADER_LENGTH + bytesToSend);
    }
    if (dbClientSocket == NULL) {
      printLog(ERR, "Could not connect to database.\n");
      printLog(DEBUG, "Query time: %llu microseconds\n",
        llu(getElapsedMicroseconds(queryStartTime)));
      printLog(TRACE,
        "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
        query);
      releaseDbClientSocket(database, dbClientSocket);
      return returnValue; // succesful is false
    }
  } else {
    // Send the header.
    int sendResult = socketSend(dbClientSocket,
      mariaDbBuffer, DB_PACKET_HEADER_LENGTH + 1);
    while ((sendResult <= 0) && (dbClientSocket != NULL)) {
      // Try a different connection to the database from the connection pool.
      tss_set(_threadClientSocket,
        socketDestroy((Socket*) tss_get(_threadClientSocket)));
      if (mariaDbConnect(database->remoteHostAddress,
        database->username, database->password, database->passwordHashType,
        &database->remoteHostAddress) == 0
      ) {
        printLog(ERR, "Could not reconnect to database.\n");
        dbClientSocket = NULL;
        mtx_lock(&database->lock);
        if (database->numConnections > 0) {
          database->numConnections--;
        }
        cnd_signal(&database->socketAvailable);
        mtx_unlock(&database->lock);
        break;
      }
      // _threadClientSocket is now set.
      dbClientSocket = (Socket*) tss_get(_threadClientSocket);
      sendResult = socketSend(dbClientSocket,
        mariaDbBuffer, DB_PACKET_HEADER_LENGTH + 1);
    }
    if (dbClientSocket == NULL) {
      printLog(ERR, "Could not connect to database.\n");
      printLog(DEBUG, "Query time: %llu microseconds\n",
        llu(getElapsedMicroseconds(queryStartTime)));
      printLog(TRACE,
        "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
        query);
      releaseDbClientSocket(database, dbClientSocket);
      return returnValue; // succesful is false
    }
    // We sent the command byte, which is part of the packet payload.
    // Adjust accordingly.
    bytesToSend -= 1;
    packetLength -= 1;
    
    // Send the query.
    unsigned char *sendPointer = ustr(query);
    while (packetLength > 0) {
      sendResult = socketSend(dbClientSocket, sendPointer, bytesToSend);
      if (sendResult <= 0) {
        printLog(ERR, "MariaDB server prematurely closed the connection.\n");
        printLog(DEBUG, "Query time: %llu microseconds\n",
          llu(getElapsedMicroseconds(queryStartTime)));
        printLog(TRACE,
          "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
          query);
        releaseDbClientSocket(database, dbClientSocket);
        return returnValue; // succesful is false
      }
      // socketSend will send everything unless there's an error, so no need to
      // check for partial sends or have a while loop that decrements bytesToSend.
      
      if ((bytesToSend == 0x00ffffff)
        || ((bytesToSend == 0x00fffffe) && (senderPacketNumber == 0))
      ) {
        // We need to send another packet.
        if ((packetLength == (u64) 0x00ffffff)
          || ((packetLength == (u64) 0x00fffffe) && (senderPacketNumber == 0))
        ) {
          // We need to send an empty packet.
          senderPacketNumber++;
          littleEndianPacketLength = 0;
          *((u32*) mariaDbBuffer) = littleEndianPacketLength;
          mariaDbBuffer[3] = senderPacketNumber;
        } else {
          if (packetLength >= (u64) 0x00ffffff) {
            // We need to send a 16 MB packet.
            bytesToSend = 0x00ffffff;
          } else {
            // We need to send the final partial packet.
            bytesToSend = (u32) packetLength;
          }
          
          senderPacketNumber++;
          littleEndianPacketLength = bytesToSend;
          hostToLittleEndian(&littleEndianPacketLength, sizeof(u32));
          *((u32*) mariaDbBuffer) = littleEndianPacketLength;
          mariaDbBuffer[3] = senderPacketNumber;
        }
        
        // Send the header.
        // Note:  If we're sending an empty packet to terminate the stream then
        // this is all we need to send.  The loop will exit when packetLength
        // reaches zero below.
        sendResult = socketSend(dbClientSocket,
          mariaDbBuffer, DB_PACKET_HEADER_LENGTH);
        if (sendResult <= 0) {
          printLog(ERR, "MariaDB server prematurely closed the connection.\n");
          printLog(DEBUG, "Query time: %llu microseconds\n",
            llu(getElapsedMicroseconds(queryStartTime)));
          printLog(TRACE,
            "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
            query);
          releaseDbClientSocket(database, dbClientSocket);
          return returnValue; // succesful is false
        }
      }
      
      packetLength -= sendResult;
      sendPointer += sendResult;
    }
  }
  printLog(DEBUG, "Send time: %llu microseconds\n",
    llu(getElapsedMicroseconds(queryStartTime)));
  
  printLog(DEBUG, "Sent query successfully.  Getting response.\n");
  u32 bytesReceived = 0;
  unsigned char *response = NULL;
  while ((bytesReceived == 0)
    && (tss_get(_threadClientSocket) != NULL)
  ) {
    printLog(TRACE, "bytesReceived == 0\n");
    if (mariaDbGetDataFromServer(&response, &bytesReceived) < 0) {
      break;
    }
  }
  printLog(DEBUG, "Response time: %llu microseconds\n",
    llu(getElapsedMicroseconds(queryStartTime)));
  printLog(DEBUG, "Received %u bytes.\n", bytesReceived);
  
  char packetNumber = 0;
  if (bytesReceived >= DB_PACKET_HEADER_LENGTH) {
    littleEndianPacketLength = (*((u32*) &response[0]));
    littleEndianToHost(&littleEndianPacketLength, sizeof(u32));
    littleEndianPacketLength &= 0x00ffffff;
    packetLength = (u64) littleEndianPacketLength;
    packetNumber = response[3];
  } else {
    // Bad response
    packetLength = 0;
    packetNumber = -1;
  }
  
  if (packetNumber != senderPacketNumber + 1) {
    // We got garbage from the server.  This may be due to the server returning
    // partial results for an earlier command that timed out and failed on our
    // side.  Close the socket so that we don't continue to get garbage on the
    // next call.
    printLog(DEBUG, "Query time: %llu microseconds\n",
      llu(getElapsedMicroseconds(queryStartTime)));
    printLog(ERR, "Out-of-sequence packet received from server.\n");
    printLog(DEBUG, "Expected 1 but got %d\n", packetNumber);
    response = (unsigned char*) pointerDestroy(response);
    tss_set(_threadClientSocket,
      socketDestroy((Socket*) tss_get(_threadClientSocket)));
    mtx_lock(&database->lock);
    if (database->numConnections > 0) {
      database->numConnections--;
    }
    cnd_signal(&database->socketAvailable);
    mtx_unlock(&database->lock);
    dbClientSocket = NULL;
    printLog(TRACE,
      "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
      query);
    releaseDbClientSocket(database, dbClientSocket);
    return returnValue; // succesful is false
  }
  
  if (packetLength == 0) {
    printLog(DEBUG, "Query time: %llu microseconds\n",
      llu(getElapsedMicroseconds(queryStartTime)));
    printLog(DEBUG, "No data returned from server for query.\n");
    response = (unsigned char*) pointerDestroy(response);
    printLog(TRACE,
      "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
      query);
    releaseDbClientSocket(database, dbClientSocket);
    return returnValue; // succesful is false
  }
  printLog(DEBUG, "Response was good.  Continuing.\n");
  
  u64 responseIndex = DB_PACKET_HEADER_LENGTH;
  u64 columnCount = 0;
  if (response[responseIndex] == 0xff) {
    printLog(DEBUG, "Query time: %llu microseconds\n",
      llu(getElapsedMicroseconds(queryStartTime)));
    printLog(DEBUG, "ERR_Packet received.\n");
    // Error packet.  Decode
    u16 errorCode = 0;
    char *sqlState = NULL;
    char *errorMessage = mariaDbDecodeErrorPacket(&response[responseIndex + 1],
      bytesReceived - (responseIndex + 1), &errorCode, &sqlState);
    
    printLog(ERR, "Received Error Packet from DB Server.\n");
    printLog(ERR, "Error code: %d\n", errorCode);
    if (sqlState != NULL) {
      printLog(ERR, "SQL State: %c%c%c%c%c\n",
        sqlState[0], sqlState[1], sqlState[2], sqlState[3], sqlState[4]); 
    }
    printLog(ERR, "Error message: \"%s\"\n", errorMessage);
    errorMessage = stringDestroy(errorMessage);
    sqlState = stringDestroy(sqlState);
    response = (unsigned char*) pointerDestroy(response);
    
    printLog(TRACE,
      "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
      query);
    releaseDbClientSocket(database, dbClientSocket);
    return returnValue; // succesful is false
  } else if (response[responseIndex] == 0x00) {
    printLog(DEBUG, "OK_Packet received.\n");
    printLog(DEBUG, "Query time: %llu microseconds\n",
      llu(getElapsedMicroseconds(queryStartTime)));
    response = (unsigned char*) pointerDestroy(response);
    returnValue->successful = true;
    printLog(TRACE,
      "EXIT _mariaDbExecQuery(query=%p) = {successful = true}\n",
      query);
    releaseDbClientSocket(database, dbClientSocket);
    return returnValue;
  } else if (response[responseIndex] == 0xfb) {
    printLog(DEBUG, "LOCAL_INFILE_Packet received.\n");
    printLog(DEBUG, "Query time: %llu microseconds\n",
      llu(getElapsedMicroseconds(queryStartTime)));
    response = (unsigned char*) pointerDestroy(response);
    returnValue->successful = true;
    printLog(TRACE,
      "EXIT _mariaDbExecQuery(query=%p) = {successful = true}\n",
      query);
    releaseDbClientSocket(database, dbClientSocket);
    return returnValue;
  } else {
    printLog(DEBUG,
      "Resultset received.  %lld Bytes in length (%d bytes received).\n",
      lli(packetLength), bytesReceived);
    columnCount
      = mariaDbGetLengthEncodedInteger(&response, &responseIndex, &bytesReceived);
    if (tss_get(_threadClientSocket) == NULL) {
      // Error in lower level function.  Bail.
      mtx_lock(&database->lock);
      if (database->numConnections > 0) {
        database->numConnections--;
      }
      cnd_signal(&database->socketAvailable);
      mtx_unlock(&database->lock);
      response = (unsigned char*) pointerDestroy(response);
      printLog(TRACE,
        "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
        query);
      return returnValue; // succesful is false
    }
    printLog(DEBUG, "Column count: %llu\n", llu(columnCount));
  }
  
  returnValue->fieldTypes
    = (TypeDescriptor**) calloc(1, sizeof(TypeDescriptor*) * columnCount);
  returnValue->rows = (void***) malloc(sizeof(Bytes*));
  if (returnValue->rows == NULL) {
    // Memory allocation failed.  Failure.
    // Cannot use printLog here.
    LOG_MALLOC_FAILURE();
    response = (unsigned char*) pointerDestroy(response);
    releaseDbClientSocket(database, dbClientSocket);
    return returnValue; // succesful is false
  }
  returnValue->rows[0] = (void**) malloc((columnCount + 1) * sizeof(Bytes));
  if (returnValue->rows[0] == NULL) {
    // Memory allocation failed.  Failure.
    // Cannot use printLog here.
    LOG_MALLOC_FAILURE();
    returnValue->rows = (void***) pointerDestroy(returnValue->rows);
    response = (unsigned char*) pointerDestroy(response);
    releaseDbClientSocket(database, dbClientSocket);
    return returnValue; // succesful is false
  }
  returnValue->rows[0][columnCount] = NULL;
  
  for (u64 columnIndex = 0; columnIndex < columnCount; columnIndex++) {
    littleEndianPacketLength = (*((u32*) &response[responseIndex]));
    littleEndianToHost(&littleEndianPacketLength, sizeof(u32));
    littleEndianPacketLength &= 0x00ffffff;
    packetLength = (u64) littleEndianPacketLength;
    responseIndex += 3;
    packetNumber = response[responseIndex];
    responseIndex++;
    u64 packetStart = responseIndex;
    
    Bytes catalog = NULL;
    Bytes schema = NULL;
    Bytes table = NULL;
    Bytes orgTable = NULL;
    Bytes columnName = NULL;
    Bytes orgColumnName = NULL;
    
    catalog = mariaDbGetLengthEncodedString(
      &response, &responseIndex, &bytesReceived);
    if (catalog == NULL) {
      // Memory allocation failed.  Failure.
      // Cannot use printLog here.
      FREE_COLUMN_METADATA();
      LOG_MALLOC_FAILURE();
      releaseDbClientSocket(database, dbClientSocket);
      return returnValue; // succesful is false
    }
    
    schema = mariaDbGetLengthEncodedString(
      &response, &responseIndex, &bytesReceived);
    //// if (schema == NULL) {
    ////   // Memory allocation failed.  Failure.
    ////   // Cannot use printLog here.
    ////   FREE_COLUMN_METADATA();
    ////   LOG_MALLOC_FAILURE();
    ////   releaseDbClientSocket(database, dbClientSocket);
    ////   return returnValue; // succesful is false
    //// }
    
    table = mariaDbGetLengthEncodedString(
      &response, &responseIndex, &bytesReceived);
    //// if (table == NULL) {
    ////   // Memory allocation failed.  Failure.
    ////   // Cannot use printLog here.
    ////   FREE_COLUMN_METADATA();
    ////   LOG_MALLOC_FAILURE();
    ////   releaseDbClientSocket(database, dbClientSocket);
    ////   return returnValue; // succesful is false
    //// }
    
    orgTable = mariaDbGetLengthEncodedString(
      &response, &responseIndex, &bytesReceived);
    //// if (orgTable == NULL) {
    ////   // Memory allocation failed.  Failure.
    ////   // Cannot use printLog here.
    ////   FREE_COLUMN_METADATA();
    ////   LOG_MALLOC_FAILURE();
    ////   releaseDbClientSocket(database, dbClientSocket);
    ////   return returnValue; // succesful is false
    //// }
    
    columnName = mariaDbGetLengthEncodedString(
      &response, &responseIndex, &bytesReceived);
    //// if (columnName == NULL) {
    ////   // Memory allocation failed.  Failure.
    ////   // Cannot use printLog here.
    ////   FREE_COLUMN_METADATA();
    ////   LOG_MALLOC_FAILURE();
    ////   releaseDbClientSocket(database, dbClientSocket);
    ////   return returnValue; // succesful is false
    //// }
    
    orgColumnName = mariaDbGetLengthEncodedString(
      &response, &responseIndex, &bytesReceived);
    //// if (orgColumnName == NULL) {
    ////   // Memory allocation failed.  Failure.
    ////   // Cannot use printLog here.
    ////   FREE_COLUMN_METADATA();
    ////   LOG_MALLOC_FAILURE();
    ////   releaseDbClientSocket(database, dbClientSocket);
    ////   return returnValue; // succesful is false
    //// }
    
    u64 lengthOfFixedLengthFields = mariaDbGetLengthEncodedInteger(
      &response, &responseIndex, &bytesReceived);
    if (lengthOfFixedLengthFields != 12) {
      printLog(DEBUG, "Query time: %llu microseconds\n",
        llu(getElapsedMicroseconds(queryStartTime)));
      catalog = bytesDestroy(catalog);
      schema = bytesDestroy(schema);
      table = bytesDestroy(table);
      orgTable = bytesDestroy(orgTable);
      columnName = bytesDestroy(columnName);
      orgColumnName = bytesDestroy(orgColumnName);
      
      printLog(ERR,
        "Error decoding packet.  Length of fixed-length fields is %llu "
        "(expected 12).\n", llu(lengthOfFixedLengthFields));
      for (u32 j = 0; j < bytesReceived; j++) {
        printLog(DEBUG, "response[%d] = 0x%02x ('%c')\n",
          j, response[j], response[j]);
      }
      for (u64 j = 0; j < columnIndex; j++) {
        returnValue->rows[0][j]
          = (void*) bytesDestroy((Bytes) returnValue->rows[0][j]);
      }
      returnValue->rows[0] = (void**) pointerDestroy(returnValue->rows[0]);
      returnValue->rows = (void***) pointerDestroy(returnValue->rows);
      response = (unsigned char*) pointerDestroy(response);
      // TODO:  Free all other metadata.
      printLog(TRACE,
        "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
        query);
      releaseDbClientSocket(database, dbClientSocket);
      return returnValue; // succesful is false
    }
    
    while (((responseIndex + sizeof(u16)) >= bytesReceived)
      && (tss_get(_threadClientSocket) != NULL)
      && (mariaDbGetDataFromServer(&response, &bytesReceived) >= 0));
    if ((responseIndex + sizeof(u16)) >= bytesReceived) {
      FREE_COLUMN_METADATA();
      printLog(ERR, "Insufficient data from MariaDB server.\n");
      printLog(TRACE,
        "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
        query);
      releaseDbClientSocket(database, dbClientSocket);
      return returnValue; // succesful is false
    }
    u16 characterSet = *((u16*) &response[responseIndex]);
    littleEndianToHost(&characterSet, sizeof(u16));
    responseIndex += sizeof(characterSet); // 2
    
    while (((responseIndex + sizeof(u32)) >= bytesReceived)
      && (tss_get(_threadClientSocket) != NULL)
      && (mariaDbGetDataFromServer(&response, &bytesReceived) >= 0));
    if ((responseIndex + sizeof(u32)) >= bytesReceived) {
      FREE_COLUMN_METADATA();
      printLog(ERR, "Insufficient data from MariaDB server.\n");
      printLog(TRACE,
        "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
        query);
      releaseDbClientSocket(database, dbClientSocket);
      return returnValue; // succesful is false
    }
    u32 columnLength = *((u32*) &response[responseIndex]);
    littleEndianToHost(&columnLength, sizeof(u32));
    responseIndex += sizeof(columnLength); // 4
    
    while (((responseIndex + sizeof(u8)) >= bytesReceived)
      && (tss_get(_threadClientSocket) != NULL)
      && (mariaDbGetDataFromServer(&response, &bytesReceived) >= 0));
    if ((responseIndex + sizeof(u8)) >= bytesReceived) {
      FREE_COLUMN_METADATA();
      printLog(ERR, "Insufficient data from MariaDB server.\n");
      printLog(TRACE,
        "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
        query);
      releaseDbClientSocket(database, dbClientSocket);
      return returnValue; // succesful is false
    }
    u8 columnType = response[responseIndex];
    responseIndex += sizeof(columnType); // 1
    returnValue->fieldTypes[columnIndex]
      = mysqlFieldTypeTypeDescriptors[
        getMysqlFieldTypeIndex((MysqlFieldType) columnType)
      ];
    printLog(DEBUG, "columnType = %s\n", 
      getMysqlFieldTypeName((MysqlFieldType) columnType));
    
    while (((responseIndex + sizeof(u16)) >= bytesReceived)
      && (tss_get(_threadClientSocket) != NULL)
      && (mariaDbGetDataFromServer(&response, &bytesReceived) >= 0));
    if ((responseIndex + sizeof(u16)) >= bytesReceived) {
      FREE_COLUMN_METADATA();
      printLog(ERR, "Insufficient data from MariaDB server.\n");
      printLog(TRACE,
        "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
        query);
      releaseDbClientSocket(database, dbClientSocket);
      return returnValue; // succesful is false
    }
    u16 flags = *((u16*) &response[responseIndex]);
    littleEndianToHost(&flags, sizeof(u16));
    responseIndex += sizeof(flags); // 2
    
    while (((responseIndex + sizeof(u8)) >= bytesReceived)
      && (tss_get(_threadClientSocket) != NULL)
      && (mariaDbGetDataFromServer(&response, &bytesReceived) >= 0));
    if ((responseIndex + sizeof(u8)) >= bytesReceived) {
      FREE_COLUMN_METADATA();
      printLog(ERR, "Insufficient data from MariaDB server.\n");
      printLog(TRACE,
        "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
        query);
      releaseDbClientSocket(database, dbClientSocket);
      return returnValue; // succesful is false
    }
    u8 decimals = response[responseIndex];
    responseIndex += sizeof(decimals); // 1
    
    while (((responseIndex + sizeof(u16)) >= bytesReceived)
      && (tss_get(_threadClientSocket) != NULL)
      && (mariaDbGetDataFromServer(&response, &bytesReceived) >= 0));
    if ((responseIndex + sizeof(u16)) >= bytesReceived) {
      FREE_COLUMN_METADATA();
      printLog(ERR, "Insufficient data from MariaDB server.\n");
      printLog(TRACE,
        "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
        query);
      releaseDbClientSocket(database, dbClientSocket);
      return returnValue; // succesful is false
    }
    u16 filler = *((u16*) &response[responseIndex]);
    littleEndianToHost(&filler, sizeof(u16));
    responseIndex += sizeof(filler); // 2
    
    if (responseIndex != packetStart + packetLength) {
      printLog(DEBUG, "Query time: %llu seconds\n",
        llu(getElapsedMicroseconds(queryStartTime)));
      catalog = bytesDestroy(catalog);
      schema = bytesDestroy(schema);
      table = bytesDestroy(table);
      orgTable = bytesDestroy(orgTable);
      columnName = bytesDestroy(columnName);
      orgColumnName = bytesDestroy(orgColumnName);
      
      printLog(ERR,
        "Error decoding packet.  Found extra Bytes when decoding column "
        "definition.\n");
      for (u32 j = 0; j < bytesReceived; j++) {
        printLog(DEBUG, "response[%d] = 0x%02x ('%c')\n",
          j, response[j], response[j]);
      }
      for (u64 j = 0; j < columnIndex; j++) {
        returnValue->rows[0][j]
          = (void*) bytesDestroy((Bytes) returnValue->rows[0][j]);
      }
      returnValue->rows[0] = (void**) pointerDestroy(returnValue->rows[0]);
      returnValue->rows = (void***) pointerDestroy(returnValue->rows);
      response = (unsigned char*) pointerDestroy(response);
      // TODO:  Free all other metadata.
      printLog(TRACE,
        "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
        query);
      releaseDbClientSocket(database, dbClientSocket);
      return returnValue; // succesful is false
    }
    
    printLog(FLOOD, "Name of column %llu is \"%s\".\n",
      llu(columnIndex), columnName);
    returnValue->rows[0][columnIndex] = (void*) columnName;
    catalog = bytesDestroy(catalog);
    schema = bytesDestroy(schema);
    table = bytesDestroy(table);
    orgTable = bytesDestroy(orgTable);
    orgColumnName = bytesDestroy(orgColumnName);
  }
  returnValue->numRows = 1;
  returnValue->numFields = columnCount;
  
  // Next packet should be EOF packet
  while (((responseIndex + DB_PACKET_HEADER_LENGTH) >= bytesReceived)
    && (tss_get(_threadClientSocket) != NULL)
    && (mariaDbGetDataFromServer(&response, &bytesReceived) >= 0));
  if ((responseIndex + DB_PACKET_HEADER_LENGTH) >= bytesReceived) {
    FREE_PARTIAL_RESULT();
    printLog(ERR, "Insufficient data from MariaDB server.\n");
    printLog(TRACE,
      "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
      query);
    releaseDbClientSocket(database, dbClientSocket);
    return returnValue; // succesful is false
  }
  littleEndianPacketLength = (*((u32*) &response[responseIndex]));
  littleEndianToHost(&littleEndianPacketLength, sizeof(u32));
  littleEndianPacketLength &= 0x00ffffff;
  packetLength = (u64) littleEndianPacketLength;
  responseIndex += 3;
  packetNumber = response[responseIndex];
  responseIndex++;
  
  while (((responseIndex + sizeof(u8)) >= bytesReceived)
    && (tss_get(_threadClientSocket) != NULL)
    && (mariaDbGetDataFromServer(&response, &bytesReceived) >= 0));
  if ((responseIndex + sizeof(u8)) >= bytesReceived) {
    FREE_PARTIAL_RESULT();
    printLog(ERR, "Insufficient data from MariaDB server.\n");
    printLog(TRACE,
      "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
      query);
    releaseDbClientSocket(database, dbClientSocket);
    return returnValue; // succesful is false
  }
  u8 headerType = response[responseIndex];
  responseIndex++;
  
  while (((responseIndex + sizeof(u16)) >= bytesReceived)
    && (tss_get(_threadClientSocket) != NULL)
    && (mariaDbGetDataFromServer(&response, &bytesReceived) >= 0));
  if ((responseIndex + sizeof(u16)) >= bytesReceived) {
    FREE_PARTIAL_RESULT();
    printLog(ERR, "Insufficient data from MariaDB server.\n");
    printLog(TRACE,
      "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
      query);
    releaseDbClientSocket(database, dbClientSocket);
    return returnValue; // succesful is false
  }
  u16 warnings = *((u16*) &response[responseIndex]);
  littleEndianToHost(&warnings, sizeof(u16));
  responseIndex += sizeof (warnings); // 2
  
  while (((responseIndex + sizeof(u16)) >= bytesReceived)
    && (tss_get(_threadClientSocket) != NULL)
    && (mariaDbGetDataFromServer(&response, &bytesReceived) >= 0));
  if ((responseIndex + sizeof(u16)) >= bytesReceived) {
    FREE_PARTIAL_RESULT();
    printLog(ERR, "Insufficient data from MariaDB server.\n");
    printLog(TRACE,
      "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
      query);
    releaseDbClientSocket(database, dbClientSocket);
    return returnValue; // succesful is false
  }
  u16 statusFlags = *((u16*) &response[responseIndex]);
  littleEndianToHost(&statusFlags, sizeof(u16));
  responseIndex += sizeof (statusFlags); // 2
  
  if (headerType != 0xfe) {
    printLog(DEBUG, "Query time: %llu microseconds\n",
      llu(getElapsedMicroseconds(queryStartTime)));
    FREE_PARTIAL_RESULT();
    printLog(ERR,
      "Error decoding packet.  EOF not found after column definitions.\n");
    printLog(TRACE,
      "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
      query);
    releaseDbClientSocket(database, dbClientSocket);
    return returnValue; // succesful is false
  }
  
  // Next packets should be the rows
  while (((responseIndex + DB_PACKET_HEADER_LENGTH) >= bytesReceived)
    && (tss_get(_threadClientSocket) != NULL)
    && (mariaDbGetDataFromServer(&response, &bytesReceived) >= 0));
  if ((responseIndex + DB_PACKET_HEADER_LENGTH) >= bytesReceived) {
    FREE_PARTIAL_RESULT();
    printLog(ERR, "Insufficient data from MariaDB server.\n");
    printLog(TRACE,
      "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
      query);
    releaseDbClientSocket(database, dbClientSocket);
    return returnValue; // succesful is false
  }
  littleEndianPacketLength = (*((int*) &response[responseIndex]));
  littleEndianToHost(&littleEndianPacketLength, sizeof(u32));
  littleEndianPacketLength &= 0x00ffffff;
  packetLength = (u64) littleEndianPacketLength;
  responseIndex += 3;
  packetNumber = response[responseIndex];
  responseIndex++;
  
  while (((responseIndex + sizeof(headerType)) >= bytesReceived)
    && (tss_get(_threadClientSocket) != NULL)
    && (mariaDbGetDataFromServer(&response, &bytesReceived) >= 0));
  if ((responseIndex + sizeof(headerType)) >= bytesReceived) {
    FREE_PARTIAL_RESULT();
    printLog(ERR, "Insufficient data from MariaDB server.\n");
    printLog(TRACE,
      "EXIT _mariaDbExecQuery(query=%p) = {successful = false}\n",
      query);
    releaseDbClientSocket(database, dbClientSocket);
    return returnValue; // succesful is false
  }
  headerType = response[responseIndex];
  while ((headerType != 0xfe) && (responseIndex < bytesReceived)) {
    void *check = realloc(returnValue->rows,
      (returnValue->numRows + 1) * sizeof(void**));
    if (check == NULL) {
      // Memory allocation failed.  Failure.
      // Cannot use printLog here.
      FREE_PARTIAL_RESULT();
      releaseDbClientSocket(database, dbClientSocket);
      return returnValue; // succesful is false
    }
    returnValue->rows = (void***) check;
    check = (Bytes*) calloc(1, (columnCount + 1) * sizeof(void*));
    if (check == NULL) {
      // Memory allocation failed.  Failure.
      // Cannot use printLog here.
      FREE_PARTIAL_RESULT();
      releaseDbClientSocket(database, dbClientSocket);
      return returnValue; // succesful is false
    }
    returnValue->rows[returnValue->numRows] = (void**) check;
    while ((responseIndex >= bytesReceived)
      && (tss_get(_threadClientSocket) != NULL)
    ) {
      printLog(DEBUG, "responseIndex >= packetStart + packetLength\n");
      if (mariaDbGetDataFromServer(&response, &bytesReceived) < 0) {
        break;
      }
    }
    
    for (u64 columnIndex = 0; columnIndex < columnCount; columnIndex++) {
      // First row is always the names of the fields, which are length-encoded
      // strings.  Default to that and change to the appropriate getter if we're
      // processing anything other than the first row.
      MariaDbGetValue valueGetter
        = getterFromType(returnValue->fieldTypes[columnIndex]);
      printLog(FLOOD, "Getting field %s of row %llu.\n",
        str(returnValue->rows[0][columnIndex]), llu(returnValue->numRows));
      check = valueGetter(
        (unsigned char**) &response, &responseIndex, &bytesReceived);
      
      // NULL is a valid value in this case, but suspicios since most things
      // are expected to have data.  NULL may mean that we had to abort the
      // connection.  Check if that's the case.
      TypeDescriptor *fieldType = returnValue->fieldTypes[columnIndex];
      if (check == NULL) {
        printLog(FLOOD, "check is NULL.\n");
        if (tss_get(_threadClientSocket) == NULL) {
          mtx_lock(&database->lock);
          if (database->numConnections > 0) {
            database->numConnections--;
          }
          cnd_signal(&database->socketAvailable);
          mtx_unlock(&database->lock);
          // Failure in lower-level function.  Can't use FREE_PARTIAL_RESULT()
          // until we've freed the partial row we've populated.
          if (returnValue->numRows > 0) {
            for (u64 j = 0; j < columnIndex; j++) {
              void* (*destructor)(volatile void*)
                = returnValue->fieldTypes[columnIndex]->destroy;
              if (returnValue->fieldTypes[columnIndex] == typeString) {
                destructor = (void* (*)(volatile void*)) bytesDestroy;
              }
              returnValue->rows[returnValue->numRows][j]
                = destructor(returnValue->rows[returnValue->numRows][j]);
            }
          } else {
            for (u64 j = 0; j < columnIndex; j++) {
              returnValue->rows[returnValue->numRows][j]
                = (void*) bytesDestroy((Bytes)returnValue->rows[returnValue->numRows][j]);
            }
          }
          returnValue->rows[returnValue->numRows]
            = (void**) pointerDestroy(returnValue->rows[returnValue->numRows]);
          FREE_PARTIAL_RESULT();
          return returnValue; // succesful is false
        } else if ((fieldType == typeString) || (fieldType == typeBytes)) {
          // We can't have a NULL value here because doing so would violate the
          // premise that DbResults are also Bytes tables.  For Bytes tables,
          // only the last index is NULL.  This is critical when representing
          // the table as a string.  Failure to adhere to this will result in an
          // incorrect number of columns being encoded in a string
          // representation, which does matter.  Allocate a new, empty Bytes
          // object here instead.
          bytesAllocate((Bytes*) &check, 0);
        }
      }
      if ((fieldType == typeString) || (fieldType == typeBytes)) {
        printLog(FLOOD, "Got value length %llu:\n",
          llu(bytesLength((Bytes) check)));
        printBinary(FLOOD, check, bytesLength((Bytes) check));
      }
      returnValue->rows[returnValue->numRows][columnIndex] = (void*) check;
    }
    returnValue->numRows++;
    
    if ((responseIndex <= bytesReceived) && (response[responseIndex] != 0xfe)) {
      while ((responseIndex + DB_PACKET_HEADER_LENGTH >= bytesReceived)
        && (tss_get(_threadClientSocket) != NULL)
      ) {
        // Get more data from the server.
        if (mariaDbGetDataFromServer(&response, &bytesReceived) < 0) {
          break;
        }
      }
      if (responseIndex + DB_PACKET_HEADER_LENGTH < bytesReceived) {
        littleEndianPacketLength = (*((u32*) &response[responseIndex]));
        littleEndianToHost(&littleEndianPacketLength, sizeof(u32));
        littleEndianPacketLength &= 0x00ffffff;
        packetLength = (u64) littleEndianPacketLength;
        printLog(FLOOD, "packetLenth = %llu\n", llu(packetLength));
        responseIndex += 3;
        packetNumber = response[responseIndex];
        printLog(FLOOD, "packetNumber = %d\n", packetNumber);
        responseIndex++;
      } else {
        printLog(FLOOD,
          "responseIndex + DB_PACKET_HEADER_LENGTH >= bytesReceived\n");
        printLog(FLOOD, "responseIndex + DB_PACKET_HEADER_LENGTH = %llu\n",
          llu(responseIndex + DB_PACKET_HEADER_LENGTH));
        printLog(FLOOD, "bytesReceived = %llu\n", llu(bytesReceived));
        printLog(FLOOD, "responseIndex = %llu\n", llu(responseIndex));
        printLog(FLOOD, "DB_PACKET_HEADER_LENGTH = %llu\n",
          llu(DB_PACKET_HEADER_LENGTH));
      }
    } else {
      printLog(FLOOD, "(responseIndex >= bytesReceived) "
        "|| (response[responseIndex] == 0xfe)\n");
      printLog(FLOOD, "responseIndex = %llu\n", llu(responseIndex));
      printLog(FLOOD, "bytesReceived = %llu\n", llu(bytesReceived));
      printLog(FLOOD, "response[responseIndex] = 0x%02x\n",
        response[responseIndex]);
    }
    
    while ((responseIndex >= bytesReceived)
      && (tss_get(_threadClientSocket) != NULL)
    ) {
      // Get more data from the server.
      if (mariaDbGetDataFromServer(&response, &bytesReceived) < 0) {
        break;
      }
    }
    
    if ((responseIndex + packetLength == bytesReceived)
        || (response[responseIndex] == 0xfe)
    ) {
      headerType = response[responseIndex];
      if (headerType == 0xfe) {
        responseIndex++;
      } else {
        // Server isn't done sending data.  Pull more.
        while ((responseIndex + packetLength == bytesReceived)
          && (tss_get(_threadClientSocket) != NULL)
        ) {
          if (mariaDbGetDataFromServer(&response, &bytesReceived) < 0) {
            break;
          }
        }
      }
    } else {
      printLog(FLOOD, "(responseIndex + packetLength != bytesReceived) "
        "&& (response[responseIndex] != 0xfe)\n");
      printLog(FLOOD, "responseIndex + packetLength = %llu\n",
        llu(responseIndex + packetLength));
      printLog(FLOOD, "bytesReceived = %llu\n", llu(bytesReceived));
      printLog(FLOOD, "responseIndex = %llu\n", llu(responseIndex));
      printLog(FLOOD, "packetLength = %llu\n", llu(packetLength));
      printLog(FLOOD, "response[responseIndex - 1] = 0x%02x\n",
        response[responseIndex - 1]);
      printLog(FLOOD, "response[responseIndex]     = 0x%02x\n",
        response[responseIndex]);
      printLog(FLOOD, "response[responseIndex + 1] = 0x%02x\n",
        response[responseIndex + 1]);
    }
  }
  response = (unsigned char*) pointerDestroy(response);
  // Add in a last NULL row
  void *check = realloc(returnValue->rows,
    (returnValue->numRows + 1) * sizeof(void**));
  if (check == NULL) {
    // Memory allocation failed.  Failure.
    // Cannot use printLog here.
    // Free the partial result.
    for (u64 i = 0; i < returnValue->numRows; i++) {
      for (u64 j = 0; j < returnValue->numFields; j++) {
        returnValue->rows[i][j]
          = (void*) bytesDestroy((Bytes) returnValue->rows[i][j]);
      }
      returnValue->rows[i] = (void**) pointerDestroy(returnValue->rows[i]);
    }
    returnValue->rows = (void***) pointerDestroy(returnValue->rows);
    returnValue->numRows = 0;
    returnValue->numFields = 0;
    releaseDbClientSocket(database, dbClientSocket);
    return returnValue; // succesful is false
  }
  returnValue->rows = (void ***) check;
  returnValue->rows[returnValue->numRows] = NULL;
  returnValue->successful = true;
  if (returnValue->numRows > 0) {
    returnValue->numResults = returnValue->numRows - 1;
  } // else returnValue->numRows remains 0
  
  printLog(DEBUG, "Query time: %llu microseconds\n",
    llu(getElapsedMicroseconds(queryStartTime)));
  
  printLog(TRACE, "EXIT _mariaDbExecQuery(query=%p) = {%llu results}\n", query,
    llu(returnValue->numResults));
  releaseDbClientSocket(database, dbClientSocket);
  return returnValue;
}

/// @fn DbResult* mariaDbExecQueryBytes(MariaDb *database, const Bytes query)
///
/// @brief Send a SQL query to the server.  If the query fails, try once more
/// in case the connection to the database had died.
///
/// @param database The MariaDb object with the database metadata.
/// @param query is the SQL query Bytes object to send.
///
/// @return Returns a DbResult containing the server's results on success,
/// an empty DbResult on failure.
DbResult* mariaDbExecQueryBytes(MariaDb *database, const Bytes query) {
  printLog(TRACE, "ENTER mariaDbExecQueryBytes(query=%p)\n", query);
  
  call_once(&_mariaDbThreadSetup, setupMariaDbThreadMetadata);
  
  DbResult *queryResult = _mariaDbExecQuery(database, query);
  if (queryResult->successful == false) {
    // It's possible the connection was broken.  If so, it will reset on the
    // second attempt.  Try once more.
    queryResult = dbFreeResult(queryResult);
    queryResult = _mariaDbExecQuery(database, query);
  }
  
  printLog(TRACE, "EXIT mariaDbExecQueryBytes(query=%p) = {successful = %s}\n", query,
    (queryResult->successful == true) ? "true" : "false");
  return queryResult;
}

/// @fn DbResult* mariaDbExecQueryString(MariaDb *database, const Bytes query)
///
/// @brief Send a SQL query string to the server.
///
/// @param database The MariaDb object with the database metadata.
/// @param query is the SQL query string to send.
///
/// @return Returns a DbResult object containing the server's results on
/// success, empty DbResult on failure.
DbResult* mariaDbExecQueryString(MariaDb *database, const char *queryString) {
  printLog(TRACE,
    "ENTER mariaDbExecQueryString(queryString=\"%s\")\n",
    queryString);
  
  Bytes queryBytes = NULL;
  bytesAddStr(&queryBytes, queryString);
  printLog(DEBUG, "Running query \"%s\"\n", queryString);
  DbResult *queryResult = mariaDbExecQueryBytes(database, queryBytes);
  queryBytes = bytesDestroy(queryBytes);
  
  printLog(TRACE,
    "EXIT mariaDbExecQueryString(queryString=\"%s\") = {successful = %s}\n",
    queryString, (queryResult->successful == true) ? "true" : "false");
  return queryResult;
}

/// @fn char *mariaDbDecodeErrorPacket(u8 *response, u32 responseLength, u16 *errorCode, char **sqlState)
///
/// @brief Decode an error packet sent by MariaDB
///
/// @param response is the response the MariaDB server provided.
/// @param responseLength is the length of the response variable.
/// @param errorCode is a pointer to the error code that will be set by
/// this function.
/// @param sqlState is a pointer to the SQL state string provided by MariaDB.
/// It will be set by this function.
///
/// @return Returns the decoded error message string.
char *mariaDbDecodeErrorPacket(u8 *response, u32 responseLength, u16 *errorCode, char **sqlState) {
  char nullByte = '\0';
  int responseIndex = 0;
  
  printLog(TRACE,
    "ENTER mariaDbDecodeErrorPacket(response=%p, responseLength=%d, errorCode=%d, sqlState=%p)\n",
    response, responseLength, *errorCode, *sqlState);
  
  *errorCode = *((u16*) &response[responseIndex]);
  littleEndianToHost(errorCode, sizeof(u16));
  responseIndex += 2;
  *sqlState = NULL;
  if (response[responseIndex] == (unsigned char) '#') {
    responseIndex++;
    dataAddData((void**) sqlState, 0, (void*) &response[responseIndex], 5);
    dataAddData((void**) sqlState, 5, (void*) &nullByte, 1);
    responseIndex += 5;
  }
  char *errorMessage = NULL;
  dataAddData((void**) &errorMessage, 0, (void*) &response[responseIndex], responseLength - responseIndex);
  dataAddData((void**) &errorMessage, responseLength - responseIndex, (void*) &nullByte, 1);
  
  printLog(TRACE,
    "EXIT mariaDbDecodeErrorPacket(response=%p, responseLength=%d, errorCode=%d, sqlState=%p) = {\"%s\"}\n",
    response, responseLength, *errorCode, *sqlState, errorMessage);
  return errorMessage;
}

/// @fn u64 mariaDbGetLengthEncodedInteger(unsigned char **packetData, u64 *index, u32 *bytesReceived)
///
/// @brief Decode and return a MariaDB length-encoded length from a packet of
/// data.
///
/// @param packetData is a pointer to the packetData variable used to make this
/// call.  The value at *packetData will be modified by this function.
/// @param index is the index of *packetData that is being examined.
/// @param *bytesReceived is a pointer to the bytesReceived variable used to
/// make this call.  The value at *bytesReceived may be updated by this
/// function.
///
/// @return Returns the decoded integer as an unsigned 64-bit value.
u64 mariaDbGetLengthEncodedInteger(
  unsigned char **packetData, u64 *index, u32 *bytesReceived
) {
  printLog(FLOOD,
    "ENTER mariaDbGetLengthEncodedInteger(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u)\n",
    (packetData != NULL) ? *packetData : (void*) NULL,
    (index != NULL) ? llu(*index) : 0,
    (bytesReceived != NULL) ? *bytesReceived : 0
  );
  
  if ((packetData == NULL) || (index == NULL) || (bytesReceived == NULL)) {
    printLog(ERR, "One or more NULL parameters provided.  Cannot process.\n");
    printLog(FLOOD,
      "EXIT mariaDbGetLengthEncodedInteger(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%llu}\n",
      packetData, index, bytesReceived, llu(0));
    return 0;
  }
  
  int numRetries = 0;
  while ((*index >= *bytesReceived)
    && (tss_get(_threadClientSocket) != NULL)
  ) {
    printLog(DEBUG, "*index >= *bytesReceived\n");
    if (mariaDbGetDataFromServer(packetData, bytesReceived) < 0) {
      break;
    }
    numRetries++;
  }
  if ((numRetries == 3) || (tss_get(_threadClientSocket) == NULL)) {
    printLog(ERR, "Insufficient data from MariaDB server.\n");
    // Close the socket for this thread.
    tss_set(_threadClientSocket,
      socketDestroy((Socket*) tss_get(_threadClientSocket)));
    printLog(FLOOD,
      "EXIT mariaDbGetLengthEncodedInteger(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%llu}\n",
      packetData, index, bytesReceived, llu(0));
    return 0;
  }
  u64 returnValue = 0;
  returnValue = (u64) *((unsigned char*) &(*packetData)[*index]);
  
  if (returnValue <= 0xfb) {
    // returnValue already has the value to return
    // Move the index one byte
    *index += 1;
    if (returnValue == 0xfb) {
      // NULL response.  The value 0xfb may be encoded in two bytes following
      // 0xfc, so zero out the returnValue here.
      returnValue = 0;
    }
  } else if (returnValue == 0xfc) {
    *index += 1;
    // returnValue is the next two bytes
    numRetries = 0;
    while ((*index + 2 >= *bytesReceived)
      && (tss_get(_threadClientSocket) != NULL)
    ) {
      printLog(DEBUG, "*index + 2 >= *bytesReceived\n");
      if (mariaDbGetDataFromServer(packetData, bytesReceived) < 0) {
        break;
      }
      numRetries++;
    }
    if ((numRetries == 3) || (tss_get(_threadClientSocket) == NULL)) {
      printLog(ERR, "Insufficient data from MariaDB server.\n");
      // Close the socket for this thread.
      tss_set(_threadClientSocket,
        socketDestroy((Socket*) tss_get(_threadClientSocket)));
      printLog(FLOOD,
        "EXIT mariaDbGetLengthEncodedInteger(packetData=%p, index=%p, "
        "bytesReceived=%p) = {%llu}\n",
        packetData, index, bytesReceived, llu(0));
      return 0;
    }
    littleEndianToHost(&((*packetData)[*index]), 2);
    returnValue = (u64) *((u16*) &((*packetData)[*index]));
    *index += 2;
  } else if (returnValue == 0xfd) {
    *index += 1;
    // returnValue is the next three bytes
    numRetries = 0;
    while ((*index + 3 >= *bytesReceived)
      && (tss_get(_threadClientSocket) != NULL)
    ) {
      printLog(DEBUG, "*index + 2 >= *bytesReceived\n");
      if (mariaDbGetDataFromServer(packetData, bytesReceived) < 0) {
        break;
      }
      numRetries++;
    }
    if ((numRetries == 3) || (tss_get(_threadClientSocket) == NULL)) {
      printLog(ERR, "Insufficient data from MariaDB server.\n");
      // Close the socket for this thread.
      tss_set(_threadClientSocket,
        socketDestroy((Socket*) tss_get(_threadClientSocket)));
      printLog(FLOOD,
        "EXIT mariaDbGetLengthEncodedInteger(packetData=%p, index=%p, "
        "bytesReceived=%p) = {%llu}\n",
        packetData, index, bytesReceived, llu(0));
      return 0;
    }
    ZEROINIT(char buffer[4]);
    memcpy(buffer, &((*packetData)[*index]), 3);
    littleEndianToHost(buffer, 4);
    returnValue = (u64) (*((u32*) buffer));
    *index += 3;
  } else if (returnValue == 0xfe) {
    *index += 1;
    // returnValue is the next eight bytes
    numRetries = 0;
    while ((*index + 8 >= *bytesReceived)
      && (tss_get(_threadClientSocket) != NULL)
    ) {
      printLog(DEBUG, "*index + 2 >= *bytesReceived\n");
      if (mariaDbGetDataFromServer(packetData, bytesReceived) < 0) {
        break;
      }
      numRetries++;
    }
    if ((numRetries == 3) || (tss_get(_threadClientSocket) == NULL)) {
      printLog(ERR, "Insufficient data from MariaDB server.\n");
      // Close the socket for this thread.
      tss_set(_threadClientSocket,
        socketDestroy((Socket*) tss_get(_threadClientSocket)));
      printLog(FLOOD,
        "EXIT mariaDbGetLengthEncodedInteger(packetData=%p, index=%p, "
        "bytesReceived=%p) = {%llu}\n",
        packetData, index, bytesReceived, llu(0));
      return 0;
    }
    littleEndianToHost(&((*packetData)[*index]), 8);
    returnValue = *((u64*) &((*packetData)[*index]));
    *index += 8;
  }
  
  printLog(FLOOD,
    "EXIT mariaDbGetLengthEncodedInteger(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u) = {%llu}\n",
    *packetData, llu(*index), *bytesReceived, llu(returnValue));
  return returnValue;
}

/// @fn int mariaDbGetDataFromServer(u8 **response, u32 *bytesReceived)
///
/// @brief This is the function that is called by mariaDbExecQuery to retrieve data
/// from the server.  It updates the contents of response and the number of
/// bytes received.
///
/// @param response is the byte array of the responses the server has sent
/// so far
/// @param bytesReceived is the number of bytes that have been received so far
///
/// @return Returns -1 on error, number of bytes read on success.
int mariaDbGetDataFromServer(u8 **response, u32 *bytesReceived) {
  printLog(FLOOD,
    "ENTER mariaDbGetDataFromServer(response=%p, *bytesReceived=%u)\n",
    response, *bytesReceived);
  
  unsigned char responsePacket[4096];
  
  // We can't use getDbClientSocket here because that might get the wrong
  // connection.  We're limited strictly to what tss_get can provide.
  Socket *dbClientSocket = (Socket*) tss_get(_threadClientSocket);
  if (dbClientSocket == NULL) {
    printLog(ERR, "mariaDbGetDataFromServer: No connection to the database is available.\n");
    printLog(FLOOD,
      "EXIT mariaDbGetDataFromServer(response=%p, *bytesReceived=%u) = {%d}\n",
      response, *bytesReceived, -1);
    return -1;
  }
  
  // Put the socket in blocking mode:
  if (socketSetBlocking(dbClientSocket) != NO_ERROR) {
    printLog(ERR, "Could not put DB socket in blocking mode.\n");
    printLog(FLOOD,
      "EXIT mariaDbGetDataFromServer(response=%p, *bytesReceived=%u) = {%d}\n",
      response, *bytesReceived, -1);
    return -1;
  }
  
  int packetBytes = socketReceive(dbClientSocket, responsePacket,
    sizeof(responsePacket));
  if (packetBytes > 0) {
    dataAddData((void**) response, *bytesReceived,
      responsePacket, packetBytes);
    *bytesReceived += (u32) packetBytes;
  } else {
    // Socket error.  We can't use this connection any longer and we need
    // help from a higher-level function to re-establish the connection and
    // continue.
    printLog(ERR, "Received %d Bytes from server.  Destroying socket.\n",
      packetBytes);
    tss_set(_threadClientSocket,
      socketDestroy((Socket*) tss_get(_threadClientSocket)));
    dbClientSocket = NULL;
  }
  
  printLog(FLOOD,
    "EXIT mariaDbGetDataFromServer(response=%p, *bytesReceived=%u) = {%d}\n",
    response, *bytesReceived, packetBytes);
  return packetBytes;
}

/// @fn char* binToHex(const unsigned char *bin, long long unsigned int length, char *outputString)
///
/// @brief Convert a binary buffer to its hexadecimal representation.
///
/// @param bin A pointer to binary character buffer to convert.
/// @param length The number of bytes at the bin pointer.
/// @param outputString The buffer to put the output into.  If this parameter
///   is NULL, a buffer will be allocated.
///
/// @return Returns a pointer to outputString on success, NULL on failure.
char* binToHex(const unsigned char *bin, long long unsigned int length,
  char *outputString
) {
  if (outputString == NULL) {
    // No user-provided buffer.  Allocate one for us.
    outputString = (char*) malloc((2 * length) + 1);
    if (outputString == NULL) {
      // User provided null and we can't allocate memory.  Bail.
      return NULL;
    }
  }
  outputString[2 * length] = '\0';
  
  for (long long unsigned int i = 0; i < length; i++) {
    sprintf(&outputString[2 * i], "%02X", bin[i]);
  }
  
  return outputString;
}

/// @fn unsigned char *generateMariaDbPasswordHash(const unsigned char *password, unsigned long passwordLength, const unsigned char *seed, u64 seedLength)
///
/// @brief Generate the a password hash according to MariaDb's hash algorithm.
///
/// @param password is the user's password.
/// @param passwordLength is the length of the user's password in bytes.
/// @param seed is the seed to use (provided in the MariaDb login handshake).
/// @param seedLength is the length of the seed in bytes.
///
/// @return Returns an array of unsigned characters SHA_DIGEST_LENGTH bytes
/// long.
unsigned char *generateMariaDbPasswordHash(const unsigned char *password,
  unsigned long passwordLength, const unsigned char *seed, u64 seedLength
) {
  char *passwordCopy = NULL, *seedCopy = NULL;
  dataAddData((void**) &passwordCopy, 0, password, passwordLength);
  dataAddData((void**) &passwordCopy, passwordLength, "", 1);
  dataAddData((void**) &seedCopy, 0, seed, seedLength);
  dataAddData((void**) &seedCopy, seedLength, "", 1);
  printLog(TRACE,
    "ENTER generateMariaDbPasswordHash(password=%p, passwordLength=?, "
    "seed=%p, seedLength=?)\n", passwordCopy, seedCopy);
  
  // MariaDb hash algorithm:
  // password hash = SHA1(passwd) ^ SHA1(seed + SHA1(SHA1(passwd)))
  unsigned char passwordSHA1[SHA_DIGEST_LENGTH];
  char hashString[(2 * SHA_DIGEST_LENGTH) + 1];
  sha1digest(passwordSHA1, hashString, password, passwordLength);
  
  unsigned char *returnValue
    = generateMariaDbPasswordHashFromSha(passwordSHA1, seed, seedLength);
  binToHex(returnValue, SHA_DIGEST_LENGTH, hashString);
  
  printLog(TRACE,
    "EXIT generateMariaDbPasswordHash(password=%p, passwordLength=?, "
    "seed=%p, seedLength=?) = {%p}\n", passwordCopy, seedCopy, hashString);
  passwordCopy = stringDestroy(passwordCopy);
  seedCopy = stringDestroy(seedCopy);
  return returnValue;
}

/// @fn unsigned char *generateMariaDbPasswordHashFromSha(const unsigned char *sha, const unsigned char *seed, u64 seedLength)
///
/// @brief Generate a password hash according to MariaDb's hash algorithm.
///
/// @param sha is the pre-computed SHA1 hash.
/// @param seed is the seed to use (provided in the MariaDb login handshake).
/// @param seedLength is the length of the seed in bytes.
///
/// @return Returns an array of unsigned characters SHA_DIGEST_LENGTH bytes
/// long.
unsigned char *generateMariaDbPasswordHashFromSha(const unsigned char *sha,
  const unsigned char *seed, u64 seedLength
) {
  char hashString[(2 * SHA_DIGEST_LENGTH) + 1];
  unsigned char passwordHash[SHA_DIGEST_LENGTH + 1];
  unsigned char *seedCopy = NULL;
  passwordHash[SHA_DIGEST_LENGTH] = '\0';
  
  dataAddData((void**) &seedCopy, 0, seed, seedLength);
  
  // MariaDb hash algorithm:
  // password hash = SHA1(passwd) ^ SHA1(seed + SHA1(SHA1(passwd)))
  sha1digest(passwordHash, hashString, sha, SHA_DIGEST_LENGTH);
  
  dataAddData((void**) &seedCopy, seedLength,
    (void*) passwordHash, SHA_DIGEST_LENGTH);
  char *seedPlusHash = binToHex(seedCopy, seedLength + SHA_DIGEST_LENGTH, NULL);
  seedPlusHash = stringDestroy(seedPlusHash);
  
  sha1digest(passwordHash, hashString,
    seedCopy, seedLength + SHA_DIGEST_LENGTH);
  seedCopy = (unsigned char*) pointerDestroy(seedCopy);
  
  for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
    passwordHash[i] = ((passwordHash[i] & 0xff) ^ (sha[i] & 0xff)) & 0xff;
  }
  binToHex(passwordHash, SHA_DIGEST_LENGTH, hashString);
  
  unsigned char *returnValue = NULL;
  dataAddData((void**) &returnValue, 0, passwordHash, SHA_DIGEST_LENGTH);
  
  return returnValue;
}

/// @fn Bytes mariaDbGetLengthEncodedString(unsigned char **packetData, u64 *index, u32 *bytesReceived)
///
/// @brief Decode and return a MariaDB length-encoded string from a packet of
/// data.
///
/// @param packetData is a pointer to the packetData variable used to make this
///   call.  The value at *packetData will be modified by this function.
/// @param index is the index of *packetData that is being examined.
/// @param *bytesReceived is a pointer to the bytesReceived variable used to
///   make this call.  The value at *bytesReceived may be updated by this
///   function.
///
/// @return Returns the decoded integer as a Bytes object.
Bytes mariaDbGetLengthEncodedString(
  unsigned char **packetData, u64 *index, u32 *bytesReceived
) {
  printLog(FLOOD,
    "ENTER mariaDbGetLengthEncodedString(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u)\n",
    (packetData != NULL) ? *packetData : (void*) NULL,
    (index != NULL) ? llu(*index) : 0,
    (bytesReceived != NULL) ? *bytesReceived : 0
  );
  
  if ((packetData == NULL) || (index == NULL) || (bytesReceived == NULL)) {
    printLog(ERR, "One or more NULL parameters provided.  Cannot process.\n");
    printLog(FLOOD,
      "EXIT mariaDbGetLengthEncodedString(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%p}\n",
      packetData, index, bytesReceived, (void*) NULL);
    return NULL;
  }
  
  u64 stringLength
    = mariaDbGetLengthEncodedInteger(packetData, index, bytesReceived);
  if (stringLength == 0) {
    // This may be due to a memory allocation failure.  Don't attempt to use
    // printLog here.
    return NULL;
  }
  
  while ((*index + stringLength >= *bytesReceived)
    && (tss_get(_threadClientSocket) != NULL)
  ) {
    printLog(DEBUG, "*index + stringLength >= *bytesReceived\n");
    printLog(DEBUG, "*index + stringLength = %llu\n",
      llu(*index + stringLength));
    printLog(DEBUG, "*index = %llu\n", llu(*index));
    printLog(DEBUG, "stringLength = %llu\n", llu(stringLength));
    printLog(DEBUG, "*bytesReceived = %llu\n", llu(*bytesReceived));
    mariaDbGetDataFromServer(packetData, bytesReceived);
  }
  if (tss_get(_threadClientSocket) == NULL) {
    printLog(ERR, "Insufficient data from MariaDB server.\n");
    printLog(ERR, "Expected %llu bytes, have %llu.\n",
      llu(*index + stringLength), llu(*bytesReceived));
    printLog(FLOOD,
      "EXIT mariaDbGetLengthEncodedString(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%p}\n",
      packetData, index, bytesReceived, (void*) NULL);
    return NULL;
  }
  
  Bytes returnValue = NULL;
  bytesAddData(&returnValue, &((*packetData)[*index]), stringLength);
  *index += stringLength;
  
  printLog(FLOOD,
    "EXIT mariaDbGetLengthEncodedString(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u) = {%s}\n",
    *packetData, llu(*index), *bytesReceived, (char*) returnValue);
  return returnValue;
}

/// @fn void* mariaDbGetI8(unsigned char **packetData, u64 *index, u32 *bytesReceived)
///
/// @brief Get a signed 8-bit integer from the packet received.
///
/// @param packetData A pointer to a pointer to the buffer of unsigned char
///   data that's been received from the server so far.
/// @param index A pointer to a u64 that indicates the index within the
///   packetData buffer that is to be processed next.
/// @param bytesReceived A pointer to a u32 holding the number of bytes
///   received in packetData so far.
///
/// @return A pointer to a newly-allocated i8 value that was read from the
/// packetData on success, NULL on failure.
void* mariaDbGetI8(
  unsigned char **packetData, u64 *index, u32 *bytesReceived
) {
  printLog(TRACE,
    "ENTER mariaDbGetI8(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u)\n",
    (packetData != NULL) ? *packetData : (void*) NULL,
    (index != NULL) ? llu(*index) : 0,
    (bytesReceived != NULL) ? *bytesReceived : 0
  );
  
  if ((packetData == NULL) || (index == NULL) || (bytesReceived == NULL)) {
    printLog(ERR, "One or more NULL parameters provided.  Cannot process.\n");
    printLog(TRACE,
      "EXIT mariaDbGetI8(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%p}\n",
      packetData, index, bytesReceived, (void*) NULL);
    return NULL;
  }
  
  Bytes data = mariaDbGetLengthEncodedString(packetData, index, bytesReceived);
  
  if (tss_get(_threadClientSocket) == NULL) {
    data = bytesDestroy(data);
    printLog(ERR, "Insufficient data from MariaDB server.\n");
    printLog(TRACE,
      "EXIT mariaDbGetI8(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%p}\n",
      packetData, index, bytesReceived, (void*) NULL);
    return NULL;
  }
  
  i64 mariaDbInteger = 0;
  if (data != NULL) {
    mariaDbInteger = (i64) strtoll(str(data), NULL, 10);
  }
  data = bytesDestroy(data);
  i8 i8Value = (i8) mariaDbInteger;
  void *returnValue = typeI8->copy(&i8Value);
  
  printLog(TRACE,
    "EXIT mariaDbGetI8(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u) = {%d}\n",
    *packetData, llu(*index), *bytesReceived, i8Value);
  return returnValue;
}

/// @fn void* mariaDbGetI16(unsigned char **packetData, u64 *index, u32 *bytesReceived)
///
/// @brief Get a signed 16-bit integer from the packet received.
///
/// @param packetData A pointer to a pointer to the buffer of unsigned char
///   data that's been received from the server so far.
/// @param index A pointer to a u64 that indicates the index within the
///   packetData buffer that is to be processed next.
/// @param bytesReceived A pointer to a u32 holding the number of bytes
///   received in packetData so far.
///
/// @return A pointer to a newly-allocated i16 value that was read from the
/// packetData on success, NULL on failure.
void* mariaDbGetI16(
  unsigned char **packetData, u64 *index, u32 *bytesReceived
) {
  printLog(TRACE,
    "ENTER mariaDbGetI16(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u)\n",
    (packetData != NULL) ? *packetData : (void*) NULL,
    (index != NULL) ? llu(*index) : 0,
    (bytesReceived != NULL) ? *bytesReceived : 0
  );
  
  if ((packetData == NULL) || (index == NULL) || (bytesReceived == NULL)) {
    printLog(ERR, "One or more NULL parameters provided.  Cannot process.\n");
    printLog(TRACE,
      "EXIT mariaDbGetI16(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%p}\n",
      packetData, index, bytesReceived, (void*) NULL);
    return NULL;
  }
  
  Bytes data = mariaDbGetLengthEncodedString(packetData, index, bytesReceived);
  
  if (tss_get(_threadClientSocket) == NULL) {
    data = bytesDestroy(data);
    printLog(ERR, "Insufficient data from MariaDB server.\n");
    printLog(TRACE,
      "EXIT mariaDbGetI16(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%p}\n",
      packetData, index, bytesReceived, (void*) NULL);
    return NULL;
  }
  
  i64 mariaDbInteger = 0;
  if (data != NULL) {
    mariaDbInteger = (i64) strtoll(str(data), NULL, 10);
  }
  data = bytesDestroy(data);
  i16 i16Value = (i16) mariaDbInteger;
  void *returnValue = typeI16->copy(&i16Value);
  
  printLog(TRACE,
    "EXIT mariaDbGetI16(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u) = {%d}\n",
    *packetData, llu(*index), *bytesReceived, i16Value);
  return returnValue;
}

/// @fn void* mariaDbGetI24(unsigned char **packetData, u64 *index, u24 *bytesReceived)
///
/// @brief Get a signed 24-bit integer from the packet received.
///
/// @note There's no such thing as a 24-bit type in C, so this function stores
/// the parsed value in a 32-bit integer.
///
/// @param packetData A pointer to a pointer to the buffer of unsigned char
///   data that's been received from the server so far.
/// @param index A pointer to a u64 that indicates the index within the
///   packetData buffer that is to be processed next.
/// @param bytesReceived A pointer to a u32 holding the number of bytes
///   received in packetData so far.
///
/// @return A pointer to a newly-allocated i32 value that was read from the
/// packetData on success, NULL on failure.
void* mariaDbGetI24(
  unsigned char **packetData, u64 *index, u32 *bytesReceived
) {
  printLog(TRACE,
    "ENTER mariaDbGetI24(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u)\n",
    (packetData != NULL) ? *packetData : (void*) NULL,
    (index != NULL) ? llu(*index) : 0,
    (bytesReceived != NULL) ? *bytesReceived : 0
  );
  
  if ((packetData == NULL) || (index == NULL) || (bytesReceived == NULL)) {
    printLog(ERR, "One or more NULL parameters provided.  Cannot process.\n");
    printLog(TRACE,
      "EXIT mariaDbGetI24(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%p}\n",
      packetData, index, bytesReceived, (void*) NULL);
    return NULL;
  }
  
  Bytes data = mariaDbGetLengthEncodedString(packetData, index, bytesReceived);
  
  if (tss_get(_threadClientSocket) == NULL) {
    data = bytesDestroy(data);
    printLog(ERR, "Insufficient data from MariaDB server.\n");
    printLog(TRACE,
      "EXIT mariaDbGetI24(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%p}\n",
      packetData, index, bytesReceived, (void*) NULL);
    return NULL;
  }
  
  i64 mariaDbInteger = 0;
  if (data != NULL) {
    mariaDbInteger = (i64) strtoll(str(data), NULL, 10);
  }
  data = bytesDestroy(data);
  i32 i24Value = (i32) mariaDbInteger;
  void *returnValue = typeI24->copy(&i24Value);
  
  printLog(TRACE,
    "EXIT mariaDbGetI24(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u) = {%d}\n",
    *packetData, llu(*index), *bytesReceived, i24Value);
  return returnValue;
}

/// @fn void* mariaDbGetI32(unsigned char **packetData, u64 *index, u32 *bytesReceived)
///
/// @brief Get a signed 32-bit integer from the packet received.
///
/// @param packetData A pointer to a pointer to the buffer of unsigned char
///   data that's been received from the server so far.
/// @param index A pointer to a u64 that indicates the index within the
///   packetData buffer that is to be processed next.
/// @param bytesReceived A pointer to a u32 holding the number of bytes
///   received in packetData so far.
///
/// @return A pointer to a newly-allocated i32 value that was read from the
/// packetData on success, NULL on failure.
void* mariaDbGetI32(
  unsigned char **packetData, u64 *index, u32 *bytesReceived
) {
  printLog(TRACE,
    "ENTER mariaDbGetI32(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u)\n",
    (packetData != NULL) ? *packetData : (void*) NULL,
    (index != NULL) ? llu(*index) : 0,
    (bytesReceived != NULL) ? *bytesReceived : 0
  );
  
  if ((packetData == NULL) || (index == NULL) || (bytesReceived == NULL)) {
    printLog(ERR, "One or more NULL parameters provided.  Cannot process.\n");
    printLog(TRACE,
      "EXIT mariaDbGetI32(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%p}\n",
      packetData, index, bytesReceived, (void*) NULL);
    return NULL;
  }
  
  Bytes data = mariaDbGetLengthEncodedString(packetData, index, bytesReceived);
  
  if (tss_get(_threadClientSocket) == NULL) {
    data = bytesDestroy(data);
    printLog(ERR, "Insufficient data from MariaDB server.\n");
    printLog(TRACE,
      "EXIT mariaDbGetI32(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%p}\n",
      packetData, index, bytesReceived, (void*) NULL);
    return NULL;
  }
  
  i64 mariaDbInteger = 0;
  if (data != NULL) {
    mariaDbInteger = (i64) strtoll(str(data), NULL, 10);
  }
  data = bytesDestroy(data);
  i32 i32Value = (i32) mariaDbInteger;
  void *returnValue = typeI32->copy(&i32Value);
  
  printLog(TRACE,
    "EXIT mariaDbGetI32(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u) = {%d}\n",
    *packetData, llu(*index), *bytesReceived, i32Value);
  return returnValue;
}

/// @fn void* mariaDbGetI64(unsigned char **packetData, u64 *index, u32 *bytesReceived)
///
/// @brief Get a signed 64-bit integer from the packet received.
///
/// @param packetData A pointer to a pointer to the buffer of unsigned char
///   data that's been received from the server so far.
/// @param index A pointer to a u64 that indicates the index within the
///   packetData buffer that is to be processed next.
/// @param bytesReceived A pointer to a u32 holding the number of bytes
///   received in packetData so far.
///
/// @return A pointer to a newly-allocated i64 value that was read from the
/// packetData on success, NULL on failure.
void* mariaDbGetI64(
  unsigned char **packetData, u64 *index, u32 *bytesReceived
) {
  printLog(TRACE,
    "ENTER mariaDbGetI64(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u)\n",
    (packetData != NULL) ? *packetData : (void*) NULL,
    (index != NULL) ? llu(*index) : 0,
    (bytesReceived != NULL) ? *bytesReceived : 0
  );
  
  if ((packetData == NULL) || (index == NULL) || (bytesReceived == NULL)) {
    printLog(ERR, "One or more NULL parameters provided.  Cannot process.\n");
    printLog(TRACE,
      "EXIT mariaDbGetI64(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%p}\n",
      packetData, index, bytesReceived, (void*) NULL);
    return NULL;
  }
  
  Bytes data = mariaDbGetLengthEncodedString(packetData, index, bytesReceived);
  
  if (tss_get(_threadClientSocket) == NULL) {
    data = bytesDestroy(data);
    printLog(ERR, "Insufficient data from MariaDB server.\n");
    printLog(TRACE,
      "EXIT mariaDbGetI32(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%p}\n",
      packetData, index, bytesReceived, (void*) NULL);
    return NULL;
  }
  
  i64 mariaDbInteger = 0;
  if (data != NULL) {
    mariaDbInteger = (i64) strtoll(str(data), NULL, 10);
  }
  data = bytesDestroy(data);
  i64 i64Value = (i64) mariaDbInteger;
  void *returnValue = typeI64->copy(&i64Value);
  
  printLog(TRACE,
    "EXIT mariaDbGetI64(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u) = {%lld}\n",
    *packetData, llu(*index), *bytesReceived, lld(i64Value));
  return returnValue;
}

/// @fn void* mariaDbGetFloat(unsigned char **packetData, u64 *index, u32 *bytesReceived)
///
/// @brief Get a single-precision floating-point value from the packet received.
///
/// @param packetData A pointer to a pointer to the buffer of unsigned char
///   data that's been received from the server so far.
/// @param index A pointer to a u64 that indicates the index within the
///   packetData buffer that is to be processed next.
/// @param bytesReceived A pointer to a u32 holding the number of bytes
///   received in packetData so far.
///
/// @return A pointer to a newly-allocated float value that was read from the
/// packetData on success, NULL on failure.
void* mariaDbGetFloat(
  unsigned char **packetData, u64 *index, u32 *bytesReceived
) {
  printLog(TRACE,
    "ENTER mariaDbGetFloat(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u)\n",
    (packetData != NULL) ? *packetData : (void*) NULL,
    (index != NULL) ? llu(*index) : 0,
    (bytesReceived != NULL) ? *bytesReceived : 0
  );
  
  if ((packetData == NULL) || (index == NULL) || (bytesReceived == NULL)) {
    printLog(ERR, "One or more NULL parameters provided.  Cannot process.\n");
    printLog(TRACE,
      "EXIT mariaDbGetFloat(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%p}\n",
      packetData, index, bytesReceived, (void*) NULL);
    return NULL;
  }
  
  Bytes data = mariaDbGetLengthEncodedString(packetData, index, bytesReceived);
  
  if (tss_get(_threadClientSocket) == NULL) {
    data = bytesDestroy(data);
    printLog(ERR, "Insufficient data from MariaDB server.\n");
    printLog(TRACE,
      "EXIT mariaDbGetFloat(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%p}\n",
      packetData, index, bytesReceived, (void*) NULL);
    return NULL;
  }
  
  float floatValue = strtof(str(data), NULL);
  data = bytesDestroy(data);
  void *returnValue = typeFloat->copy(&floatValue);
  
  printLog(TRACE,
    "EXIT mariaDbGetFloat(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u) = {%f}\n",
    *packetData, llu(*index), *bytesReceived, floatValue);
  return returnValue;
}

/// @fn void* mariaDbGetDouble(unsigned char **packetData, u64 *index, u32 *bytesReceived)
///
/// @brief Get a double-precision floating-point value from the packet received.
///
/// @param packetData A pointer to a pointer to the buffer of unsigned char
///   data that's been received from the server so far.
/// @param index A pointer to a u64 that indicates the index within the
///   packetData buffer that is to be processed next.
/// @param bytesReceived A pointer to a u32 holding the number of bytes
///   received in packetData so far.
///
/// @return A pointer to a newly-allocated double value that was read from the
/// packetData on success, NULL on failure.
void* mariaDbGetDouble(
  unsigned char **packetData, u64 *index, u32 *bytesReceived
) {
  printLog(TRACE,
    "ENTER mariaDbGetDouble(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u)\n",
    (packetData != NULL) ? *packetData : (void*) NULL,
    (index != NULL) ? llu(*index) : 0,
    (bytesReceived != NULL) ? *bytesReceived : 0
  );
  
  if ((packetData == NULL) || (index == NULL) || (bytesReceived == NULL)) {
    printLog(ERR, "One or more NULL parameters provided.  Cannot process.\n");
    printLog(TRACE,
      "EXIT mariaDbGetDouble(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%p}\n",
      packetData, index, bytesReceived, (void*) NULL);
    return NULL;
  }
  
  Bytes data = mariaDbGetLengthEncodedString(packetData, index, bytesReceived);
  
  if (tss_get(_threadClientSocket) == NULL) {
    data = bytesDestroy(data);
    printLog(ERR, "Insufficient data from MariaDB server.\n");
    printLog(TRACE,
      "EXIT mariaDbGetDouble(packetData=%p, index=%p, "
      "bytesReceived=%p) = {%p}\n",
      packetData, index, bytesReceived, (void*) NULL);
    return NULL;
  }
  
  double doubleValue = strtod(str(data), NULL);
  data = bytesDestroy(data);
  void *returnValue = typeDouble->copy(&doubleValue);
  
  printLog(TRACE,
    "EXIT mariaDbGetDouble(*packetData=%p, *index=%llu, "
    "*bytesReceived=%u) = {%lf}\n",
    *packetData, llu(*index), *bytesReceived, doubleValue);
  return returnValue;
}

/// @fn DbResult* mariaDbGetDatabaseNames(SqlDatabase *database)
///
/// @brief Get the names of the databases managed by the SQL system.
///
/// @param database A pointer to the SqlDatabase object representing the database
///   system to query.
///
/// @return Returns a DbResult object with the names of the databases.
DbResult* mariaDbGetDatabaseNames(SqlDatabase *database) {
  printLog(TRACE,
    "ENTER mariaDbGetDatabaseNames(database=%p))\n",
    database);
  
  DbResult *queryResult
    = mariaDbExecQueryString((MariaDb*) database->connection, "show databases;");
  
  printLog(TRACE,
    "EXIT mariaDbGetDatabaseNames(database=%p) = {%s}\n",
    database, (queryResult->successful) ? "successful" : "NOT successful");
  return queryResult;
}

/// @fn DbResult* mariaDbGetTableNames(SqlDatabase *database, const char *dbString)
///
/// @brief Get the names of the tables in a database.
///
/// @param database A pointer to the SqlDatabase object representing the database
///   system to query.
/// @param dbString The name of the database to query.
///
/// @return Returns a DbResult object with the names of the tables.
DbResult* mariaDbGetTableNames(SqlDatabase *database, const char *dbString) {
  char *dbName = NULL;
  straddstr(&dbName, dbString);
  straddstr(&dbName, dbInstance);
  
  printLog(TRACE,
    "ENTER mariaDbGetTableNames(database=%p, dbName=\"%s\")\n",
    database, dbName);
  
  DbResult *returnValue = (DbResult*) calloc(1, sizeof(DbResult));
  if (returnValue == NULL) {
    LOG_MALLOC_FAILURE();
    exit(1);
  }
  Bytes query = NULL;
  DbResult *queryResult = NULL;
  
  // Have to do a "use" for getting table names.  Use a transaction so that we
  // can group the commands.
  MariaDb *mariaDb = (MariaDb*) database->connection;
  tss_set(mariaDb->transactionInProgress, VOID_POINTER_TRUE);
  bytesAddStr(&query, "use ");
  bytesAddStr(&query, dbName);
  bytesAddStr(&query, ";");
  queryResult = mariaDbExecQueryBytes((MariaDb*) database->connection, query);
  query = bytesDestroy(query);
  if (queryResult->successful == false) {
    printLog(TRACE,
      "EXIT mariaDbGetTableNames(database=%p, dbName=\"%s\") "
      "= {could not set database}\n",
      database, dbName);
    queryResult = dbFreeResult(queryResult);
    tss_set(mariaDb->transactionInProgress, VOID_POINTER_FALSE);
    dbName = stringDestroy(dbName);
    return returnValue;
  }
  queryResult = dbFreeResult(queryResult);
  
  bytesAddStr(&query, "show tables;");
  queryResult = mariaDbExecQueryBytes((MariaDb*) database->connection, query);
  query = bytesDestroy(query);
  tss_set(mariaDb->transactionInProgress, VOID_POINTER_FALSE);
  
  returnValue = dbFreeResult(returnValue);
  returnValue = queryResult;
  
  printLog(TRACE,
    "EXIT mariaDbGetTableNames(database=%p, dbName=\"%s\") = {%llu results}\n",
    database, dbName, llu(returnValue->numRows));
  dbName = stringDestroy(dbName);
  return returnValue;
}

/// @fn bool mariaDbAddDatabase(SqlDatabase *database, const char *dbString)
///
/// @brief Add a new database to a MariaDB server.
///
/// @param database A pointer to the SqlDatabase object representing the database
///   system to add the database to.
/// @param dbString The name of the database to add.
///
/// @return Returns true on success, false on failure.
bool mariaDbAddDatabase(SqlDatabase *database, const char *dbName) {
  printLog(TRACE,
    "ENTER mariaDbAddDatabase(database=%p, dbName=\"%s\")\n",
    database, dbName);
  
  char *query = NULL;
  DbResult *queryResult = NULL;
  if (asprintf(&query, "create database %s%s;", dbName,
    (dbInstance) ? dbInstance : "") <= 0
  ) {
    LOG_MALLOC_FAILURE();
    printLog(TRACE,
      "EXIT mariaDbAddDatabase(database=%p, dbName=\"%s\") = {NOT successful}\n",
      database, dbName);
    return false;
  }
  queryResult = mariaDbExecQueryString((MariaDb*) database->connection, query);
  query = stringDestroy(query);
  bool returnValue = queryResult->successful;
  queryResult = dbFreeResult(queryResult);
  
  printLog(TRACE,
    "EXIT mariaDbAddDatabase(database=%p, dbName=\"%s\") = {%s}\n",
    database, dbName, (returnValue) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn bool mariaDbDeleteDatabase(SqlDatabase *database, const char *dbString)
///
/// @brief Delete a database from a MariaDB server.
///
/// @param database A pointer to the SqlDatabase object representing the database
///   system to delete the database from.
/// @param dbString The name of the database to delete.
///
/// @return Returns true on success, false on failure.
bool mariaDbDeleteDatabase(SqlDatabase *database, const char *dbName) {
  printLog(TRACE,
    "ENTER mariaDbDeleteDatabase(database=%p, dbName=\"%s\")\n",
    database, dbName);
  
  char *query = NULL;
  DbResult *queryResult = NULL;
  if (asprintf(&query, "drop database %s%s;", dbName,
    (dbInstance) ? dbInstance : "") <= 0
  ) {
    LOG_MALLOC_FAILURE();
    printLog(TRACE,
      "EXIT mariaDbDeleteDatabase(database=%p, dbName=\"%s\") = {NOT successful}\n",
      database, dbName);
    return false;
  }
  queryResult = mariaDbExecQueryString((MariaDb*) database->connection, query);
  query = stringDestroy(query);
  bool returnValue = queryResult->successful;
  queryResult = dbFreeResult(queryResult);
  
  printLog(TRACE,
    "EXIT mariaDbDeleteDatabase(database=%p, dbName=\"%s\") = {%s}\n",
    database, dbName, (returnValue) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn Bytes mariaDbMakeBytesLiteral(const Bytes input)
///
/// @brief Converts a Bytes object with unwanted characters to its escaped
///   equivalent.
///
/// @param input The Bytes object to convert.
///
/// @return Returns a newly-allocated Bytes object that has been escaped on
/// success, NULL on failure.
Bytes mariaDbMakeBytesLiteral(const Bytes input) {
  Bytes returnValue = NULL;
  
  printLog(FLOOD, "ENTER mariaDbMakeBytesLiteral(input=\"%s\")\n",
    (input != NULL) ? (char*) input : "");
  
  if (input != NULL) {
    u64 length = bytesLength(input);
    // Minimum length is the input length plus two ' characters.
    bytesAllocate(&returnValue, length + 2);
    bytesAddStr(&returnValue, "'");
    ZEROINIT(Byte escapedBuffer[sizeof(BytesHeader) + 3]);
    Bytes escapedValue = escapedBuffer + sizeof(BytesHeader);
    escapedValue[2] = '\0'; // To be compliant with a true Bytes object.
    for (u64 i = 0; i < length ; i++) {
      if ((input[i] == '\'')
        || (input[i] == '\\')
        || (input[i] == '"')
        || (input[i] == '\0')
      ) {
        escapedValue[0] = '\\';
        escapedValue[1] = input[i];
        bytesSetLength(escapedValue, 2);
      } else {
        escapedValue[0] = input[i];
        bytesSetLength(escapedValue, 1);
      }
      bytesAddBytes(&returnValue, escapedValue);
    }
    bytesAddStr(&returnValue, "'");
  } else {
    bytesAddStr(&returnValue, "NULL");
  }
  
  printLog(FLOOD, "EXIT mariaDbMakeBytesLiteral(input=\"%s\") = {%s}\n",
    (input != NULL) ? (char*) input : "", returnValue);
  return returnValue;
}

/// @fn Bytes mariaDbMakeStringLiteral(const char *input)
///
/// @brief Converts a stirng with unwanted characters to its escaped equivalent.
///
/// @param input The string to convert.
///
/// @return Returns a newly-allocated Bytes object that has been escaped on
/// success, NULL on failure.
Bytes mariaDbMakeStringLiteral(const char *input) {
  Bytes returnValue = NULL;
  
  printLog(FLOOD, "ENTER mariaDbMakeStringLiteral(input=\"%s\")\n",
    (input != NULL) ? input : "");
  
  if (input != NULL) {
    u64 length = (u64) strlen(input);
    // Minimum length is the input length plus two ' characters.
    bytesAllocate(&returnValue, length + 2);
    bytesAddStr(&returnValue, "'");
    ZEROINIT(Byte escapedBuffer[sizeof(BytesHeader) + 2]);
    Bytes escapedValue = escapedBuffer + sizeof(BytesHeader);
    bytesSetLength(escapedValue, 1); // The default and expected case.
    for (u64 i = 0; i < length ; i++) {
      if ((input[i] != '\'')
        && (input[i] != '\\')
        && (input[i] != '"')
        // Can't escape '\0' since that's the string terminator.
      ) {
        escapedValue[0] = input[i];
        // length is already set to 1
        bytesAddBytes(&returnValue, escapedValue);
      } else {
        escapedValue[0] = '\\';
        escapedValue[1] = input[i];
        bytesSetLength(escapedValue, 2);
        bytesAddBytes(&returnValue, escapedValue);
        bytesSetLength(escapedValue, 1);
      }
    }
    bytesAddStr(&returnValue, "'");
  } else {
    bytesAddStr(&returnValue, "NULL");
  }
  
  printLog(FLOOD, "EXIT mariaDbMakeStringLiteral(input=\"%s\") = {%s}\n",
    (input != NULL) ? input : "", (char*) returnValue);
  return returnValue;
}

/// @fn bool mariaDbStartTransaction(SqlDatabase *database)
///
/// @brief Start a transaction for this thread in the database.
///
/// @note Transactions are managed by connection on the SQL server side and
/// connections are managed by thread on our side, so this transaction will
/// only apply to this thread.
///
/// @param database A pointer to the SqlDatabase object representing the database
///   system to query.
///
/// @return Returns true if starting the transaction was successful, false
/// if not.
bool mariaDbStartTransaction(SqlDatabase *database) {
  printLog(TRACE, "ENTER mariaDbStartTransaction(database=%p)\n", database);
  
  if (database == NULL) {
    // Nothing to do.
    printLog(TRACE,
      "EXIT mariaDbStartTransaction(database=%p) = {NOT successful}\n",
      database);
    return false;
  }
  
  bool querySuccessful = true;
  MariaDb *mariaDb = (MariaDb*) database->connection;
  if (tssEqual(mariaDb->transactionCount, 0)) {
    tss_set(mariaDb->transactionInProgress, VOID_POINTER_TRUE);
    DbResult *queryResult
      = database->stringQuery(database->connection, "start transaction;");
    querySuccessful = queryResult->successful;
    queryResult = dbFreeResult(queryResult);
    if (querySuccessful) {
      tssInc(mariaDb->transactionCount);
    } else {
      printLog(ERR, "Could not start MariaDB transaction.\n");
      
      Socket *dbClientSocket = (Socket*) tss_get(_threadClientSocket);
      tss_set(mariaDb->transactionInProgress, VOID_POINTER_FALSE);
      releaseDbClientSocket(mariaDb, dbClientSocket);
    }
  } else {
    tss_set(mariaDb->transactionInProgress, VOID_POINTER_TRUE);
    tssInc(mariaDb->transactionCount);
  }
  
  printLog(TRACE, "EXIT mariaDbStartTransaction(database=%p) = {%s}\n",
    database, (querySuccessful) ? "successful" : "NOT successful");
  return querySuccessful;
}

/// @fn SqlDatabase* mariaDbDisconnect(SqlDatabase *sqlDatabase)
///
/// @brief Disconnect from a MariaDB connection.
///
/// @param sqlDatabase A pointer to a SqlDatabase object that hosts a pointer to
///   a MariaDb object.
///
/// @return Returns NULL on success, the original unmodified sqlDatabase pointer
/// on failure.
SqlDatabase* mariaDbDisconnect(SqlDatabase *sqlDatabase) {
  printLog(TRACE, "ENTER mariaDbDisconnect(sqlDatabase=%p)\n", sqlDatabase);
  
  if (sqlDatabase == NULL) {
    // No-op.
    printLog(TRACE,
      "EXIT mariaDbDisconnect(sqlDatabase=%p) = {successful}\n",
      sqlDatabase);
    return sqlDatabase;
  } else if (sqlDatabase->sqlDbType != MARIADB) {
    printLog(ERR,
      "sqlDatabase->connection member is not a MariaDB connection.\n");
    printLog(TRACE,
      "EXIT mariaDbDisconnect(sqlDatabase=%p) = {NOT successful}\n",
      sqlDatabase);
    return sqlDatabase;
  }
  
  MariaDb *mariaDb = (MariaDb*) sqlDatabase->connection;
  if (mariaDb != NULL) {
    if (mariaDb->availableDbSockets != NULL) {
      // Disconnect all the sockets.
      Queue *socketQueue = mariaDb->availableDbSockets;
      mtx_lock(_inUseMariaDbObjects->lock);
      for (HashNode *cur = _inUseMariaDbObjects->head; cur != NULL;) {
        void *key = cur->key;
        void *value = (void*) cur->value;
        cur = cur->next;
        if (value == socketQueue) {
          htRemoveEntry(_inUseMariaDbObjects, key);
        }
      }
      mtx_unlock(_inUseMariaDbObjects->lock);
      for (QueueNode *cur = socketQueue->head; cur != NULL; cur = cur->next) {
        Socket *socket = (Socket*) cur->value;
        cur->value = socketDestroy(socket);
        if (cur->value != NULL) {
          printLog(ERR, "Could not destroy socket connection.\n");
          printLog(TRACE,
            "EXIT mariaDbDisconnect(sqlDatabase=%p) = {NOT successful}\n",
            sqlDatabase);
          return sqlDatabase;
        }
      }
      mariaDb->availableDbSockets = queueDestroy(mariaDb->availableDbSockets);
    }
    
    // Free the MariaDb object.
    if (_defaultHostAddress == mariaDb->remoteHostAddress) {
      _defaultHostAddress = NULL;
    }
    mariaDb->remoteHostAddress = stringDestroy(mariaDb->remoteHostAddress);
    mariaDb->username = stringDestroy(mariaDb->username);
    mariaDb->password = stringDestroy(mariaDb->password);
    mtx_destroy(&mariaDb->lock);
    cnd_destroy(&mariaDb->socketAvailable);
    tss_delete(mariaDb->transactionInProgress);
    tss_delete(mariaDb->tablesLocked);
    tss_delete(mariaDb->transactionCount);
    mariaDb = (MariaDb*) pointerDestroy(mariaDb);
  }
  
  sqlDatabase->tableDescriptions = htDestroy(sqlDatabase->tableDescriptions);
  
  // Free the SqlDatabase object.
  sqlDatabase = (SqlDatabase*) pointerDestroy(sqlDatabase);
  
  printLog(TRACE,
    "EXIT mariaDbDisconnect(sqlDatabase=%p) = {successful}\n",
    sqlDatabase);
  return sqlDatabase;
}

/// @fn char *sha1ToHexString(const unsigned char *sha)
///
/// @param sha is the SHA1 digest to convert.
///
/// @return Returns a hexadecimal-encoded string representation of the sha.
char *sha1ToHexString(const unsigned char *sha) {
  printLog(TRACE, "ENTER sha1ToHexString(sha=%p)\n", sha);
  
  char *returnValue = NULL;
  for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
    char hexValue[3];
    sprintf(hexValue, "%02x", sha[i]);
    straddstr(&returnValue, hexValue);
  }
  
  printLog(TRACE, "EXIT sha1ToHexString(sha=%p) = {%p}\n",
    sha, returnValue);
  return returnValue;
}

/// @fn char *hexStringToSha1(const char *hexString)
///
/// @param hexString is the string to convert.
///
/// @return Returns an array of SHA_DIGEST_LENGTH characters representing the
/// decoded SHA.
unsigned char *hexStringToSha1(const char *hexString) {
  printLog(TRACE, "ENTER hexStringToSha1(hexString=\"%s\")\n", hexString);
  
  unsigned char *returnValue = (unsigned char*) malloc(SHA_DIGEST_LENGTH);
  if (returnValue == NULL) {
    printLog(ERR, "Could not allocate memory.\n");
    printLog(TRACE, "EXIT hexStringToSha1(hexString=\"%s\") = {%p}\n",
      hexString, (void*) NULL);
    return NULL;
  }
  
  for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
    if ((hexString[i << 1] == '\0') || (hexString[(i << 1) + 1] == '\0')) {
      printLog(ERR, "Premature termination of input hexString.\n");
      return returnValue;
    }
    unsigned int value = 0;
    sscanf(&hexString[i << 1], "%02x", &value);
    returnValue[i] = (unsigned char) value;
  }
  
  printLog(TRACE, "EXIT hexStringToSha1(hexString=\"%s\") = {%p}\n",
    hexString, returnValue);
  return returnValue;
}

/// @fn DbResult* mariaDbDescribeTable(MariaDb *database, const char *dbString, const char *tableName)
///
/// @brief Describe a table in a SQLite database.
///
/// @param database A pointer to the MariaDb object representing the database
///   system to query.
/// @param dbString The name of the database to query.
/// @param tableName The name of the table to describe.
///
/// @return Returns a DbResult object with the names of the table fields and
/// their types.
DbResult* mariaDbDescribeTable(MariaDb *database, const char *dbString,
  const char *tableName
) {
  printLog(TRACE,
    "ENTER mariaDbDescribeTable(database=%p, dbString=%s, tableName=%s)\n",
    database, dbString, tableName);
  
  char *dbName = NULL;
  straddstr(&dbName, dbString);
  straddstr(&dbName, dbInstance);
  Bytes query = NULL;
  abprintf(&query, "describe %s.%s;", dbName, tableName);
  dbName = stringDestroy(dbName);
  DbResult *queryResult = mariaDbExecQueryBytes(database, query);
  query = bytesDestroy(query);
  
  DbResult *returnValue = (DbResult*) calloc(1, sizeof(DbResult));
  if (returnValue == NULL) {
    LOG_MALLOC_FAILURE();
    exit(1);
  }
  if (queryResult->successful == true) {
    returnValue->successful = true;
    
    returnValue->numFields = 3;
    returnValue->fieldTypes
      = (TypeDescriptor**) calloc(1, 3 * sizeof(TypeDescriptor*));
    returnValue->fieldTypes[0] = typeBytes;
    returnValue->fieldTypes[1] = typeBytes;
    returnValue->fieldTypes[2] = typeBytes;
    
    returnValue->numRows = queryResult->numRows;
    returnValue->numResults = queryResult->numResults;
    returnValue->rows
      = (void***) calloc(1, (returnValue->numRows + 1) * sizeof(void**));
    
    returnValue->rows[0]
      = (void**) calloc(1, returnValue->numFields * sizeof(void*));
    bytesAddStr((Bytes*) &returnValue->rows[0][0], "fieldName");
    bytesAddStr((Bytes*) &returnValue->rows[0][1], "typeInfo");
    bytesAddStr((Bytes*) &returnValue->rows[0][2], "primaryKey");
    
    for (u64 row = 1; row < returnValue->numRows; row++) {
      returnValue->rows[row]
        = (void**) calloc(1, returnValue->numFields * sizeof(void*));
      bytesAddBytes((Bytes*) &returnValue->rows[row][0],
        dbGetBytesByName(queryResult, row - 1, "Field"));
      bytesAddBytes((Bytes*) &returnValue->rows[row][1],
        dbGetBytesByName(queryResult, row - 1, "Type"));
      if (!strstr(dbGetStringByName(queryResult, row - 1, "Key"), "PRI")) {
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
    "EXIT mariaDbDescribeTable(database=%p, dbString=%s, tableName=%s) = {%s}\n",
    database, dbString, tableName,
    (returnValue->successful == true) ? "successful" : "NOT successful");
  return returnValue;
}

/// @fn int mariaDbCompare(void *db1, void *db2)
///
/// @brief Compare two MariaDb instances to see if they refer to the same
/// underlying instance.
///
/// @param db1 A pointer to the first MariaDb instance, cast to a void pointer.
/// @param db2 A pointer to the second MariaDb instance, cast to a void pointer.
///
/// @return Returns 0 if the two MariaDb instances refer to the same MariaDb
/// connection, non-zero otherwise.
int mariaDbCompare(void *db1, void *db2) {
  SCOPE_ENTER("db1=%p, db2=%p", db1, db2);
  
  int returnValue = 0; // Equal until proven different.
  MariaDb *mariaDb1 = (MariaDb*) db1;
  MariaDb *mariaDb2 = (MariaDb*) db2;
  
  printLog(DEBUG, "mariaDb1->remoteHostAddress = \"%s\"\n",
    mariaDb1->remoteHostAddress);
  printLog(DEBUG, "mariaDb2->remoteHostAddress = \"%s\"\n",
    mariaDb2->remoteHostAddress);
  
  returnValue
    = strcmp(mariaDb1->remoteHostAddress, mariaDb2->remoteHostAddress);
  
  SCOPE_EXIT("db1=%p, db2=%p", "%d", db1, db2, returnValue);
  return returnValue;
}

/// @fn bool mariaDbChangeFieldType(void *db, const char *dbString, const char *tableName, const char *fieldName, const void *type)
///
/// @brief Add a new field to an existing table in a database.
///
/// @param db A pointer to the SqlDatabase object representing the database
///   system to query cast to a void pointer.
/// @param dbString The name of the database in the SqlDatabase to address.
/// @param tableName The name of the table to add a field to.
/// @param fieldName The name of the new field to change.
/// @param type A TypeDescriptor defining the new type of the field
///   -OR- a character pointer holding the number of characters for the field.
///
/// @return Returns true if changing the type of the field was successful,
/// false if not.
bool mariaDbChangeFieldType(void *db, const char *dbString,
  const char *tableName, const char *fieldName, const void *type
) {
  SqlDatabase *database = (SqlDatabase*) db;
  
  printLog(TRACE,
    "ENTER mariaDbChangeFieldType(database=%p, dbString=\"%s\", tableName=\"%s\", "
    "fieldName=\"%s\", type=%p)\n",
    database, dbString, tableName, fieldName, type);
  
  if ((database == NULL) || (dbString == NULL) || (tableName == NULL)
    || (fieldName == NULL) || (*fieldName == '\0') || (type == NULL)
  ) {
    // Nothing to do.
    printLog(TRACE,
      "EXIT mariaDbChangeFieldType(database=%p, dbString=\"%s\", tableName=\"%s\", "
      "fieldName=\"%s\", type=%p) = {NOT successful}\n",
      database, dbString, tableName, fieldName, type);
    return false;
  }
  
  char *typeName = (char*) typeDescriptorToSqlTypeName((TypeDescriptor*) type);
  bool typeNameAllocated = false;
  if (typeName == NULL) {
    // type is non-NULL and not a type.  It is a string holding the number of
    // characters for the field.
    straddstr(&typeName, "VARCHAR(");
    straddstr(&typeName, (char*) type);
    straddstr(&typeName, ")");
    typeNameAllocated = true;
  }
  
  Bytes dbName = NULL;
  bytesAddStr(&dbName, dbString);
  bytesAddStr(&dbName, dbInstance);
  
  Bytes query = NULL;
  // Change the column's type.
  abprintf(&query, "alter table %s.%s modify %s %s;",
    str(dbName), tableName, fieldName, typeName);
  if (typeNameAllocated) {
    typeName = stringDestroy(typeName);
  }
  DbResult *queryResult = database->bytesQuery(database->connection, query);
  bool querySuccessful = queryResult->successful;
  queryResult = dbFreeResult(queryResult);
  query = bytesDestroy(query);
  
  dbName = bytesDestroy(dbName);
  
  // Invalidate the cache.
  sqlInvalidateTableDescription(database, dbString, tableName);
  
  printLog(TRACE,
    "EXIT mariaDbChangeFieldType(database=%p, dbString=\"%s\", tableName=\"%s\", "
    "fieldName=\"%s\", type=%p) = {%s}\n",
    database, dbString, tableName, fieldName, type,
    (querySuccessful) ? "successful" : "NOT successful");
  return querySuccessful;
}

/// @fn i64 mariaDbGetSize(void *db, const char *dbName)
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
i64 mariaDbGetSize(void *db, const char *dbName) {
  SCOPE_ENTER("db=%p, dbName=\"%s\"", db, strOrNull(dbName));
  
  i64 returnValue = -1;
  Bytes query = NULL;
  abprintf(&query, "SELECT table_schema AS 'Database', SUM(data_length + "
    "index_length) AS 'Size' FROM information_schema.TABLES where table_schema "
    "= '%s%s';", dbName, strOrEmpty(dbInstance));
  scopeAdd(query, bytesDestroy);
  
  SqlDatabase *database = (SqlDatabase*) db;
  DbResult *queryResult = database->bytesQuery(database->connection, query);
  scopeAdd(queryResult, dbFreeResult);
  if (queryResult->successful == true) {
    const char *dbSize = dbGetStringByName(queryResult, 0, "Size");
    if (dbSize != NULL) {
      returnValue = (i64) strtoll(dbSize, NULL, 10);
    } else {
      printLog(ERR,
        "Query to get size of MariaDb database \"%s\" returned no Size.\n",
        dbName);
      // returnValue remains -1 to indicate an error.
    }
  }
  
  SCOPE_EXIT("db=%p, dbName=\"%s\"", "%lld", db, strOrNull(dbName),
    lld(returnValue));
  return returnValue;
}

/// @fn bool mariaDbRenameDatabase(void *db, const char *oldDbName, const char *newDbName)
///
/// @brief Rename an existing database.
///
/// @param db A pointer to a SqlDatabase object cast to a void*.
/// @param oldDbName The name of the database as it exists at the time the call
///   to this function as made.
/// @param newDbName The desired name of the database when this call completes.
///
/// @return Returns true on success, false on failure.
bool mariaDbRenameDatabase(void *db,
  const char *oldDbName, const char *newDbName
) {
  SCOPE_ENTER("db=%p, oldDbName=\"%s\", newDbName=\"%s\"",
    db, strOrNull(oldDbName), strOrNull(newDbName));
  
  bool returnValue = false;
  SqlDatabase *database = (SqlDatabase*) db;
  
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
  
  // Make sure the old database exists and the new database does NOT exist
  // before doing anything else.
  DbResult *existingDatabases = mariaDbGetDatabaseNames(database);
  if (existingDatabases == NULL) {
    printLog(ERR, "Could not get names of existing databases from database.\n");
    SCOPE_EXIT("db=%p, oldDbName=\"%s\", newDbName=\"%s\"", "%s",
      db, strOrNull(oldDbName), strOrNull(newDbName), boolNames[returnValue]);
    return returnValue;
  }
  scopeAdd(existingDatabases, dbFreeResult);
  // returnValue is currently false.
  for (u64 ii = 0; ii < existingDatabases->numResults; ii++) {
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
  // returnValue is currently true.
  for (u64 ii = 0; ii < existingDatabases->numResults; ii++) {
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
  
  // Algorithm:
  // 1. Create new empty database with the new name.
  // 2. RENAME all the tables in the old database to be in the new database.
  // 3. Delete the old database.
  
  returnValue = mariaDbAddDatabase(database, newDbName);
  if (returnValue == false) {
    printLog(ERR, "Could not create new database \"%s\".\n", newDbName);
    printLog(ERR, "Cannot rename old database \"%s\".\n", oldDbName);
    SCOPE_EXIT("db=%p, oldDbName=\"%s\", newDbName=\"%s\"", "%s",
      db, strOrNull(oldDbName), strOrNull(newDbName), boolNames[returnValue]);
    return returnValue;
  }
  
  DbResult *oldTables = mariaDbGetTableNames(database, oldDbName);
  if (oldTables == NULL) {
    returnValue = false;
    printLog(ERR, "Could not get list of tables from database \"%s\".\n",
      oldDbName);
    SCOPE_EXIT("db=%p, oldDbName=\"%s\", newDbName=\"%s\"", "%s",
      db, strOrNull(oldDbName), strOrNull(newDbName), boolNames[returnValue]);
    return returnValue;
  }
  scopeAdd(oldTables, dbFreeResult);
  
  const char *tableName = NULL;
  Bytes query = NULL;
  DbResult *queryResult = NULL;
  bool successful = false;
  for (u64 ii = 0; ii < oldTables->numResults; ii++) {
    tableName = dbGetStringByIndex(oldTables, ii, 0);
    abprintf(&query, "RENAME TABLE %s.%s TO %s.%s;",
      oldDbNameWithInstance, tableName, newDbNameWithInstance, tableName);
    queryResult = database->bytesQuery(database->connection, query);
    query = bytesDestroy(query);
    successful = queryResult->successful;
    queryResult = dbFreeResult(queryResult);
    if (successful == false) {
      returnValue = false;
      printLog(ERR, "Could not rename %s.%s to %s.%s.\n",
        oldDbNameWithInstance, tableName, newDbNameWithInstance, tableName);
      printLog(ERR, "Undoing partial rename.\n");
      
      for (u64 jj = ii - 1; jj != ((u64) -1); jj--) {
        tableName = dbGetStringByIndex(oldTables, jj, 0);
        abprintf(&query, "RENAME TABLE %s.%s TO %s.%s;",
          newDbNameWithInstance, tableName, oldDbNameWithInstance, tableName);
        queryResult = database->bytesQuery(database->connection, query);
        query = bytesDestroy(query);
        successful = queryResult->successful;
        queryResult = dbFreeResult(queryResult);
        if (successful == false) {
          printLog(ERR, "Could not rename %s.%s back to %s.%s.\n",
            newDbNameWithInstance, tableName, oldDbNameWithInstance, tableName);
          printLog(ERR, "Aborting operation.\n");
          printLog(ERR, "Manual database maintenance needed.\n");
          SCOPE_EXIT("db=%p, oldDbName=\"%s\", newDbName=\"%s\"", "%s",
            db, strOrNull(oldDbName), strOrNull(newDbName),
            boolNames[returnValue]);
          return returnValue;
        }
      }
      
      if (!mariaDbDeleteDatabase(database, newDbName)) {
        printLog(ERR, "Could not delete new database \"%s\".\n", newDbName);
      }
      
      SCOPE_EXIT("db=%p, oldDbName=\"%s\", newDbName=\"%s\"", "%s",
        db, strOrNull(oldDbName), strOrNull(newDbName), boolNames[returnValue]);
      return returnValue;
    }
  }
  
  if (!mariaDbDeleteDatabase(database, oldDbName)) {
    printLog(WARN, "Could not delete old database \"%s\".\n", oldDbName);
  }
  
  SCOPE_EXIT("db=%p, oldDbName=\"%s\", newDbName=\"%s\"", "%s",
    db, strOrNull(oldDbName), strOrNull(newDbName), boolNames[returnValue]);
  return returnValue;
}

/// @fn bool mariaDbCommitTransaction(SqlDatabase *database)
///
/// @brief Commit an in-progress transaction for this thread in the database.
///
/// @note Transactions are managed by connection on the SQL server side and
/// connections are managed by thread on our side, so this commit will
/// only apply to transactions in progress on this thread.
///
/// @param database A pointer to the SqlDatabase object representing the database
///   system to query.
///
/// @return Returns true if committing the transaction was successful, false
/// if not.
bool mariaDbCommitTransaction(SqlDatabase *database) {
  printLog(TRACE, "ENTER mariaDbCommitTransaction(database=%p)\n", database);
  
  if (database == NULL) {
    // Nothing to do.
    printLog(TRACE,
      "EXIT mariaDbCommitTransaction(database=%p) = {NOT successful}\n",
      database);
    return false;
  }
  
  bool querySuccessful = true;
  MariaDb *mariaDb = (MariaDb*) database->connection;
  tssDec(mariaDb->transactionCount);
  if (tssEqual(mariaDb->transactionCount, 0)) {
    DbResult *queryResult = database->stringQuery(database->connection, "commit;");
    querySuccessful = queryResult->successful;
    queryResult = dbFreeResult(queryResult);
    
    Socket *dbClientSocket = (Socket*) tss_get(_threadClientSocket);
    tss_set(mariaDb->tablesLocked, VOID_POINTER_FALSE);
    tss_set(mariaDb->transactionInProgress, VOID_POINTER_FALSE);
    releaseDbClientSocket(mariaDb, dbClientSocket);
  }
  
  printLog(TRACE, "EXIT mariaDbCommitTransaction(database=%p) = {%s}\n",
    database, (querySuccessful) ? "successful" : "NOT successful");
  return querySuccessful;
}

/// @fn bool mariaDbRollbackTransaction(SqlDatabase *database)
///
/// @brief Rollback an in-progress transaction for this thread in the database.
///
/// @note Transactions are managed by connection on the SQL server side and
/// connections are managed by threads on our side, so this rollback will
/// only apply to transactions in progress on this thread.
///
/// @param database A pointer to the SqlDatabase object representing the database
///   system to query.
///
/// @return Returns true if rolling back the transaction was successful, false
/// if not.
bool mariaDbRollbackTransaction(SqlDatabase *database) {
  printLog(TRACE, "ENTER mariaDbRollbackTransaction(database=%p)\n", database);
  
  if (database == NULL) {
    // Nothing to do.
    printLog(TRACE,
      "EXIT mariaDbRollbackTransaction(database=%p) = {NOT successful}\n",
      database);
    return false;
  }
  
  bool querySuccessful = true;
  MariaDb *mariaDb = (MariaDb*) database->connection;
  tssDec(mariaDb->transactionCount);
  if (tssEqual(mariaDb->transactionCount, 0)) {
    DbResult *queryResult = database->stringQuery(database->connection, "rollback;");
    querySuccessful = queryResult->successful;
    queryResult = dbFreeResult(queryResult);
    
    Socket *dbClientSocket = (Socket*) tss_get(_threadClientSocket);
    tss_set(mariaDb->tablesLocked, VOID_POINTER_FALSE);
    tss_set(mariaDb->transactionInProgress, VOID_POINTER_FALSE);
    releaseDbClientSocket(mariaDb, dbClientSocket);
  }
  
  printLog(TRACE, "EXIT mariaDbRollbackTransaction(database=%p) = {%s}\n",
    database, (querySuccessful) ? "successful" : "NOT successful");
  return querySuccessful;
}

/// @fn bool mariaDbLockTablesDict(SqlDatabase *database, const Dictionary *tablesToLock)
///
/// @brief Get a write lock in the database on the specified tables.
///
/// @param database A pointer to the SqlDatabase object representing the database
///   system to query.
/// @param tablesToLock A Dictionary (HashTable) where the keys are the names of
///   the tables to lock.  Values are ignored.  This function does *NOT* take
///   ownership of this data structure.  The caller must destroy it.
///
/// @return Returns true if the tables were locked successfully, false if not.
bool mariaDbLockTablesDict(SqlDatabase *database, const Dictionary *tablesToLock) {
  printLog(TRACE,
    "ENTER mariaDbLockTablesDict(database=%p, tablesToLock=%p)\n",
    database, tablesToLock);
  
  // Argument check.
  if ((database == NULL) || (tablesToLock == NULL)) {
    // Can't work with a NULL argument.
    printLog(TRACE,
      "EXIT mariaDbLockTablesDict(database=%p, tablesToLock=%p) = {unsuccessful}\n",
      database, tablesToLock);
    return false;
  } else if (tablesToLock->size == 0) {
    // Not an error per se, but nothing to do.
    printLog(WARN, "Zero-length Dictionary provided to mariaDbLockTablesDict.\n");
    printLog(TRACE,
      "EXIT mariaDbLockTablesDict(database=%p, tablesToLock=%p) = {successful}\n",
      database, tablesToLock);
    return false;
  }
  
  bool querySuccessful = true;
  MariaDb *mariaDb = (MariaDb*) database->connection;
  if (tssEqual(mariaDb->transactionCount, 0)) {
    tss_set(mariaDb->tablesLocked, VOID_POINTER_TRUE);
    DbResult *queryResult
      = database->stringQuery(database->connection, "start transaction;");
    querySuccessful = queryResult->successful;
    queryResult = dbFreeResult(queryResult);
    if (querySuccessful) {
      tssInc(mariaDb->transactionCount);
    } else {
      printLog(ERR, "Could not start transaction for table lock.\n");
    
      Socket *dbClientSocket = (Socket*) tss_get(_threadClientSocket);
      tss_set(mariaDb->tablesLocked, VOID_POINTER_FALSE);
      releaseDbClientSocket(mariaDb, dbClientSocket);
    }
  } else {
    tss_set(mariaDb->tablesLocked, VOID_POINTER_TRUE);
    tssInc(mariaDb->transactionCount);
  }

  if (!querySuccessful) {
    // Nothing else we can do here.
    // NOTE:  If lockedTables is not NULL, it will remain in it's current
    // state.  This query should never fail, so the system is already in
    // a bad state and keeping lockedTables as-is is desirable in this case.
    printLog(TRACE,
      "EXIT mariaDbLockTablesDict(database=%p, tablesToLock=%p) = {unsuccessful}\n",
      database, tablesToLock);
    return false;
  }
  
  printLog(TRACE,
    "EXIT mariaDbLockTablesDict(database=%p, tablesToLock=%p) = {successful}\n",
    database, tablesToLock);
  return querySuccessful;
}

/// @fn bool mariaDbUnlockTables(SqlDatabase *database, const Dictionary *tableLock)
///
/// @brief Unlock previously-locked tables and clear the cache of locked tables
/// for this thread.
///
/// @param database A pointer to the SqlDatabase object representing the database
///   system to query.
/// @param tableLock A pointer to a Dictionary returned by a previous call to
///   one of the sqlLockTables functions.
///
/// @return This function always returns NULL.
bool mariaDbUnlockTables(SqlDatabase *database, const Dictionary *tableLock) {
  printLog(TRACE,
    "ENTER mariaDbUnlockTables(database=%p, tableLock=%p)\n",
    database, tableLock);
  
  if ((database == NULL) || (tableLock == NULL)) {
    // Nothing to do.
    printLog(TRACE,
      "EXIT mariaDbUnlockTables(database=%p, tableLock=%p) = {NOT successful}\n",
      database, tableLock);
    return false;
  }
  
  bool querySuccessful = true;
  MariaDb *mariaDb = (MariaDb*) database->connection;
  tssDec(mariaDb->transactionCount);
  if (tssEqual(mariaDb->transactionCount, 0)) {
    DbResult *queryResult = database->stringQuery(database->connection, "commit;");
    querySuccessful = queryResult->successful;
    queryResult = dbFreeResult(queryResult);
    
    Socket *dbClientSocket = (Socket*) tss_get(_threadClientSocket);
    tss_set(mariaDb->tablesLocked, VOID_POINTER_FALSE);
    tss_set(mariaDb->transactionInProgress, VOID_POINTER_FALSE);
    releaseDbClientSocket(mariaDb, dbClientSocket);
  }
  
  printLog(TRACE, "EXIT mariaDbUnlockTables(database=%p, tableLock=%p) = {%s}\n",
    database, tableLock,
    (querySuccessful == true) ? "successful" : "NOT successful");
  return querySuccessful;
}

// Taken from https://github.com/CTrabant/teeny-sha1/blob/main/teeny-sha1.c

/*******************************************************************************
 * Teeny SHA-1
 *
 * The below sha1digest() calculates a SHA-1 hash value for a
 * specified data buffer and generates a hex representation of the
 * result.  This implementation is a re-forming of the SHA-1 code at
 * https://github.com/jinqiangshou/EncryptionLibrary.
 *
 * Copyright (c) 2017 CTrabant
 *
 * License: MIT, see included LICENSE file for details.
 *
 * To use the sha1digest() function either copy it into an existing
 * project source code file or include this file in a project and put
 * the declaration (example below) in the sources files where needed.
 ******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Declaration:
extern int sha1digest(uint8_t *digest, char *hexdigest, const uint8_t *data, size_t databytes);
*/

/*******************************************************************************
 * sha1digest: https://github.com/CTrabant/teeny-sha1
 *
 * Calculate the SHA-1 value for supplied data buffer and generate a
 * text representation in hexadecimal.
 *
 * Based on https://github.com/jinqiangshou/EncryptionLibrary, credit
 * goes to @jinqiangshou, all new bugs are mine.
 *
 * @input:
 *    data      -- data to be hashed
 *    databytes -- bytes in data buffer to be hashed
 *
 * @output:
 *    digest    -- the result, MUST be at least 20 bytes
 *    hexdigest -- the result in hex, MUST be at least 41 bytes
 *
 * At least one of the output buffers must be supplied.  The other, if not 
 * desired, may be set to NULL.
 *
 * @return: 0 on success and non-zero on error.
 ******************************************************************************/
int
sha1digest(uint8_t *digest, char *hexdigest, const uint8_t *data, size_t databytes)
{
#define SHA1ROTATELEFT(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

  uint32_t W[80];
  uint32_t H[] = {0x67452301,
                  0xEFCDAB89,
                  0x98BADCFE,
                  0x10325476,
                  0xC3D2E1F0};
  uint32_t a;
  uint32_t b;
  uint32_t c;
  uint32_t d;
  uint32_t e;
  uint32_t f = 0;
  uint32_t k = 0;

  uint32_t idx;
  uint32_t lidx;
  uint32_t widx;
  uint32_t didx = 0;

  int32_t wcount;
  uint32_t temp;
  uint64_t databits = ((uint64_t)databytes) * 8;
  uint32_t loopcount = (databytes + 8) / 64 + 1;
  uint32_t tailbytes = 64 * loopcount - databytes;
  uint8_t datatail[128] = {0};

  if (!digest && !hexdigest)
    return -1;

  if (!data)
    return -1;

  /* Pre-processing of data tail (includes padding to fill out 512-bit chunk):
     Add bit '1' to end of message (big-endian)
     Add 64-bit message length in bits at very end (big-endian) */
  datatail[0] = 0x80;
  datatail[tailbytes - 8] = (uint8_t) (databits >> 56 & 0xFF);
  datatail[tailbytes - 7] = (uint8_t) (databits >> 48 & 0xFF);
  datatail[tailbytes - 6] = (uint8_t) (databits >> 40 & 0xFF);
  datatail[tailbytes - 5] = (uint8_t) (databits >> 32 & 0xFF);
  datatail[tailbytes - 4] = (uint8_t) (databits >> 24 & 0xFF);
  datatail[tailbytes - 3] = (uint8_t) (databits >> 16 & 0xFF);
  datatail[tailbytes - 2] = (uint8_t) (databits >> 8 & 0xFF);
  datatail[tailbytes - 1] = (uint8_t) (databits >> 0 & 0xFF);

  /* Process each 512-bit chunk */
  for (lidx = 0; lidx < loopcount; lidx++)
  {
    /* Compute all elements in W */
    memset (W, 0, 80 * sizeof (uint32_t));

    /* Break 512-bit chunk into sixteen 32-bit, big endian words */
    for (widx = 0; widx <= 15; widx++)
    {
      wcount = 24;

      /* Copy byte-per byte from specified buffer */
      while (didx < databytes && wcount >= 0)
      {
        W[widx] += (((uint32_t)data[didx]) << wcount);
        didx++;
        wcount -= 8;
      }
      /* Fill out W with padding as needed */
      while (wcount >= 0)
      {
        W[widx] += (((uint32_t)datatail[didx - databytes]) << wcount);
        didx++;
        wcount -= 8;
      }
    }

    /* Extend the sixteen 32-bit words into eighty 32-bit words, with potential optimization from:
       "Improving the Performance of the Secure Hash Algorithm (SHA-1)" by Max Locktyukhin */
    for (widx = 16; widx <= 31; widx++)
    {
      W[widx] = SHA1ROTATELEFT ((W[widx - 3] ^ W[widx - 8] ^ W[widx - 14] ^ W[widx - 16]), 1);
    }
    for (widx = 32; widx <= 79; widx++)
    {
      W[widx] = SHA1ROTATELEFT ((W[widx - 6] ^ W[widx - 16] ^ W[widx - 28] ^ W[widx - 32]), 2);
    }

    /* Main loop */
    a = H[0];
    b = H[1];
    c = H[2];
    d = H[3];
    e = H[4];

    for (idx = 0; idx <= 79; idx++)
    {
      if (idx <= 19)
      {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999;
      }
      else if (idx >= 20 && idx <= 39)
      {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      }
      else if (idx >= 40 && idx <= 59)
      {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      }
      else if (idx >= 60 && idx <= 79)
      {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }
      temp = SHA1ROTATELEFT (a, 5) + f + e + k + W[idx];
      e = d;
      d = c;
      c = SHA1ROTATELEFT (b, 30);
      b = a;
      a = temp;
    }

    H[0] += a;
    H[1] += b;
    H[2] += c;
    H[3] += d;
    H[4] += e;
  }

  /* Store binary digest in supplied buffer */
  if (digest)
  {
    for (idx = 0; idx < 5; idx++)
    {
      digest[idx * 4 + 0] = (uint8_t) (H[idx] >> 24);
      digest[idx * 4 + 1] = (uint8_t) (H[idx] >> 16);
      digest[idx * 4 + 2] = (uint8_t) (H[idx] >> 8);
      digest[idx * 4 + 3] = (uint8_t) (H[idx]);
    }
  }

  /* Store hex version of digest in supplied buffer */
  if (hexdigest)
  {
    snprintf (hexdigest, 41, "%08x%08x%08x%08x%08x",
              H[0],H[1],H[2],H[3],H[4]);
  }

  return 0;
}  /* End of sha1digest() */
