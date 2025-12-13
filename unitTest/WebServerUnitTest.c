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

#include "WebServerLib.h"
// LoggingLib is not optional for this library.
#include "LoggingLib.h"
#include "HashTable.h"
#include "OsApi.h"

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
  
  Dictionary *outputParams = dictionaryCreate(typeString);
  dictionaryAddEntry(outputParams,
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
  xmlToRedBlackTree,                       // deserializeFromXml
  rbTreeCreate_,                           // wsRequestObjectCreate
  (WsSerializeToJson) listToJson,          // serializeToJson
  jsonToRedBlackTree,                      // deserializeFromJson
  rbTreeDestroy,                           // requestObjectDestroy
  rbTreeDestroy,                           // responseObjectDestroy
  rbTreeGetValue,                          // getRequestValue
  rbTreeGetValue,                          // getResponseValue
  NULL,                                    // registerThread
  NULL,                                    // unregisterThread
  rbTreeAddEntry_,                         // addRequestValue_
  wcAddResponseValue_,                     // addResponseValue_
  rbTreeRemove,                            // removeResponseValue
  (WsRequestObjectToString) listToString,  // requestObjectToString
  (WsResponseObjectToString) listToString, // responseObjectToString
  NULL,                                    // context
};

bool webServerUnitTest(void) {
  const char *indexHtmlContent = "Hello world!";
  size_t indexHtmlSize = strlen(indexHtmlContent);
  
  putFileContent("/tmp/index.html", indexHtmlContent, indexHtmlSize);
  WebServerCreateOptions webServerCreateOptions = {
    .interfacePath = "/tmp",
    .serverName = "UnitTestServer",
    .timeout = 15,
    .socketMode = PLAIN,
    .certificate = NULL,
    .key = NULL,
    .redirectProtocol = NULL,
    .redirectPort = 0,
    .redirectFunction = 0,
    .webService = 0,
  };
  WebServer* webServer = webServerCreate(8999, &webServerCreateOptions);
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
  
  webServerCreateOptions.socketMode = TLS;
  webServer = webServerCreate(9000, &webServerCreateOptions);
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
  
  webServerCreateOptions.socketMode = PLAIN;
  webServerCreateOptions.redirectProtocol = "https";
  webServerCreateOptions.redirectPort = 1;
  webServer = webServerCreate(9001, &webServerCreateOptions);
  
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
  
  webServerCreateOptions.socketMode = TLS;
  webServerCreateOptions.redirectProtocol = NULL;
  webServerCreateOptions.redirectPort = 0;
  webServerCreateOptions.webService = &unitTestWebService;
  webServer = webServerCreate(9002, &webServerCreateOptions);
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
  
  webServerCreateOptions.interfacePath = NULL;
  webServerCreateOptions.socketMode = PLAIN;
  webServerCreateOptions.redirectProtocol = NULL;
  webServerCreateOptions.redirectPort = 0;
  webServerCreateOptions.redirectFunction = redirectUnitTestFunction;
  webServerCreateOptions.webService = &unitTestWebService;
  WebServer *redirectServer = webServerCreate(9003, &webServerCreateOptions);
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

