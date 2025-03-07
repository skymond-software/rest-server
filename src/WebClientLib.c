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

#include "WebClientLib.h"
#include "Queue.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#include "Scope.h"
#include "Vector.h"

#ifdef LOGGING_ENABLED
#include "LoggingLib.h"
#else
#undef printLog
#define printLog(...) {}
#define logFile stderr
#define shouldLog(...) (false)
#endif

#include "OsApi.h"

/// @fn Dictionary *wcSendSync_(const char* remoteHostAddress, const char *webService, const char *commandName, int timeoutMilliseconds, ...)
///
/// @brief Send a synchronous command to the remote host address.
///
/// @param remoteHostAddress is the IP address of the remote host.
/// @param webService The web service (starting with a leading '/') to operate on.
/// @param commandName is the name of the command to send.
/// @param timeoutMilliseconds The number of milliseconds to wait for a
///   response.  A negative values is "wait forever".  This is *NOT*
///   recommended.
/// @param ... All other arguments take the form kvName, kvValue until a kvName
/// is NULL.
///
/// @return Returns the value returned by the clilent or empty list on failure.
///
/// @note This function is wrapped by a #define (without the leading
/// underscore) that automatically provides the ending NULL.
Dictionary *wcSendSync_(const char* remoteHostAddress, const char *webService,
  const char *commandName, int timeoutMilliseconds, ...
) {
  WsResponseObject *requestList = NULL;
  Dictionary *responseList = NULL;
  char *key = NULL, *value = NULL, *fieldArg = NULL;
  
  printLog(TRACE,
    "ENTER wcSendSync(remoteHostAddress=\"%s\", webService=\"%s\", "
    "commandName=\"%s\", timeoutMilliseconds=%d)\n",
    remoteHostAddress, webService, commandName, timeoutMilliseconds);
  
  va_list args;
  va_start(args, timeoutMilliseconds);
  
  // Construct a key value list from the function arguments.
  fieldArg = va_arg(args, char*);
  while (fieldArg != NULL) {
    printLog(DEBUG, "In va_arg while.\n");
    key = fieldArg;
    fieldArg = va_arg(args, char*);
    if (fieldArg != NULL) {
      value = fieldArg;
      wcAddResponseValue(&requestList, key, value);
    } else {
      wcAddResponseValue(&requestList, key, "");
    }
    fieldArg = va_arg(args, char*);
  }
  va_end(args);
  
  responseList = wcSendSyncKvList(remoteHostAddress, webService, commandName,
    timeoutMilliseconds, requestList);
  requestList = _wsResponseObjectDestroy(requestList);
  
  printLog(TRACE,
    "EXIT wcSendSync(remoteHostAddress=\"%s\", webService=\"%s\", "
    "commandName=\"%s\", timeoutMilliseconds=%d) = { %p }\n",
    remoteHostAddress, webService, commandName, timeoutMilliseconds,
    responseList);
  return responseList;
}
 
/// @fn Dictionary *wcSendSyncKvList(const char* remoteHostAddress, const char *webService, const char *commandName, int timeoutMilliseconds, WsResponseObject *requestList)
///
/// @brief Send a synchronous command to the remote host address.
///
/// @param remoteHostAddress is the IP address of the remote host.
/// @param webService The web service (starting with a leading '/') to operate on.
/// @param commandName is the name of the command to send.
/// @param timeoutMilliseconds The number of milliseconds to wait for a
///   response.  A negative values is "wait forever".  This is *NOT*
///   recommended.
/// @param requestList is the WsResponseObject of parameters to send to the command.
///
/// @return Returns the value returned by the clilent or empty list on failure.
Dictionary *wcSendSyncKvList(const char* remoteHostAddress,
  const char *webService, const char *commandName, int timeoutMilliseconds,
  WsResponseObject *requestList
) {
  printLog(TRACE,
    "ENTER wcSendSyncKvList(remoteHostAddress=\"%s\", webService=\"%s\", "
    "commandName=\"%s\", timeoutMilliseconds=%d)\n",
    remoteHostAddress, webService, commandName, timeoutMilliseconds);
  
  Dictionary *responseList = NULL;
  Bytes request = NULL;
  Bytes messageBody = NULL;
  
  // Construct the XML from the key value list (requestList).
  messageBody = wcSerialize(commandName, requestList, "Request");
  if (messageBody == NULL) {
    // Nothing to send.
    printLog(TRACE,
      "EXIT wcSendSyncKvList(remoteHostAddress=\"%s\", webService=\"%s\", "
      "commandName=\"%s\", timeoutMilliseconds=%d) = { %p }\n",
      remoteHostAddress, webService, commandName, timeoutMilliseconds,
      responseList);
    return responseList;
  }
  
  Bytes location = NULL;
  bytesAddStr(&location, "/");
  bytesAddStr(&location, webService);
  bytesAddStr(&location, "/");
  bytesAddStr(&location, commandName);
  
  // The first line for the HTTP command will be added by wcSendRequest.
  // Add the headers.
  bytesAddStr(&request, "Accept-Encoding: identity\r\n");
  Bytes contentLength = NULL;
  if (abprintf(&contentLength, "Content-Length: %d\r\n",
    (int) bytesLength(messageBody)) > 0
  ) {
    bytesAddBytes(&request, contentLength);
    contentLength = bytesDestroy(contentLength);
  }
  bytesAddStr(&request, "SOAPAction: \"http://127.0.0.1/");
  bytesAddStr(&request, webService);
  bytesAddStr(&request, "/");
  bytesAddStr(&request, commandName);
  bytesAddStr(&request, "\"\r\n");
  bytesAddStr(&request, "User-Agent: Python-urllib/2.7\r\n");
  bytesAddStr(&request, "Connection: close\r\n");
  bytesAddStr(&request, "Content-Type: text/xml; charset=utf-8\r\n\r\n");
  bytesAddBytes(&request, messageBody);
  messageBody = bytesDestroy(messageBody);
  printLog(DEBUG, "Request: \"%s\"\n", request);
  
  Bytes fullResponse = wcSendRequest("POST",
    remoteHostAddress, str(location),
    timeoutMilliseconds, request);
  request = bytesDestroy(request);
  location = bytesDestroy(location);
  
  if (fullResponse != NULL) {
    char *body = str(fullResponse);
    responseList = xmlToDictionary(body);
  }
  
  fullResponse = bytesDestroy(fullResponse);
  
  printLog(TRACE,
    "EXIT wcSendSyncKvList(remoteHostAddress=\"%s\", webService=\"%s\", "
    "commandName=\"%s\", timeoutMilliseconds=%d) = { %p }\n",
    remoteHostAddress, webService, commandName, timeoutMilliseconds, responseList);
  return responseList;
}

