////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//                     Copyright (c) 2012-2025 James Card                     //
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

#include "Dictionary.h"
#include "Vector.h"

#ifdef DS_LOGGING_ENABLED
#include "LoggingLib.h"
#else
#undef printLog
#define printLog(...) {}
#undef shouldLog
#define shouldLog(...) (false)
#define logFile stderr
#endif

// Dictionary functions.
Dictionary* (*jsonToDictionary)(const char *jsonText, long long int *position)
  = jsonToRedBlackTree;
char* (*dictionaryToString)(const Dictionary *dictionary)
  = (char* (*)(const Dictionary*)) listToString;
Dictionary* (*dictionaryCreate_)(TypeDescriptor *type, bool disableThreadSafety,
  ...) = rbTreeCreate_;
DictionaryEntry* (*dictionaryAddEntry_)(Dictionary *dictionary,
  const volatile void *key, const volatile void *value, TypeDescriptor *type,
  ...) = rbTreeAddEntry_;
int (*dictionaryRemove)(Dictionary*, const volatile void*)
  = (int (*)(Dictionary*, const volatile void*)) rbTreeRemove;
DictionaryEntry* (*dictionaryGetEntry)(const Dictionary*, const volatile void*)
  = (DictionaryEntry* (*)(const Dictionary*, const volatile void*))
  rbTreeGetEntry;
int (*dictionaryCompare)(const Dictionary *dictionaryA,
  const Dictionary *dictionaryB) = rbTreeCompare;
Dictionary* (*dictionaryDestroy)(Dictionary*) = rbTreeDestroy;
Bytes (*dictionaryToXml_)(
  const Dictionary *dictionary, const char *elementName, bool indent, ...)
  = (Bytes (*)(const Dictionary *, const char *, bool, ...)) listToXml_;
void* (*dictionaryGetValue)(
  const Dictionary *dictionary, const volatile void *key) = rbTreeGetValue;
List* (*dictionaryToList)(Dictionary *dictionary)
  = (List* (*)(Dictionary*)) listCopy;
Bytes (*dictionaryToJson)(const Dictionary *dictionary)
  = (Bytes (*)(const Dictionary *)) listToJson;
char* (*dictionaryToKeyValueString)(const Dictionary *dictionary,
  const char *separator)
  = (char* (*)(const Dictionary*, const char*)) listToKeyValueString;
Dictionary* (*dictionaryCopy)(const Dictionary *dictionary) = rbTreeCopy;
Dictionary* (*listToDictionary)(const List *list) = listToRbTree;
Dictionary* (*dictionaryFromBlob_)(const volatile void *array, u64 *length,
  bool inPlaceData, bool disableThreadSafety, ...) = rbTreeFromBlob_;
Bytes (*dictionaryToBlob)(const Dictionary *dictionary)
  = (Bytes (*)(const Dictionary*)) listToBlob;
Dictionary* (*xmlToDictionary)(const char *inputData) = xmlToRedBlackTree;
int (*dictionaryDestroyNode)(Dictionary *dictionary, DictionaryEntry *node)
  = rbTreeDestroyNode;

/// @fn int keyValueStringToDictionaryEntry(Dictionary *kvList, char *inputString)
///
/// @brief Convert an '='-delimited key-value string to a DictionaryEntry.
///
/// @param kvList is the list to put the parsed key-value pairs into.
/// @param inputString is the string to parse.  It is not modified.
///
/// @return Returns 0 on success, any other value is failure
int keyValueStringToDictionaryEntry(Dictionary *kvList, char *inputString) {
  int returnValue = 0;
  printLog(TRACE, "ENTER keyValueStringToDictionaryEntry(inputString=\"%s\")\n", inputString);
  
  if (inputString != NULL) {
    char *stringCopy = NULL;
    straddstr(&stringCopy, inputString);
    char *equalAt = strstr(stringCopy, "=");
    if (equalAt != NULL) {
      *equalAt = '\0';
      equalAt++;
      char *key = stringCopy;
      char *value = equalAt;
      unescapeString(key);
      unescapeString(value);
      
      returnValue = -(dictionaryAddEntry(kvList, key, value) == NULL);
    }
    stringCopy = stringDestroy(stringCopy);
  }
  
  printLog(TRACE, "EXIT keyValueStringToDictionaryEntry(inputString=\"%s\") = {\"%d\"}\n", inputString, returnValue);
  return returnValue;
}

