////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                   Copyright (c) 2012-2024 Skymond, LLC.                    //
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

#include "WebServerLib.h"
// LoggingLib is not optional for this library.
#include "LoggingLib.h"
#include "HashTable.h"
#include "OsApi.h"

/// @struct WsThreadInfo
///
/// @brief Sturcture to hold information about a new connection.  A pointer to
///   one of these structures is passed to wsConnectionThread.
///
/// @param clientSocket The Socket to use to communicate with the client.
/// @param interfacePath The path to the root of the static content.
/// @param serverName The name of the web server as defined in the WebServer
///   object.
/// @param numRunningConnectionThreads A pointer to the number of currently
///   running connection threads for this server.
/// @param numRunningConnectionThreadsMutex A mutex to protect access to
///   numRunningConnectionThreads.
/// @param webService A copy of the WebService provided to wsCreate (if any).
///   Note that this is a complete copy instead of a copy of the pointer because
///   the expectation is that all memebers of the WebService will be needed.
///   Having the pointer here would force two levels of indirection any time one
///   of the members is needed instead of just one.
/// @param webServiceFunctions A HashTable of HashTables of web service
///   functions created from webService.functionDescriptors when wsInit is
///   called.
/// @param redirectProtocol The protocol that should be redirected to from this
///   connection (if any).
/// @param redirectPort The port that should be redirected to from this
///   connection (if any).
/// @param redirectFunction The RedirectFunction that should be used to
///   dynamically redirect from this connection (if any).
/// @param httpParams A Dictionary of the HTTP parameters parsed from the header
///   received.
/// @param cookiesDict A Dictionary of the parsed and adjusted cookies from the
///   HTTP header (if any).
/// @param body A pointer to the body of the request received.
typedef struct WsThreadInfo {
  Socket              *clientSocket;
  const char          *interfacePath;
  const char          *serverName;
  int                 *numRunningConnectionThreads;
  mtx_t               *numRunningConnectionThreadsMutex;
  WebService           webService;
  HashTable           *webServiceFunctions;
  char                *redirectProtocol;
  int                  redirectPort;
  RedirectFunction     redirectFunction;
  Dictionary          *httpParams;
  Dictionary          *cookiesDict;
  const unsigned char *body;
} WsThreadInfo;

/// @fn int wsMsleep(int milliseconds)
///
/// @brief Sleep for the specified number of milliseconds.
///
/// @param milliseconds The number of milliseconds to sleep.
///
/// @return Returns 0 on success, negative value on failure.
int wsMsleep(int milliseconds) {
  int returnValue = 0;
  
#ifdef _WIN32
  Sleep(milliseconds);
#else // POSIX
  struct timespec sleepTime = {0, milliseconds * 1000000};
  returnValue = nanosleep(&sleepTime, NULL);
#endif
  
  return returnValue;
}

/// @fn Bytes getServerDateHeader()
///
/// @brief Generate a proper Date header for an HTTP response.
///
/// @return Returns a newly-allocated Bytes object with the header on success,
/// NULL on failure (malloc failure).
Bytes getServerDateHeader() {
  printLog(TRACE, "ENTER getServerDateHeader()");
  
  tzset();
  time_t now = time(NULL);
  ZEROINIT(struct tm nowTm);
  gmtime_r(&now, &nowTm);

  const char *dateWeekdays[] = {
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat"
  };
  const char *dateMonths[] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
  };
  Bytes dateHeader = NULL;
  abprintf(&dateHeader, "%s, %d %s %d %d:%d:%d GMT\r\n",
    dateWeekdays[nowTm.tm_wday],
    nowTm.tm_mday,
    dateMonths[nowTm.tm_mon],
    nowTm.tm_year + 1900,
    nowTm.tm_hour,
    nowTm.tm_min,
    nowTm.tm_sec);

  printLog(TRACE, "EXIT getServerDateHeader() = {%s}", str(dateHeader));
  return dateHeader;
}

/// @fn bool redirectClient(WsThreadInfo *wsThreadInfo)
///
/// @brief Use the information in the wsThreadInfo to determine if we should
/// redirect the user to a page by the same name on a different port or
/// protocol.
///
/// @param wsThreadInfo The WsThreadInfo structure for the connection.
///
/// @return Returns true if the user was redirected, false if not.
bool redirectClient(WsThreadInfo *wsThreadInfo) {
  printLog(TRACE, "ENTER redirectClient(wsThreadInfo=%p)\n", wsThreadInfo);
  
  char *redirectProtocol = wsThreadInfo->redirectProtocol;
  int redirectPort = wsThreadInfo->redirectPort;
  RedirectFunction redirectFunction = wsThreadInfo->redirectFunction;
  if ((redirectProtocol == NULL) && (redirectPort == 0)
    && (redirectFunction == NULL)
  ) {
    // We're not supposed to be redirecting traffic on this protocol and port
    // elsewhere.
    printLog(TRACE,
      "EXIT redirectClient(wsThreadInfo=%p) = {%s}\n", wsThreadInfo, "false");
    return false;
  }
  
  // redirectFunction takes precedence over the other data.
  if (redirectFunction != NULL) {
    Dictionary *response = redirectFunction(wsThreadInfo->clientSocket,
      wsThreadInfo->interfacePath, wsThreadInfo->httpParams, wsThreadInfo->body,
      wsThreadInfo->cookiesDict);
    char *redirectUrl = (char*) dictionaryGetValue(response, "redirectUrl");
    if (redirectUrl == NULL) {
      printLog(DEBUG, "Supplied redirection function did not provide a "
        "redirectUrl return parameter.");
      response = dictionaryDestroy(response);
      printLog(TRACE,
        "EXIT redirectClient(wsThreadInfo=%p) = {%s}\n", wsThreadInfo, "false");
      return false;
    }
    
    Bytes location = NULL;
    abprintf(&location, "Location: %s\n\n", redirectUrl);
    
    if (location != NULL) {
      Bytes sendbuf = NULL;
      bytesAddStr(&sendbuf, "HTTP/1.1 301 Moved Permanently\n");
      bytesAddBytes(&sendbuf, location);
      
      Socket *clientSocket = wsThreadInfo->clientSocket;
      socketSend(clientSocket, sendbuf, bytesLength(sendbuf));
      sendbuf = bytesDestroy(sendbuf);
    }
    location = bytesDestroy(location);
    
    response = dictionaryDestroy(response);
    return true;
  }
  
  Dictionary *parameters = wsThreadInfo->httpParams;
  Bytes headerHost = NULL;
  bytesAddBytes(&headerHost, (Bytes) dictionaryGetValue(parameters, "Host"));
  if (headerHost == NULL) {
    // There's no Host in the header.  We can't proceed.
    printLog(TRACE,
      "EXIT redirectClient(wsThreadInfo=%p) = {%s}\n", wsThreadInfo, "false");
    return false;
  }
  
  if (redirectPort != 0) {
    // We're going to change the port.
    // We don't want the part of the host header after the colon.
    // Termiante the string there.
    char *portAt = strchr((char*) headerHost, ':');
    if (portAt != NULL) {
      *portAt = '\0';
    }
  }
  
  // Construct the host to redirect to.
  char *host = NULL;
  if ((redirectProtocol == NULL) && (redirectPort != 0)) {
    if (asprintf(&host, "%s:%d", (char*) headerHost, redirectPort) < 0) {
      host = NULL;
    }
  } else if ((redirectProtocol != NULL) && (redirectPort == 0)) {
    if (asprintf(&host, "%s://%s", redirectProtocol, (char*) headerHost) < 0) {
      host = NULL;
    }
  } else { // redirectProtocol != NULL && redirectPort != 0
    if (asprintf(&host, "%s://%s:%d", redirectProtocol, (char*) headerHost,
      redirectPort) < 0
    ) {
      host = NULL;
    }
  }
  headerHost = bytesDestroy(headerHost);
  
  char *httpLocation = (char*) dictionaryGetValue(parameters, "_httpLocation");
  if (httpLocation == NULL) {
    printLog(ERR, "Request to redirect to NULL location.\n");
    host = stringDestroy(host);
    // We have to return true here because the EXPECTED behavior is for us to
    // redirect, even though we don't have the ability to actually redirect.
    printLog(TRACE,
      "EXIT redirectClient(wsThreadInfo=%p) = {%s}\n", wsThreadInfo, "true");
    return true;
  }
  Bytes location = NULL;
  abprintf(&location, "Location: %s%s\n\n", host, httpLocation);
  host = stringDestroy(host);
  
  if (location != NULL) {
    Bytes sendbuf = NULL;
    bytesAddStr(&sendbuf, "HTTP/1.1 301 Moved Permanently\n");
    bytesAddBytes(&sendbuf, location);
    
    Socket *clientSocket = wsThreadInfo->clientSocket;
    socketSend(clientSocket, sendbuf, bytesLength(sendbuf));
    sendbuf = bytesDestroy(sendbuf);
  }
  location = bytesDestroy(location);
  
  printLog(TRACE,
    "EXIT redirectClient(wsThreadInfo=%p) = {%s}\n", wsThreadInfo, "true");
  return true;
}

/// @fn WsResponseObject* webServiceCall(WsThreadInfo *wsThreadInfo, const char *wsNamespace, const char *functionName, Dictionary *inputParams)
///
/// Go through the provided web services and if there's a match with the
/// provided wsNamespace and function name, call the function with the provided
/// arguments.
///
/// @param wsThreadInfo A pointer to the WsThreadInfo
///   structure provided to this thread.
/// @param wsNamespace A string representing the wsNamespace of the web service.
///   This corresponds to the key of the webServices list.
/// @param functionName A string representing the name of the function.  This
///   corresponds to the name in the FunctionDescriptors for the web service.
/// @param inputParams A Dictionary of arguments parsed from the client's
///   request.
///
/// @return Returns an allocated WsResponseObject if the wsNamespace and
/// functionName match the name of a registered web serivce function, NULL
/// otherwise.
WsResponseObject* webServiceCall(WsThreadInfo *wsThreadInfo,
  const char *wsNamespace, const char *functionName, Dictionary *inputParams
) {
  printLog(TRACE,
    "ENTER webServiceCall(wsThreadInfo=%p, wsNamespace=\"%s\", "
    "functionName=\"%s\", inputParams=%p)\n",
    wsThreadInfo, wsNamespace, functionName, inputParams);
  
  if ((wsThreadInfo == NULL) || (wsNamespace == NULL)
    || (functionName == NULL)
  ) {
    printLog(ERR, "One or more NULL parameters.  Cannot execute.\n");
    printLog(TRACE,
      "EXIT webServiceCall(wsThreadInfo=%p, wsNamespace=\"%s\", "
      "functionName=\"%s\", inputParams=%p) = {NULL}\n",
      wsThreadInfo, wsNamespace, functionName, inputParams);
    return NULL;
  }
  
  // Return value.  Needs to be declared here.
  WsResponseObject *outputParams = NULL;
  
  // Set the environment for the web service call.
  if (wsThreadInfo->webService.requestObjectHandler != NULL) {
    wsThreadInfo->webService.requestObjectHandler(inputParams);
  }
  
  if (wsThreadInfo->webServiceFunctions != NULL) {
    // Find the right set of function descriptors.
    HashTable *namespaceFunctions
      = (HashTable*) htGetValue(
        wsThreadInfo->webServiceFunctions, wsNamespace);
    if (namespaceFunctions != NULL) {
      // Find the right function.
      WsFunction wsFunction
        = (WsFunction) htGetValue(namespaceFunctions, functionName);
      if (wsFunction != NULL) {
        // Setup the parameters.
        WsConnectionInfo wsConnectionInfo;
        wsConnectionInfo.clientSocket   = wsThreadInfo->clientSocket;
        wsConnectionInfo.interfacePath  = wsThreadInfo->interfacePath;
        wsConnectionInfo.httpParams     = wsThreadInfo->httpParams;
        wsConnectionInfo.body           = wsThreadInfo->body;
        wsConnectionInfo.functionParams = inputParams;

        // Call the function.
        outputParams = wsFunction(&wsThreadInfo->webService, &wsConnectionInfo);
      }
    }
  }
  
  printLog(TRACE,
    "EXIT webServiceCall(wsThreadInfo=%p, wsNamespace=\"%s\", "
    "functionName=\"%s\", inputParams=%p) = {%p}\n",
    wsThreadInfo, wsNamespace, functionName, inputParams, outputParams);
  return outputParams;
}

/// @fn u64 sendBuffer(const Bytes buffer, Socket *clientSocket)
///
/// @brief Send a buffer to a client on a socket.
///
/// @param buffer A Bytes object containing the data to send.
/// @param clientSocket A pointer to a Socket to send the data on.
///
/// @returns the number of bytes remaining to be sent, so 0 on success and
/// a positive value on failure.
u64 sendBuffer(const Bytes buffer, Socket *clientSocket) {
  u64 bufferLength = bytesLength(buffer);
  int chunkSize = 0x7fffffff;
  int numBytesToSend = chunkSize;
  if (bufferLength < (u64) chunkSize) {
    numBytesToSend = (int) bufferLength;
  }
  Bytes bytesToSend = buffer;
  while (bufferLength > llu(0)) {
    int bytesSent = socketSend(clientSocket, bytesToSend, numBytesToSend);
    if (bytesSent <= 0) {
      printLog(ERR, "Client prematurely closed connection.\n");
      printLog(DEBUG, "clientSocket = %s\n", socketToString(clientSocket));
      printLog(DEBUG, "buffer = %p\n", buffer);
      printLog(DEBUG, "bufferLength = %llu\n", llu(bufferLength));
      break;
    }
    bufferLength -= (u64) bytesSent;
    numBytesToSend = chunkSize;
    if (bufferLength < (u64) chunkSize) {
      numBytesToSend = (int) bufferLength;
    }
    bytesToSend += bytesSent;
  }
  
  return bufferLength;
}

/// @fn int sendResponseToClient(const Bytes header, const Bytes body, Socket *clientSocket)
///
/// @brief Send a full response to the client.
///
/// @param header The HTTP header that has been generated up to this point.
///   Content-Type and Content-Length headers are expected to be part of this.
/// @param body The body to send.
/// @param clientSocket The Socket the client is connected at and the location
///   to send the full reponse to.
///
/// @return Returns 0 on success, any other value is failure.
int sendResponseToClient(const Bytes header, const Bytes body, Socket *clientSocket) {
  printLog(TRACE,
    "ENTER sendResponseToClient(header=%p, body=%p, clientSocket=%s)\n",
    header, body, socketToString(clientSocket));
  
  // Send the HTTP OK portion
  Bytes buffer = NULL;
  bytesAddStr(&buffer, "HTTP/1.1 200 OK\r\n");
  Bytes date = getServerDateHeader();
  bytesAddStr(&buffer, "Date: ");
  bytesAddBytes(&buffer, date);
  bytesAddStr(&buffer, "Vary: Accept-Encoding\r\n");
  /* TODO?: bytesAddStr(&buffer, "Content-Encoding: deflate\r\n"); */
  bytesAddStr(&buffer, "Connection: close\r\n");
  bytesAddStr(&buffer, "Cache-Control: no-store\r\n");
  // We don't intend to allow the client to cache these pages, so mark the
  // expiration time the current time.
  bytesAddStr(&buffer, "Expires: ");
  bytesAddBytes(&buffer, date);
  date = bytesDestroy(date);
  /* bytesAddStr(&buffer, "Content-Security-Policy: default-src 'self' "
   *   "'unsafe-eval' 'unsafe-inline' 'unsafe-hashes' http://www.w3.org;\r\n");
   */
  if (sendBuffer(buffer, clientSocket) > 0) {
    printLog(ERR, "Could not send OK buffer to client.\n");
    buffer = bytesDestroy(buffer);
    return -1;
  }
  buffer = bytesDestroy(buffer);
  
  if (sendBuffer(header, clientSocket) > 0) {
    printLog(ERR, "Could not send header buffer to client.\n");
    return -1;
  }
  
  bytesAddStr(&buffer, "\r\n");
  if (sendBuffer(buffer, clientSocket) > 0) {
    printLog(ERR, "Could not send header terminator buffer to client.\n");
    buffer = bytesDestroy(buffer);
    return -1;
  }
  buffer = bytesDestroy(buffer);
  
  if (sendBuffer(body, clientSocket) > 0) {
    printLog(ERR, "Could not send body buffer to client.\n");
    return -1;
  }
  
  printLog(TRACE,
    "EXIT sendResponseToClient(header=%p, body=%p, clientSocket=%s) = {%d}\n",
    header, body, socketToString(clientSocket), 0);
  return 0;
}

