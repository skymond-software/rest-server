################################################################################
##                                                                            ##
##            (c) Copyright 2022 James Card. All Rights Reserved.             ##
##                                                                            ##
##                         https://www.jamescard.org                          ##
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
import logging
import argparse
import os
import time
import sys
import inspect


### Globals


# Root log levels.
STEP     = 70
BOX      = 60
CRITICAL = logging.CRITICAL # 50
ERR      = logging.ERROR    # 40
WARN     = logging.WARNING  # 30
INFO     = logging.INFO     # 20
DETAIL   = 15
DEBUG    = logging.DEBUG    # 10
TRACE    = 5
FLOOD    = 2
NEVER    = logging.NOTSET   #0

logLevelNames = {
    STEP:     "STEP",
    BOX:      "BOX",
    CRITICAL: "CRITICAL",
    ERR:      "ERROR",
    WARN:     "WARN",
    INFO:     "INFO",
    DETAIL:   "DETAIL",
    DEBUG:    "DEBUG",
    TRACE:    "TRACE",
    FLOOD:    "FLOOD",
    NEVER:    "NEVER",
}

# Root-level log functions.
step = 1
def logStep(message: str="", *args, **kwargs) -> None: # {
    """
    Log a step message.

    Parameters:
        message (str): The message to be logged.
        args (list): Any additional arguments passed as a list.
        kwargs (dict): Any additional named arguments passed as a dict.

    Returns:
        This function returns no value.
    """
    global step
    message = "Step " + str(step) + ": " + message
    step += 1
    logging.log(STEP, message, args, **kwargs)
# }

def logBox(message: str="", *args, **kwargs) -> None: # {
    """
    Log a message with an ASCII box around it.

    Parameters:
        message (str): The message to be logged.
        args (list): Any additional arguments passed as a list.
        kwargs (dict): Any additional named arguments passed as a dict.

    Returns:
        This function returns no value.
    """
    logging.log(BOX, message, args, **kwargs)
# }

def logError(message: str="", *args, **kwargs) -> None: # {
    """
    Log an ERR-level message.

    Parameters:
        message (str): The message to be logged.
        args (list): Any additional arguments passed as a list.
        kwargs (dict): Any additional named arguments passed as a dict.

    Returns:
        This function returns no value.
    """
    logging.log(ERR, message, args, **kwargs)
# }

def logDetail(message: str="", *args, **kwargs) -> None: # {
    """
    Log a DETAIL-level message.

    Parameters:
        message (str): The message to be logged.
        args (list): Any additional arguments passed as a list.
        kwargs (dict): Any additional named arguments passed as a dict.

    Returns:
        This function returns no value.
    """
    logging.log(DETAIL, message, args, **kwargs)
# }

def logTrace(message: str="", *args, **kwargs) -> None: # {
    """
    Log a TRACE-level message.

    Parameters:
        message (str): The message to be logged.
        args (list): Any additional arguments passed as a list.
        kwargs (dict): Any additional named arguments passed as a dict.

    Returns:
        This function returns no value.
    """
    logging.log(TRACE, message, args, **kwargs)
# }

def logFlood(message: str="", *args, **kwargs) -> None: # {
    """
    Log a FLOOD-level message.

    Parameters:
        message (str): The message to be logged.
        args (list): Any additional arguments passed as a list.
        kwargs (dict): Any additional named arguments passed as a dict.

    Returns:
        This function returns no value.
    """
    logging.log(FLOOD, message, args, **kwargs)
# }

