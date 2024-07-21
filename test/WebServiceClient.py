################################################################################
##                                                                            ##
##                   (c) Copyright 2012-2024 Skymond, LLC.                    ##
##                                                                            ##
##                            https://skymond.io                              ##
##                                                                            ##
## Permission is hereby granted, free of charge, to any person obtaining a    ##
## copy of this software and associated documentation files (the "Software"), ##
## to deal in the Software without restriction, including without limitation  ##
## the rights to use, copy, modify, merge, publish, distribute, sublicense,   ##
## and#or sell copies of the Software, and to permit persons to whom the      ##
## Software is furnished to do so, subject to the following conditions:       ##
##                                                                            ##
## The above copyright notice and this permission notice shall be included    ##
## in all copies or substantial portions of the Software.                     ##
##                                                                            ##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR ##
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   ##
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    ##
## THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER ##
## LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    ##
## FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        ##
## DEALINGS IN THE SOFTWARE.                                                  ##
##                                                                            ##
################################################################################


### Sytem Imports
import requests
import urllib
import sys
import traceback
import re
import Logger
import Randomization
from typing import Union
from typing import Callable


class DefaultResponse(object): # {
    """
    Default object in the event that a web service call does not generate a
    response object.
    """
    def __init__(self): # {
        """
        Constructor for a DefaultResponse object.

        Parameters:
            self (DefaultResponse): A DefaultResponse object being initialized.

        Returns:
            This method returns no value.
        """
        self.type = ""
        self.status = ""
    # }
# }

class PrematureTerminationException(Exception): # {
    """
    Exception to manage the premature (non-passing) exit of a test case.
    """
    def __init__(self, level: int, message: str=""): # {
        super(PrematureTerminationException, self).__init__(message)
        self.level = level
    # }
# }

# Forwarad declaration.
class WebServiceClient(object): # {
    pass
# }