/// @fn int sendResponseObjectToClient(WsThreadInfo *wsThreadInfo, const char *functionName, WsResponseObject *outputParams)
///
/// @brief Send the contents of the provided WsResponseObject to the client.
///
/// @param wsThreadInfo A pointer to the WsThreadInfo
///   structure passed to this thread.
/// @param functionName The name of the function that generated the response
///   object.
/// @param outputParams The WsResponseObject generated by a call to a web
///   service function.
///
/// @return Returns 0 on success, any other value is failure.
int sendResponseObjectToClient(WsThreadInfo *wsThreadInfo,
  const char *functionName, WsResponseObject *outputParams
) {
  printLog(TRACE,
    "ENTER sendResponseObjectToClient(wsThreadInfo=%p, "
    "functionName=\"%s\", outputParams=%p)\n", wsThreadInfo,
    functionName, outputParams);
  
  Bytes header = NULL;
  Bytes body = NULL;
  
  if (wsThreadInfo->webService.getResponseValue(
    outputParams, "Content-Type") == NULL
  ) {
    // need to serialize outputParams for the response
    Bytes contentType
      = (Bytes) dictionaryGetValue(wsThreadInfo->httpParams, "Content-Type");
    const char *responseContentType = NULL;
    if ((contentType == NULL) || (strstr(str(contentType), "application/json"))) {
      body = wsThreadInfo->webService.serializeToJson(outputParams);
      responseContentType
        = "Content-Type: application/json; charset=utf-8\r\n";
    } else if (strstr(str(contentType), "text/xml")) {
      body = wsThreadInfo->webService.serializeToXml(
        functionName, outputParams, "Response");
      responseContentType
        = "Content-Type: application/soap+xml; charset=utf-8\r\n";
    } // else we have no parser for this body
    
    abprintf(&header, "Content-Length: %llu\r\n", llu(bytesLength(body)));
    bytesAddStr(&header, responseContentType);
  } else {
    // response is fully-defined in outputParams
    body = (Bytes) wsThreadInfo->webService.getResponseValue(
      outputParams, "body");
    abprintf(&header, "Content-Length: %llu\r\n", llu(bytesLength(body)));
    u32 bodyU32 = *((u32*) "body");
    
    // Fill the rest of the header parameters provided.
    for (
      WsResponseNode *node = outputParams->head;
      node != NULL;
      node = node->next
    ) {
      if (*((u32*) node->key) == bodyU32) {
        // This is the body node.  We've already accounted for that.  Skip it.
        // We do need to mark the value pointer to NULL, though, so that we
        // don't try to free it twice.
        node->value = NULL;
        continue;
      }
      
      bytesAddStr(&header, (char*) node->key);
      bytesAddStr(&header, ": ");
      bytesAddStr(&header, (char*) node->value);
      bytesAddStr(&header, "\r\n");
    }
  }
  
  bytesAddStr(&header, "Server: ");
  bytesAddStr(&header, wsThreadInfo->serverName);
  bytesAddStr(&header, "\r\n");
  int returnValue
    = sendResponseToClient(header, body, wsThreadInfo->clientSocket);
  header = bytesDestroy(header);
  body = bytesDestroy(body);
  
  printLog(TRACE,
    "EXIT sendResponseObjectToClient(wsThreadInfo=%p, "
    "functionName=\"%s\", outputParams=%p) = {%d}\n", wsThreadInfo,
    functionName, outputParams, returnValue);
  return returnValue;
}

/// @fn int handlePostRequest(WsThreadInfo *wsThreadInfo)
///
/// @brief Handle a POST request from a client.
///
/// @param wsThreadInfo The WsThreadInfo structure for the connection.
///
/// @return Returns 0 on success.  Any other value is an error.
int handlePostRequest(WsThreadInfo *wsThreadInfo) {
  printLog(TRACE, "ENTER handlePostRequest(wsThreadInfo=%p)\n", wsThreadInfo);
  
  int returnValue = 0;
  if (wsThreadInfo == NULL) {
    printLog(ERR, "NULL wsThreadInfo parameter provided.  Cannot continue.\n");
    returnValue = 1;
    printLog(TRACE,
      "EXIT handlePostRequest(wsThreadInfo=%p) = {%d}\n",
      wsThreadInfo, returnValue);
    return returnValue;
  }
  
  // We won't redirect a POST request if redirectPort or redirectProtocol are
  // set, however if redirectFunction is specified, we have to do as we're
  // told.
  if ((wsThreadInfo->redirectFunction) && (redirectClient(wsThreadInfo))) {
    // Nothing else for us to do.
    printLog(TRACE,
      "EXIT handlePostRequest(wsThreadInfo=%p) = {%d}\n",
      wsThreadInfo, 0);
    return returnValue; // 0
  }
  
  // There are a few possibilities for how the client could communicate the
  // desired operation.  It may provide the information in one of the header
  // fields or it may only provide it in the path (or it may provide both).
  // We will let the information in the header override the information in the
  // path if it's available.
  Bytes hostName = NULL;
  bytesAddBytes(&hostName,
    (Bytes) dictionaryGetValue(wsThreadInfo->httpParams, "Host"));
  if (hostName != NULL) {
    bytesAddStr(&hostName, "/"); // Trailing / we'll look for in the SOAPAction.
  }
  
  Bytes wsNamespace = NULL;
  Bytes functionName = NULL;
  Bytes soapAction
    = (Bytes) dictionaryGetValue(wsThreadInfo->httpParams, "SoapAction");
  if ((soapAction != NULL) && (hostName != NULL)) {
    printLog(DEBUG, "SOAP action detected.\n");
    // SoapAction was specified.  Use that to get the function name.
    // Depending on the client, this may come all on one line or it may come
    // with the wsNamespace on the top line and the function on the second.  Look
    // for the newline first.  If that fails, get the function name from the
    // last slash.
    printLog(DEBUG, "Getting part of \"%s\" between \"%s\" and \"%s\".\n",
      (char*) soapAction, (char*) hostName, "/");
    wsNamespace = getBytesBetweenBytes(soapAction, (char*) hostName, "/");
    if (wsNamespace != NULL) { // The expected case
      printLog(DEBUG, "wsNamespace = \"%s\".\n", wsNamespace);
      Bytes namespacePlusSlash = NULL;
      bytesAddBytes(&namespacePlusSlash, wsNamespace);
      bytesAddStr(&namespacePlusSlash, "/");
      if (soapAction[0] == '"') {
        // SOAP actions are supposed to be in double quotes, so this is the
        // expected case.
        functionName
          = getBytesBetweenBytes(soapAction, (char*) namespacePlusSlash, "\"");
      } else {
        // Copy to the end of the line.
        functionName
          = getBytesBetweenBytes(soapAction, (char*) namespacePlusSlash, "");
      }
      namespacePlusSlash = bytesDestroy(namespacePlusSlash);
    }
  }
  hostName = bytesDestroy(hostName);
  
  if ((wsNamespace == NULL) || (functionName == NULL)) {
    if (wsNamespace == NULL) {
      printLog(DEBUG, "wsNamespace is NULL.\n");
    }
    if (functionName == NULL) {
      printLog(DEBUG, "functionName is NULL.\n");
    }
    
    Bytes path
      = (Bytes) dictionaryGetValue(wsThreadInfo->httpParams, "_httpLocation");
    if (path == NULL) {
      // No SoapAction and no path.  Can't proceed.
      printLog(ERR, "Malformed HTTP header.\n");
      functionName = bytesDestroy(functionName);
      wsNamespace = bytesDestroy(wsNamespace);
      returnValue = 1;
      
      // Caller will do necessary cleanup.
      printLog(TRACE,
        "EXIT handlePostRequest(wsThreadInfo=%p) = {%d}\n", wsThreadInfo, 1);
      return returnValue;
    }
    
    // There is a bug in Internet Explorer which prevents reposting from
    // behaving correctly--the browser only sends the header on subsoquent
    // calls.  See http://support.microsoft.com/kb/895954 for more details.
    // As a work-around, the client will send a URL-encoded timestamp as a
    // "GET-style" parameter with each call.  This will force Internet Explorer
    // to treat each call as a new call but we need to make sure we ignore the
    // rest of the URL in this case.  Clients that handle things properly (like
    // Firefox and Python) won't send the extra data but that's OK.
    //
    // JBC 2012-06-09
    char *charAt = strchr((char*) path, '?');
    if (charAt != NULL) {
      *charAt = '\0';
    }
    
    if (wsNamespace == NULL) {
      wsNamespace = getBytesBetween((char*) path, "/", "/");
    }
    
    if (functionName == NULL) {
      // Pull it from the path.  Should be from last / to end of path.
      char *slashAt = strrchr((char*) path, '/');
      if (slashAt != NULL) {
        slashAt++; // Move to the beginning of the function name.
        bytesAddStr(&functionName, slashAt);
      }
    }
  }
  
  // Get the function parameters from the body of the request.
  // body will be guaranteed to be non-NULL because we succeeded at getting
  // httpHeader earlier.
  WsRequestObject *inputParams = NULL;
  Bytes contentType
    = (Bytes) dictionaryGetValue(wsThreadInfo->httpParams, "Content-Type");
  if (contentType == NULL) {
    printLog(ERR, "No Content-Type header provided.  Cannot parse input.\n");
    returnValue = -1;
    functionName = bytesDestroy(functionName);
    wsNamespace = bytesDestroy(wsNamespace);
    
    printLog(TRACE,
      "EXIT handlePostRequest(wsThreadInfo=%p) = {%d}\n",
      wsThreadInfo, returnValue);
    return returnValue;
  }
  if ((strstr(str(contentType), "text/xml") || strstr(str(contentType), "soap"))
    && (wsThreadInfo->webService.deserializeFromXml != NULL)
  ) {
    inputParams = wsThreadInfo->webService.deserializeFromXml(
      (const char*) wsThreadInfo->body);
  } else if ((strstr(str(contentType), "application/json"))
    && (wsThreadInfo->webService.deserializeFromJson != NULL)
  ) {
    long long int startPosition = 0;
    inputParams = wsThreadInfo->webService.deserializeFromJson(
      (const char*) wsThreadInfo->body, &startPosition);
  } // else we have no parser for this body
  
  // webServiceCall will handle NULL parameters, so no need to double-check.
  WsResponseObject *outputParams = NULL;
  outputParams = webServiceCall(
    wsThreadInfo, (char*) wsNamespace, (char*) functionName, inputParams);
  if (inputParams != NULL) {
    // We have to guard this because inputParams may be NULL because no
    // serialization/deserialization functions were specified.
    inputParams = wsThreadInfo->webService.requestObjectDestroy(inputParams);
  }
  
  if (outputParams != NULL) {
    // A return value of 0 from sendResponseObjectToClient is good status.
    // The same is true for this function.  However, this being a top-level
    // handler, we can only return zero or positive values to our caller.
    // We need to restrict our return value to reflect this.
    returnValue = (sendResponseObjectToClient(wsThreadInfo,
      (char*) functionName, outputParams) != 0);
    outputParams = wsThreadInfo->webService.responseObjectDestroy(outputParams);
  }
  
  functionName = bytesDestroy(functionName);
  wsNamespace = bytesDestroy(wsNamespace);
  
  printLog(TRACE,
    "EXIT handlePostRequest(wsThreadInfo=%p) = {%d}\n",
    wsThreadInfo, returnValue);
  return returnValue;
}

// Defined at the bottom of the library.
extern const char *_mimeTypes[][2];
extern const i64 NUM_MIME_TYPES;

/// @var _mimeHashTable
///
/// @brief File-local variable to hold the file extension-to-MIME type
/// information used for returning HTTP data.
static HashTable *_mimeHashTable = NULL;

/// @fn const char *getMimeType(const char *fileExtension)
///
/// @brief Get the appropriate MIME type for a file based on its file extension.
///
/// @prarm fileExtension The last '.' character of the file name and everything
///   after it.
///
/// @return Returns the appropriate MIME type.  If the file extension is not
/// found then the default of "text/plain" is returned.
const char *getMimeType(const char *fileExtension) {
  printLog(TRACE, "ENTER getMimeType(fileExtension=\"%s\")", fileExtension);
  
  const char *mimeType = NULL;
  if (fileExtension != NULL) {
    if (_mimeHashTable != NULL) {
      // The hash table is set up and we have a file extension.
      // This is the expected case.
      mimeType = (const char*) htGetValue(_mimeHashTable, fileExtension);
    } else {
      // Setting up the hash table failed for some reason.  Perform a
      // binary search.
      i64 searchIndex = NUM_MIME_TYPES >> 1; // divide by two
      for (i64 i = 0, j = NUM_MIME_TYPES; i <= j;) {
        int difference = strcmpci(fileExtension, _mimeTypes[searchIndex][0]);
        if (difference == 0) {
          mimeType = _mimeTypes[searchIndex][1];
          break;
        } else if (difference < 0) {
          // move left
          j = searchIndex - 1;
          if (j < 0) {
            // searchIndex is 0
            break;
          }
        } else {
          // move right
          i = searchIndex + 1;
          if (i == NUM_MIME_TYPES) {
            // searchIndex is NUM_MIME_TYPES - 1
            break;
          }
        }
        searchIndex = (i + j) >> 1; // divide by two
      }
    }
  }
  if (mimeType == NULL) {
    // Unknown file extension.  Default to text/plain.
    mimeType = "text/plain";
  }
  
  printLog(TRACE, "EXIT getMimeType(fileExtension=\"%s\") = {%s}",
    fileExtension, mimeType);
  return mimeType;
}

/// @fn Bytes wsGetFile(WsThreadInfo *wsThreadInfo, const char *path, const char *targetNamespace, Bytes *header)
///
/// @brief Get the contents of a file requested by the client.  Support function
/// for handleGetRequest.
///
/// @param wsThreadInfo The WsThreadInfo structure for the
///   connection.
/// @param path The path of the file (relative to
///   wsThreadInfo->interfacePath) to return.
/// @param targetNamespace The wsNamespace to use if the client is requesting
///   a WSDL or XSD.
/// @param header Output parameter containing the HTTP header for the file.
///
/// @return Returns a Bytes object of the contents of the file.
Bytes wsGetFile(WsThreadInfo *wsThreadInfo,
  const char *path, const char *targetNamespace, Bytes *header
) {
  char *fullPath = NULL;
  
  Bytes outputBytes = NULL;
  
  printLog(TRACE, "ENTER wsGetFile(path=\"%s\")\n", path);
  printLog(DETAIL, "Processing request to GET \"%s\".\n", path);
  
  if (strstr(path, "../")) {
    printLog(ERR, "Attempt to get relative path.\n");
    printLog(TRACE, "EXIT wsGetFile(path=\"%s\") = {%p}\n",
      path, outputBytes);
    return outputBytes;
  }
  
  // Correct the path so that we get the correct file.
  printLog(DEBUG, "interfacePath = \"%s\"\n",
    wsThreadInfo->interfacePath);
  straddstr(&fullPath, wsThreadInfo->interfacePath);
  printLog(DEBUG, "path = \"%s\"\n", path);
  printLog(DEBUG, "Got the base path.\n");
  straddstr(&fullPath, path);
  // See if we should really be returning index.html.
  if (path[strlen(path) - 1] == '/') {
    // Request was for a directory.
    straddstr(&fullPath, "index.html");
  }
  printLog(DEBUG, "Determined if path should be index.html.\n");
  printLog(DEBUG, "fullPath = \"%s\"\n", fullPath);
  FILE *fullPathFile = fopen(fullPath, "r");
  if (fullPathFile == NULL) {
    printLog(DEBUG, "Could not open \"%s\".\n", fullPath);
    // Try once more with "/index.html" appended.
    straddstr(&fullPath, "/index.html");
    printLog(DEBUG, "Attempting to open \"%s\".", fullPath);
    fullPathFile = fopen(fullPath, "r");
    if (fullPathFile == NULL) {
      // File not found on disk.  Return nothing.
      fullPath = stringDestroy(fullPath);
      printLog(ERR, "File not found.\n");
      printLog(TRACE, "EXIT wsGetFile(path=\"%s\") = {%p}\n",
        path, outputBytes);
      return outputBytes;
    } else {
      printLog(DEBUG, "Successfully opened \"%s\".", fullPath);
      // Reset fullPath for the code below.
      fullPath = stringDestroy(fullPath);
      straddstr(&fullPath, path);
    }
  }
  fclose(fullPathFile);
  printLog(DEBUG, "File check complete.\n");
  
  // Determine content type.
  char *fileExtension =  strrchr(fullPath, '.');
  const char *mimeType = getMimeType(fileExtension);
  bytesAddStr(header, "Content-type: ");
  bytesAddStr(header, mimeType);
  bytesAddStr(header, "\r\n");
  printLog(DEBUG, "Determined Content-type: %s\n", mimeType);
  
  // Get the file content.
  Bytes fileContent = getFileContent(fullPath);
  printLog(DEBUG, "Got file content for \"%s\".\n", fullPath);
  u64 fileLength = bytesLength(fileContent);
  printLog(DEBUG, "fileLength = %llu\n", llu(fileLength));
  bytesAddData(&fileContent, "", 1);
  printLog(DEBUG, "Added NUL terminator to fileContent.\n");
  
  // Adjust for WSDL material
  if ((fileExtension != NULL)
    && ((strcmp(fileExtension, ".xsd") == 0)
      || (strcmp(fileExtension, ".wsdl") == 0)
    )
  ) {
    Bytes newContent
      = bytesReplaceStr(fileContent, "<<TARGET_NAMESPACE>>",
      targetNamespace);
    if (fileContent != NULL) {
      fileContent = bytesDestroy(fileContent);
    }
    fileContent = newContent;
    if (fileContent != NULL) {
      fileLength = bytesLength(fileContent);
    } else {
      fileLength = 0;
    }
  }
  
  // Adjust for directory get
  if ((fileLength == 0) && (path[strlen(path) - 1] != '/')) {
    printLog(DEBUG, "Checking to see if %s is a directory.", path);
    fullPath = stringDestroy(fullPath);
    straddstr(&fullPath, wsThreadInfo->interfacePath);
    straddstr(&fullPath, path);
    straddstr(&fullPath, "/index.html");
    fullPathFile = fopen(fullPath, "r");
    if (fullPathFile != NULL) {
      fclose(fullPathFile);
      
      // Requested file is actually a directory.  Tell the client to request
      // this properly by adding a trailing '/' to the request.
      // fileContent was never initialized in this case so we're free to use it
      bytesAddStr(header, "Content-Type: text/html\r\n");
      
      bytesAddStr(&fileContent,
        " <html> <head> <meta http-equiv=\"refresh\" content=\"0;URL='");
      bytesAddStr(&fileContent, path);
      bytesAddStr(&fileContent, "/"); // Trailing '/' character on path
      bytesAddStr(&fileContent, "'\" /> </head> </html> ");
      fileLength = bytesLength(fileContent);
    }
  }
  
  Bytes contentLength = NULL;
  printLog(DEBUG, "Allocating contentLength.\n");
  abprintf(&contentLength, "Content-Length: %llu\r\n", llu(fileLength));
  printLog(DEBUG, "contentLength = \"%s\".\n", contentLength);
  
  // Create the output string.
  bytesAddBytes(header, contentLength);
  contentLength = bytesDestroy(contentLength);
  outputBytes = fileContent;
  fullPath = stringDestroy(fullPath);
  printLog(DEBUG, "outputBytes fully allocated.  Exiting.\n");
  
  printLog(TRACE, "EXIT wsGetFile(path=\"%s\") = {%p}\n", path, outputBytes);
  
  return outputBytes;
}