class Formatter(logging.Formatter): # {
    """
    Class for the custom formatting our Logger instance will use.
    """
    def __init__(self) -> None: # {
        """
        Constructor for custom Formatter class based off of logging.Formatter.
        Used in handlers we set up for our custom logger.

        Parameters:
            self (Formatter): A Formatter object to be initialized.

        Returns:
            This function returns no value.
        """
        super(Formatter, self).__init__()

        hostname = os.getenv("HOSTNAME")
        if ((hostname is None) or (hostname == "")):
            hostname = os.getenv("COMPUTERNAME")
        if ((hostname is None) or (hostname == "")):
            hostname = "localhost"
        self.hostname = hostname

        self.step = 1
    # }

    def getBoxHeader(self, message: str="|  |") -> str: # {
        """
        Get the line used at the top and bottom of an ASCII text box.  This line
        will be the same length as the input string, will begin and end with a
        '+', and will contain '-' characters between the endpoints.

        Parameters:
            self (Formatter): A previously-initialized Formatter object.
            message (str): The message to build a header for.  This string is
                expected to already begin with "| ", end with " |", and contain
                the message to be logged between the endpoints.

        Returns:
            A string the same length as the input that can be used as the top
            and bottom of an ASCII text box.
        """
        dashLength = len(message) - 2
        dashIndex = 0
        header = "+"
        while (dashIndex < dashLength):
            header += "-"
            dashIndex += 1
        header += "+"

        return header
    # }

    def format(self, record: logging.LogRecord) -> str: # {
        """
        Format a LogRecord into an string to be logged by the logger.

        Parameters:
            self (Formatter): A previously-initialized Formatter object.
            record (logging.LogRecord): A logging.LogRecord instance containing
                the information to be logged to the logger.

        Returns:
            A fully-formatted log message to be routed to the logger.
        """
        message = record.msg

        processId = record.process
        if (processId is None):
            processId = 0

        threadId = record.thread
        if (threadId is None):
            threadId = 0

        fileName = \
            sys.modules[inspect.getmodule(inspect.stack()[8][0]).__name__].__file__
        if (fileName is not None):
            if ("/" in fileName):
                fileName = fileName.split("/")[-1]
            elif ("\\" in fileName):
                fileName = fileName.split("\\")[-1]
        else:
            fileName = record.filename

        lineNumber = inspect.getframeinfo(inspect.stack()[8][0]).lineno
        if (lineNumber is None):
            lineNumber = 0

        functionName = inspect.stack()[8].function
        messageHeader = ("[%s %s:%u.%u %s:%s.%d %s] " % (
                Logger.getTimestamp(record.created), self.hostname,
                processId, threadId, fileName, functionName,
                lineNumber, logLevelNames[record.levelno]
            )
        )

        formattedMessage = ""
        if (record.levelno < BOX):
            formattedMessage = messageHeader + message

        else:
            if (record.levelno == STEP):
                message = "Step " + str(self.step) + ": " + message
                self.step += 1
            message = "| " + message + " |"
            boxHeader = self.getBoxHeader(message)

            formattedMessage  = messageHeader + "\n"
            formattedMessage += messageHeader + boxHeader + "\n"
            formattedMessage += messageHeader + message   + "\n"
            formattedMessage += messageHeader + boxHeader

        return formattedMessage
    # }

# }