/// @fn Bytes wcSerialize(const char *methodName, WsResponseObject *kvList, const char *commandType)
///
/// @brief Converts a Dictionary to an XML string.
///
/// @param methodName is the string version of the method being requested or
///   returned
/// @param kvList is a pointer to the Dictionary to serialize.
/// @param commandType is either "Request" or "Response".  Anything else will
///   cause an error.
///
/// @return Returns a Bytes object with the XML on success, a null pointer
/// on failure.
Bytes wcSerialize(const char *methodName, WsResponseObject *kvList, const char *commandType) {
  Bytes returnValue = NULL;
  char *listString = NULL;
  
  listString = listToString((List*) kvList);
  printLog(TRACE,
    "ENTER wcSerialize(methodName=\"%s\", kvList={%s}, commandType=\"%s\")\n",
    methodName, listString, commandType);
  
  bytesAddStr(&returnValue, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  bytesAddStr(&returnValue, "<SOAP-ENV:Envelope\n");
  bytesAddStr(&returnValue, "  xmlns:ns3=\"http://www.w3.org/2001/XMLSchema\"\n");
  bytesAddStr(&returnValue, "  xmlns:SOAP-ENC=\"http://schemas.xmlsoap.org/soap/encoding/\"\n");
  bytesAddStr(&returnValue, "  xmlns:ns0=\"http://schemas.xmlsoap.org/soap/encoding/\"\n");
  bytesAddStr(&returnValue, "  xmlns:ns1=\"http://127.0.0.1\"\n");
  bytesAddStr(&returnValue, "  xmlns:ns2=\"http://schemas.xmlsoap.org/soap/envelope/\"\n");
  bytesAddStr(&returnValue, "  xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n");
  bytesAddStr(&returnValue, "  xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\"\n");
  bytesAddStr(&returnValue, "  SOAP-ENV:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n");
  bytesAddStr(&returnValue, "  <SOAP-ENV:Header />\n");
  bytesAddStr(&returnValue, "  <ns2:Body>\n");
  
  bytesAddStr(&returnValue, "<ns1:");
  bytesAddStr(&returnValue, methodName);
  bytesAddStr(&returnValue, ">");
  
  char *elementName = NULL;
  straddstr(&elementName, methodName);
  straddstr(&elementName, commandType);
  
  Bytes kvXml = listToXml((List*) kvList, elementName);
  elementName = stringDestroy(elementName);
  /* Doing it this way is prettier but causes parsing problems on the client.
   * Remove until I find a way to be pretty and accurate
   * JBC 2017-02-06
   * char *indentedKvXml = indentText(kvXml, 6);
   * kvXml = stringDestroy(kvXml);
   * bytesAddStr(&returnValue, indentedKvXml);
   * indentedKvXml = stringDestroy(indentedKvXml);
   */
  bytesAddBytes(&returnValue, kvXml);
  kvXml = bytesDestroy(kvXml);
  
  bytesAddStr(&returnValue, "</ns1:");
  bytesAddStr(&returnValue, methodName);
  bytesAddStr(&returnValue, ">");
  
  bytesAddStr(&returnValue, "  </ns2:Body>\n");
  bytesAddStr(&returnValue, "</SOAP-ENV:Envelope>\n");
  
  printLog(TRACE,
    "EXIT wcSerialize(methodName=\"%s\", kvList={%s}, commandType=\"%s\") = {%s}\n",
    methodName, listString, commandType, returnValue);
  listString = stringDestroy(listString);
  return returnValue;
}

/// @fn WsResponseNode* wcAddResponseValue_(WsResponseObject **response, const char *key, const volatile void *value, TypeDescriptor *type, ...)
///
/// @brief Add a key/value pair to a response object.
///
/// @note This function is wrapped by the wcAddResponseValue macro that
///   provides a default type if none is specified by the user.
///
/// @param response A pointer to a WsResponseObject pointer.  The value pointed
///   to by response may be a NULL pointer.  A new WsResponseObject will be
///   created and initialized in this case.
/// @param key A pointer to the key to add.
/// @param value A pointer to the value to add.
/// @param type A pointer to a TypeDescriptor describing the type of the
///   value parameter.
///
/// @return Returns a pointer to a new WsResponseNode in the response object on
///   success, NULL on failure.
WsResponseNode* wcAddResponseValue_(WsResponseObject **response,
  const char *key, const volatile void *value, TypeDescriptor *type, ...
) {
  printLog(TRACE, "ENTER wcAddResponseValue(response=\"%p\", "
    "key=%s, value=%p, type=%p)", response, key, value, type);
  
  if ((response == NULL) || (key == NULL) || (type == NULL)) {
    printLog(ERR, "Invalid parameters.\n");
    printLog(TRACE, "EXIT wcAddResponseValue(response=\"%p\", "
      "key=%p, value=%p, type=%p) = {NULL}",
      response, key, value, type);
    return NULL;
  }
  
  // Make sure the list is good.
  if (*response == NULL) {
    *response = rbTreeCreate(typeString);
    if (*response == NULL) {
      printLog(ERR,  "Could not create red black tree for dictionary.\n");
      printLog(TRACE,
        "EXIT wcAddResponseValue(response={%p}, key=\"%s\", value=\"%s\") = {NULL}\n",
        response, key, (type == typeString) ? (char*) value : type->name);
      return NULL;
    }
  }
  
  WsResponseNode *returnValue = rbInsert(*response, key, value, type);
  if (returnValue == NULL) {
    printLog(ERR, "Could not add entry into red black tree.\n");
    printLog(TRACE,
      "EXIT wcAddResponseValue(response={%p}, key=\"%s\", value=\"%s\") = {NULL}\n",
      response, key, (type == typeString) ? (char*) value : type->name);
    return NULL;
  }
  
  printLog(TRACE, "EXIT wcAddResponseValue(response=\"%p\", "
    "key=\"%s\", value=%p, type=%p) = {%p}",
    response, key, value, type, returnValue);
  return returnValue;
}

/// @fn Bytes wcGet(const char *address, const char *location, int timeoutMilliseconds)
///
/// @brief Issue an HTTP GET call to a remote web server and get the body of the
/// response.
///
/// @param address The full address, including protocol and optinal port.
/// @param location The location on the remote server to get.
/// @param timeoutMilliseconds The number of milliseconds to wait for a
///   response.  A negative values is "wait forever".  This is *NOT*
///   recommended.
///
/// @return Returns a Bytes object containing the response on success, NULL on
/// failure.
Bytes wcGet(const char *address, const char *location,
  int timeoutMilliseconds
) {
  SCOPE_ENTER("address=\"%s\", location=\"%s\", timeoutMilliseconds=%d",
    address, location, timeoutMilliseconds);
  
  Bytes responseContent = wcSendRequest("GET",
    address, location, timeoutMilliseconds, NULL);
  
  SCOPE_EXIT("address=\"%s\", location=\"%s\"", "%p", address, location,
    responseContent);
  return responseContent;
}

/// @fn Variant wcSendJsonArgs_(const char* remoteHostAddress, const char *webService, const char *commandName, int timeoutMilliseconds, ...)
///
/// @brief Serialize the provided arguments as a JSON object string and send
/// it to a specified HTTP host.
///
/// @param remoteHostAddress The full address for the remote host, including
///   protocol.
/// @param webService The web serivce namespace to address.
/// @param commandName The name of the command to issue.
/// @param timeoutMilliseconds The number of milliseconds to wait for a
///   response.  A negative values is "wait forever".  This is *NOT*
///   recommended.
/// @param ... All further parameters are tuples of three values.  The values
///   of the tuples are the name of the parameter, a TypeDescriptor for the
///   value, and the value itself.  The parameter list is NULL terminated and
///   a NULL is automatically provided by the wcSendJsonArgs macro that wraps
///   this function.
///
/// @return Returns a Dictionary with the deserialized response on success,
/// NULL on failure.
Variant wcSendJsonArgs_(const char* remoteHostAddress, const char *webService,
  const char *commandName, int timeoutMilliseconds, ...
) {
  SCOPE_ENTER("remoteHostAddress=\"%s\", webService=\"%s\", "
    "commandName=\"%s\", timeoutMilliseconds=%d",
    remoteHostAddress, webService, commandName, timeoutMilliseconds);
  
  WsResponseObject *requestObject = NULL;
  ZEROINIT(Variant responseObject);
  char *key = NULL;
  TypeDescriptor *type = NULL;
  void *value = NULL;
  
  va_list args;
  va_start(args, timeoutMilliseconds);
  
  // Consume all the arguments and construct the requestObject.
  key = va_arg(args, char*);
  while (key != NULL) {
    type = va_arg(args, TypeDescriptor*);
    int typeIndex = getIndexFromTypeDescriptor(type);
    
    bool  boolParam = false;
    bool *boolParamP = NULL;
    i8  i8Param = 0;
    i8 *i8ParamP = NULL;
    u8  u8Param = 0;
    u8 *u8ParamP = NULL;
    i16  i16Param = 0;
    i16 *i16ParamP = NULL;
    u16  u16Param = 0;
    u16 *u16ParamP = NULL;
    i32  i32Param = 0;
    i32 *i32ParamP = NULL;
    u32  u32Param = 0;
    u32 *u32ParamP = NULL;
    i64  i64Param = false;
    i64 *i64ParamP = NULL;
    u64  u64Param = false;
    u64 *u64ParamP = NULL;
    i128  i128Param = false;
    i128 *i128ParamP = NULL;
    u128  u128Param = false;
    u128 *u128ParamP = NULL;
    float  floatParam = false;
    float *floatParamP = NULL;
    double  doubleParam = false;
    double *doubleParamP = NULL;
    long double  longDoubleParam = false;
    long double *longDoubleParamP = NULL;
    char *stringParam = NULL;
    Bytes bytesParam = NULL;
    List  listParam;
    List *listParamP = NULL;
    Queue  queueParam;
    Queue *queueParamP = NULL;
    Stack  stackParam;
    Stack *stackParamP = NULL;
    RedBlackTree  rbTreeParam;
    RedBlackTree *rbTreeParamP = NULL;
    HashTable  hashTableParam;
    HashTable *hashTableParamP = NULL;
    Vector  vectorParam;
    Vector *vectorParamP = NULL;
    
    switch (typeIndex) {
      case TYPE_BOOL: {
        boolParam = va_arg(args, int);
        value = &boolParam;
        break;
      }
      case TYPE_BOOL_NO_COPY: {
        boolParamP = va_arg(args, bool*);
        value = boolParamP;
        break;
      }
      case TYPE_I8: {
        i8Param = va_arg(args, int);
        value = &i8Param;
        break;
      }
      case TYPE_I8_NO_COPY: {
        i8ParamP = va_arg(args, i8*);
        value = i8ParamP;
        break;
      }
      case TYPE_U8: {
        u8Param = va_arg(args, unsigned int);
        value = &u8Param;
        break;
      }
      case TYPE_U8_NO_COPY: {
        u8ParamP = va_arg(args, u8*);
        value = u8ParamP;
        break;
      }
      case TYPE_I16: {
        i16Param = va_arg(args, int);
        value = &i16Param;
        break;
      }
      case TYPE_I16_NO_COPY: {
        i16ParamP = va_arg(args, i16*);
        value = i16ParamP;
        break;
      }
      case TYPE_U16: {
        u16Param = va_arg(args, unsigned int);
        value = &u16Param;
        break;
      }
      case TYPE_U16_NO_COPY: {
        u16ParamP = va_arg(args, u16*);
        value = u16ParamP;
        break;
      }
      case TYPE_I32: {
        i32Param = va_arg(args, i32);
        value = &i32Param;
        break;
      }
      case TYPE_I32_NO_COPY: {
        i32ParamP = va_arg(args, i32*);
        value = i32ParamP;
        break;
      }
      case TYPE_U32: {
        u32Param = va_arg(args, u32);
        value = &u32Param;
        break;
      }
      case TYPE_U32_NO_COPY: {
        u32ParamP = va_arg(args, u32*);
        value = u32ParamP;
        break;
      }
      case TYPE_I64: {
        i64Param = va_arg(args, i64);
        value = &i64Param;
        break;
      }
      case TYPE_I64_NO_COPY: {
        i64ParamP = va_arg(args, i64*);
        value = i64ParamP;
        break;
      }
      case TYPE_U64: {
        u64Param = va_arg(args, u64);
        value = &u64Param;
        break;
      }
      case TYPE_U64_NO_COPY: {
        u64ParamP = va_arg(args, u64*);
        value = u64ParamP;
        break;
      }
      case TYPE_I128: {
        i128Param = va_arg(args, i128);
        value = &i128Param;
        break;
      }
      case TYPE_I128_NO_COPY: {
        i128ParamP = va_arg(args, i128*);
        value = i128ParamP;
        break;
      }
      case TYPE_U128: {
        u128Param = va_arg(args, u128);
        value = &u128Param;
        break;
      }
      case TYPE_U128_NO_COPY: {
        u128ParamP = va_arg(args, u128*);
        value = u128ParamP;
        break;
      }
      case TYPE_FLOAT: {
        floatParam = va_arg(args, double);
        value = &floatParam;
        break;
      }
      case TYPE_FLOAT_NO_COPY: {
        floatParamP = va_arg(args, float*);
        value = floatParamP;
        break;
      }
      case TYPE_DOUBLE: {
        doubleParam = va_arg(args, double);
        value = &doubleParam;
        break;
      }
      case TYPE_DOUBLE_NO_COPY: {
        doubleParamP = va_arg(args, double*);
        value = doubleParamP;
        break;
      }
      case TYPE_LONG_DOUBLE: {
        longDoubleParam = va_arg(args, long double);
        value = &longDoubleParam;
        break;
      }
      case TYPE_LONG_DOUBLE_NO_COPY: {
        longDoubleParamP = va_arg(args, long double*);
        value = longDoubleParamP;
        break;
      }
      case TYPE_STRING:
      case TYPE_STRING_NO_COPY:
      case TYPE_STRING_CI:
      case TYPE_STRING_CI_NO_COPY: {
        stringParam = va_arg(args, char*);
        value = stringParam;
        type = typeStringNoCopy;
        break;
      }
      case TYPE_BYTES:
      case TYPE_BYTES_NO_COPY: {
        bytesParam = va_arg(args, Bytes);
        value = (void*) bytesParam;
        type = typeBytesNoCopy;
        break;
      }
      case TYPE_LIST: {
        listParam = va_arg(args, List);
        value = &listParam;
        type = typeListNoCopy;
        break;
      }
      case TYPE_LIST_NO_COPY: {
        listParamP = va_arg(args, List*);
        value = listParamP;
        type = typeListNoCopy;
        break;
      }
      case TYPE_QUEUE: {
        queueParam = va_arg(args, Queue);
        value = &queueParam;
        type = typeQueueNoCopy;
        break;
      }
      case TYPE_QUEUE_NO_COPY: {
        queueParamP = va_arg(args, Queue*);
        value = queueParamP;
        type = typeQueueNoCopy;
        break;
      }
      case TYPE_STACK: {
        stackParam = va_arg(args, Stack);
        value = &stackParam;
        type = typeStackNoCopy;
        break;
      }
      case TYPE_STACK_NO_COPY: {
        stackParamP = va_arg(args, Stack*);
        value = stackParamP;
        type = typeStackNoCopy;
        break;
      }
      case TYPE_RB_TREE: {
        rbTreeParam = va_arg(args, RedBlackTree);
        value = &rbTreeParam;
        type = typeRbTreeNoCopy;
        break;
      }
      case TYPE_RB_TREE_NO_COPY: {
        rbTreeParamP = va_arg(args, RedBlackTree*);
        value = rbTreeParamP;
        type = typeRbTreeNoCopy;
        break;
      }
      case TYPE_HASH_TABLE: {
        hashTableParam = va_arg(args, HashTable);
        value = &hashTableParam;
        type = typeHashTableNoCopy;
        break;
      }
      case TYPE_HASH_TABLE_NO_COPY: {
        hashTableParamP = va_arg(args, HashTable*);
        value = hashTableParamP;
        type = typeHashTableNoCopy;
        break;
      }
      case TYPE_VECTOR: {
        vectorParam = va_arg(args, Vector);
        value = &vectorParam;
        type = typeVectorNoCopy;
        break;
      }
      case TYPE_VECTOR_NO_COPY: {
        vectorParamP = va_arg(args, Vector*);
        value = vectorParamP;
        type = typeVectorNoCopy;
        break;
      }
      case TYPE_POINTER:
      case TYPE_POINTER_NO_COPY: {
        void *param = va_arg(args, void*);
        value = param;
        type = typePointerNoCopy;
        break;
      }
      default: {
        printLog(ERR, "Unknown type in wcSendJsonArgs.  Cannot proceed.\n");
        SCOPE_EXIT("remoteHostAddress=\"%s\", webService=\"%s\", "
          "commandName=\"%s\", timeoutMilliseconds=%d", "NULL",
          remoteHostAddress, webService, commandName, timeoutMilliseconds);
        return responseObject; // NULL
      }
    }
    
    wcAddResponseValue(&requestObject, key, value, type);
    key = va_arg(args, char*);
  }
  
  va_end(args);
  
  scopeAdd(requestObject, wsResponseObjectDestroy);
  
  responseObject = wcSendJsonObject(remoteHostAddress, webService, commandName,
    timeoutMilliseconds, requestObject);
  
  SCOPE_EXIT("remoteHostAddress=\"%s\", webService=\"%s\", "
    "commandName=\"%s\", timeoutMilliseconds=%d", "%p",
    remoteHostAddress, webService, commandName, timeoutMilliseconds,
    responseObject.value);
  return responseObject;
}

/// @fn Variant wcSendJsonObject(const char* remoteHostAddress, const char *webService, const char *commandName, int timeoutMilliseconds, WsResponseObject *requestObject)
///
/// @brief Serialize the provided arguments as a JSON object string and send
/// it to a specified HTTP host.
///
/// @param remoteHostAddress The full address for the remote host, including
///   protocol.
/// @param webService The web serivce namespace to address.
/// @param commandName The name of the command to issue.
/// @param timeoutMilliseconds The number of milliseconds to wait for a
///   response.  A negative values is "wait forever".  This is *NOT*
///   recommended.
/// @param requestObject A pointer to a WsResponseObject that contains the
///   typed key-value pairs to serialize and send.
///
/// @return Returns a Variant with the deserialized response on success,
/// empty Variant on failure.
Variant wcSendJsonObject(const char* remoteHostAddress, const char *webService,
  const char *commandName, int timeoutMilliseconds, WsResponseObject *requestObject
) {
  SCOPE_ENTER("remoteHostAddress=\"%s\", webService=\"%s\", "
    "commandName=\"%s\", timeoutMilliseconds=%d",
    remoteHostAddress, webService, commandName, timeoutMilliseconds);
  
  ZEROINIT(Variant responseObject);
  
  Bytes request = NULL, contentLength = NULL, messageBody = NULL;
  
  // Construct the JSON from the key value list (requestList).
  messageBody = wsResponseObjectToJson(requestObject);
  
  Bytes location = NULL;
  bytesAddStr(&location, "/");
  bytesAddStr(&location, webService);
  bytesAddStr(&location, "/");
  bytesAddStr(&location, commandName);
  scopeAdd(location, bytesDestroy);
  
  // The first line for the HTTP command will be added by wcSendRequest.
  // Add the headers.
  bytesAddStr(&request, "Accept-Encoding: identity\r\n");
  if (abprintf(&contentLength, "Content-Length: %d\r\n",
    (int) bytesLength(messageBody)) > 0
  ) {
    bytesAddBytes(&request, contentLength);
    contentLength = bytesDestroy(contentLength);
  }
  bytesAddStr(&request, "User-Agent: Python-urllib/2.7\r\n");
  bytesAddStr(&request, "Connection: close\r\n");
  bytesAddStr(&request, "Content-Type: application/json; charset=utf-8\r\n\r\n");
  bytesAddBytes(&request, messageBody);
  messageBody = bytesDestroy(messageBody);
  scopeAdd(request, bytesDestroy);
  printLog(DEBUG, "Request: \"%s\"\n", request);
  
  Bytes fullResponse = wcSendRequest("POST",
    remoteHostAddress, str(location),
    timeoutMilliseconds, request);
  
  if (fullResponse != NULL) {
    char *body = str(fullResponse);
    long long int startPosition = 0;
    if (*body == '{') {
      responseObject.value = jsonToDictionary(body, &startPosition);
      responseObject.type = typeDictionary;
    } else if (*body == '[') {
      responseObject.value = jsonToVector(body, &startPosition);
      responseObject.type = typeVector;
    }
  }
  fullResponse = bytesDestroy(fullResponse);
  
  const char *responseString = "";
  if (shouldLog(TRACE)) {
    if (responseObject.type == typeDictionary) {
      responseString
        = (char*) scopeAdd(
        dictionaryToString((Dictionary*) responseObject.value),
        pointerDestroyFunction);
    } else if (responseObject.type == typeVector) {
      responseString
        = (char*) scopeAdd(vectorToString((Vector*) responseObject.value),
        pointerDestroyFunction);
    }
  }
  (void) responseString; // For when logging is disabled.
  SCOPE_EXIT("remoteHostAddress=\"%s\", webService=\"%s\", "
    "commandName=\"%s\", timeoutMilliseconds=%d", "%s",
    remoteHostAddress, webService, commandName, timeoutMilliseconds,
    responseString);
  return responseObject;
}

/// @var _redirects
///
/// @brief RedBlackTree to hold a cache of redirects we get from servers so that
/// we don't have to send and then redirect every time if something has moved.
RedBlackTree *_redirects = NULL;

/// @var _redirectsSetup
///
/// @brief A once_flag to keep track of whether or not the _redirects tre
/// has been initialized.
static once_flag _redirectsSetup = ONCE_FLAG_INIT;

/// @fn void initRedirects(void)
///
/// @brief Function to run once to initialize the _redirects lookup tree.
///
/// @return This function returns no value.
void initRedirects(void) {
  _redirects = rbTreeCreate(typeBytes);
}

/// @fn Bytes wcSendRequest(const char *method, const char *remoteHostAddress, const char *location, int timeoutMilliseconds, Bytes request)
///
/// @brief Send a request from this web client to a remote server at the
/// specified address and, port, and location via the specified HTTP method.
///
/// @param *method The HTTP method to use for the request.
/// @param remoteHostAddress The URL of the protocol, address or host name,
///   and port to connect to.
/// @param location The path to get at the remote host.
/// @param timeoutMilliseconds The number of milliseconds to wait before timing
///   out on a send or receive.
/// @param request The full set of headers and body to send, minus the first
/// HTTP command line.
///
/// @return Returns the body of the response on success, NULL on failure.
Bytes wcSendRequest(const char *method, const char *remoteHostAddress,
  const char *location, int timeoutMilliseconds, Bytes request
) {
  SCOPE_ENTER("method=%s, remoteHostAddress=%s, location=%s, "
    "timeoutMilliseconds=%d, request=%p", strOrNull(method),
    strOrNull(remoteHostAddress), strOrNull(location), timeoutMilliseconds,
    request);
  
  Bytes response = NULL;
  Bytes **headers = NULL;
  
  if ((method == NULL) || (remoteHostAddress == NULL) || (location == NULL)) {
    printLog(ERR, "One or more NULL parameters.\n");
    SCOPE_EXIT("method=%s, remoteHostAddress=%s, location=\"%s\", "
      "timeoutMilliseconds=%d, request=%p", "NULL", method, remoteHostAddress,
      location, timeoutMilliseconds, request);
    return response; // NULL
  }
  
  const char *remoteHostAddressToUse = remoteHostAddress;
  const char *locationToUse = location;
  
  call_once(&_redirectsSetup, initRedirects);
  Bytes key = NULL;
  bytesAddStr(&key, method);
  bytesAddStr(&key, remoteHostAddress);
  bytesAddStr(&key, location);
  scopeAdd(key, bytesDestroy);
  RedBlackTree *redirect = (RedBlackTree*) rbTreeGetValue(_redirects, key);
  if (redirect != NULL) {
    printLog(DEBUG, "Found redirect for %s%s.\n", remoteHostAddress, location);
    remoteHostAddressToUse = str(rbTreeGetValue(redirect, "remoteHostAddress"));
    printLog(DEBUG, "New remoteHostAddress = \"%s\".\n", remoteHostAddressToUse);
    locationToUse = str(rbTreeGetValue(redirect, "location"));
    printLog(DEBUG, "New location = \"%s\".\n", locationToUse);
  }
  
  SocketMode socketMode = PLAIN;
  if (strncmp(remoteHostAddressToUse, "https://", 8) == 0) {
    remoteHostAddressToUse += 8;
    if (tlsSocketsEnabled() == true) {
      socketMode = TLS;
    } else {
      printLog(ERR, "Request to communicate over HTTPS without TLS support.\n");
      SCOPE_EXIT("method=%s, remoteHostAddress=%s, location=%s, "
        "timeoutMilliseconds=%d, request=%p", "NULL", method, remoteHostAddress,
        location, timeoutMilliseconds, request);
      return response; // NULL
    }
  } else if (strncmp(remoteHostAddressToUse, "http://", 7) == 0) {
    remoteHostAddressToUse += 7;
  }
  
  u32 numKeyRemovals = 0;
  while ((response == NULL) && (numKeyRemovals < 2)) {
    Bytes fullRequest = NULL;
    bytesAddStr(&fullRequest, method);
    bytesAddStr(&fullRequest, " ");
    bytesAddStr(&fullRequest, locationToUse);
    bytesAddStr(&fullRequest, " HTTP/1.1\r\n");
    bytesAddStr(&fullRequest, "Host: ");
    bytesAddStr(&fullRequest, remoteHostAddressToUse);
    bytesAddStr(&fullRequest, "\r\n");
    if (bytesLength(request) > 0) {
      // This is the expected case.
      bytesAddBytes(&fullRequest, request);
    } else {
      // Terminate the header.
      bytesAddStr(&fullRequest, "\r\n");
    }
    scopeAdd(fullRequest, bytesDestroy);
    
    Socket *sock = NULL;
    int numRetries = 0;
    u64 startTime = getElapsedMicroseconds(0);
    do {
      sock = socketCreate(CLIENT, TCP, remoteHostAddressToUse, socketMode,
        /*certificate=*/ NULL, /*key=*/ NULL, timeoutMilliseconds);
      if (sock == NULL) {
        msleep(1);
        numRetries++;
      }
    } while ((sock == NULL) && (numRetries < 100)
      && (getElapsedMicroseconds(startTime) < 100000));
    if (sock == NULL) {
      printLog(ERR, "Could not create socket connection to remote host at %s.\n",
        remoteHostAddressToUse);
      if ((remoteHostAddressToUse != remoteHostAddress)
        && (remoteHostAddressToUse != (remoteHostAddress + 7))
        && (remoteHostAddressToUse != (remoteHostAddress + 8))
      ) {
        // We are using a remote host and location that was previously determined
        // from an HTTP redirect.  That connection has now failed us.  We need to
        // go back to the original host and try again.
        rbTreeRemove(_redirects, key);
        numKeyRemovals++;
        remoteHostAddressToUse = remoteHostAddress;
        if (strncmp(remoteHostAddressToUse, "https://", 8) == 0) {
          remoteHostAddressToUse += 8;
          if (tlsSocketsEnabled() == true) {
            socketMode = TLS;
          } else {
            printLog(ERR, "Request to communicate over HTTPS without TLS support.\n");
            break;
          }
        } else if (strncmp(remoteHostAddressToUse, "http://", 7) == 0) {
          remoteHostAddressToUse += 7;
          socketMode = PLAIN;
        }
        locationToUse = location;
        fullRequest = (Bytes) scopeDestroy(fullRequest);
        continue;
      }
      
      SCOPE_EXIT("method=%s, remoteHostAddress=%s, location=%s, "
        "timeoutMilliseconds=%d, request=%p", "NULL", method, remoteHostAddress,
        location, timeoutMilliseconds, request);
      return response; // NULL
    }
    scopeAdd(sock, socketDestroy);
    
    // Send the request synchronously.
    if (socketSend(sock, (const void*) fullRequest, bytesLength(fullRequest)
      ) < 0
    ) {
      printLog(ERR, "Synchronous send of request failed\n");
      SCOPE_EXIT("method=%s, remoteHostAddress=%s, location=%s, "
        "timeoutMilliseconds=%d, request=%p", "NULL", method, remoteHostAddress,
        location, timeoutMilliseconds, request);
      return response; // NULL
    }
    fullRequest = (Bytes) scopeDestroy(fullRequest);
    printLog(DEBUG, "Sent request to server.  Awaiting response.\n");
    
    // Get the remote host's response.
    ZEROINIT(char responseBuffer[JUMBO_FRAME_SIZE]);
    Bytes fullResponse = NULL;
    startTime = getElapsedMicroseconds(0);
    u64 headerLength = 0;
    Bytes contentLengthHeader = NULL;
    u64 contentLength = 0;
    char *body = NULL;
    const char *newline = "\r\n";
    u32 newlineLength = 2;
    while (((body == NULL)
        || ((body != NULL)
          && ((bytesLength(fullResponse) - headerLength) < contentLength)))
      && ((getElapsedMicroseconds(startTime)
          < (((u64) timeoutMilliseconds) * 1000))
        || (timeoutMilliseconds < 0))
    ) {
      int responseLength
        = socketReceive(sock, responseBuffer, JUMBO_FRAME_SIZE,
        timeoutMilliseconds);
      if (responseLength <= 0) {
        printLog(ERR, "socketReceive failed\n");
        break;
      } else if (body != NULL) {
        bytesAddData(&fullResponse, responseBuffer, responseLength);
        if (newlineLength == 2) {
          body = strstr((char*) fullResponse, "\r\n\r\n") + 4;
        } else { // newlineLength == 1
          body = strstr((char*) fullResponse, "\n\n") + 2;
        }
        if ((bytesLength(fullResponse) - headerLength) == contentLength) {
          break;
        }
      } else {
        bytesAddData(&fullResponse, responseBuffer, responseLength);
        // Try to parse the header.
        body = strstr((char*) fullResponse, "\r\n\r\n");
        if (body != NULL) {
          body += 4; // Move past the \r\n\r\n
        } else {
          body = strstr((char*) fullResponse, "\n\n");
          if (body != NULL) {
            newline = "\n";
            newlineLength = 1;
            body += 2; // Move past the \n\n
          }
        }
        if (body != NULL) {
          headerLength = ((uintptr_t) body) - ((uintptr_t) fullResponse);
          contentLengthHeader
            = getBytesBetweenCi(str(fullResponse), "content-length: ", newline);
          contentLength
            = (u64) strtoll(strOrEmpty(contentLengthHeader), NULL, 10);
          contentLengthHeader = bytesDestroy(contentLengthHeader);
        }
      }
    }
    
    if ((fullResponse == NULL)
      && (remoteHostAddressToUse != remoteHostAddress)
      && (remoteHostAddressToUse != (remoteHostAddress + 7))
      && (remoteHostAddressToUse != (remoteHostAddress + 8))
    ) {
      // We are using a remote host and location that was previously determined
      // from an HTTP redirect.  That connection has now failed us.  We need to
      // go back to the original host and try again.
      rbTreeRemove(_redirects, key);
      numKeyRemovals++;
      remoteHostAddressToUse = remoteHostAddress;
      if (strncmp(remoteHostAddressToUse, "https://", 8) == 0) {
        remoteHostAddressToUse += 8;
        if (tlsSocketsEnabled() == true) {
          socketMode = TLS;
        } else {
          printLog(ERR, "Request to communicate over HTTPS without TLS support.\n");
          break;
        }
      } else if (strncmp(remoteHostAddressToUse, "http://", 7) == 0) {
        remoteHostAddressToUse += 7;
        socketMode = PLAIN;
      }
      locationToUse = location;
      sock = (Socket*) scopeDestroy(sock);
      continue;
    }
    
    while (bytesLength(fullResponse) != (headerLength + contentLength)) {
      int responseLength
        = socketReceive(sock, responseBuffer, JUMBO_FRAME_SIZE,
        timeoutMilliseconds);
      if (responseLength <= 0) {
        printLog(ERR, "socketReceive failed\n");
        break;
      } else {
        bytesAddData(&fullResponse, responseBuffer, responseLength);
        if (newlineLength == 2) {
          body = strstr((char*) fullResponse, "\r\n\r\n") + 4;
        } else { // newlineLength == 1
          body = strstr((char*) fullResponse, "\n\n") + 2;
        }
      }
    }
    
    if (bytesLength(fullResponse) == (headerLength + contentLength)) {
      printLog(DEBUG, "All expected data received.\n");
    } else {
      printLog(DEBUG, "All expected data *NOT* received.\n");
    }
    printLog(DEBUG, "bytesLength(fullResponse) = %llu\n",
      llu(bytesLength(fullResponse)));
    printLog(DEBUG, "headerLength = %llu\n", llu(headerLength));
    printLog(DEBUG, "contentLength = %llu\n", llu(contentLength));
    
    if ((headerLength > 0) && (bytesLength(fullResponse) > headerLength)) {
      printLog(DEBUG, "Adding %llu bytes to response.\n",
        llu(bytesLength(fullResponse) - headerLength));
      printLog(DEBUG, "body = \"%s\"\n", body);
      bytesAddData(&response, body, bytesLength(fullResponse) - headerLength);
      printLog(DEBUG, "fullResponse = \"%s\"\n", fullResponse);
      fullResponse = bytesDestroy(fullResponse);
      printLog(DEBUG, "response = \"%s\"\n", response);
    } else {
      // Header-only response.  This likely indicates that the requested resource
      // has been moved.  We need to update our information and try again.
      if (headers != NULL) {
        headers = (Bytes**) scopeDestroy(headers);
      }
      headers = stringToBytesTable(str(fullResponse), newline, ": ");
      scopeAdd(headers, freeBytesTable);
      fullResponse = bytesDestroy(fullResponse);
      
      if ((headers == NULL) || (headers[0] == NULL)) {
        // Invalid response.  Exit.
        printLog(ERR, "Could not parse headers.\n");
        break;
      }
      
      // First line is status.  Figure out what we got
      const char *spaceAt = strchr(str(headers[0][0]), ' ');
      const char *statusString = NULL;
      if (spaceAt != NULL) {
        statusString = spaceAt + 1;
      }
      int statusCode = (int) strtol(strOrEmpty(statusString), NULL, 10);
      
      if ((statusCode == 301) || (statusCode == 302)
        || (statusCode == 307) || (statusCode == 308)
      ) {
        const char *newLocation = NULL;
        for (int i = 1; headers[i] != NULL; i++) {
          if (strcmpci(strOrEmpty(headers[i][0]), "Location") == 0) {
            newLocation = str(headers[i][1]);
            break;
          }
        }
        
        if (newLocation == NULL) {
          // Invalid response.  Exit.
          printLog(ERR, "Received redirect status without Location header.\n");
          break;
        }
        
        remoteHostAddressToUse = newLocation;
        if (strncmp(remoteHostAddressToUse, "https://", 8) == 0) {
          remoteHostAddressToUse += 8;
          if (tlsSocketsEnabled() == true) {
            socketMode = TLS;
          } else {
            printLog(ERR, "Request to communicate over HTTPS without TLS support.\n");
            break;
          }
        } else if (strncmp(remoteHostAddressToUse, "http://", 7) == 0) {
          remoteHostAddressToUse += 7;
        }
        
        char *slashAt = (char*) strchr(remoteHostAddressToUse, '/');
        if (slashAt != NULL) {
          locationToUse = slashAt;
        } else {
          locationToUse = "/";
        }
        
        rbTreeRemove(_redirects, key);
        
        if ((strncmp(newLocation, remoteHostAddress,
            strlen(remoteHostAddress)) == 0)
          && (strcmp(locationToUse, location) == 0)
        ) {
          // We were redirected to the same place we started.  We can't do this.
          // This would create an infinite loop.  Bail.
          break;
        }
        
        // Create a redirect entry so that we skip over this in the future.
        redirect = rbTreeCreate(typeString);
        locationToUse = (char*) rbTreeAddEntry(redirect,
          "location", locationToUse, typeString)->value;
        if (slashAt != NULL) {
          *slashAt = '\0';
        }
        rbTreeAddEntry(redirect,
          "remoteHostAddress", newLocation, typeString);
        rbTreeAddEntry(_redirects, key, redirect,
          typeDictionaryNoCopy)->type = typeDictionary;
      } else {
        // Not something we can handle.
        printLog(WARN, "Received unhandled status code %d from server.\n",
          statusCode);
        break;
      }
    }
  }
  
  SCOPE_EXIT("method=%s, remoteHostAddress=%s, location=%s, "
    "timeoutMilliseconds=%d, request=%p", "%s", method, remoteHostAddress,
    location, timeoutMilliseconds, request, response);
  return response;
}