/// @fn int handleGetRequest(WsThreadInfo *wsThreadInfo, Bytes receiveBuffer)
///
/// @brief Handle a GET request from a client.
///
/// @param wsThreadInfo The WsThreadInfo structure for the connection.
///
/// @return Returns 0 on success.  Any other value is an error.
int handleGetRequest(WsThreadInfo *wsThreadInfo) {
  printLog(TRACE,
    "ENTER handleGetRequest(wsThreadInfo=%p)\n", wsThreadInfo);
  
  int returnValue = 0;
  if (wsThreadInfo == NULL) {
    printLog(ERR, "NULL wsThreadInfo parameter provided.  Cannot continue.\n");
    returnValue = 1;
    printLog(TRACE,
      "EXIT handlePostRequest(wsThreadInfo=%p) = {%d}\n",
      wsThreadInfo, returnValue);
    return returnValue;
  }
  
  // POST requests may be handled on any port they come in on, but GET requests
  // need to be redirected if we've been instructed to redirect.  See if we
  // should redirect the client.
  if (redirectClient(wsThreadInfo)) {
    // Nothing else for us to do.
    printLog(TRACE,
      "EXIT handleGetRequest(wsThreadInfo=%p) = {%d}\n",
      wsThreadInfo, 0);
    return returnValue; // 0
  }
  
  // There are two possibilities for a GET request:  The client may be
  // requesting a web service function or may be requesting a static file.
  // Web service functions will take precedence over static files of the same
  // name, so check for a web service first and then fall back to looking for
  // a static file if the web service lookup fails.
  
  // We need a copy of the path because we'll be unescaping it later, which
  // modifies the buffer in place.
  Bytes path = NULL;
  bytesAddBytes(&path,
    (Bytes) dictionaryGetValue(wsThreadInfo->httpParams, "_httpLocation"));
  if (path == NULL) {
    printLog(ERR, "No _httpLocation provided in handleGetRequest.\n");
    returnValue = 1;
    printLog(TRACE,
      "EXIT handleGetRequest(wsThreadInfo=%p) = {%d}\n",
      wsThreadInfo, 1);
    return returnValue;
  }
  
  Bytes wsNamespace = getBytesBetween((char*) path, "/", "/");
  if (wsNamespace != NULL) {
    // Check to see if there's a matching web service call.
    
    Bytes wsNamespacePlusSlash = NULL;
    bytesAddBytes(&wsNamespacePlusSlash, wsNamespace);
    bytesAddStr(&wsNamespacePlusSlash, "/");
    // Get everything after the wsNamespace
    Bytes functionAndArgsString
      = getBytesBetween((char*) path, (char*) wsNamespacePlusSlash, "");
    wsNamespacePlusSlash = bytesDestroy(wsNamespacePlusSlash);
    if (functionAndArgsString == NULL) {
      // Memory allocation failed.
      wsNamespace = bytesDestroy(wsNamespace);
      path = bytesDestroy(path);
      returnValue = 1;
      printLog(TRACE,
        "EXIT handleGetRequest(wsThreadInfo=%p) = {%d}\n",
        wsThreadInfo, 1);
      return returnValue;
    }
    
    // Separate the string into its function and its args
    Bytes *functionAndArgsArray
      = stringToBytesArray((char*) functionAndArgsString, "?");
    functionAndArgsString = bytesDestroy(functionAndArgsString);
    if (functionAndArgsArray == NULL) {
      // Memory allocation failed.
      wsNamespace = bytesDestroy(wsNamespace);
      path = bytesDestroy(path);
      returnValue = 1;
      LOG_MALLOC_FAILURE();
      printLog(TRACE,
        "EXIT handleGetRequest(wsThreadInfo=%p) = {%d}\n",
        wsThreadInfo, 1);
      return returnValue;
    }
    
    // functionAndArgsArray[0] is guaranteed to be non-NULL and
    // functionAndArgsArray[1] is guaranteed to be allocated, but that's it.
    // kvStringToDictionary handles NULL inputs.
    Bytes functionName = functionAndArgsArray[0];
    Dictionary *args
      = kvStringToDictionary((char*) functionAndArgsArray[1], "&");
    if (args == NULL) {
      // Memory allocation failed.
      functionAndArgsArray = freeBytesArray(functionAndArgsArray);
      wsNamespace = bytesDestroy(wsNamespace);
      path = bytesDestroy(path);
      returnValue = 1;
      LOG_MALLOC_FAILURE();
      printLog(TRACE,
        "EXIT handleGetRequest(wsThreadInfo=%p) = {%d}\n",
        wsThreadInfo, 1);
      return returnValue;
    }
    Dictionary *tempDict = NULL;
    // Prepend "GET:" to all parameter names.  This is to distinguish them from
    // POST parameters.  GET parameters can be faked by cross-site request
    // forgery while POST parameters cannot.  This will force the called method
    // to prepend "GET:" to the parameter names it requests so that the
    // developer is reminded about this (and hopefully, doesn't do any data
    // modifications while in a GET method).
    for (DictionaryEntry *node = args->head; node != NULL; node = node->next) {
      char *parameterName = NULL;
      straddstr(&parameterName, "GET:");
      straddstr(&parameterName, (char*) node->key);
      dictionaryAddEntry(&tempDict, parameterName, node->value);
      parameterName = stringDestroy(parameterName);
    }
    args = dictionaryDestroy(args);
    args = tempDict;
    
    WsResponseObject *outputParams = webServiceCall(
      wsThreadInfo, (char*) wsNamespace, (char*) functionName, args);
    args = dictionaryDestroy(args);
    
    if (outputParams != NULL) {
      // We had a web service match.  Return the parameters to the client and
      // exit.
      // A return value of 0 from sendResponseObjectToClient is good status.
      // The same is true for this function.  However, this being a top-level
      // handler, we can only return zero or positive values to our caller.
      // We need to restrict our return value to reflect this.
      returnValue = (sendResponseObjectToClient(wsThreadInfo,
        (char*) functionName, outputParams) != 0);
      outputParams = wsThreadInfo->webService.responseObjectDestroy(outputParams);
      functionAndArgsArray = freeBytesArray(functionAndArgsArray);
      functionName = NULL;
      outputParams = wsThreadInfo->webService.responseObjectDestroy(outputParams);
      wsNamespace = bytesDestroy(wsNamespace);
      path = bytesDestroy(path);
      
      printLog(TRACE,
        "EXIT handleGetRequest(wsThreadInfo=%p) = {%d}\n",
        wsThreadInfo, returnValue);
      return returnValue;
    }
    functionAndArgsArray = freeBytesArray(functionAndArgsArray);
    functionName = NULL;
  } // else this is a request for a file at the root level
  
  // If we made it this far then we need to see if there's a static file to get.
  char *charAt = strchr((char*) path, '?');
  if (charAt != NULL) {
    // We don't care about the rest of the URL.  Terminate the string.
    *charAt = '\0';
  }
  
  // Construct the target wsNamespace to use in the event the client is
  // requesting a WSDL or XSD.
  char *targetNamespace = NULL;
  if (wsNamespace != NULL) {
    Socket *clientSocket = wsThreadInfo->clientSocket;
    Bytes hostName
      = (Bytes) dictionaryGetValue(wsThreadInfo->httpParams, "Host");
    if (hostName != NULL) {
      Bytes *hostArray = stringToBytesArray((char*) hostName, ":");
      // hostArray[0] is guaranteed to be non-NULL
      // hostArray[1] is guaranteed to be allocated
      Bytes portString = hostArray[1];
      if (portString != NULL) {
        if (asprintf(&targetNamespace, "%s://%s:%s/%s",
          // clientSocket->socketMode can only be TLS if tlsSocketsEnabled() is
          // true, so no need to check that here.
          (clientSocket->socketMode == TLS) ? "https" :
          "http",
          (char*) hostArray[0], (char*) portString, (char*) wsNamespace) < 0
        ) {
          targetNamespace = NULL;
        }
      } else {
        if (asprintf(&targetNamespace, "%s://%s/%s",
          // clientSocket->socketMode can only be TLS if tlsSocketsEnabled() is
          // true, so no need to check that here.
          (clientSocket->socketMode == TLS)
          ? "https" :
          "http",
          (char*) hostName, (char*) wsNamespace) < 0
        ) {
          targetNamespace = NULL;
        }
      }
      hostArray = freeBytesArray(hostArray);
    }
    wsNamespace = bytesDestroy(wsNamespace);
  }
  
  unescapeString((char*) path);
  printLog(DEBUG, "Getting file \"%s\".\n", (char*) path);
  Bytes header = NULL;
  Bytes body
    = wsGetFile(wsThreadInfo, (char*) path, targetNamespace, &header);
  targetNamespace = stringDestroy(targetNamespace);
  path = bytesDestroy(path);
  bytesAddStr(&header, "Server: ");
  bytesAddStr(&header, wsThreadInfo->serverName);
  bytesAddStr(&header, "\r\n");
  // A return value of 0 from sendResponseObjectToClient is good status.
  // The same is true for this function.  However, this being a top-level
  // handler, we can only return zero or positive values to our caller.
  // We need to restrict our return value to reflect this.
  returnValue = (sendResponseToClient(header, body,
    wsThreadInfo->clientSocket) != 0);
  header = bytesDestroy(header);
  body = bytesDestroy(body);
  
  printLog(TRACE,
    "EXIT handleGetRequest(wsThreadInfo=%p) = {%d}\n",
    wsThreadInfo, returnValue);
  return returnValue;
}

/// @fn int parseCookies(WsThreadInfo *wsThreadInfo)
///
/// @brief Set the thread for this session if it's in the header's cookies.
///
/// @param wsThreadInfo The connection context for this thread.
///
/// @return Returns 0 on success, negative error code on failure.
int parseCookies(WsThreadInfo *wsThreadInfo) {
  printLog(TRACE, "ENTER parseCookies(wsThreadInfo=%p)\n", wsThreadInfo);
  
  if ((wsThreadInfo == NULL)
    || (wsThreadInfo->webService.cookiesHandler == NULL)
    || (wsThreadInfo->httpParams == NULL)
  ) {
    printLog(ERR, "One or more NULL parameters.  Cannot parse cookies.\n");
    printLog(TRACE,
      "EXIT parseCookies(wsThreadInfo=%p) = {%d}\n", wsThreadInfo, -1);
    return -1;
  }
  
  Bytes cookiesString =
    (Bytes) dictionaryGetValue(wsThreadInfo->httpParams, "Cookie");
  if (cookiesString == NULL) {
    // No cookies.  Can't get cookies.
    // Nothing to free.
    printLog(TRACE,
      "EXIT parseCookies(wsThreadInfo=%p) = {%d}\n", wsThreadInfo, -2);
    return -2;
  }
  
  Dictionary *cookiesDict = kvStringToDictionary((char*) cookiesString, "; ");
  if (cookiesDict == NULL) {
    // Memory allocation failed.  Can't get sessionToken.
    // cookiesString has already been freed.  Nothing else to free.
    printLog(TRACE,
      "EXIT parseCookies(wsThreadInfo=%p) = {%d}\n", wsThreadInfo, -3);
    return -3;
  }
  
  Bytes host
    = (Bytes) dictionaryGetValue(wsThreadInfo->httpParams, "Host");
  const char *protocol = 
    // wsThreadInfo->clientSocket->socketMode can only be TLS if
    // tlsSocketsEnabled() is true, so no need to check that here.
    (wsThreadInfo->clientSocket->socketMode == TLS) ? "https" :
    "http";

  Dictionary *tempDict = NULL;
  char *protocolAndDomain = NULL;
  if (asprintf(&protocolAndDomain, "%s://%s.",
    protocol, (char*) host) > 0
  ) {
    // Strip the host namespace from all the cookie names.
    for (DictionaryEntry *node = cookiesDict->head;
      node != NULL;
      node = node->next
    ) {
      printLog(DEBUG, "Replacing \"%s\" in \"%s\".\n", protocolAndDomain,
        (char*) node->key);
      char *cookieName = strReplaceStr((char*) node->key, protocolAndDomain, "");
      printLog(DEBUG, "Replacing \"%s\" with \"%s\".\n",
        (char*) node->key, cookieName);
      dictionaryAddEntry(&tempDict, cookieName, node->value);
      cookieName = stringDestroy(cookieName);
    }
  }
  protocolAndDomain = stringDestroy(protocolAndDomain);
  if (tempDict != NULL) {
    cookiesDict = dictionaryDestroy(cookiesDict);
    cookiesDict = tempDict;
  }
  
  int returnValue = wsThreadInfo->webService.cookiesHandler(cookiesDict);
  wsThreadInfo->cookiesDict = cookiesDict;
  
  printLog(TRACE,
    "EXIT parseCookies(wsThreadInfo=%p) = {%d}\n", wsThreadInfo, returnValue);
  return returnValue;
}