class Logger(logging.Logger): # {
    """
    Class for doing custom logging.
    """

    # Supported log levels.
    STEP     = STEP
    BOX      = BOX
    CRITICAL = CRITICAL
    ERR      = ERR
    WARN     = WARN
    INFO     = INFO
    DETAIL   = DETAIL
    DEBUG    = DEBUG
    TRACE    = TRACE
    FLOOD    = FLOOD
    NEVER    = NEVER

    def __init__(self, args: argparse.Namespace=None) -> None: # {
        """
        Constructor for the Logger class.

        Parameters:
            self (Logger): A Logger object to initialize.
            args (argparse.Namespace): Parsed command line arguments, if any.

        Returns:
            This method returns no value.
        """
        logDir = "."
        if ((args is not None) and (args.logDir is not None)):
            logDir = args.logDir

        hostname = os.getenv("HOSTNAME")
        if ((hostname is None) or (hostname == "")):
            hostname = os.getenv("COMPUTERNAME")
        if ((hostname is None) or (hostname == "")):
            hostname = "localhost"

        timestamp = Logger.getTimestamp()
        # Fix invalid filename characters.
        timestampList = list(timestamp)
        timestampList[10] = '_' # underscore
        timestampList[13] = '-' # hyphen
        timestampList[16] = '-' # hyphen
        timestamp = "".join(timestampList)
        if (args is not None):
            logFilename = logDir + "/" + args.prog + "_" \
                + hostname + "_" + timestamp + ".log"
        else:
            logFilename = logDir + "/" + __name__ + "_" \
                + hostname + "_" + timestamp + ".log"

        super(Logger, self).__init__(name=logFilename)
        self.logFilename = logFilename

        # Add the global loggers
        logging.addLevelName(self.STEP, "STEP")
        setattr(logging, "STEP", self.STEP)
        setattr(logging, "step", logStep)

        logging.addLevelName(self.BOX, "BOX")
        setattr(logging, "BOX", self.BOX)
        setattr(logging, "box", logBox)

        logging.addLevelName(self.ERR, "ERROR")
        setattr(logging, "ERR", self.ERR)
        setattr(logging, "error", logError)

        logging.addLevelName(self.DETAIL, "DETAIL")
        setattr(logging, "DETAIL", self.DETAIL)
        setattr(logging, "detail", logDetail)

        logging.addLevelName(self.TRACE, "TRACE")
        setattr(logging, "TRACE", self.TRACE)
        setattr(logging, "trace", logTrace)

        logging.addLevelName(self.FLOOD, "FLOOD")
        setattr(logging, "FLOOD", self.FLOOD)
        setattr(logging, "flood", logFlood)

        self.fileHandler = None
        if (args is not None):
            # Create the file logger.
            formatter = Formatter()
            self.fileHandler = logging.FileHandler(self.logFilename)
            self.fileHandler.setLevel(self.FLOOD)
            self.fileHandler.setFormatter(formatter)
            self.addHandler(self.fileHandler)

        # Create the stdout logger.
        formatter = Formatter()
        self.streamHandler = logging.StreamHandler()
        self.streamHandler.setLevel(self.FLOOD)
        self.streamHandler.setFormatter(formatter)
        self.addHandler(self.streamHandler)
    # }

    @classmethod
    def getTimestamp(cls, timeToGet: float=0.0) -> str: # {
        """
        Get a standard timestamp, presumably (but not necessarily) for logging
        purposes.  This is a class method, not an object method.

        Parameters:
            cls (Logger): A Logger class to work from.
            timeToGet (float): A time to convert, as returned by time.time().

        Returns:
            A string containing a timestamp in YYYY-MM-DD hh:mm:ss.NNNNNNNNN
            format.
        """
        if (timeToGet == 0.0):
            timeToGet = time.time()
        nanoseconds = int((timeToGet - int(timeToGet)) * 1000000000)
        return ("%s.%09d" % (
            time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(timeToGet)),
            nanoseconds)
        )
    # }

    def step(self, message: str="", *args, **kwargs) -> None: # {
        """
        Log a step message.

        Parameters:
            self (Logger): A previously-initialized Logger object to log with.
            message (str): The message to be logged.
            args (list): Any additional arguments passed as a list.
            kwargs (dict): Any additional named arguments passed as a dict.

        Returns:
            This function returns no value.
        """
        if (self.isEnabledFor(self.STEP)):
            self._log(self.STEP, message, args, **kwargs)
    # }

    def box(self, message: str="", *args, **kwargs) -> None: # {
        """
        Log a message with an ASCII box around it.

        Parameters:
            self (Logger): A previously-initialized Logger object to log with.
            message (str): The message to be logged.
            args (list): Any additional arguments passed as a list.
            kwargs (dict): Any additional named arguments passed as a dict.

        Returns:
            This function returns no value.
        """
        if (self.isEnabledFor(self.BOX)):
            self._log(self.BOX, message, args, **kwargs)
    # }

    def err(self, message: str="", *args, **kwargs) -> None: # {
        """
        Log a ERR-level message.

        Parameters:
            self (Logger): A previously-initialized Logger object to log with.
            message (str): The message to be logged.
            args (list): Any additional arguments passed as a list.
            kwargs (dict): Any additional named arguments passed as a dict.

        Returns:
            This function returns no value.
        """
        if (self.isEnabledFor(self.ERR)):
            self._log(self.ERR, message, args, **kwargs)
    # }

    def detail(self, message: str="", *args, **kwargs) -> None: # {
        """
        Log a DETAIL-level message.

        Parameters:
            self (Logger): A previously-initialized Logger object to log with.
            message (str): The message to be logged.
            args (list): Any additional arguments passed as a list.
            kwargs (dict): Any additional named arguments passed as a dict.

        Returns:
            This function returns no value.
        """
        if (self.isEnabledFor(self.DETAIL)):
            self._log(self.DETAIL, message, args, **kwargs)
    # }

    def trace(self, message: str="", *args, **kwargs) -> None: # {
        """
        Log a TRACE-level message.

        Parameters:
            self (Logger): A previously-initialized Logger object to log with.
            message (str): The message to be logged.
            args (list): Any additional arguments passed as a list.
            kwargs (dict): Any additional named arguments passed as a dict.

        Returns:
            This function returns no value.
        """
        if (self.isEnabledFor(self.TRACE)):
            self._log(self.TRACE, message, args, **kwargs)
    # }

    def flood(self, message: str="", *args, **kwargs) -> None: # {
        """
        Log a FLOOD-level message.

        Parameters:
            self (Logger): A previously-initialized Logger object to log with.
            message (str): The message to be logged.
            args (list): Any additional arguments passed as a list.
            kwargs (dict): Any additional named arguments passed as a dict.

        Returns:
            This function returns no value.
        """
        if (self.isEnabledFor(self.FLOOD)):
            self._log(self.FLOOD, message, args, **kwargs)
    # }

    def logCommandLine(self) -> None: # {
        """
        Log the command line that was used to start this script.

        Parameters:
            self (Logger): A previously-initialized Logger object to log with.

        Returns:
            This method returns no value.
        """
        commandLine = ""
        for arg in sys.argv:
            commandLine += arg + " "

        self.critical("Command line: " + commandLine)
    # }

    def setLogThreshold(self, logThreshold: int): # {
        """
        Set the log threshold for the logger.

        Parameters:
            self (Logger): A previously-initialized Logger object to configure.
            logThreshold (int): The new log thershold to apply.

        Returns:
            This method returns no value.
        """
        # Note:  It might be better to just make thie an alias for setLevel().
        if (self.fileHandler is not None):
            # Delete the old file handler.
            self.removeHandler(self.fileHandler)

            # Create the file logger.
            formatter = Formatter()
            self.fileHandler = logging.FileHandler(self.logFilename)
            self.fileHandler.setLevel(logThreshold)
            self.fileHandler.setFormatter(formatter)
            self.addHandler(self.fileHandler)

        # Delete the old stream handler.
        self.removeHandler(self.streamHandler)

        # Create the stdout logger.
        formatter = Formatter()
        self.streamHandler = logging.StreamHandler()
        self.streamHandler.setLevel(logThreshold)
        self.streamHandler.setFormatter(formatter)
        self.addHandler(self.streamHandler)
    # }

# }

