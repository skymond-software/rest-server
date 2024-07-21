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

#include "WebServerLib.h"
#include "WebClientLib.h"
#include "RedBlackTree.h"
#include "LoggingLib.h"

/// @struct ExampleService
///
/// @brief Container that will hold the context for this service.
typedef struct ExampleService {
  RedBlackTree *currentSessionTokens;
} ExampleService;

/// @def VALID_SESSION_TOKEN
///
/// @brief Value for a valid session token (i.e. non-NULL).
#define VALID_SESSION_TOKEN ((void*) ((intptr_t) 0x1))

/// @fn WsResponseObject *login(WebService *webService, WsConnectionInfo *wsConnectionInfo)
///
/// @brief Login to the web service.
///
/// @param webService A pointer to the WebService that represents the service
///   being handled.
/// @param wsConnectionInfo A pointer to the WsConnectionInfo for the current
///   client connection.
///
/// @note wsConnectionInfo->functionParams contains the deserialized POST
/// portion of the input.  Within that:
/// - username The name of the user logging in.
/// - password The password of the user logging in.
///
/// @return Returns a serializable object containing the result of the request.
/// - type The type of response being delivered (good or bad).
/// - status The human-readable description of the response.
/// - sessionToken A numeric token to use in further API calls on success.
///   Not provided on failure.
WsResponseObject *login(WebService *webService,
  WsConnectionInfo *wsConnectionInfo
) {
  ExampleService *exampleService = (ExampleService*) webService->context;
  RedBlackTree *currentSessionTokens = exampleService->currentSessionTokens;
  WsRequestObject *inputParams = wsConnectionInfo->functionParams;
  WsResponseObject *outputParams = NULL;
  
  char *username
    = (char*) webService->getRequestValue(inputParams, "username");
  if (username == NULL) {
    printLog(ERR, "username value not present in inputParams.\n");
    webService->addResponseValue(&outputParams, "type", "badLoginResponse");
    webService->addResponseValue(&outputParams,
      "status", "username not present.");
    return outputParams;
  }
  
  char *password
    = (char*) webService->getRequestValue(inputParams, "password");
  if (password == NULL) {
    printLog(ERR, "password value not present in inputParams.\n");
    webService->addResponseValue(&outputParams, "type", "badLoginResponse");
    webService->addResponseValue(&outputParams,
      "status", "password not present.");
    return outputParams;
  }
  
  if (strcmp(username, password) != 0) {
    printLog(ERR, "Invavlid credentials:  %s/%s\n", username, password);
    webService->addResponseValue(&outputParams, "type", "badLoginResponse");
    webService->addResponseValue(&outputParams,
      "status", "Invalid credentials.");
    return outputParams;
  }
  
  i64 sessionToken = rand();
  rbTreeAddEntry(currentSessionTokens, &sessionToken, VALID_SESSION_TOKEN,
    typePointerNoCopy);
  
  webService->addResponseValue(&outputParams, "type", "goodLoginResponse");
  webService->addResponseValue(&outputParams,
    "status", "Returning good status.");
  webService->addResponseValue(&outputParams, "sessionToken", &sessionToken,
    typeI64);
  
  return outputParams;
}

