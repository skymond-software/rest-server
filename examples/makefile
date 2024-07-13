CC32     := gcc -m32 -march=i686 -mtune=i686
CC       := gcc
CXX      := g++
MKDIR    := mkdir -p
BUILDDIR := ../build
FLAGS    := -rdynamic

SRC_FILES := \
    ../lib/cnext/src/Coroutines.c \
    ../lib/cnext/src/DataTypes.c \
    ../lib/cnext/src/Dictionary.c \
    ../lib/cnext/src/DirectoryLib.c \
    ../lib/cnext/src/HashTable.c \
    ../lib/cnext/src/List.c \
    ../lib/cnext/src/PosixCThreads.c \
    ../lib/cnext/src/PosixProcesses.c \
    ../lib/cnext/src/Queue.c \
    ../lib/cnext/src/RedBlackTree.c \
    ../lib/cnext/src/Scope.c \
    ../lib/cnext/src/Sockets.c \
    ../lib/cnext/src/Stack.c \
    ../lib/cnext/src/StringLib.c \
    ../lib/cnext/src/Vector.c \
    ../lib/cnext/src/WinCThreads.c \
    ../lib/cnext/src/WinProcesses.c \
    ../src/DbClientLib.c \
    ../src/LoggingLib.c \
    ../src/MariaDbLib.c \
    ../src/SqlClientLib.c \
    ../src/SqliteLib.c \
    ../src/WebClientLib.c \
    ../src/WebServerLib.c \

INCLUDES := \
    -I../include \
    -I../lib/cnext/include \

LINKS := \
    -lpthread \
    -lcrypto \

DEFINES := \
    -DWsResponseObject=RedBlackTree \
    -DWsRequestObject=RedBlackTree \
    -DWsResponseNode=RedBlackNode \
    -DWsRequestNode=RedBlackNode \
    -DwsResponseObjectDestroy=rbTreeDestroy \
    -DwsResponseObjectToJson=rbTreeToJson \

all:
	$(MKDIR) $(BUILDDIR)
	$(CC) -c ../src/sqlite3.c -o $(BUILDDIR)/sqlite3.o
	$(CXX) $(FLAGS) $(INCLUDES) $(DEFINES) $(BUILDDIR)/sqlite3.o $(SRC_FILES) ExampleService.c $(LINKS) -o $(BUILDDIR)/web-service

32:
	$(MKDIR) $(BUILDDIR)
	$(CC32) $(FLAGS) $(INCLUDES) $(DEFINES) ../src/sqlite3.c $(SRC_FILES) ExampleService.c $(LINKS) -o $(BUILDDIR)/web-service