/// @fn Dictionary *kvStringToDictionary(const char *inputString, const char *separator)
///
/// @brief Convert a separator-separated key-value list to a Dictionary
///
/// @param inputString is the string to parse.  It is not modified.
/// @param separator is the string that divides the key-value pairs.
///
/// @return Returns a pointer to a Dictionary on success, NULL on failure.
Dictionary *kvStringToDictionary(const char *inputString,
  const char *separator
) {
  Dictionary *returnValue = dictionaryCreate(typeString);
  printLog(TRACE,
    "ENTER kvStringToDictionary(inputString=\"%s\", separator=\"%s\")\n",
    inputString, separator);
  
  int separatorLength = strlen(separator);
  if ((inputString != NULL) && (separator != NULL)) {
    char *stringCopy = NULL;
    straddstr(&stringCopy, inputString);
    char *originalStringCopy = stringCopy;
    
    char *separatorAt = strstr(stringCopy, separator);
    while (separatorAt != NULL) {
      *separatorAt = '\0';
      if (keyValueStringToDictionaryEntry(returnValue, stringCopy) != 0) {
        printLog(ERR, "Could not add \"%s\" to new Dictionary.\n",
          stringCopy);
      }
      separatorAt += separatorLength;
      stringCopy = separatorAt;
      separatorAt = strstr(separatorAt, separator);
    }
    if (keyValueStringToDictionaryEntry(returnValue, stringCopy) != 0) {
      printLog(ERR, "Could not add \"%s\" to new Dictionary.\n",
        stringCopy);
    }
    originalStringCopy = stringDestroy(originalStringCopy);
  }
  
  printLog(TRACE,
    "EXIT kvStringToDictionary(inputString=\"%s\", separator=\"%s\") = {\"%p\"}\n",
    inputString, separator, returnValue);
  return returnValue;
}

/// @fn Dictionary *parseCommandLine(int argc, char **argv)
///
/// @brief Parse any command-line arguments passed in and create a dictionary
///   representation.
///
/// @param argc is the number of arguments passed in from the command line
/// @param argv is an array of strings representing the arguments passed in
///
/// @return A Dictionary representing the arguments on success, NULL otherwise.
Dictionary *parseCommandLine(int argc, char **argv) {
  Dictionary *returnValue = NULL;
  u32 unnamedParameterIndex = 0;
  
  printLog(TRACE, "ENTER parseCommandLine(argc=%d, argv=%p)\n", argc, argv);
  
  returnValue = dictionaryCreate(typeString);
  dictionaryAddEntry(returnValue, "programPath", argv[0]);
  
  if (argc > 1) {
    // There are arguments to parse.  Parse them.
    for (int i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
        // Argument is some sort of flag
        if (argv[i][1] == '-') {
          // Argument is long flag
          char *name = NULL;
          straddstr(&name, &(argv[i][2]));
          if (strchr(name, '=')) {
            // Argument is in the form --name=value
            char *value = strchr(name, '=');
            *value = '\0';
            value++;
            dictionaryAddEntry(returnValue, name, value);
          } else if (i < argc - 1 && argv[i + 1][0] != '-') {
            // The next argument is the value for the argument.
            char *value = argv[i + 1];
            dictionaryAddEntry(returnValue, name, value);
            i++; // Skip over the value for the next pass.
          } else {
            // Argument represents a boolean.
            const char *value = "";
            dictionaryAddEntry(returnValue, name, value);
          }
          name = stringDestroy(name);
        } else {
          // Argument is one or more single-character flags.
          int arglen = strlen(argv[i]);
          char name[2];
          name[0] = '\0';
          name[1] = '\0';
          const char *value = "";
          for (int j = 1; j < arglen - 1; j++) {
            name[0] = argv[i][j];
            dictionaryAddEntry(returnValue, name, value);
          }
          // We've taken care of all but the last flag in this argument.
          name[0] = argv[i][arglen - 1];
          if ((i == argc - 1) || (argv[i + 1][0] == '-')) {
            // Next argument is named or does not exist.
            // This flag is a boolean.
            dictionaryAddEntry(returnValue, name, value);
          } else {
            // Next argument is unnamed.  Use it as the value.
            value = argv[i + 1];
            dictionaryAddEntry(returnValue, name, value);
            i++; // Skip over the value for the next pass.
          }
        }
      } else {
        char *name = NULL;
        char *value = argv[i];
        if (asprintf(&name, "unnamedParameter%u", unnamedParameterIndex++) > 0) {
          dictionaryAddEntry(returnValue, name, value);
          name = stringDestroy(name);
        }
      }
    }
  }
  // else:  No arguments to parse.
  
  printLog(TRACE, "EXIT parseCommandLine(argc=%d, argv=%p) = {%p}\n", argc,
    argv, returnValue);
  return returnValue;
}

/// @fn char* getUserValue(Dictionary *args, const char *argName, const char *prompt, const char *defaultValue)
///
/// @brief Get a value from the user.
///
/// @param args The parsed command line arguments.
/// @param argName The name of the argument on the command line, if provided.
/// @param prompt The prompt to provide the user if there's no command line
///   value.
/// @param defaultValue The default value to provide if the user provides none.
///
/// @return Returns a newly-allocated string value with the appropriate value.
char* getUserValue(Dictionary *args, const char *argName, const char *prompt,
  const char *defaultValue
) {
  char *returnValue = NULL;
  static char buffer[4096];

  char *argValue = (char*) dictionaryGetValue(args, argName);
  if (argValue == NULL) {
    printf("%s [%s]: ", prompt, defaultValue);
    argValue = fgets(buffer, sizeof(buffer), stdin);
    if (*argValue == '\n') {
      argValue = NULL;
    } else {
      // Remove the trailing newline.
      argValue[strlen(argValue) - 1] = '\0';
    }
  }

  if (argValue == NULL) {
    straddstr(&returnValue, defaultValue);
  } else {
    straddstr(&returnValue, argValue);
  }

  return returnValue;
}