/// @fn WsResponseObject *logout(WebService *webService, WsConnectionInfo *wsConnectionInfo)
///
/// @brief Login to the web service.
///
/// @param webService A pointer to the WebService that represents the service
///   being handled.
/// @param wsConnectionInfo A pointer to the WsConnectionInfo for the current
///   client connection.
///
/// @note wsConnectionInfo->functionParams contains the deserialized POST
/// portion of the input.  Within that:
/// - username The name of the user logging in.
/// - password The password of the user logging in.
///
/// @return Returns a serializable object containing the result of the request.
/// - type The type of response being delivered (good or bad).
/// - status The human-readable description of the response.
/// - sessionToken A numeric token to use in further API calls on success.
///   Not provided on failure.
WsResponseObject *logout(WebService *webService,
  WsConnectionInfo *wsConnectionInfo
) {
  ExampleService *exampleService = (ExampleService*) webService->context;
  RedBlackTree *currentSessionTokens = exampleService->currentSessionTokens;
  WsRequestObject *inputParams = wsConnectionInfo->functionParams;
  WsResponseObject *outputParams = NULL;
  
  i64 *sessionToken
    = (i64*) webService->getRequestValue(inputParams, "sessionToken");
  if (sessionToken == NULL) {
    printLog(ERR, "No sessionToken provided.\n");
    webService->addResponseValue(&outputParams, "type", "badLogoutResponse");
    webService->addResponseValue(&outputParams,
      "status", "Missing sessionToken.");
    return outputParams;
  } else if (!rbTreeGetValue(currentSessionTokens, sessionToken)) {
    printLog(ERR, "%lld is not a current session.\n", lld(*sessionToken));
    webService->addResponseValue(&outputParams, "type", "badLogoutResponse");
    webService->addResponseValue(&outputParams,
      "status", "Invalid sessionToken.");
    return outputParams;
  }
  
  rbTreeRemove(currentSessionTokens, sessionToken);
  
  webService->addResponseValue(&outputParams, "type", "goodLogoutResponse");
  webService->addResponseValue(&outputParams,
    "status", "Successfully logged out.");
  
  return outputParams;
}

/// @var exampleServiceFunctions
///
/// @brief Array of WsFunctionDescrirptors with the names and function
/// pointers of the functions tha make up the web service.
WsFunctionDescriptor exampleServiceFunctions[] = {
  {"login", login},
  {"logout", logout},
  {NULL, NULL}
};

/// @var exampleServiceFunctionDescriptors
///
/// @brief Array of the WsFunctionDescriptor arrays that make up the full set
/// of functionality supported in the web service namespace.
WsFunctionDescriptor *exampleServiceFunctionDescriptors[] = {
  exampleServiceFunctions,
  NULL
};

/// @var exampleServerNamespaces
///
/// @brief Array of WsNamespaces with the names of the namespaces served by the
/// web serivce and the WsFunctionDescriptors that provide the functionality
/// within the namespaces.
WsNamespace exampleServerNamespaces[] = {
  {"webService", exampleServiceFunctionDescriptors},
  {NULL, NULL}
};

/// @var webService
///
/// @brief The WebService instance that fully describes the web service provided
/// by the web server instance.
WebService webService = {
  exampleServerNamespaces,                 // namespaces
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

/// @fn int main(int argc, char **argv)
///
/// @brief Entry point for the program.
///
/// @param argc The number of command line arguments as an integer.
/// @param argv A one-dimensional array of C strings with the values of the
///   command line arguments.
///
/// @return Returns 0 on success.  Any other value is an error.
int main(int argc, char **argv) {
  (void) argc;
  (void) argv;
  
  ExampleService exampleService;
  exampleService.currentSessionTokens = rbTreeCreate(typeI64);
  webService.context = &exampleService;
  
  WebServer* webServer = webServerCreate(
    /*interfacePath=*/ ".", /*portNumber=*/ 9000,
    /*serverName=*/ "ExampleServer/1.0", /*timeout=*/ 15,
    /*socketMode=*/ PLAIN, /*certificate=*/ NULL,
    /*key=*/ NULL, /*redirectProtocol=*/ NULL, /*redirectPort=*/ 0,
    /*redirectFunction=*/ NULL, /*webService=*/ &webService
  );
  if (webServer == NULL) {
    printLog(ERR, "webServerCreate returned NULL.\n");
    return 1;
  }
  for (int i = 0; (i < 15) && (webServer->socket == NULL); i++) sleep(1);
  if (webServer->socket == NULL) {
    printLog(ERR, "webServer failed to initalize.\n");
    return 1;
  }
  
  while (1) {
    sleep(1);
  }
  
  webServer = webServerDestroy(webServer);
  
  return 0;
}