/// @fn Dictionary* parseHeader(const Bytes fullReceiveBuffer)
///
/// @brief Parse the received HTTP header into a Dictionary.
///
/// @param receiveBuffer The content received that contains the HTTP header.
///
/// @return Returns a pointer to a new Dictionary with the HTTP parameter names
///   as keys and their corresponding values as values on success, NULL on
///   failure.
Dictionary* parseHeader(const Bytes receiveBuffer) {
  printLog(TRACE,
    "ENTER parseHeader(receiveBuffer=%p)\n", receiveBuffer);
  
  Dictionary *parameters = dictionaryCreate(typeStringCi);
  if (parameters == NULL) {
    LOG_MALLOC_FAILURE();
    return NULL;
  }
  
  const char *newline = "\r\n";
  u64 newlineLength = 2;
  Bytes header = getBytesBetweenBytes(receiveBuffer, "", "\r\n\r\n");
  if (header == NULL) {
    header = getBytesBetweenBytes(receiveBuffer, "", "\n\n");
    if (header != NULL) {
      newline = "\n";
      newlineLength = 1;
    } else {
      // Single-line header.  This is a command-only header and there are no
      // parameters, so we're done.
      parameters = dictionaryDestroy(parameters);
      printLog(TRACE,
        "EXIT parseHeader(receiveBuffer=%p) = {NULL}\n", receiveBuffer);
      return NULL;
    }
  }
  u64 headerLength = bytesLength(header);
  if (headerLength <= newlineLength) {
    // Empty header.  We're done.
    header = bytesDestroy(header);
    parameters = dictionaryDestroy(parameters);
    printLog(TRACE,
      "EXIT parseHeader(receiveBuffer=%p) = {NULL}\n", receiveBuffer);
    return NULL;
  }
  
  Bytes commandLine
    = getDataBetween(header, headerLength, "", 0, newline, newlineLength);
  if (commandLine == NULL) {
    // If we got here then the entire header is just the command.  Just make a
    // copy.
    bytesAddBytes(&commandLine, header);
  }
  u64 commandLineLength = bytesLength(commandLine);
  Bytes commandLinePointer = commandLine;
  
  // The first line is the HTTP command.  All HTTP parameters start with
  // a letter, so we'll prefix the command portion with "_http", which should
  // never collide with any parameter.  If, however, there ever is a collision,
  // the faulty parameters (which would be part of a malformed header most
  // likely from a malicious client) will never be found by the get operations
  // since they will be added after these.
  if (commandLineLength > 2) { // 2 characters for the space delimiters.
    Bytes httpCommand
      = getDataBetween(commandLinePointer, commandLineLength, "", 0, " ", 1);
    u64 httpCommandLength = bytesLength(httpCommand);
    DictionaryEntry *entry
      = dictionaryAddEntry(&parameters, "_httpCommand",
        httpCommand, typeBytesNoCopy);
    printLog(DEBUG, "httpCommand = \"%s\"\n", str(httpCommand));
    if (entry == NULL) {
      commandLine = bytesDestroy(commandLine);
      header = bytesDestroy(header);
      parameters = dictionaryDestroy(parameters);
      LOG_MALLOC_FAILURE();
      return NULL;
    }
    // Change the type to typeBytes so that the destructor works properly.
    entry->type = typeBytes;
    commandLinePointer += httpCommandLength + 1; // 1 character for the space
    commandLineLength -= httpCommandLength + 1;  // delimiter.
    
    Bytes httpLocation
      = getDataBetween(commandLinePointer, commandLineLength, "", 0, " ", 1);
    if (httpLocation == NULL) {
      // Missing protocol?
      httpLocation
        = getDataBetween(commandLinePointer, commandLineLength, "", 0, "", 0);
    }
    u64 httpLocationLength = bytesLength(httpLocation);
    entry = dictionaryAddEntry(&parameters, "_httpLocation",
      httpLocation, typeBytesNoCopy);
    if (entry == NULL) {
      commandLine = bytesDestroy(commandLine);
      header = bytesDestroy(header);
      parameters = dictionaryDestroy(parameters);
      LOG_MALLOC_FAILURE();
      return NULL;
    }
    // Change the type to typeBytes so that the destructor works properly.
    entry->type = typeBytes;
    commandLinePointer += httpLocationLength;
    commandLineLength -= httpLocationLength;
    if (*commandLinePointer == ' ') {
      commandLinePointer += 1;
      commandLineLength -= 1;
    }
    
    Bytes httpProtocol
      = getDataBetween(commandLinePointer, commandLineLength, "", 0, "", 0);
    entry = dictionaryAddEntry(&parameters, "_httpProtocol",
      httpProtocol, typeBytesNoCopy);
    if (entry == NULL) {
      commandLine = bytesDestroy(commandLine);
      header = bytesDestroy(header);
      parameters = dictionaryDestroy(parameters);
      LOG_MALLOC_FAILURE();
      return NULL;
    }
    // Change the type to typeBytes so that the destructor works properly.
    entry->type = typeBytes;
  } else {
    printLog(DEBUG, "commandLine = \"%s\"\n", str(commandLine));
    printLog(DEBUG, "header = \"%s\"\n", str(header));
    printLog(DEBUG, "receiveBuffer = \"%s\"\n", str(receiveBuffer));
  }
  // Reset the commandLineLength for the code below.
  commandLineLength = bytesLength(commandLine);
  commandLine = bytesDestroy(commandLine);
  
  if (bytesLength(header) > (commandLineLength + newlineLength)) {
    char *headerPointer = ((char*) header) + commandLineLength + newlineLength;
    Bytes **headers = stringToBytesTable(headerPointer, newline, ": ");
    if (headers != NULL) {
      for (u64 i = 0; headers[i] != NULL; i++) {
        Bytes parameterName = headers[i][0];
        if (parameterName != NULL) {
          Bytes parameterValue = NULL;
          if (bytesLength(headers[i][1]) != 0) {
            bytesAddBytes(&parameterValue, headers[i][1]);
          } else {
            // We can't have a named parameter with a NULL value as this would
            // prevent it from being found later.  Supply an empty string.
            bytesAddData(&parameterValue, "", 1);
          }
          
          if ((parameterValue[0] == '"')
            && ((parameterValue[bytesLength(parameterValue) - 1] != '"')
              || (bytesLength(parameterValue) == 1))
          ) {
            // This is a quoted value and the rest of the quote is on the next
            // line.  (Probably a soap action.)
            i++;
            bytesAddBytes(&parameterValue, headers[i][0]);
          }
          
          DictionaryEntry *entry
            = dictionaryAddEntry(&parameters, parameterName,
              parameterValue, typeBytesNoCopy);
          if (entry == NULL) {
            LOG_MALLOC_FAILURE();
            parameterValue = bytesDestroy(parameterValue);
            headers = freeBytesTable(headers);
            header = bytesDestroy(header);
            parameters = dictionaryDestroy(parameters);
            printLog(TRACE,
              "EXIT parseHeader(receiveBuffer=%p) = {NULL}\n", receiveBuffer);
            return NULL;
          }
          // Convert the value type to typeBytes now so that the destructor works
          // properly.
          entry->type = typeBytes;
        }
        // else we have an anonymous parameter, which is a malformed header.
        // Skip it.
      }
    }
    headers = freeBytesTable(headers);
  }
  
  // Release our copy of the header.
  header = bytesDestroy(header);
  
  printLog(TRACE,
    "EXIT parseHeader(receiveBuffer=%p) = {%p}\n", receiveBuffer, parameters);
  return parameters;
}

/// @fn int wsConnectionThread(void *args)
///
/// @brief Handle an individual client connection.
///
/// @param args is the WsThreadInfo structure for the connection to
/// the client cast to a void*.
///
/// @return Returns 0 on success.  Any other value is an error.
int wsConnectionThread(void *args) {
  char recvbuf[1024];
  ssize_t recvbufLen = 0;
  Bytes fullReceiveBuffer = NULL;
  WsThreadInfo *wsThreadInfo = (WsThreadInfo*) args;
  
  printLog(TRACE, "ENTER wsConnectionThread(args=%p)\n", args);
  
  if (wsThreadInfo == NULL) {
    printLog(ERR, "wsThreadInfo is NULL.  Cannot process connection.\n");
    
    printLog(TRACE, "EXIT wsConnectionThread(args=%p) = {1}\n", args);
    return 1;
  }
  
  // In this case, this function appears prior to the printLog to ensure that
  // the printLog uses the proper threadId.
  if (wsThreadInfo->webService.registerThread != NULL) {
    wsThreadInfo->webService.registerThread();
  }
  
  Socket *clientSocket = wsThreadInfo->clientSocket;
  printLog(DETAIL, "Processing connection for %s\n",
    socketAddress(clientSocket));
  
  // There will be a three-second timeout on receive enforced by the while below
  i64 startTime = (i64) time(NULL);
  do {
    int timeout = (3 - (((i64) time(NULL)) - startTime)) * 1000;
    if (timeout <= 0) {
      break;
    }
    recvbufLen = socketReceive(clientSocket, recvbuf, sizeof(recvbuf), timeout);
    if (recvbufLen >= 0) {
      bytesAddData(&fullReceiveBuffer, recvbuf, recvbufLen);
    } else {
      recvbuf[0] = '\0';
      if (clientSocket->sockfd < 0) {
        // Client has closed the connection.  Exit the loop.
        break;
      }
    }
    if (recvbufLen > 0) {
      printLog(DEBUG, "fullReceiveBuffer: %s\n", (char*) fullReceiveBuffer);
    }
  } while (
    ((fullReceiveBuffer == NULL)
      || ((strstr((char*) fullReceiveBuffer, "\r\n\r\n") == NULL)
           && (strstr((char*) fullReceiveBuffer, "\n\n") == NULL)
      )
    )
    && (((i64) time(NULL)) < startTime + 3)
  );
  printLog(DEBUG, "fullReceiveBuffer: %s\n", (char*) fullReceiveBuffer);
  
  if (fullReceiveBuffer == NULL) {
    printLog(WARN, "Nothing received from client.\n");
    mtx_lock(wsThreadInfo->numRunningConnectionThreadsMutex);
    (*wsThreadInfo->numRunningConnectionThreads)--;
    mtx_unlock(wsThreadInfo->numRunningConnectionThreadsMutex);
    clientSocket = socketDestroy(clientSocket);
    
    printLog(TRACE, "EXIT wsConnectionThread(args=%p) = {0}\n", args);
    if (wsThreadInfo->webService.unregisterThread != NULL) {
      wsThreadInfo->webService.unregisterThread(NULL);
    }
    wsThreadInfo->redirectProtocol = stringDestroy(wsThreadInfo->redirectProtocol);
    wsThreadInfo->redirectProtocol = NULL;
    // wsThreadInfo->cookiesDict hasn't been populated yet.
    wsThreadInfo = (WsThreadInfo*) pointerDestroy(wsThreadInfo);
    args = NULL;
    printLog(TRACE, "EXIT wsConnectionThread(args=%p) = {1}\n", args);
    return 0;
  }
  
  // We should have the header at this point.
  wsThreadInfo->httpParams = parseHeader(fullReceiveBuffer);
  
  Bytes contentLengthString
    = (Bytes) dictionaryGetValue(wsThreadInfo->httpParams, "Content-Length");
  u64 contentLength = 0;
  if (contentLengthString != NULL) {
    contentLength = (u64) strtol((char*) contentLengthString, NULL, 10);
  }
  char *bodyStart = strstr((char*) fullReceiveBuffer, "\r\n\r\n");
  if (bodyStart != NULL) {
    bodyStart += 4;
  } else {
    // Try searching for just \n\n
    bodyStart = strstr((char*) fullReceiveBuffer, "\n\n");
    if (bodyStart != NULL) {
      bodyStart += 2;
    }
  }
  
  // There will be a three-second timeout on receive enforced by the while below
  startTime = time(NULL);
  while ((bodyStart != NULL)
    && ((bytesLength(fullReceiveBuffer)
      - (((Bytes) bodyStart) - fullReceiveBuffer)) < contentLength
    )
    && (time(NULL) < startTime + 3)
  ) {
    int timeout = (3 - (time(NULL) - startTime)) * 1000;
    if (timeout <= 0) {
      break;
    }
    recvbufLen = socketReceive(clientSocket, recvbuf, sizeof(recvbuf), timeout);
    if (recvbufLen > 0) {
      bytesAddData(&fullReceiveBuffer, recvbuf, recvbufLen);
    } else if (clientSocket->sockfd < 0) {
      // Client has closed the connection.  Exit the loop.
      break;
    }
    bodyStart = strstr((char*) fullReceiveBuffer, "\r\n\r\n");
    if (bodyStart != NULL) {
      bodyStart += 4; // strlen("\r\n\r\n")
    } else {
      bodyStart = strstr((char*) fullReceiveBuffer, "\n\n");
      if (bodyStart != NULL) {
        bodyStart += 2;
      }
    }
  }
  printLog(DEBUG, "fullReceiveBuffer: %s\n", (char*) fullReceiveBuffer);
  wsThreadInfo->body = (unsigned char*) bodyStart;
  
  // Handle the cookies for this connection.
  if (wsThreadInfo->webService.cookiesHandler != NULL) {
    parseCookies(wsThreadInfo);
  }
  
  // Get the request method (POST or GET).
  Bytes method
    = (Bytes) dictionaryGetValue(wsThreadInfo->httpParams, "_httpCommand");
  if (method == NULL) {
    printLog(ERR, "Malformed HTTP header.\n");
    
    wsThreadInfo->body = NULL;
    wsThreadInfo->httpParams = dictionaryDestroy(wsThreadInfo->httpParams);
    fullReceiveBuffer = bytesDestroy(fullReceiveBuffer);
    mtx_lock(wsThreadInfo->numRunningConnectionThreadsMutex);
    (*wsThreadInfo->numRunningConnectionThreads)--;
    mtx_unlock(wsThreadInfo->numRunningConnectionThreadsMutex);
    clientSocket = socketDestroy(clientSocket);
    
    printLog(TRACE, "EXIT wsConnectionThread(args=%p) = {1}\n", args);
    if (wsThreadInfo->webService.unregisterThread != NULL) {
      wsThreadInfo->webService.unregisterThread(NULL);
    }
    wsThreadInfo->redirectProtocol = stringDestroy(wsThreadInfo->redirectProtocol);
    wsThreadInfo->redirectProtocol = NULL;
    wsThreadInfo->cookiesDict = dictionaryDestroy(wsThreadInfo->cookiesDict);
    wsThreadInfo = (WsThreadInfo*) pointerDestroy(wsThreadInfo);
    args = NULL;
    printLog(TRACE, "EXIT wsConnectionThread(args=%p) = {1}\n", args);
    return 1;
  }
  
  int returnValue = 0;
  // Most requests will be GET requests, so check for that first.
  if (strcmp((char*) method, "GET") == 0) {
    returnValue = handleGetRequest(wsThreadInfo);
  } else if (strcmp((char*) method, "POST") == 0) {
    returnValue = handlePostRequest(wsThreadInfo);
  } else {
    printLog(WARN, "Received unsupported HTTP request method \"%s\".\n",
      method);
    returnValue = 1;
  }
  
  wsThreadInfo->body = NULL;
  wsThreadInfo->httpParams = dictionaryDestroy(wsThreadInfo->httpParams);
  fullReceiveBuffer = bytesDestroy(fullReceiveBuffer);
  mtx_lock(wsThreadInfo->numRunningConnectionThreadsMutex);
  (*wsThreadInfo->numRunningConnectionThreads)--;
  mtx_unlock(wsThreadInfo->numRunningConnectionThreadsMutex);
  clientSocket = socketDestroy(clientSocket);
  
  printLog(TRACE, "EXIT wsConnectionThread(args=%p) = {%d}\n", args, returnValue);
  if (wsThreadInfo->webService.unregisterThread != NULL) {
    wsThreadInfo->webService.unregisterThread(NULL);
  }
  wsThreadInfo->redirectProtocol
     = stringDestroy(wsThreadInfo->redirectProtocol);
  wsThreadInfo->redirectProtocol = NULL;
  wsThreadInfo->cookiesDict = dictionaryDestroy(wsThreadInfo->cookiesDict);
  wsThreadInfo = (WsThreadInfo*) pointerDestroy(wsThreadInfo);
  args = NULL;
  return returnValue;
}

