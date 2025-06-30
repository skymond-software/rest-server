///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// @date              02.12.2017
///
/// @file              WebServerLib.h
///
/// @brief             This is the library of functions specific to the web
///                    server.
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

#ifndef WEB_SERVER_LIB_H
#define WEB_SERVER_LIB_H

#include <fcntl.h>

#include "DirectoryLib.h"
#include "Sockets.h"
#include "CThreads.h"
#include "StringLib.h"
#include "Dictionary.h"
#include "List.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef WsResponseObject
#error Type for WsResponseObject *MUST* be defined.
#endif

#ifndef WsRequestObject
#error Type for WsRequestObject *MUST* be defined.
#endif

#ifndef WsResponseNode
#error Type for WsResponseNode *MUST* be defined.
#endif

#ifndef WsRequestNode
#error Type for WsRequestNode *MUST* be defined.
#endif

// Forward declaration to avoid circular dependency.
typedef struct WebService WebService;
typedef struct WsConnectionInfo WsConnectionInfo;

typedef WsResponseObject* (*WsFunction)(WebService*, WsConnectionInfo*);

/// @struct WsFunctionDescriptor
///
/// @brief Node to associate a web-service function with a specified name
///   string.
///
/// @param name is the C-string name of the function.
/// @param pointer is the pointer to the function.
typedef struct WsFunctionDescriptor {
  const char *name;
  WsFunction pointer;
} WsFunctionDescriptor;

/// @struct WsNamespace
///
/// @brief Node to associate arrays of WsFunctionDescriptors with a namespace.
///
/// @param name The name of the namespace.
/// @param functionDescriptors A two-dimensional array of WsFunctionDescriptors.
///   The first dimension is terminated by a NULL pointer and the second
///   dimension is termined by a WsFunctionDescriptor with a NULL name and
///   pointer.
typedef struct WsNamespace {
  const char            *name;
  WsFunctionDescriptor **functionDescriptors;
} WsNamespace;

typedef int (*WsCookiesHandler)(Dictionary *cookiesDict);
typedef int (*WsRequestObjectHandler)(WsRequestObject *inputParameters);
typedef Bytes (*WsSerializeToXml)(const char *methodName, WsResponseObject *kvList, const char *commandType);
typedef WsRequestObject* (*WsRequestObjectCreate)(TypeDescriptor *type, bool disableThreadSafety, ...);
typedef WsRequestObject* (*WsDeserializeFromXml)(const char*);
typedef Bytes (*WsSerializeToJson)(WsResponseObject *responseObject);
typedef WsRequestObject* (*WsDeserializeFromJson)(const char *jsonText, long long int *position);
typedef WsRequestObject* (*WsRequestObjectDestroy)(WsRequestObject*);
typedef WsResponseObject* (*WsResponseObjectDestroy)(WsResponseObject*);
typedef void* (*WsGetRequestValue)(const WsRequestObject *inputParameters, const volatile void *key);
typedef void* (*WsGetResponseValue)(const WsResponseObject *inputParameters, const volatile void *key);
typedef void (*WsRegisterThread)(void);
typedef void (*WsUnregisterThread)(void *arg);
typedef WsRequestNode* (*WsAddRequestValue)(WsRequestObject *response,
  const volatile void *key, const volatile void *value,
  TypeDescriptor *type, ...);
typedef WsResponseNode* (*WsAddResponseValue)(WsResponseObject **response,
  const char *key, const volatile void *value, TypeDescriptor *type, ...);
typedef int (*WsRemoveResponseValue)(WsResponseObject *response, const volatile void *key);
typedef char* (*WsRequestObjectToString)(const WsRequestObject*);
typedef char* (*WsResponseObjectToString)(const WsResponseObject*);