class WebServiceClient(object): # {
    """
    Class for connecting to a web service API on a remote host.
    """
    # Status Codes
    STATUS_PASS       = 0
    STATUS_INCOMPLETE = 1 # The test itself should NEVER exit like this
    # 2 is reserved for BASH script errors
    STATUS_FAIL       = 3 # The core of the test case failed.
    STATUS_BLOCK      = 4 # The setup for the test case failed.
    STATUS_ERROR      = 5 # An error occurred which did not affect the test itself (probably in cleanup).
    STATUS_HELP       = 6 # The test did not run because the user requested help.
    STATUS_DEBUG      = 7 # The test exited prematurely because of a debug call.
    STATUS_TERMINATED = 8 # The test terminated without generating a status itself.  The wrapper is reporting on the test.

    statusNames = {
        STATUS_PASS:       "PASS",
        STATUS_INCOMPLETE: "INCOMPLETE",
        2:                 "BASH_ERROR",
        STATUS_FAIL:       "FAIL",
        STATUS_BLOCK:      "BLOCK",
        STATUS_ERROR:      "ERROR",
        STATUS_HELP:       "HELP",
        STATUS_DEBUG:      "DEBUG",
        STATUS_TERMINATED: "TERMINATED"
    }

    def __init__(self, baseUrl: str, namespace: str): # {
        """
        Constructor for the WebServiceClient class.

        Parameters:
            self (WebServiceClient): A WebServiceClient object being
                initialized.
            baseUrl (str): The full URL of the remote host to connect to.
            namespace (str): The namespace to connect to at the baseUrl.

        Returns:
            This method returns no value.
        """
        self.sessionToken = None

        if (not baseUrl.startswith("http://") and not baseUrl.startswith("https://")):
            # Default to plaintext
            baseUrl = "http://" + baseUrl

        if (not baseUrl.endswith("/")):
            baseUrl += "/"

        baseUrl += namespace + "/"
        self.baseUrl = baseUrl

        self.logger = Logger.Logger()
        self.rand = Randomization.Randomization()
    # }

    ###
    ### Test Infrastructure methods.
    ###

    def setLogThreshold(self, level) -> None: # {
        """
        Set the log level below which we should not log messages.

        Parameters:
            self (WebServiceClient): A WebServiceClient object with an instantiated logger
                member.
            level (int): The log level to set the threshold to.

        Returns:
            This method returns no value.
        """
        __logger.setLevel(level)
    # }

    def term(self, level: int, message: str="",
            cleanupFunction: Union[Callable[[], bool], Callable[[WebServiceClient], bool]]=None,
            *args, **kwargs
    ) -> None: # {
        """
        Terminate the test gracefully, printing out the docstring of the failing
        method.

        Parameters:
            self (WebServiceClient): A WebServiceClient object in the process of execution.
            message (str): The messaage to print out that summarizes the
                termination reason.
            args (list): Any additional arguments passed as a list.
            kwargs (dict): Any additional named arguments passed as a dict.

        Returns:
            This method returns no value.
        """
        self.logger._log(Logger.Logger.CRITICAL, message, args, kwargs)

        if (cleanupFunction is not None):
            # We don't care what the return value of the cleanup function is.
            # The level specified when calling the term function takes
            # precedence.
            cleanupFunction()

        raise PrematureTerminationException(level, message)
    # }

    def getFailingMethodDocstring(self, error: Exception): # {
        """
        Get the docstring from the failing test method.  This is the last method
        in the stack from the file with the main driver in it.

        Parameters:
            self (WebServiceClient): A WebServiceClient object in the process of execution.
            error (Exception): The Exception raised when the test terminated.

        Returns:
            A string containing the docstring of the failing method.
        """
        fileName = sys.modules[self.__module__].__file__

        stackList \
            = traceback.format_exception(None, error, sys.exc_info()[2])
        index = 0
        # Find the first instance of our module
        while ((fileName not in stackList[index])
                and (index < len(stackList))
        ):
            index += 1
        # Find the first instance past our module
        while ((fileName in stackList[index])
                and (index < len(stackList))
        ):
            index += 1

        # Back up to the last instance of our module
        index -= 1
        methodInfo = stackList[index].split("\n")
        methodLine = methodInfo[0]
        matchObj = re.search(
            r"^  File \"[0-9A-Za-z/.]*\", line [0-9]*, in ([A-Za-z]*)",
            methodLine
        )
        methodDocstring = None
        if (matchObj):
            methodName = matchObj.group(1)
            if (methodName):
                method = getattr(self, methodName)
                methodDocstring = method.__doc__

        return methodDocstring
    # }

    def run(self): # {
        """
        Run the main method on the derived test class (if any), log the result,
        and return the status to the caller.

        Parameters:
            self (WebServiceClient): An object derived from WebServiceClient with a main
                method defined.

        Returns:
            The return status of self.main on success, WebServiceClient.STATUS_BLOCK
            on failure.
        """
        returnStatus = self.STATUS_INCOMPLETE # Default until proven otherwise.
        if (hasattr(self, "main")):
            self.logger.critical("Starting " + sys.argv[0])
            self.logger.logCommandLine()
            self.logger.critical("Log file: " + self.logger.logFilename)
            self.logger.critical("Using seed: " + str(self.rand.seed))
            self.logger.critical("")

            # Run the test case proper.
            try:
                returnStatus = self.main()
                if ((returnStatus == WebServiceClient.STATUS_PASS)
                    and (hasattr(self, "cleanup"))
                ) :
                    returnStatus = self.cleanup()
            except PrematureTerminationException as error:
                # This is a graceful exit.  We just need the information from
                # the last function in the stack (before self.term).
                methodDocstring = self.getFailingMethodDocstring(error)
                if (methodDocstring is None):
                    methodDocstring = ""
                self.logger.critical(f"FailingMethodDetails:\n{methodDocstring}")
                # Use the exit status raised in the exception.
                returnStatus = error.level
            except Exception as error:
                # Test ended badly.  Log the exception.
                self.logger.critical("StackTrace:")
                self.logger.critical(traceback.format_exc())
                methodDocstring = self.getFailingMethodDocstring(error)
                if (methodDocstring is None):
                    methodDocstring = ""
                self.logger.critical(f"FailingMethodDetails:\n{methodDocstring}")
                returnStatus = self.STATUS_TERMINATED

        else:
            self.logger.critical("No main method found on test class.")
            self.logger.critical("Cannot execute.")
            returnStatus = WebServiceClient.STATUS_BLOCK

        # Collect and log the result.
        statusName = self.statusNames[returnStatus]
        if (statusName is None):
            statusName = "UNKNOWN"
        self.logger.critical(sys.argv[0] + " exited with status " + statusName)

        # Return status to the caller.
        return returnStatus
    # }

    ###
    ### Web Service methods.
    ###

    def call(self, **kwargs): # {
        """
        Call a method on the remote host using the underlying transport.

        Parameters:
            self (WebServiceClient): A previously-initialized WebServiceClient
                object.
            kwargs (variable named arguments): Argument name and value pairs
                that are appropriate for the method being called.

        Returns:
            A response object from the underlying transport.
        """
        # Generate the request object.
        method = kwargs["method"]
        del kwargs["method"]

        # Call the method on the web service and return the response
        wsResponse = requests.post(self.baseUrl + method, json=kwargs)

        response = DefaultResponse()
        if ((wsResponse is not None) and (wsResponse.json() is not None)):
            response.__dict__ = wsResponse.json()

        if ((hasattr(response, "type") is False) or (response.type is None)
                or (response.type == "")):
            response.type = "bad" + method + "Response"

        if ((hasattr(response, "status") is False) or (response.status is None)
                or (response.status == "")):
            response.status = "Web service call failed."

        response.status = urllib.parse.unquote_plus(response.status)

        return response
    # }

    def login(self, username: str, password: str): # {
        """
        Login to the web service.

        Parameters:
            self (WebServiceClient): A previously-initialized WebServiceClient
                object.
            username (str): The name of the user logging in.
            password (str): The password of the user logging in.

        Returns:
            A response object from the underlying transport.  If the call is
            successful, stores the value of the returned sessionToken as
            self.sessionToken.
        """
        loginResponse = self.call(method="login",
            username=username, password=password)

        if (loginResponse.type == "goodLoginResponse"):
            self.sessionToken = loginResponse.sessionToken

        return loginResponse
    # }

    def logout(self, sessionToken=None): # {
        """
        Logout of the web service.

        Parameters:
            self (WebServiceClient): A previously-initialized WebServiceClient
                object.
            sessionToken (int): The session token from a previous successful
                login call.  If this value is omitted, the value of
                self.sessionToken will be used.

        Returns:
            A response object from the underlying transport.  If the call is
            successful, the self.sessionToken is removed (set to None).
        """
        if (sessionToken is None):
            sessionToken = self.sessionToken

        logoutResponse = self.call(method="logout",
            sessionToken=sessionToken)

        if (logoutResponse.type == "goodLogoutResponse"):
            self.sessionToken = None

        return logoutResponse
    # }

# }