/// @fn int wsInit(int portNumber, List *webServices)
///
/// @brief Initialize the web server and poll for incoming requests.
///
/// @param args A pointer to a WsInitArgs structure that initializes this
///   function.
///
/// @return Returns 0 on success, negative error value on failure.
int wsInit(void *args) {
  WebServer *wsInitArgs = (WebServer*) args;
  
  if (wsInitArgs == NULL) {
    // Nothing to do.
    return -5;
  }
  
  WebService *webService = wsInitArgs->webService;
  if ((webService != NULL) && (webService->registerThread != NULL)) {
    webService->registerThread();
  }
  
  printLog(TRACE, "ENTER wsInit(args=%p)\n", args);
  
  int portNumber = wsInitArgs->portNumber;
  SocketMode socketMode = wsInitArgs->socketMode;
  char *certificate = wsInitArgs->certificate;
  char *key = wsInitArgs->key;
  int timeout = wsInitArgs->timeout;
  char *interfacePath = NULL;
  straddstr(&interfacePath, wsInitArgs->interfacePath);
  char *serverName = NULL;
  straddstr(&serverName, wsInitArgs->serverName);
  // We don't need the original anymore.
  wsInitArgs->interfacePath = stringDestroy(wsInitArgs->interfacePath);
  // We can't copy wsInitArgs->redirectProtocol, wsInitArgs->redirectPort,
  // and wsInitArgs->redirectFunction because they may be dynamically changed
  // while we're running.
  Socket *webServerSocket = NULL;
  
  int *numRunningConnectionThreads = (int*) calloc(1, sizeof(int));
  if (numRunningConnectionThreads == NULL) {
    LOG_MALLOC_FAILURE();
    printLog(NEVER, "EXIT wsInit(args=%p) = {-1}\n", args);
    if ((webService != NULL) && (webService->unregisterThread != NULL)) {
      webService->unregisterThread(NULL);
    }
    serverName = stringDestroy(serverName);
    interfacePath = stringDestroy(interfacePath);
    return -1;
  }
  mtx_t *numRunningConnectionThreadsMutex = (mtx_t*) calloc(1, sizeof(mtx_t));
  if (numRunningConnectionThreadsMutex == NULL) {
    LOG_MALLOC_FAILURE();
    printLog(NEVER, "EXIT wsInit(args=%p) = {-1}\n", args);
    if ((webService != NULL) && (webService->unregisterThread != NULL)) {
      webService->unregisterThread(NULL);
    }
    numRunningConnectionThreads
      = (int*) pointerDestroy(numRunningConnectionThreads);
    serverName = stringDestroy(serverName);
    interfacePath = stringDestroy(interfacePath);
    return -1;
  }
  if (mtx_init(numRunningConnectionThreadsMutex, mtx_plain) != thrd_success) {
    // System error.  Bail.
    printLog(ERR, "Could not initialize mutex.  Cannot start web server.\n");
    printLog(TRACE, "EXIT wsInit(args=%p) = {-1}\n", args);
    if ((webService != NULL) && (webService->unregisterThread != NULL)) {
      webService->unregisterThread(NULL);
    }
    numRunningConnectionThreadsMutex
      = (mtx_t*) pointerDestroy(numRunningConnectionThreadsMutex);
    numRunningConnectionThreads
      = (int*) pointerDestroy(numRunningConnectionThreads);
    serverName = stringDestroy(serverName);
    interfacePath = stringDestroy(interfacePath);
    return -2;
  }
  
  Socket *clientSocket = NULL;
  WsThreadInfo *wsThreadInfo = NULL;
  
  char *address = NULL;
  HashTable *webServiceFunctions = NULL;
  bool initNeeded = true;
  while (initNeeded == true) {
    initNeeded = false;
    
    if (asprintf(&address, "0.0.0.0:%d", portNumber) < 0) {
      address = NULL;
    }
    webServerSocket = socketCreate(SERVER, TCP, address,
      socketMode, certificate, key);
    if (webServerSocket == NULL) {
      printLog(WARN, "Could not create %s web server socket.\n",
        SocketModeNames[socketMode]);
      if (timeout > 0) {
        printLog(WARN, "Retrying for up to %d seconds.\n", timeout);
      } else {
        printLog(WARN, "Retrying until successful.\n");
      }
      for (int i = 0;
        ((i < timeout) || (timeout == 0)) && (wsInitArgs != NULL)
          && (webServerSocket == NULL) && (wsInitArgs->exitNow == false);
        i++
      ) {
        sleep(1);
        webServerSocket = socketCreate(SERVER, TCP, address,
          socketMode, certificate, key);
      }
      if (webServerSocket == NULL) {
        printLog(ERR, "Could not create %s web server socket.\n",
          SocketModeNames[socketMode]);
        address = stringDestroy(address);
        mtx_destroy(numRunningConnectionThreadsMutex);
        numRunningConnectionThreadsMutex
          = (mtx_t*) pointerDestroy(numRunningConnectionThreadsMutex);
        numRunningConnectionThreads
          = (int*) pointerDestroy(numRunningConnectionThreads);
        printLog(TRACE, "EXIT wsInit(args=%p) = {-3}\n", args);
        if ((webService != NULL) && (webService->unregisterThread != NULL)) {
          webService->unregisterThread(NULL);
        }
        serverName = stringDestroy(serverName);
        interfacePath = stringDestroy(interfacePath);
        return -3;
      }
    }
    address = stringDestroy(address);
    
    // Construct the web service lookup table.
    if ((webService != NULL) && (webService->namespaces != NULL)) {
      webServiceFunctions = htCreate(typeString);
      for (WsNamespace *wsNamespace = webService->namespaces;
        wsNamespace->name != NULL;
        wsNamespace++
      ) {
        HashTable *namespaceFunctions = htCreate(typeString);
        if (namespaceFunctions == NULL) {
          LOG_MALLOC_FAILURE();
          webServiceFunctions = htDestroy(webServiceFunctions);
          break;
        }
        for (WsFunctionDescriptor **wsFdList = wsNamespace->functionDescriptors;
          *wsFdList != NULL;
          wsFdList++
        ) {
          for (WsFunctionDescriptor *wsFdCommand = *wsFdList;
            wsFdCommand->name != NULL;
            wsFdCommand++
          ) {
            if (htAddEntry(namespaceFunctions,
              wsFdCommand->name, (void*) wsFdCommand->pointer, typePointerNoCopy) == NULL
            ) {
              LOG_MALLOC_FAILURE();
              namespaceFunctions = htDestroy(namespaceFunctions);
              webServiceFunctions = htDestroy(webServiceFunctions);
              break;
            }
          }
          if (namespaceFunctions == NULL) {
            // malloc failure has already been logged.  Exit the loop.
            break;
          }
        }
        if (namespaceFunctions == NULL) {
          // malloc failure has already been logged.  Exit the loop.
          break;
        }
        HashNode *node = htAddEntry(webServiceFunctions,
          wsNamespace->name, namespaceFunctions, typeHashTableNoCopy);
        if (node == NULL) {
          LOG_MALLOC_FAILURE();
          namespaceFunctions = htDestroy(namespaceFunctions);
          webServiceFunctions = htDestroy(webServiceFunctions);
        }
        node->type = typeHashTable;
      }
      if (webServiceFunctions == NULL) {
        // Cannot run the intended web services.  The appropriate error (malloc
        // failure) has already been logged, so just bail.
        printLog(NEVER, "EXIT wsInit(args=%p) = {-4}\n", args);
        if ((webService != NULL) && (webService->unregisterThread != NULL)) {
          webService->unregisterThread(NULL);
        }
        webServerSocket = socketDestroy(webServerSocket);
        mtx_destroy(numRunningConnectionThreadsMutex);
        numRunningConnectionThreadsMutex
          = (mtx_t*) pointerDestroy(numRunningConnectionThreadsMutex);
        numRunningConnectionThreads
          = (int*) pointerDestroy(numRunningConnectionThreads);
        serverName = stringDestroy(serverName);
        interfacePath = stringDestroy(interfacePath);
        return -4;
      }
    }
    
    // Signal to anyone above us that we're running.
    wsInitArgs->socket = webServerSocket;
    wsInitArgs->isRunning = true;
    
    while (wsInitArgs->exitNow == false) {
      thrd_t myThread;
      
      clientSocket = socketAccept(webServerSocket);
      if (wsInitArgs->exitNow == true) {
        // We've been told to exit.  Do not proceed.  Socket may not be valid.
        clientSocket = socketDestroy(clientSocket);
        break;
      }
      
      if (clientSocket == NULL) {
        printLog(ERR, "Error connecting to client!\n");
        if (wsInitArgs->socket != NULL) {
#ifndef _MSC_VER
          // On POSIX, errno is either EAGAIN or EWOULDBLOCK.
          if ((errno == EBADF) && (wsInitArgs->exitNow == false))
#else // _MSC_VER defined
          // On Windows, the non-blocking socket should make WSAGetLastError
          // return WSAEWOULDBLOCK.
          if ((WSAGetLastError() == WSAEBADF) && (wsInitArgs->exitNow == false))
#endif
          {
            webServerSocket = socketDestroy(webServerSocket);
            webServiceFunctions = htDestroy(webServiceFunctions);
            // This will start us back at the outer while loop
            initNeeded = true;
            break;
          } else {
            continue;
          }
        } else {
          // The server socket has been destroyed and is no longer valid.
          // Exit the while loop.
          // This probably indicates that a non-main thread is trying to restart
          // the server, possibly a subordinate thread.  Decrement our count
          // of connection threads accordingly to avoid cirular dependencies.
          break;
        }
      }
      
      wsThreadInfo =
        (WsThreadInfo*) calloc(1, sizeof(WsThreadInfo));
      if (wsThreadInfo == NULL) {
        // Out of memory.  Continue?
        // TODO:  Should we do something else here?
        LOG_MALLOC_FAILURE();
        clientSocket = socketDestroy(clientSocket);
        continue;
      }
      wsThreadInfo->clientSocket = clientSocket;
      wsThreadInfo->interfacePath = interfacePath;
      wsThreadInfo->serverName = serverName;
      if (webService != NULL) {
        wsThreadInfo->webService = *webService;
        wsThreadInfo->webServiceFunctions = webServiceFunctions;
      }
      wsThreadInfo->numRunningConnectionThreads
        = numRunningConnectionThreads;
      wsThreadInfo->numRunningConnectionThreadsMutex
        = numRunningConnectionThreadsMutex;
      // See the note at the beginning of the function about why we can't use a
      // simple pointer for redirectProtocol.
      if (wsInitArgs->redirectProtocol != NULL) {
        straddstr(&wsThreadInfo->redirectProtocol,
          wsInitArgs->redirectProtocol);
      } // else wsThreadInfo->redirectProtocol is already NULL from calloc
      wsThreadInfo->redirectPort = wsInitArgs->redirectPort;
      wsThreadInfo->redirectFunction = wsInitArgs->redirectFunction;

      mtx_lock(numRunningConnectionThreadsMutex);
      (*numRunningConnectionThreads)++;
      mtx_unlock(numRunningConnectionThreadsMutex);
      if (thrd_create(&myThread,
        wsConnectionThread, (void*) wsThreadInfo) == thrd_success
      ) {
        thrd_detach(myThread);
      } else {
        // Creating the thread failed.  Clean up the memory it should have freed.
        // TODO:  This indicates a system problem.  Should we do something else?
        printLog(ERR, "Could not create thread for connection to %s.\n",
          socketAddress(clientSocket));
        mtx_lock(numRunningConnectionThreadsMutex);
        (*numRunningConnectionThreads)--;
        mtx_unlock(numRunningConnectionThreadsMutex);
        clientSocket = socketDestroy(clientSocket);
        wsThreadInfo = (WsThreadInfo*) pointerDestroy(wsThreadInfo);
      }
    }
  }

  // Block until all threads have exited.  This is to avoid segmentation faults
  // in the threads if they attempt to access any of the variables we free.
  while (((*numRunningConnectionThreads) > 0)
    && (wsInitArgs->exitNow == false)
  );
  mtx_lock(numRunningConnectionThreadsMutex);
  if (numRunningConnectionThreads == 0) {
    // The server socket was destroyed and all subordinate threads have exited.
    // This is a clean exit and it's safe to destroy and free what the
    // subordinate threads depend on.
    mtx_unlock(numRunningConnectionThreadsMutex);
    mtx_destroy(numRunningConnectionThreadsMutex);
    numRunningConnectionThreadsMutex
      = (mtx_t*) pointerDestroy(numRunningConnectionThreadsMutex);
    numRunningConnectionThreads
      = (int*) pointerDestroy(numRunningConnectionThreads);
    if (webServiceFunctions != NULL) {
      webServiceFunctions = htDestroy(webServiceFunctions);
    }
    serverName = stringDestroy(serverName);
    interfacePath = stringDestroy(interfacePath);
  } else {
    mtx_unlock(numRunningConnectionThreadsMutex);
  }
  // else we're in an emergency exit situation and there may be subordinate
  // threads running.  In order to avoid them segfaulting and causing more (and
  // misleading) problems, don't clean up the memory.  This will cause a memory
  // leak but we don't care because we're about to exit the entire process.
  wsInitArgs->isRunning = false;

  printLog(TRACE, "EXIT wsInit(args=%p) = {0}\n", args);
  if ((webService != NULL) && (webService->unregisterThread != NULL)) {
    webService->unregisterThread(NULL);
  }
  return 0;
}

/// @var _mimeHashTableSetup
///
/// @brief A once_flag to keep track of whether or not the HashTable for MIME
/// types for this library has been initialized.
static once_flag _mimeHashTableSetup = ONCE_FLAG_INIT;

/// @fn void setupMimeHashTable(void)
///
/// @brief Setup the file extenstion-to-MIME type HashTable.
///
/// @return This function returns no value.
void setupMimeHashTable(void) {
  _mimeHashTable = htCreate(typeStringCi);
  if (_mimeHashTable == NULL) {
    // We'll just run a binary search instead in lookup.
    printLog(ERR, "htCreate failed.\n");
    return;
  }
  // Go through the array and populate the hash table.
  for (i64 i = 0; i < NUM_MIME_TYPES; i++) {
    if (!htAddEntry(_mimeHashTable, _mimeTypes[i][0], _mimeTypes[i][1])) {
      printLog(ERR,
        "Could not add \"%s\", \"%s\" to MIME type hash table.\n",
        _mimeTypes[i][0], _mimeTypes[i][1]);
        _mimeHashTable = htDestroy(_mimeHashTable);
        break;
    }
  }
}

/// @fn WebServer* webServerCreate(const char *interfacePath, int portNumber, const char *serverName, int timeout, SocketMode socketMode, const char *certificate, const char *key, const char *redirectProtocol, int redirectPort, WsFunction redirectFunction, WebService *webService)
///
/// @brief Create and start a WebServer instance.
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
/// @param redirectProtocol The protocol (http or https) to redirect to instead
///   of processing requests at this socket.
/// @param redirectPort The TCP port to redirect to instead of processing
///   requests at this socket.
/// @param redirectFunction A RedirectFunction to dynamically generate a
///   location header.  Takes precedence over redirectProtocol and redirectPort
///   if present.
/// @param webService A pointer to a populated WebService instance.  This
///   instance is expected to be persistent across the lifetime of the
///   WebServer.
///
/// @return Returns a pointer to a newly-allocated WebServer instance on
/// success, NULL on failure.
WebServer* webServerCreate(const char *interfacePath, int portNumber,
  const char *serverName, int timeout, SocketMode socketMode,
  const char *certificate, const char *key, const char *redirectProtocol,
  int redirectPort, RedirectFunction redirectFunction, WebService *webService
) {
  printLog(TRACE,
    "ENTER webServerCreate(interfacePath=\"%s\", portNumber=%d, "
    "serverName=\"%s\", timeout=%d, socketMode=%s, certificate=%p, key=%p, "
    "redirectProtocol=\"%s\", redirectPort=%d, redirectFunction=%p, "
    "webService=%p)\n",
    interfacePath, portNumber, serverName, timeout,
    (socketMode < NUM_SOCKET_MODES) ? SocketModeNames[socketMode] : "INVALID",
    certificate, key, (redirectProtocol == NULL) ? "" : redirectProtocol,
    redirectPort, redirectFunction, webService);
  
  // This needs to be done before anything else.
  call_once(&_mimeHashTableSetup, setupMimeHashTable);
  
  WebServer *webServer = (WebServer*) calloc(1, sizeof(WebServer));
  
  straddstr(&webServer->interfacePath, interfacePath);
  webServer->portNumber = portNumber;
  straddstr(&webServer->serverName, serverName);
  webServer->timeout = timeout;
  webServer->socketMode = socketMode;
  straddstr(&webServer->certificate, certificate);
  straddstr(&webServer->key, key);
  if (redirectProtocol != NULL) {
    straddstr(&webServer->redirectProtocol, redirectProtocol);
  } // else webServer->redirectProtocol is already NULL from calloc
  webServer->redirectPort = redirectPort;
  webServer->redirectFunction = redirectFunction;
  webServer->webService = webService;
  
  // webServer->socket is initialized to NULL, webServer->threadId is
  // initialized to 0, and webServer->isRunning and webServer->exitNow are
  // initialized to false by calloc above.  webServer->socket will be set by
  // wsInit if socket creation is successful before the specified timeout.
  // webServer->isRunning will be set to true once everything necessary for the
  // server and services is configured and running.
  if (thrd_create(&webServer->threadId,
    wsInit, (void*) webServer) != thrd_success
  ) {
    // Can't start the thread.  System problem.
    printLog(ERR, "Cannot start server thread.\n");
    webServer = webServerDestroy(webServer);
  }
  
  printLog(TRACE,
    "EXIT webServerCreate(interfacePath=\"%s\", portNumber=%d, "
    "serverName=\"%s\", timeout=%d, socketMode=%s, certificate=%p, key=%p, "
    "redirectProtocol=\"%s\", redirectPort=%d, redirectFunction=%p, "
    "webService=%p) = {%p}\n",
    interfacePath, portNumber, serverName, timeout,
    (socketMode < NUM_SOCKET_MODES) ? SocketModeNames[socketMode] : "INVALID",
    certificate, key, (redirectProtocol == NULL) ? "" : redirectProtocol,
    redirectPort, redirectFunction, webService, webServer);
  return webServer;
}

/// @fn WebServer* webServerDestroy(WebServer *webServer)
///
/// @brief Terminate a WebServer instance and free its resources.
///
/// @param webServer A pointer to the WebServer instance to destroy.
///
/// @return This function always returns NULL.
WebServer* webServerDestroy(WebServer *webServer) {
  printLog(TRACE, "ENTER webServerDestroy(webServer=%p)\n", webServer);
  
  if (webServer == NULL) {
    // Nothing to do.
    printLog(TRACE, "EXIT webServerDestroy(webServer=%p) = {NULL}\n", webServer);
    return NULL;
  }
  
  // Attempt a graceful exit first.
  webServer->exitNow = true;
  webServer->socket = socketDestroy(webServer->socket);
  int numMilliseconds = 0;
  while ((webServer->isRunning) && (numMilliseconds < 100)) {
    wsMsleep(1);
    ++numMilliseconds;
  }
  
  int result = 0;
  if (!webServer->isRunning) {
    // This is the expected case.
    thrd_join(webServer->threadId, &result);
  } else {
    printLog(ERR, "Web server thread did not exit.\n");
    printLog(ERR, "Freeing remaining resources.\n");
    thrd_detach(webServer->threadId);
    result = -1;
  }
  
  // Server has exited.  Free resources.
  webServer->interfacePath = stringDestroy(webServer->interfacePath);
  webServer->serverName = stringDestroy(webServer->serverName);
  webServer->certificate = stringDestroy(webServer->certificate);
  webServer->key = stringDestroy(webServer->key);
  webServer->redirectProtocol = stringDestroy(webServer->redirectProtocol);
  webServer = (WebServer*) pointerDestroy(webServer);
  
  printLog(TRACE, "EXIT webServerDestroy(webServer=%p) = {NULL}\n", webServer);
  return NULL;
}

