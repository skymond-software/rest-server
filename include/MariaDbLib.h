///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              04.17.2017
///
/// @file              MariaDbLib.h
///
/// @brief             This is the library of functions specific to MariaDb.
///                    Generic database functions are built on top of this
///                    library.
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

#ifndef MARIA_DB_LIB_H
#define MARIA_DB_LIB_H

#include <openssl/sha.h>
#include <fcntl.h>
#include "CThreads.h"
#include "DbInterface.h"
#include "Sockets.h"
#include "Queue.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum MysqlFieldType {
  MYSQL_TYPE_DECIMAL     = 0,
  MYSQL_TYPE_TINY        = 1,
  MYSQL_TYPE_SHORT       = 2,
  MYSQL_TYPE_LONG        = 3,
  MYSQL_TYPE_FLOAT       = 4,
  MYSQL_TYPE_DOUBLE      = 5,
  MYSQL_TYPE_NULL        = 6,
  MYSQL_TYPE_TIMESTAMP   = 7,
  MYSQL_TYPE_LONGLONG    = 8,
  MYSQL_TYPE_INT24       = 9,
  MYSQL_TYPE_DATE        = 10,
  MYSQL_TYPE_TIME        = 11,
  MYSQL_TYPE_DATETIME    = 12,
  MYSQL_TYPE_YEAR        = 13,
  MYSQL_TYPE_NEWDATE     = 14,
  MYSQL_TYPE_VARCHAR     = 15,
  MYSQL_TYPE_BIT         = 16,
  MYSQL_TYPE_TIMESTAMP2  = 17,
  MYSQL_TYPE_DATETIME2   = 18,
  MYSQL_TYPE_TIME2       = 19,
  MYSQL_TYPE_TYPED_ARRAY = 20,
  MYSQL_TYPE_JSON        = 245, // 21
  MYSQL_TYPE_NEWDECIMAL  = 246, // 22
  MYSQL_TYPE_ENUM        = 247, // 23
  MYSQL_TYPE_SET         = 248, // 24
  MYSQL_TYPE_TINY_BLOB   = 249, // 25
  MYSQL_TYPE_MEDIUM_BLOB = 250, // 26
  MYSQL_TYPE_LONG_BLOB   = 251, // 27
  MYSQL_TYPE_BLOB        = 252, // 28
  MYSQL_TYPE_VAR_STRING  = 253, // 29
  MYSQL_TYPE_STRING      = 254, // 30
  MYSQL_TYPE_GEOMETRY    = 255, // 31
  NUM_MYSQL_FIELD_TYPES  = 32
} MysqlFieldType;

typedef enum MysqlDataType {
  int0,
  int1,
  int2,
  int4,
  int7,
  int8,
  int11,
  float4,
  float8,
  lenStr
} MysqlDataType;

#define DB_PACKET_HEADER_LENGTH                                4
#define DB_SMALL_PACKET_SIZE     (512 - DB_PACKET_HEADER_LENGTH)

/// @def DB_QUERY_RESPONSE_TO_US
///
/// @brief Query respones timeout in microseconds (uS).
///
/// @details
/// According to this article:
/// https://blog.kernl.us/2019/10/wordpress-database-performance-showdown-mysql-vs-mariadb-vs-percona/
/// 100% of MariaDB queries complete within 2,000 milliseconds.  However, on a
/// severly resource constrained system with a large set of results, I have
/// seen queries take over 9 seconds to complete.  That's pretty outrageous,
/// but I can't argue with data.  To be on the safe side, I'm making the
/// timeout ten seconds.
#define DB_QUERY_RESPONSE_TO_US                          10000000 // ten seconds timeout

#define CLIENT_MYSQL                                            1
#define FOUND_ROWS                                              2
#define CONNECT_WITH_DB                                         8 //  One can specify db on connect
#define COMPRESS                                               32 //  Can use compression protocol
#define LOCAL_FILES                                           128 //  Can use LOAD DATA LOCAL
#define IGNORE_SPACE                                          256 //  Ignore spaces before '('
#define CLIENT_PROTOCOL_41                               (1 << 9) //  4.1 protocol
#define CLIENT_INTERACTIVE                              (1 << 10) //
#define CLIENT_SSL                                      (1 << 11) // Can use SSL
#define TRANSACTIONS                                    (1 << 12) //
#define SECURE_CONNECTION                               (1 << 13) // 4.1 authentication
#define MULTI_STATEMENTS                                (1 << 16) // Enable/disable multi-stmt support
#define MULTI_RESULTS                                   (1 << 17) // Enable/disable multi-results
#define PS_MULTI_RESULTS                                (1 << 18) // Enable/disable multi-results for PrepareStatement
#define PLUGIN_AUTH                                     (1 << 19) // Client supports plugin authentication
#define CONNECT_ATTRS                                   (1 << 20) // Client send connection attributes
#define PLUGIN_AUTH_LENENC_CLIENT_DATA                  (1 << 21) // Enable authentication response packet to be larger than 255 bytes
#define CLIENT_SESSION_TRACK                            (1 << 23) // Enable/disable session tracking in OK_Packet
#define CLIENT_DEPRECATE_EOF                            (1 << 24) // EOF_Packet deprecation :
                                                                  //   * OK_Packet replace EOF_Packet in end of Resulset when in text format
                                                                  //   * EOF_Packet between columns definition and resultsetRows is deleted