/// @struct WebService
///
/// @brief Object to describe an individual web serivice.  This is to be passed
/// to wsCreate and becomes part of the WebServer object.
///
/// @param namespaces A one-dimensional array of WsNamespaces.  The array is
///   terminated by a WsNamespace with a NULL name and functionDescriptors.
/// @param cookiesHandler A WsCookiesHandler function pointer to run when the
///   cookies in an HTTP header have been parsed.
/// @param requestObjectHandler A WsRequestObjectHandler function pointer
///   to run prior to making the call to the web service function.
/// @param serializeToXml A WsSerializeToXml function pointer to call to convert a
///   WsResponseObject returned by the web service function into an XML string
///   that can be sent over HTTP.
/// @param deserializeFromXml A WsDeserializeFromXml function pointer to call to
///   convert the XML body of an HTTP request into a WsRequestObject that can be
///   passed to a web service function.
/// @param wsRequestObjectCreate The constructor to use for a WsRequestObject.
/// @param serializeToJson A WsSerializeToXml function pointer to call to convert a
///   WsResponseObject returned by the web service function into an JSON string
///   that can be sent over HTTP.
/// @param deserializeFromJson A WsDeserializeFromXml function pointer to call to
///   convert the JSON body of an HTTP request into a WsRequestObject that can
///   be passed to a web service function.
/// @param requestObjectDestroy A WsRequestObjectDestroy function pointer to
///   call to destroy a WsRequestObject.
/// @param responseObjectDestroy A WsResponseObjectDestroy function pointer to
///   call to destroy a WsResponseObject.
/// @param getRequestValue A WsGetRequestValue function pointer for a web
///   service function to call to get an individual value from a
///   WsRequestObject.
/// @param getResponseValue A WsGetResponseValue function pointer for this
///   library to call to get an individual value from a WsResponseObject.
/// @param registerThread A WsRegisterThread function pointer to call to
///   register a thread handling a new connection from a client.
/// @param unregisterThread A WsUnregisterThread function pointer to call to
///   unregister an exiting thread that was handling a connection from a client.
/// @param _addRequestValue A WsAddRequestValue function pointer for a web
///   service function to call to add a value to a WsRequestObject.
/// @def addRequestValue A wrapper around _wsAddRequestValue that provides
///   a type of string if none is specified.
/// @param _addResponseValue A WsAddResponseValue function pointer for a web
///   service function to call to add a value to a WsResponseObject.
/// @def addResponseValue A wrapper around _wsAddResponseValue that provides
///   a type of string if none is specified.
/// @param removeResponseValue A WsRemoveResponseValue function pointer that
///   will remove a value from a WsResponseObject.
/// @param requestObjectToString A WsRequestObjectToString function pointer
///   that converts a WsRequestObject to a string.
/// @param responseObjectToString A WsResponseObjectToString function pointer
///   that converts a WsResponseObject to a string.
/// @param context Any web service-specific information to be provided to calls.
typedef struct WebService {
  WsNamespace              *namespaces;
  WsCookiesHandler          cookiesHandler;
  WsRequestObjectHandler    requestObjectHandler;
  WsSerializeToXml          serializeToXml;
  WsDeserializeFromXml      deserializeFromXml;
  WsRequestObjectCreate     _wsRequestObjectCreate;
  #define wsRequestObjectCreate(keyType, ...) \
    _wsRequestObjectCreate(keyType, ##__VA_ARGS__, 0)
  WsSerializeToJson         serializeToJson;
  WsDeserializeFromJson     deserializeFromJson;
  WsRequestObjectDestroy    requestObjectDestroy;
  WsResponseObjectDestroy   responseObjectDestroy;
  WsGetRequestValue         getRequestValue;
  WsGetResponseValue        getResponseValue;
  WsRegisterThread          registerThread;
  WsUnregisterThread        unregisterThread;
  WsAddRequestValue         _addRequestValue;
  #define addRequestValue(_response_, _key_, _value_, ...) \
    _addRequestValue(_response_, _key_, _value_, ##__VA_ARGS__, typeString)
  WsAddResponseValue        _addResponseValue;
  #define addResponseValue(_response_, _key_, _value_, ...) \
    _addResponseValue(_response_, _key_, _value_, ##__VA_ARGS__, typeString)
  WsRemoveResponseValue     removeResponseValue;
  WsRequestObjectToString   requestObjectToString;
  WsResponseObjectToString  responseObjectToString;
  void                     *context;
} WebService;

typedef Dictionary* (*RedirectFunction)(Socket *clientSocket,
  const char *interfacePath, Dictionary *httpParams, const unsigned char *body,
  Dictionary *cookiesDict);

/// @struct WebServer
///
/// @brief Structure to hold args that define a web server and initialzie the
///   wsInit function.
///
/// @param interfacePath The path to the static pages the web server is to serve
///   to web clients.
/// @param portNumber The port number to bind to.
/// @param serverName The name (and version, etc.) of the web server.
/// @param timeout The number of seconds to retry socket creation before giving
///   up.  A value of 0 means infinite timeout.
/// @param socketMode The mode of the listener socket for the server (TLS or
///   plaintext.
/// @param certificate The .pem-formatted certificate if the listener socket
///   is a TLS socket.
/// @param key The .pem-formatted private key if the listener socket is a TLS
///   socket.
/// @param redirectProtocol The protocl (http or https) to redirect to instead
///   of processing requests at this socket.
/// @param redirectPort The TCP port to redirect to instead of processing
///   requests at this socket.
/// @param redirectFunction A RedirectFunction that is used to dynamically
///   generate a location header.  Takes precedence over redirectProtocol and
///   redirectPort if present.
/// @param webService A populated WebService object that defines the web service
///   that is to run on this server, if any.
/// @param socket The Socket that is constructed by wsInit for this listener.
/// @param threadId The ID of the thread that's started for the server.
/// @param isRunning A Boolean to communicate from the web server thread to the
///   parent whether or not it is currently running.
/// @param exitNow A Boolean to communicate from the parent to the server thread
///  whether or not it should exit.
typedef struct WebServer {
  char             *interfacePath;
  int               portNumber;
  char             *serverName;
  int               timeout;
  SocketMode        socketMode;
  char             *certificate;
  char             *key;
  char             *redirectProtocol;
  int               redirectPort;
  RedirectFunction redirectFunction;
  WebService       *webService;
  Socket           *socket;
  thrd_t            threadId;
  bool              isRunning;
  bool              exitNow;
} WebServer;

/// @struct WsConnectionInfo
///
/// @brief Sturcture to hold information about an in-progress connection.  A
///   pointer to one of these structures is passed to a called WsFunction.
///
/// @param clientSocket The Socket to use to communicate with the client.
/// @param interfacePath The path to the root of the static content.
/// @param httpParams A Dictionary of the HTTP parameters parsed from the header
///   received.
/// @param body A pointer to the body of the request received.
/// @param functionParams A pointer to a WsRequestObject that contains the
///   parsed parameters for the function call (if any).
typedef struct WsConnectionInfo {
  Socket              *clientSocket;
  const char          *interfacePath;
  Dictionary          *httpParams;
  const unsigned char *body;
  WsRequestObject     *functionParams;
} WsConnectionInfo;

WebServer* webServerCreate(const char *interfacePath, int portNumber,
  const char *serverName, int timeout, SocketMode socketMode,
  const char *certificate, const char *key, const char *redirectProtocol,
  int redirectPort, RedirectFunction redirectFunction, WebService *webService
);
WebServer* webServerDestroy(WebServer *webServer);
const char *getMimeType(const char *fileExtension);


#ifdef __cplusplus
} // extern "C"
#endif

#endif // WEB_SERVER_LIB_H