/// @var _mimeTypes
///
/// @brief File-extension to MIME type mapping array.
const char *_mimeTypes[][2] = {
  {".3dm", "x-world/x-3dmf"},
  {".3dmf", "x-world/x-3dmf"},
  {".3g2", "video/3gpp2"},
  {".3gp", "video/3gpp"},
  {".7z", "application/x-7z-compressed"},
  {".a", "application/octet-stream"},
  {".aab", "application/x-authorware-bin"},
  {".aac", "audio/aac"},
  {".aam", "application/x-authorware-map"},
  {".aas", "application/x-authorware-seg"},
  {".abc", "text/vnd.abc"},
  {".abw", "application/x-abiword"},
  {".acgi", "text/html"},
  {".afl", "video/animaflex"},
  {".ai", "application/postscript"},
  {".aif", "audio/aiff"},
  {".aif", "audio/x-aiff"},
  {".aifc", "audio/aiff"},
  {".aifc", "audio/x-aiff"},
  {".aiff", "audio/aiff"},
  {".aiff", "audio/x-aiff"},
  {".aim", "application/x-aim"},
  {".aip", "text/x-audiosoft-intra"},
  {".ani", "application/x-navi-animation"},
  {".aos", "application/x-nokia-9000-communicator-add-on-software"},
  {".aps", "application/mime"},
  {".arc", "application/x-freearc"},
  {".arj", "application/arj"},
  {".arj", "application/octet-stream"},
  {".art", "image/x-jg"},
  {".asf", "video/x-ms-asf"},
  {".asm", "text/x-asm"},
  {".asp", "text/asp"},
  {".asx", "application/x-mplayer2"},
  {".asx", "video/x-ms-asf"},
  {".asx", "video/x-ms-asf-plugin"},
  {".au", "audio/basic"},
  {".au", "audio/x-au"},
  {".avi", "video/x-msvideo"},
  {".avif", "image/avif"},
  {".avs", "video/avs-video"},
  {".azw", "application/vnd.amazon.ebook"},
  {".bcpio", "application/x-bcpio"},
  {".bin", "application/octet-stream"},
  {".bm", "image/bmp"},
  {".bmp", "image/bmp"},
  {".boo", "application/book"},
  {".book", "application/book"},
  {".boz", "application/x-bzip2"},
  {".bsh", "application/x-bsh"},
  {".bz", "application/x-bzip"},
  {".bz2", "application/x-bzip2"},
  {".c", "text/x-c"},
  {".c++", "text/x-c"},
  {".cat", "application/vnd.ms-pki.seccat"},
  {".cc", "text/x-c"},
  {".ccad", "application/clariscad"},
  {".cco", "application/x-cocoa"},
  {".cda", "application/x-cdf"},
  {".cdf", "application/cdf"},
  {".cdf", "application/x-cdf"},
  {".cdf", "application/x-netcdf"},
  {".cer", "application/pkix-cert"},
  {".cer", "application/x-x509-ca-cert"},
  {".cha", "application/x-chat"},
  {".chat", "application/x-chat"},
  {".class", "application/java"},
  {".class", "application/java-byte-code"},
  {".class", "application/x-java-class"},
  {".com", "application/octet-stream"},
  {".com", "text/plain"},
  {".conf", "text/plain"},
  {".cpio", "application/x-cpio"},
  {".cpp", "text/x-c"},
  {".cpt", "application/mac-compactpro"},
  {".cpt", "application/x-compactpro"},
  {".cpt", "application/x-cpt"},
  {".crl", "application/pkcs-crl"},
  {".crl", "application/pkix-crl"},
  {".crt", "application/pkix-cert"},
  {".crt", "application/x-x509-ca-cert"},
  {".crt", "application/x-x509-user-cert"},
  {".csh", "application/x-csh"},
  {".css", "text/css"},
  {".csv", "text/csv"},
  {".cxx", "text/plain"},
  {".dcr", "application/x-director"},
  {".deepv", "application/x-deepv"},
  {".def", "text/plain"},
  {".der", "application/x-x509-ca-cert"},
  {".dif", "video/x-dv"},
  {".dir", "application/x-director"},
  {".dl", "video/dl"},
  {".dl", "video/x-dl"},
  {".doc", "application/msword"},
  {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
  {".dot", "application/msword"},
  {".dp", "application/commonground"},
  {".drw", "application/drafting"},
  {".dump", "application/octet-stream"},
  {".dv", "video/x-dv"},
  {".dvi", "application/x-dvi"},
  {".dwf", "drawing/x-dwf (old)"},
  {".dwf", "model/vnd.dwf"},
  {".dwg", "application/acad"},
  {".dwg", "image/vnd.dwg"},
  {".dwg", "image/x-dwg"},
  {".dxf", "application/dxf"},
  {".dxf", "image/vnd.dwg"},
  {".dxf", "image/x-dwg"},
  {".dxr", "application/x-director"},
  {".el", "text/x-script.elisp"},
  {".elc", "application/x-bytecode.elisp (compiled elisp)"},
  {".elc", "application/x-elc"},
  {".env", "application/x-envoy"},
  {".eot", "application/vnd.ms-fontobject"},
  {".eps", "application/postscript"},
  {".epub", "application/epub+zip"},
  {".es", "application/x-esrehber"},
  {".etx", "text/x-setext"},
  {".evy", "application/envoy"},
  {".evy", "application/x-envoy"},
  {".exe", "application/vnd.microsoft.portable-executable"},
  {".f", "text/plain"},
  {".f", "text/x-fortran"},
  {".f77", "text/x-fortran"},
  {".f90", "text/plain"},
  {".f90", "text/x-fortran"},
  {".fdf", "application/vnd.fdf"},
  {".fif", "application/fractals"},
  {".fif", "image/fif"},
  {".fli", "video/fli"},
  {".fli", "video/x-fli"},
  {".flo", "image/florian"},
  {".flx", "text/vnd.fmi.flexstor"},
  {".fmf", "video/x-atomic3d-feature"},
  {".for", "text/plain"},
  {".for", "text/x-fortran"},
  {".fpx", "image/vnd.fpx"},
  {".fpx", "image/vnd.net-fpx"},
  {".frl", "application/freeloader"},
  {".funk", "audio/make"},
  {".g", "text/plain"},
  {".g3", "image/g3fax"},
  {".gif", "image/gif"},
  {".gl", "video/gl"},
  {".gl", "video/x-gl"},
  {".gsd", "audio/x-gsm"},
  {".gsm", "audio/x-gsm"},
  {".gsp", "application/x-gsp"},
  {".gss", "application/x-gss"},
  {".gtar", "application/x-gtar"},
  {".gz", "application/gzip"},
  {".gzip", "application/gzip"},
  {".h", "text/x-c"},
  {".h++", "text/x-c"},
  {".hdf", "application/x-hdf"},
  {".help", "application/x-helpfile"},
  {".hgl", "application/vnd.hp-hpgl"},
  {".hh", "text/x-c"},
  {".hlb", "text/x-script"},
  {".hlp", "application/hlp"},
  {".hlp", "application/x-helpfile"},
  {".hlp", "application/x-winhelp"},
  {".hpg", "application/vnd.hp-hpgl"},
  {".hpgl", "application/vnd.hp-hpgl"},
  {".hpp", "text/x-c"},
  {".hqx", "application/binhex"},
  {".hqx", "application/binhex4"},
  {".hqx", "application/mac-binhex"},
  {".hqx", "application/mac-binhex40"},
  {".hqx", "application/x-binhex40"},
  {".hqx", "application/x-mac-binhex40"},
  {".hta", "application/hta"},
  {".htc", "text/x-component"},
  {".htm", "text/html"},
  {".html", "text/html"},
  {".htmls", "text/html"},
  {".htt", "text/webviewhtml"},
  {".htx", "text/html"},
  {".ice", "x-conference/x-cooltalk"},
  {".ico", "image/vnd.microsoft.icon"},
  {".ics", "text/calendar"},
  {".idc", "text/plain"},
  {".ief", "image/ief"},
  {".iefs", "image/ief"},
  {".iges", "application/iges"},
  {".iges", "model/iges"},
  {".igs", "application/iges"},
  {".igs", "model/iges"},
  {".ima", "application/x-ima"},
  {".imap", "application/x-httpd-imap"},
  {".inf", "application/inf"},
  {".ins", "application/x-internett-signup"},
  {".ip", "application/x-ip2"},
  {".isu", "video/x-isvideo"},
  {".it", "audio/it"},
  {".iv", "application/x-inventor"},
  {".ivr", "i-world/i-vrml"},
  {".ivy", "application/x-livescreen"},
  {".jam", "audio/x-jam"},
  {".jar", "application/java-archive"},
  {".jav", "text/plain"},
  {".jav", "text/x-java-source"},
  {".java", "text/plain"},
  {".java", "text/x-java-source"},
  {".jcm", "application/x-java-commerce"},
  {".jfif", "image/jpeg"},
  {".jfif", "image/pjpeg"},
  {".jfif-tbnl", "image/jpeg"},
  {".jpe", "image/jpeg"},
  {".jpe", "image/pjpeg"},
  {".jpeg", "image/jpeg"},
  {".jpg", "image/jpeg"},
  {".jps", "image/x-jps"},
  {".js", "text/javascript"},
  {".json", "application/json"},
  {".jsonld", "application/ld+json"},
  {".jut", "image/jutvision"},
  {".kar", "audio/midi"},
  {".kar", "music/x-karaoke"},
  {".ksh", "application/x-ksh"},
  {".ksh", "text/x-script.ksh"},
  {".la", "audio/nspaudio"},
  {".la", "audio/x-nspaudio"},
  {".lam", "audio/x-liveaudio"},
  {".latex", "application/x-latex"},
  {".lha", "application/lha"},
  {".lha", "application/octet-stream"},
  {".lha", "application/x-lha"},
  {".lhx", "application/octet-stream"},
  {".list", "text/plain"},
  {".lma", "audio/nspaudio"},
  {".lma", "audio/x-nspaudio"},
  {".log", "text/plain"},
  {".lsp", "application/x-lisp"},
  {".lsp", "text/x-script.lisp"},
  {".lst", "text/plain"},
  {".lsx", "text/x-la-asf"},
  {".ltx", "application/x-latex"},
  {".lzh", "application/octet-stream"},
  {".lzh", "application/x-lzh"},
  {".lzx", "application/lzx"},
  {".lzx", "application/octet-stream"},
  {".lzx", "application/x-lzx"},
  {".m", "text/plain"},
  {".m", "text/x-m"},
  {".m1v", "video/mpeg"},
  {".m2a", "audio/mpeg"},
  {".m2v", "video/mpeg"},
  {".m3u", "audio/x-mpequrl"},
  {".man", "application/x-troff-man"},
  {".map", "application/x-navimap"},
  {".mar", "text/plain"},
  {".mbd", "application/mbedlet"},
  {".mc$", "application/x-magic-cap-package-1.0"},
  {".mcd", "application/mcad"},
  {".mcd", "application/x-mathcad"},
  {".mcf", "image/vasa"},
  {".mcf", "text/mcf"},
  {".mcp", "application/netmc"},
  {".me", "application/x-troff-me"},
  {".mht", "message/rfc822"},
  {".mhtml", "message/rfc822"},
  {".mid", "audio/midi"},
  {".midi", "audio/midi"},
  {".mif", "application/x-frame"},
  {".mif", "application/x-mif"},
  {".mime", "message/rfc822"},
  {".mime", "www/mime"},
  {".mjf", "audio/x-vnd.audioexplosion.mjuicemediafile"},
  {".mjpg", "video/x-motion-jpeg"},
  {".mjs", "text/javascript"},
  {".mm", "application/base64"},
  {".mm", "application/x-meme"},
  {".mme", "application/base64"},
  {".mod", "audio/mod"},
  {".mod", "audio/x-mod"},
  {".moov", "video/quicktime"},
  {".mov", "video/quicktime"},
  {".movie", "video/x-sgi-movie"},
  {".mp2", "video/mpeg"},
  {".mp3", "audio/mpeg3"},
  {".mp4", "video/mp4"},
  {".mpa", "audio/mpeg"},
  {".mpa", "video/mpeg"},
  {".mpc", "application/x-project"},
  {".mpe", "video/mpeg"},
  {".mpeg", "video/mpeg"},
  {".mpg", "video/mpeg"},
  {".mpga", "audio/mpeg"},
  {".mpkg", "application/vnd.apple.installer+xml"},
  {".mpp", "application/vnd.ms-project"},
  {".mpt", "application/x-project"},
  {".mpv", "application/x-project"},
  {".mpx", "application/x-project"},
  {".mrc", "application/marc"},
  {".ms", "application/x-troff-ms"},
  {".mv", "video/x-sgi-movie"},
  {".my", "audio/make"},
  {".mzz", "application/x-vnd.audioexplosion.mzz"},
  {".nap", "image/naplps"},
  {".naplps", "image/naplps"},
  {".nc", "application/x-netcdf"},
  {".ncm", "application/vnd.nokia.configuration-message"},
  {".nif", "image/x-niff"},
  {".niff", "image/x-niff"},
  {".nix", "application/x-mix-transfer"},
  {".nsc", "application/x-conference"},
  {".nvd", "application/x-navidoc"},
  {".o", "application/octet-stream"},
  {".oda", "application/oda"},
  {".odp", "application/vnd.oasis.opendocument.presentation"},
  {".ods", "application/vnd.oasis.opendocument.spreadsheet"},
  {".odt", "application/vnd.oasis.opendocument.text"},
  {".oga", "audio/ogg"},
  {".ogv", "video/ogg"},
  {".ogx", "application/ogg"},
  {".omc", "application/x-omc"},
  {".omcd", "application/x-omcdatamaker"},
  {".omcr", "application/x-omcregerator"},
  {".opus", "audio/opus"},
  {".otf", "font/otf"},
  {".p", "text/x-pascal"},
  {".p10", "application/pkcs10"},
  {".p10", "application/x-pkcs10"},
  {".p12", "application/pkcs-12"},
  {".p12", "application/x-pkcs12"},
  {".p7a", "application/x-pkcs7-signature"},
  {".p7c", "application/pkcs7-mime"},
  {".p7c", "application/x-pkcs7-mime"},
  {".p7m", "application/pkcs7-mime"},
  {".p7m", "application/x-pkcs7-mime"},
  {".p7r", "application/x-pkcs7-certreqresp"},
  {".p7s", "application/pkcs7-signature"},
  {".part", "application/pro_eng"},
  {".pas", "text/pascal"},
  {".pbm", "image/x-portable-bitmap"},
  {".pcl", "application/vnd.hp-pcl"},
  {".pcl", "application/x-pcl"},
  {".pct", "image/x-pict"},
  {".pcx", "image/x-pcx"},
  {".pdb", "chemical/x-pdb"},
  {".pdf", "application/pdf"},
  {".pfunk", "audio/make"},
  {".pfunk", "audio/make.my.funk"},
  {".pgm", "image/x-portable-graymap"},
  {".pgm", "image/x-portable-greymap"},
  {".php", "application/x-httpd-php"},
  {".pic", "image/pict"},
  {".pict", "image/pict"},
  {".pkg", "application/x-newton-compatible-pkg"},
  {".pko", "application/vnd.ms-pki.pko"},
  {".pl", "text/plain"},
  {".pl", "text/x-script.perl"},
  {".plx", "application/x-pixclscript"},
  {".pm", "image/x-xpixmap"},
  {".pm", "text/x-script.perl-module"},
  {".pm4", "application/x-pagemaker"},
  {".pm5", "application/x-pagemaker"},
  {".png", "image/png"},
  {".pnm", "application/x-portable-anymap"},
  {".pnm", "image/x-portable-anymap"},
  {".pot", "application/mspowerpoint"},
  {".pot", "application/vnd.ms-powerpoint"},
  {".pov", "model/x-pov"},
  {".ppa", "application/vnd.ms-powerpoint"},
  {".ppm", "image/x-portable-pixmap"},
  {".pps", "application/mspowerpoint"},
  {".pps", "application/vnd.ms-powerpoint"},
  {".ppt", "application/vnd.ms-powerpoint"},
  {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
  {".ppz", "application/mspowerpoint"},
  {".pre", "application/x-freelance"},
  {".prt", "application/pro_eng"},
  {".ps", "application/postscript"},
  {".psd", "application/octet-stream"},
  {".pvu", "paleovu/x-pv"},
  {".pwz", "application/vnd.ms-powerpoint"},
  {".py", "text/x-script.phyton"},
  {".pyc", "application/x-bytecode.python"},
  {".qcp", "audio/vnd.qcelp"},
  {".qd3", "x-world/x-3dmf"},
  {".qd3d", "x-world/x-3dmf"},
  {".qif", "image/x-quicktime"},
  {".qt", "video/quicktime"},
  {".qtc", "video/x-qtc"},
  {".qti", "image/x-quicktime"},
  {".qtif", "image/x-quicktime"},
  {".ra", "audio/x-pn-realaudio"},
  {".ra", "audio/x-pn-realaudio-plugin"},
  {".ra", "audio/x-realaudio"},
  {".ram", "audio/x-pn-realaudio"},
  {".rar", "application/vnd.rar"},
  {".ras", "application/x-cmu-raster"},
  {".ras", "image/cmu-raster"},
  {".ras", "image/x-cmu-raster"},
  {".rast", "image/cmu-raster"},
  {".rexx", "text/x-script.rexx"},
  {".rf", "image/vnd.rn-realflash"},
  {".rgb", "image/x-rgb"},
  {".rm", "application/vnd.rn-realmedia"},
  {".rm", "audio/x-pn-realaudio"},
  {".rmi", "audio/mid"},
  {".rmm", "audio/x-pn-realaudio"},
  {".rmp", "audio/x-pn-realaudio"},
  {".rmp", "audio/x-pn-realaudio-plugin"},
  {".rng", "application/ringing-tones"},
  {".rng", "application/vnd.nokia.ringing-tone"},
  {".rnx", "application/vnd.rn-realplayer"},
  {".roff", "application/x-troff"},
  {".rp", "image/vnd.rn-realpix"},
  {".rpm", "audio/x-pn-realaudio-plugin"},
  {".rt", "text/richtext"},
  {".rt", "text/vnd.rn-realtext"},
  {".rtf", "application/rtf"},
  {".rtx", "application/rtf"},
  {".rtx", "text/richtext"},
  {".rv", "video/vnd.rn-realvideo"},
  {".s", "text/x-asm"},
  {".s3m", "audio/s3m"},
  {".saveme", "application/octet-stream"},
  {".sbk", "application/x-tbook"},
  {".scm", "application/x-lotusscreencam"},
  {".scm", "text/x-script.guile"},
  {".scm", "text/x-script.scheme"},
  {".scm", "video/x-scm"},
  {".sdml", "text/plain"},
  {".sdp", "application/sdp"},
  {".sdp", "application/x-sdp"},
  {".sdr", "application/sounder"},
  {".sea", "application/sea"},
  {".sea", "application/x-sea"},
  {".set", "application/set"},
  {".sgm", "text/sgml"},
  {".sgm", "text/x-sgml"},
  {".sgml", "text/sgml"},
  {".sgml", "text/x-sgml"},
  {".sh", "application/x-sh"},
  {".shar", "application/x-bsh"},
  {".shar", "application/x-shar"},
  {".shtml", "text/html"},
  {".shtml", "text/x-server-parsed-html"},
  {".sid", "audio/x-psid"},
  {".sit", "application/x-sit"},
  {".sit", "application/x-stuffit"},
  {".skd", "application/x-koan"},
  {".skm", "application/x-koan"},
  {".skp", "application/x-koan"},
  {".skt", "application/x-koan"},
  {".sl", "application/x-seelogo"},
  {".smi", "application/smil"},
  {".smil", "application/smil"},
  {".snd", "audio/basic"},
  {".snd", "audio/x-adpcm"},
  {".sol", "application/solids"},
  {".spc", "application/x-pkcs7-certificates"},
  {".spc", "text/x-speech"},
  {".spl", "application/futuresplash"},
  {".spr", "application/x-sprite"},
  {".sprite", "application/x-sprite"},
  {".src", "application/x-wais-source"},
  {".ssi", "text/x-server-parsed-html"},
  {".ssm", "application/streamingmedia"},
  {".sst", "application/vnd.ms-pki.certstore"},
  {".step", "application/step"},
  {".stl", "application/sla"},
  {".stl", "application/vnd.ms-pki.stl"},
  {".stl", "application/x-navistyle"},
  {".stp", "application/step"},
  {".sv4cpio", "application/x-sv4cpio"},
  {".sv4crc", "application/x-sv4crc"},
  {".svf", "image/vnd.dwg"},
  {".svf", "image/x-dwg"},
  {".svg", "image/svg+xml"},
  {".svr", "application/x-world"},
  {".svr", "x-world/x-svr"},
  {".swf", "application/x-shockwave-flash"},
  {".t", "application/x-troff"},
  {".talk", "text/x-speech"},
  {".tar", "application/x-tar"},
  {".tbk", "application/toolbook"},
  {".tbk", "application/x-tbook"},
  {".tcl", "application/x-tcl"},
  {".tcl", "text/x-script.tcl"},
  {".tcsh", "text/x-script.tcsh"},
  {".tex", "application/x-tex"},
  {".texi", "application/x-texinfo"},
  {".texinfo", "application/x-texinfo"},
  {".text", "application/plain"},
  {".text", "text/plain"},
  {".tgz", "application/gnutar"},
  {".tgz", "application/x-compressed"},
  {".tif", "image/tiff"},
  {".tiff", "image/tiff"},
  {".tr", "application/x-troff"},
  {".ts", "video/mp2t"},
  {".tsi", "audio/tsp-audio"},
  {".tsp", "application/dsptype"},
  {".tsp", "audio/tsplayer"},
  {".tsv", "text/tab-separated-values"},
  {".ttf", "font/ttf"},
  {".turbot", "image/florian"},
  {".txt", "text/plain"},
  {".uil", "text/x-uil"},
  {".uni", "text/uri-list"},
  {".unis", "text/uri-list"},
  {".unv", "application/i-deas"},
  {".uri", "text/uri-list"},
  {".uris", "text/uri-list"},
  {".ustar", "application/x-ustar"},
  {".ustar", "multipart/x-ustar"},
  {".uu", "application/octet-stream"},
  {".uu", "text/x-uuencode"},
  {".uue", "text/x-uuencode"},
  {".vcd", "application/x-cdlink"},
  {".vcs", "text/x-vcalendar"},
  {".vda", "application/vda"},
  {".vdo", "video/vdo"},
  {".vew", "application/groupwise"},
  {".viv", "video/vivo"},
  {".viv", "video/vnd.vivo"},
  {".vivo", "video/vivo"},
  {".vivo", "video/vnd.vivo"},
  {".vmd", "application/vocaltec-media-desc"},
  {".vmf", "application/vocaltec-media-file"},
  {".voc", "audio/voc"},
  {".voc", "audio/x-voc"},
  {".vos", "video/vosaic"},
  {".vox", "audio/voxware"},
  {".vqe", "audio/x-twinvq-plugin"},
  {".vqf", "audio/x-twinvq"},
  {".vql", "audio/x-twinvq-plugin"},
  {".vrml", "application/x-vrml"},
  {".vrml", "model/vrml"},
  {".vrml", "x-world/x-vrml"},
  {".vrt", "x-world/x-vrt"},
  {".vsd", "application/vnd.visio"},
  {".vst", "application/x-visio"},
  {".vsw", "application/x-visio"},
  {".w60", "application/wordperfect6.0"},
  {".w61", "application/wordperfect6.1"},
  {".w6w", "application/msword"},
  {".wav", "audio/wav"},
  {".wb1", "application/x-qpro"},
  {".wbmp", "image/vnd.wap.wbmp"},
  {".web", "application/vnd.xara"},
  {".weba", "audio/webm"},
  {".webm", "video/webm"},
  {".webp", "image/webp"},
  {".wiz", "application/msword"},
  {".wk1", "application/x-123"},
  {".wmf", "windows/metafile"},
  {".wml", "text/vnd.wap.wml"},
  {".wmlc", "application/vnd.wap.wmlc"},
  {".wmls", "text/vnd.wap.wmlscript"},
  {".wmlsc", "application/vnd.wap.wmlscriptc"},
  {".woff", "font/woff"},
  {".woff2", "font/woff2"},
  {".word", "application/msword"},
  {".wp", "application/wordperfect"},
  {".wp5", "application/wordperfect"},
  {".wp5", "application/wordperfect6.0"},
  {".wp6", "application/wordperfect"},
  {".wpd", "application/wordperfect"},
  {".wpd", "application/x-wpwin"},
  {".wq1", "application/x-lotus"},
  {".wri", "application/mswrite"},
  {".wri", "application/x-wri"},
  {".wrl", "application/x-world"},
  {".wrl", "model/vrml"},
  {".wrl", "x-world/x-vrml"},
  {".wrz", "model/vrml"},
  {".wrz", "x-world/x-vrml"},
  {".wsc", "text/scriplet"},
  {".wsdl", "text/xml"},
  {".wsrc", "application/x-wais-source"},
  {".wtk", "application/x-wintalk"},
  {".x-png", "image/png"},
  {".xbm", "image/x-xbitmap"},
  {".xbm", "image/x-xbm"},
  {".xbm", "image/xbm"},
  {".xdr", "video/x-amt-demorun"},
  {".xgz", "xgl/drawing"},
  {".xhtml", "application/xhtml+xml"},
  {".xif", "image/vnd.xiff"},
  {".xl", "application/excel"},
  {".xla", "application/excel"},
  {".xlb", "application/vnd.ms-excel"},
  {".xlc", "application/vnd.ms-excel"},
  {".xld", "application/excel"},
  {".xlk", "application/excel"},
  {".xll", "application/vnd.ms-excel"},
  {".xlm", "application/vnd.ms-excel"},
  {".xls", "application/vnd.ms-excel"},
  {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
  {".xlt", "application/excel"},
  {".xlv", "application/excel"},
  {".xlw", "application/excel"},
  {".xlw", "application/vnd.ms-excel"},
  {".xm", "audio/xm"},
  {".xml", "application/xml"},
  {".xmz", "xgl/movie"},
  {".xpix", "application/x-vnd.ls-xpix"},
  {".xpm", "image/x-xpixmap"},
  {".xpm", "image/xpm"},
  {".xsd", "text/xml"},
  {".xsr", "video/x-amt-showrun"},
  {".xul", "application/vnd.mozilla.xul+xml"},
  {".xwd", "image/x-xwd"},
  {".xyz", "chemical/x-pdb"},
  {".z", "application/x-compress"},
  {".z", "application/x-compressed"},
  {".zip", "application/zip"},
  {".zoo", "application/octet-stream"},
  {".zsh", "text/x-script.zsh"},
};

const i64 NUM_MIME_TYPES = sizeof(_mimeTypes) / sizeof (char *[2]);

#include "WebClientLib.h"
#include "Dictionary.h"
#include "Scope.h"

WsResponseObject *soapUnitTestFunction(WebService *webService,
  WsConnectionInfo *wsConnectionInfo
) {
  WsRequestObject *inputParams = wsConnectionInfo->functionParams;
  (void) inputParams;
  WsResponseObject *outputParams = NULL;
  
  webService->addResponseValue(&outputParams, "type", "goodUnitTestFunctionResponse");
  webService->addResponseValue(&outputParams, "status", "Returning good status.");
  
  return outputParams;
}

WsResponseObject *restUnitTestFunction(WebService *webService,
  WsConnectionInfo *wsConnectionInfo
) {
  WsRequestObject *inputParams = wsConnectionInfo->functionParams;
  WsResponseObject *outputParams = NULL;
  
  char *stringValue = (char*) webService->getRequestValue(inputParams, "stringValue");
  if (stringValue == NULL) {
    printLog(ERR, "stringValue value not present in inputParams.\n");
    webService->addResponseValue(&outputParams, "type", "badUnitTestFunctionResponse");
    webService->addResponseValue(&outputParams, "status", "String not present.");
    return outputParams;
  } else if (strcmp(stringValue, "Hello, world!") != 0) {
    printLog(ERR, "Expected stringValue value to be \"Hello, world!\", got \"%s\".\n",
      stringValue);
    webService->addResponseValue(&outputParams, "type", "badUnitTestFunctionResponse");
    webService->addResponseValue(&outputParams, "status", "String not correct.");
    return outputParams;
  }
  
  i64 *integerValue = (i64*) webService->getRequestValue(inputParams, "integerValue");
  if (integerValue == NULL) {
    printLog(ERR, "integerValue value not present in inputParams.\n");
    webService->addResponseValue(&outputParams, "type", "badUnitTestFunctionResponse");
    webService->addResponseValue(&outputParams, "status", "Integer not present.");
    return outputParams;
  } else if (*integerValue != 7) {
    printLog(ERR, "Expected integerValue value to be 7, got %lld.\n",
      lld(*integerValue));
    webService->addResponseValue(&outputParams, "type", "badUnitTestFunctionResponse");
    webService->addResponseValue(&outputParams, "status", "Integer not correct.");
    return outputParams;
  }
  
  double *doubleValue = (double*) webService->getRequestValue(inputParams, "doubleValue");
  if (doubleValue == NULL) {
    printLog(ERR, "doubleValue value not present in inputParams.\n");
    webService->addResponseValue(&outputParams, "type", "badUnitTestFunctionResponse");
    webService->addResponseValue(&outputParams, "status", "Double not present.");
    return outputParams;
  } else if (*doubleValue != 3.14) {
    printLog(ERR, "Expected doubleValue value to be 3.14, got %lf.\n", *doubleValue);
    webService->addResponseValue(&outputParams, "type", "badUnitTestFunctionResponse");
    webService->addResponseValue(&outputParams, "status", "Double not correct.");
    return outputParams;
  }
  
  bool *boolValue = (bool*) webService->getRequestValue(inputParams, "boolValue");
  if (boolValue == NULL) {
    printLog(ERR, "boolValue value not present in inputParams.\n");
    webService->addResponseValue(&outputParams, "type", "badUnitTestFunctionResponse");
    webService->addResponseValue(&outputParams, "status", "Boolean not present.");
    return outputParams;
  } else if (*boolValue != true) {
    printLog(ERR, "Expected boolValue value to be true, got %d.\n",
      *boolValue);
    webService->addResponseValue(&outputParams, "type", "badUnitTestFunctionResponse");
    webService->addResponseValue(&outputParams, "status", "Boolean not correct.");
    return outputParams;
  }
  
  void *nullValue = (void*) webService->getRequestValue(inputParams, "nullValue");
  if (nullValue != NULL) {
    printLog(ERR, "nullValue value not correct in inputParams.\n");
    webService->addResponseValue(&outputParams, "type", "badUnitTestFunctionResponse");
    webService->addResponseValue(&outputParams, "status", "Null not correct.");
    return outputParams;
  }
  
  webService->addResponseValue(&outputParams, "type", "goodUnitTestFunctionResponse");
  webService->addResponseValue(&outputParams, "status", "Returning good status.");
  
  return outputParams;
}

Dictionary* redirectUnitTestFunction(Socket *clientSocket,
  const char *interfacePath, Dictionary *httpParams, const unsigned char *body,
  Dictionary *cookiesDict
) {
  SCOPE_ENTER("clientSocket=%p, interfacePath=%s, httpParams=%p, body=%p, "
    "cookiesDict=%p\n", clientSocket, interfacePath, httpParams, body,
    cookiesDict);
  Dictionary *parameters = httpParams;
  Bytes headerHost = NULL;
  bytesAddBytes(&headerHost, (Bytes) dictionaryGetValue(parameters, "Host"));
  if (headerHost == NULL) {
    // There's no Host in the header.  We can't proceed.
    SCOPE_EXIT("clientSocket=%p, interfacePath=%s, httpParams=%p, body=%p, "
      "cookiesDict=%p\n", "{NULL}", clientSocket, interfacePath, httpParams,
      body, cookiesDict);
    return NULL;
  }
  
  // We're going to change the port.
  // We don't want the part of the host header after the colon.
  // Termiante the string there.
  char *portAt = strchr((char*) headerHost, ':');
  if (portAt != NULL) {
    *portAt = '\0';
  }
  int redirectPort = 9002;
  const char *redirectProtocol = "https";
  
  // Construct the host to redirect to.
  char *host = NULL;
  if ((redirectProtocol == NULL) && (redirectPort != 0)) {
    if (asprintf(&host, "%s:%d", (char*) headerHost, redirectPort) < 0) {
      host = NULL;
    }
  } else if ((redirectProtocol != NULL) && (redirectPort == 0)) {
    if (asprintf(&host, "%s://%s", redirectProtocol, (char*) headerHost) < 0) {
      host = NULL;
    }
  } else { // redirectProtocol != NULL && redirectPort != 0
    if (asprintf(&host, "%s://%s:%d", redirectProtocol, (char*) headerHost,
      redirectPort) < 0
    ) {
      host = NULL;
    }
  }
  headerHost = bytesDestroy(headerHost);
  
  char *httpLocation = (char*) dictionaryGetValue(parameters, "_httpLocation");
  if (httpLocation == NULL) {
    printLog(ERR, "Request to redirect to NULL location.\n");
    host = stringDestroy(host);
    SCOPE_EXIT("clientSocket=%p, interfacePath=%s, httpParams=%p, body=%p, "
      "cookiesDict=%p\n", "{NULL}", clientSocket, interfacePath, httpParams,
      body, cookiesDict);
    return NULL;
  }
  char *redirectUrl = NULL;
  asprintf(&redirectUrl, "%s%s", host, httpLocation);
  host = stringDestroy(host);
  
  Dictionary *outputParams = NULL;
  dictionaryAddEntry(&outputParams,
    "redirectUrl", redirectUrl, typeStringNoCopy)->type = typeString;
  
  SCOPE_EXIT("clientSocket=%p, interfacePath=%s, httpParams=%p, body=%p, "
    "cookiesDict=%p\n", "%p", clientSocket, interfacePath, httpParams,
    body, cookiesDict, outputParams);
  return outputParams;
}

WsFunctionDescriptor webServiceFunctions[] = {
  {"soapUnitTestFunction", soapUnitTestFunction},
  {"restUnitTestFunction", restUnitTestFunction},
  {NULL, NULL}
};

WsFunctionDescriptor *webServiceFunctionDescriptors[] = {
  webServiceFunctions,
  NULL
};

WsNamespace webServerNamespaces[] = {
  {"webService", webServiceFunctionDescriptors},
  {NULL, NULL}
};

WebService unitTestWebService = {
  webServerNamespaces,                     // namespaces
  NULL,                                    // cookiesHandler
  NULL,                                    // requestObjectHandler
  wcSerialize,                             // serializeToXml
  xmlToDictionary,                         // deserializeFromXml
  (WsSerializeToJson) listToJson,          // serializeToJson
  jsonToDictionary,                        // deserializeFromJson
  dictionaryDestroy,                       // requestObjectDestroy
  rbTreeDestroy,                           // responseObjectDestroy
  rbTreeGetValue,                          // getRequestValue
  rbTreeGetValue,                          // getResponseValue
  NULL,                                    // registerThread
  NULL,                                    // unregisterThread
  (WsAddRequestValue) dictionaryAddEntry_, // addRequestValue_
  wcAddResponseValue_,                     // addResponseValue_
  rbTreeRemove,                            // removeResponseValue
  dictionaryToString,                      // requestObjectToString
  (WsResponseObjectToString) listToString, // responseObjectToString
  NULL,                                    // context
};

bool webServerUnitTest() {
  const char *indexHtmlContent = "Hello world!";
  size_t indexHtmlSize = strlen(indexHtmlContent);
  
  putFileContent("/tmp/index.html", indexHtmlContent, indexHtmlSize);
  WebServer* webServer = webServerCreate("/tmp", 8999,
    "UnitTestServer", 15, /*socketMode=*/ PLAIN, /*certificate=*/ NULL,
    /*key=*/ NULL, /*redirectProtocol=*/ NULL, /*redirectPort=*/ 0,
    /*redirectFunction=*/ NULL,
    /*webService=*/ NULL
  );
  if (webServer == NULL) {
    printLog(ERR, "webServerCreate returned NULL.\n");
    return false;
  }
  for (int i = 0; (i < 15) && (webServer->socket == NULL); i++) sleep(1);
  
  Bytes response = wcGet("http://127.0.0.1:8999", "/", 500);
  if (response == NULL) {
    printLog(ERR, "No response from web server for request to get \"/\".\n");
    webServer = webServerDestroy(webServer);
    return false;
  }
  if (strcmp(str(response), indexHtmlContent) != 0) {
    printLog(ERR, "Expected \"%s\" from web server, got \"%s\".\n",
      indexHtmlContent, str(response));
    response = bytesDestroy(response);
    webServer = webServerDestroy(webServer);
    return false;
  }
  response = bytesDestroy(response);
  
  response = wcGet("http://127.0.0.1:8999", "/index.html", 500);
  if (response == NULL) {
    printLog(ERR,
      "No response from web server for request to get \"/index.html\".\n");
    webServer = webServerDestroy(webServer);
    return false;
  }
  if (strcmp(str(response), indexHtmlContent) != 0) {
    printLog(ERR, "Expected \"%s\" from web server, got \"%s\".\n",
      indexHtmlContent, str(response));
    response = bytesDestroy(response);
    webServer = webServerDestroy(webServer);
    return false;
  }
  response = bytesDestroy(response);
  
  response = wcGet("http://127.0.0.1:8999", "/marklar", 500);
  if (response != NULL) {
    printLog(ERR,
      "Expected NULL response for request to get \"/marklar\". Got \"%s\"\n",
      str(response));
    response = bytesDestroy(response);
    webServer = webServerDestroy(webServer);
    return false;
  }
  
  Socket *clientSocket
     = socketCreate(CLIENT, TCP, "127.0.0.1:8999", PLAIN,
      /*certificate=*/ NULL, /*key=*/ NULL, 500);
  const char *sendBuffer = "GET /index.html\r\n\r\n\n";
  if (socketSend(clientSocket, sendBuffer, strlen(sendBuffer)) < 0) {
    printLog(ERR, "Could not send \"%s\" to http://127.0.0.1:8999.\n",
      sendBuffer);
    clientSocket = socketDestroy(clientSocket);
    webServer = webServerDestroy(webServer);
    return false;
  }
  ZEROINIT(char responseBuffer[JUMBO_FRAME_SIZE]);
  if (socketReceive(clientSocket, responseBuffer, JUMBO_FRAME_SIZE, 500) < 0) {
    printLog(ERR, "Could not receive index.html from http://127.0.0.1:8999.\n");
    clientSocket = socketDestroy(clientSocket);
    webServer = webServerDestroy(webServer);
    return false;
  }
  responseBuffer[strlen(indexHtmlContent)] = '\0';
  const char *expectedResponse = "HTTP/1.1 200";
  if (strncmp(str(responseBuffer),
    expectedResponse, strlen(expectedResponse)) != 0
  ) {
    printLog(ERR, "Expected \"%s\" from web server, got \"%s\".\n",
      indexHtmlContent, str(responseBuffer));
    clientSocket = socketDestroy(clientSocket);
    webServer = webServerDestroy(webServer);
    return false;
  }
  clientSocket = socketDestroy(clientSocket);
  
  webServer = webServerDestroy(webServer);
  
  webServer = webServerCreate("/tmp", 9000,
    "UnitTestServer", 15, /*socketMode=*/ TLS, /*certificate=*/ NULL,
    /*key=*/ NULL, /*redirectProtocol=*/ NULL, /*redirectPort=*/ 0,
    /*redirectFunction=*/ NULL,
    /*webService=*/ NULL
  );
  if (webServer == NULL) {
    printLog(ERR, "webServerCreate returned NULL.\n");
    return false;
  }
  for (int i = 0; (i < 15) && (webServer->socket == NULL); i++) sleep(1);
  
  response = wcGet("https://127.0.0.1:9000", "/", 15000);
  if (response == NULL) {
    printLog(ERR, "No response from web server for request to get \"/\".\n");
    webServer = webServerDestroy(webServer);
    return false;
  }
  response = bytesDestroy(response);
  
  response = wcGet("https://127.0.0.1:9000", "/index.html", 15000);
  if (response == NULL) {
    printLog(ERR,
      "No response from web server for request to get \"/index.html\".\n");
    webServer = webServerDestroy(webServer);
    return false;
  }
  response = bytesDestroy(response);
  
  response = wcGet("https://127.0.0.1:9000", "/marklar", 15000);
  if (response != NULL) {
    printLog(ERR,
      "Expected NULL response for request to get \"/marklar\". Got \"%s\"\n",
      str(response));
    response = bytesDestroy(response);
    webServer = webServerDestroy(webServer);
    return false;
  }
  
  webServer = webServerDestroy(webServer);
  
  webServer = webServerCreate("/tmp", 9001,
    "UnitTestServer", 15, /*socketMode=*/ PLAIN, /*certificate=*/ NULL,
    /*key=*/ NULL, /*redirectProtocol=*/ "https", /*redirectPort=*/ 1,
    /*redirectFunction=*/ NULL, /*webService=*/ NULL
  );
  
  response = wcGet("http://127.0.0.1:9001", "/marklar", 500);
  if (response != NULL) {
    printLog(ERR,
      "Expected NULL response for request to get \"/marklar\". Got \"%s\"\n",
      str(response));
    response = bytesDestroy(response);
    webServer = webServerDestroy(webServer);
    return false;
  }
  
  webServer = webServerDestroy(webServer);
  
  webServer = webServerCreate("/tmp", 9002,
    "UnitTestServer", 15, /*socketMode=*/ TLS, /*certificate=*/ NULL,
    /*key=*/ NULL, /*redirectProtocol=*/ NULL, /*redirectPort=*/ 0,
    /*redirectFunction=*/ NULL,
    &unitTestWebService
  );
  if (webServer == NULL) {
    printLog(ERR, "webServerCreate returned NULL.\n");
    return false;
  }
  for (int i = 0; (i < 15) && (webServer->socket == NULL); i++) sleep(1);
  
  Dictionary *webServiceReturnValue
    = wcSendSync("https://127.0.0.1:9002", "webService", "soapUnitTestFunction",
    15000);
  if (webServiceReturnValue == NULL) {
    printLog(ERR, "wcSendSync for port 9002 returned NULL.");
    webServer = webServerDestroy(webServer);
    return false;
  } else if (dictionaryGetValue(webServiceReturnValue, "type") == NULL) {
    printLog(ERR, "No return type in web service return value.\n");
    webServiceReturnValue = dictionaryDestroy(webServiceReturnValue);
    webServer = webServerDestroy(webServer);
    return false;
  } else if (dictionaryGetValue(webServiceReturnValue, "status") == NULL) {
    printLog(ERR, "No return status in web service return value.\n");
    webServiceReturnValue = dictionaryDestroy(webServiceReturnValue);
    webServer = webServerDestroy(webServer);
    return false;
  }
  
  const char *type = (char*) dictionaryGetValue(webServiceReturnValue, "type");
  if (strcmp(type, "goodUnitTestFunctionResponse") != 0) {
    printLog(ERR, "Expected goodUnitTestFunctionResponse, got \"%s\".\n", type);
    webServiceReturnValue = dictionaryDestroy(webServiceReturnValue);
    webServer = webServerDestroy(webServer);
    return false;
  }
  
  const char *status
    = (char*) dictionaryGetValue(webServiceReturnValue, "status");
  if (strcmp(status, "Returning good status.") != 0) {
    printLog(ERR, "Expected \"Returning good status.\", got \"%s\".\n", status);
    webServiceReturnValue = dictionaryDestroy(webServiceReturnValue);
    webServer = webServerDestroy(webServer);
    return false;
  }
  
  webServiceReturnValue = dictionaryDestroy(webServiceReturnValue);
  
  webServiceReturnValue
    = wcSendSync("https://127.0.0.1:9002", "webService", "badUnitTestFunction",
    15000);
  if (webServiceReturnValue != NULL) {
    char *webServiceReturnValueString = dictionaryToString(webServiceReturnValue);
    printLog(ERR, "wcSendSync for badUnitTestFunction returned %s.",
      webServiceReturnValueString);
    webServiceReturnValueString = stringDestroy(webServiceReturnValueString);
    webServiceReturnValue = dictionaryDestroy(webServiceReturnValue);
    webServer = webServerDestroy(webServer);
    return false;
  }
  printLog(INFO, "Calling badUnitTestFunction over SOAP failed as expected.\n");
  
  logThreshold = TRACE;
  Variant jsonReturnValue
    = wcSendJsonArgs("https://127.0.0.1:9002", "webService", "restUnitTestFunction", 15000,
    "stringValue", typeString, "Hello, world!",
    "integerValue", typeI32, 7,
    "doubleValue", typeDouble, 3.14,
    "boolValue", typeBool, true,
    "nullValue", typePointer, NULL);
  logThreshold = DEBUG;
  printLog(DEBUG, "Returned from calling wcSendJsonArgs.\n");
  if (jsonReturnValue.value == NULL) {
    printLog(ERR, "wcSendJsonArgs for port 9002 returned NULL.");
    webServer = webServerDestroy(webServer);
    return false;
  } else if (jsonReturnValue.type != typeDictionary) {
    printLog(ERR,
      "Return value from wcSendJsonArgs was not a Dictionary as expected.\n");
    jsonReturnValue.value = jsonReturnValue.type->destroy(jsonReturnValue.value);
    webServer = webServerDestroy(webServer);
    return false;
  } else if (dictionaryGetValue((Dictionary*) jsonReturnValue.value, "type") == NULL) {
    printLog(ERR, "No return type in web service return value.\n");
    jsonReturnValue.value = jsonReturnValue.type->destroy(jsonReturnValue.value);
    webServer = webServerDestroy(webServer);
    return false;
  } else if (dictionaryGetValue((Dictionary*) jsonReturnValue.value, "status") == NULL) {
    printLog(ERR, "No return status in web service return value.\n");
    jsonReturnValue.value = jsonReturnValue.type->destroy(jsonReturnValue.value);
    webServer = webServerDestroy(webServer);
    return false;
  }
  
  type = (char*) dictionaryGetValue((Dictionary*) jsonReturnValue.value, "type");
  if (strcmp(type, "goodUnitTestFunctionResponse") != 0) {
    printLog(ERR, "Expected goodUnitTestFunctionResponse, got \"%s\".\n", type);
    jsonReturnValue.value = jsonReturnValue.type->destroy(jsonReturnValue.value);
    webServer = webServerDestroy(webServer);
    return false;
  }
  
  status = (char*) dictionaryGetValue((Dictionary*) jsonReturnValue.value, "status");
  if (strcmp(status, "Returning good status.") != 0) {
    printLog(ERR, "Expected \"Returning good status.\", got \"%s\".\n", status);
    jsonReturnValue.value = jsonReturnValue.type->destroy(jsonReturnValue.value);
    webServer = webServerDestroy(webServer);
    return false;
  }
  
  jsonReturnValue.value = jsonReturnValue.type->destroy(jsonReturnValue.value);
  
  jsonReturnValue
    = wcSendJsonArgs("https://127.0.0.1:9002", "webService", "badUnitTestFunction", 15000,
    "stringValue", typeString, "Hello, world!",
    "integerValue", typeI32, 7,
    "doubleValue", typeDouble, 3.14,
    "boolValue", typeBool, true,
    "nullValue", typePointer, NULL);
  if (jsonReturnValue.value != NULL) {
    char *webServiceReturnValueString
      = jsonReturnValue.type->toString(jsonReturnValue.value);
    printLog(ERR, "wcSendJsonArgs for badUnitTestFunction returned %s.",
      webServiceReturnValueString);
    webServiceReturnValueString = stringDestroy(webServiceReturnValueString);
    jsonReturnValue.value = jsonReturnValue.type->destroy(jsonReturnValue.value);
    webServer = webServerDestroy(webServer);
    return false;
  }
  
  WebServer *redirectServer = webServerCreate(NULL, 9003,
    "UnitTestServer", 15, /*socketMode=*/ PLAIN, /*certificate=*/ NULL,
    /*key=*/ NULL, /*redirectProtocol=*/ NULL, /*redirectPort=*/ 0,
    /*redirectFunction=*/ redirectUnitTestFunction,
    &unitTestWebService
  );
  if (webServer == NULL) {
    printLog(ERR, "webServerCreate returned NULL.\n");
    return false;
  }
  for (int i = 0; (i < 15) && (webServer->socket == NULL); i++) sleep(1);
  
  // Sending requests to port 9003 should now result in redirecting to the
  // server on  port 9002 and all should be well.
  
  jsonReturnValue
    = wcSendJsonArgs("http://127.0.0.1:9003", "webService", "restUnitTestFunction", 500,
    "stringValue", typeString, "Hello, world!",
    "integerValue", typeI32, 7,
    "doubleValue", typeDouble, 3.14,
    "boolValue", typeBool, true,
    "nullValue", typePointer, NULL);
  printLog(DEBUG, "Returned from calling wcSendJsonArgs.\n");
  if (jsonReturnValue.value == NULL) {
    printLog(ERR, "wcSendJsonArgs for port 9003 returned NULL.");
    redirectServer = webServerDestroy(redirectServer);
    webServer = webServerDestroy(webServer);
    return false;
  } else if (jsonReturnValue.type != typeDictionary) {
    printLog(ERR,
      "Return value from wcSendJsonArgs was not a Dictionary as expected.\n");
    jsonReturnValue.value = jsonReturnValue.type->destroy(jsonReturnValue.value);
    redirectServer = webServerDestroy(redirectServer);
    webServer = webServerDestroy(webServer);
    return false;
  } else if (dictionaryGetValue((Dictionary*) jsonReturnValue.value, "type") == NULL) {
    printLog(ERR, "No return type in web service return value.\n");
    jsonReturnValue.value = jsonReturnValue.type->destroy(jsonReturnValue.value);
    redirectServer = webServerDestroy(redirectServer);
    webServer = webServerDestroy(webServer);
    return false;
  } else if (dictionaryGetValue((Dictionary*) jsonReturnValue.value, "status") == NULL) {
    printLog(ERR, "No return status in web service return value.\n");
    jsonReturnValue.value = jsonReturnValue.type->destroy(jsonReturnValue.value);
    redirectServer = webServerDestroy(redirectServer);
    webServer = webServerDestroy(webServer);
    return false;
  }
  
  type = (char*) dictionaryGetValue((Dictionary*) jsonReturnValue.value, "type");
  if (strcmp(type, "goodUnitTestFunctionResponse") != 0) {
    printLog(ERR, "Expected goodUnitTestFunctionResponse, got \"%s\".\n", type);
    jsonReturnValue.value = jsonReturnValue.type->destroy(jsonReturnValue.value);
    redirectServer = webServerDestroy(redirectServer);
    webServer = webServerDestroy(webServer);
    return false;
  }
  
  status = (char*) dictionaryGetValue((Dictionary*) jsonReturnValue.value, "status");
  if (strcmp(status, "Returning good status.") != 0) {
    printLog(ERR, "Expected \"Returning good status.\", got \"%s\".\n", status);
    jsonReturnValue.value = jsonReturnValue.type->destroy(jsonReturnValue.value);
    redirectServer = webServerDestroy(redirectServer);
    webServer = webServerDestroy(webServer);
    return false;
  }
  
  jsonReturnValue.value = jsonReturnValue.type->destroy(jsonReturnValue.value);
  
  jsonReturnValue
    = wcSendJsonArgs("http://127.0.0.1:9003", "webService", "restUnitTestFunction", 500,
    "stringValue", typeString, "Hello, world!",
    "integerValue", typeI32, 7,
    "doubleValue", typeDouble, 3.14,
    "boolValue", typeBool, true,
    "nullValue", typePointer, NULL);
  if (jsonReturnValue.value == NULL) {
    printLog(ERR, "No response from second call to wcSendJsonArgs.\n");
    redirectServer = webServerDestroy(redirectServer);
    webServer = webServerDestroy(webServer);
    return false;
  }
  jsonReturnValue.value = jsonReturnValue.type->destroy(jsonReturnValue.value);
  
  response = wcGet("http://127.0.0.1:9003", "/", 500);
  if (response == NULL) {
    printLog(ERR, "No response from web server for request to get \"/\".\n");
    redirectServer = webServerDestroy(redirectServer);
    webServer = webServerDestroy(webServer);
    return false;
  }
  response = bytesDestroy(response);
  
  response = wcGet("http://127.0.0.1:9003", "/", 500);
  if (response == NULL) {
    printLog(ERR, "No response from second call to wcGet.\n");
    redirectServer = webServerDestroy(redirectServer);
    webServer = webServerDestroy(webServer);
    return false;
  }
  response = bytesDestroy(response);
  
  redirectServer = webServerDestroy(redirectServer);
  webServer = webServerDestroy(webServer);
  
  // Make sure destroying NULL works.
  webServer = webServerDestroy(webServer);
  
  return true;
}