#define MARIADB_CLIENT_PROGRESS                         (1 << 32) // Client support progress indicator (since 10.2)
#define MARIADB_CLIENT_COM_MULTI                        (1 << 33) // Permit COM_MULTI protocol
#define MARIADB_CLIENT_STMT_BULK_OPERATIONS             (1 << 34) // Permit bulk insert

/// @def MAX_CONCURRENT_MARIADB_CONNECTIONS
///
/// @brief The maximum number of concurrent connections that a single MariaDb
///  object is allowed to make.
#define MAX_CONCURRENT_MARIADB_CONNECTIONS                     99

/// @enum MariaDbPasswordHashType
///
/// @brief Enum to determine what kind of hash (if any) a password is.
typedef enum MariaDbPasswordHashType {
  MARIADB_PASSWORD_PLAINTEXT = 0,
  MARIADB_PASSWORD_SHA1,
  MARIADB_PASSWORD_NUM_HASHES
} MariaDbPasswordHashType;

// Support data structures.
/// @struct MariaDb
///
/// @brief Object containing the things needed to maintain a connection to a
/// MariaDb server.
///
/// @param remoteHostAddress The "host:port" string representing the address of
///   the server.
/// @param username The username of the account to use for the connection.
/// @param password The password of the account to use for the connection.
/// @param passwordHashType MariaDbPasswordHashType value specifying what kind
///   of value is encoded in the password field.
/// @param availableDbSockets The pool of sockets to use for queries to the
///   database.  The queue enforces round-robin balancing.
/// @param lock An embedded mtx_t mutex to ensure mutual exclusion to an
///   instance of this object when necessary.
/// @param maxConnections The maximum number of connections that this client is
///   allowed to make to the MariaDB server.
// @param numConnections The current number of connections that have been made
///   to the MariaDB server.
/// @param socketAvailable A condition variable to allow for signalling when a
///   connection socket becomes available.
/// @param transactionInProgress Thread-specific storage to keep track of
///   whether or not a transaction is in progress on the current thread.
/// @param tablesLocked Thread-specific storage to keep track of whether or not
///   tables are locked by the current thread.
/// @param transactionCount The number of start transaction calls that have been
///   made without a corresponding call to rollback or commit.  This is thread-
///   specific storage, so counts are on a per-thread basis.
/// @param socketMetadata Dictionary with socket pointers as keys that serves as
///   a lookup mechanism for debug metadata for sockets used to connect to the
///   MariaDB server.
typedef struct MariaDb {
  char *remoteHostAddress;
  char *username;
  char *password;
  MariaDbPasswordHashType passwordHashType;
  Queue *availableDbSockets;
  mtx_t lock;
  u64 maxConnections;
  u64 numConnections;
  cnd_t socketAvailable;
  tss_t transactionInProgress;
  tss_t tablesLocked;
  tss_t transactionCount;
  Dictionary *socketMetadata;
} MariaDb;

// External API.
Database* mariaDbInit_(const char *ignored,
  const char *remoteHostAddress,
  const char *username, const char *password,
  int passwordHashType, ...
);
#define mariaDbInit(...) \
  mariaDbInit_(NULL, ##__VA_ARGS__, 0, 0, 0, 0)
unsigned char *generateMariaDbPasswordHash(const unsigned char *password,
  unsigned long passwordLength, const unsigned char *seed, u64 seedLength);
unsigned char *generateMariaDbPasswordHashFromSha(const unsigned char *sha,
  const unsigned char *seed, u64 seedLength);
char *sha1ToHexString(const unsigned char *sha);
unsigned char *hexStringToSha1(const char *hexString);
char *mariaDbGetStatistics(MariaDb *database);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // MARIA_DB_LIB_H

