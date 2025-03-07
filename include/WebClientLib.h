///////////////////////////////////////////////////////////////////////////////
///
/// @author            James Card
/// Created:           03.10.2016
///
/// @file              WebClientLib.h
///
/// @brief             This is the library of functions specific to the web
///                    client.
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

#ifndef WEB_CLIENT_LIB_H
#define WEB_CLIENT_LIB_H

#include "Dictionary.h"
#include "Sockets.h"

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

#ifndef wsResponseObjectDestroy
#error wsResponseObjectDestroy function *MUST* be defined.
#endif
#define _wsResponseObjectDestroy wsResponseObjectDestroy

#ifndef wsResponseObjectToJson
#error wsResponseObjectToJson *MUST* be defined.
#endif

Dictionary *wcSendSync_(const char* remoteHostAddress, const char *webService, \
  const char *commandName, int timeoutMilliseconds, ...);
/// @def wcSendSync
/// Convenience macro that calls wcSendSync_ with terminating NULL parameter.
#define wcSendSync(remoteHostAddress, webService, commandName, timeoutMilliseconds, ...) \
  wcSendSync_(remoteHostAddress, webService, commandName, timeoutMilliseconds, ##__VA_ARGS__, NULL)
Dictionary *wcSendSyncKvList(const char* remoteHostAddress,
  const char *webService, const char *commandName, int timeoutMilliseconds,
  WsResponseObject *requestList);
Bytes wcSerialize(const char *methodName, WsResponseObject *kvList,
  const char *commandType);
WsResponseNode* wcAddResponseValue_(WsResponseObject **response,
  const char *key, const volatile void *value, TypeDescriptor *type, ...);
#define wcAddResponseValue(response, key, value, ...) \
  wcAddResponseValue_(response, key, value, ##__VA_ARGS__, typeString)
Bytes wcGet(const char *address, const char *location, int timeoutMilliseconds);
Variant wcSendJsonArgs_(const char* remoteHostAddress, const char *webService,
  const char *commandName, int timeoutMilliseconds, ...);
#define wcSendJsonArgs(remoteHostAddress, webService, commandName, timeoutMilliseconds, ...) \
  wcSendJsonArgs_(remoteHostAddress, webService, commandName, timeoutMilliseconds, ##__VA_ARGS__, NULL)
Variant wcSendJsonObject(const char* remoteHostAddress, const char *webService,
  const char *commandName, int timeoutMilliseconds, WsResponseObject *requestObject);
Bytes wcSendRequest(const char *method, const char *remoteHostAddress,
  const char *location, int timeoutMilliseconds, Bytes request);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // WEB_CLIENT_LIB_H
