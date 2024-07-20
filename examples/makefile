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
    ../lib/cnext/src/Glibc27CompatLib.c \
    ../lib/cnext/src/HashTable.c \
    ../lib/cnext/src/List.c \
    ../lib/cnext/src/LoggingLib.c \
    ../lib/cnext/src/PosixCThreads.c \
    ../lib/cnext/src/PosixProcesses.c \
    ../lib/cnext/src/Queue.c \
    ../lib/cnext/src/RandomLib.c \
    ../lib/cnext/src/RedBlackTree.c \
    ../lib/cnext/src/Scope.c \
    ../lib/cnext/src/Sockets.c \
    ../lib/cnext/src/Stack.c \
    ../lib/cnext/src/StringLib.c \
    ../lib/cnext/src/TimeUtils.c \
    ../lib/cnext/src/Vector.c \
    ../lib/cnext/src/WinCThreads.c \
    ../lib/cnext/src/WinProcesses.c \
    ../lib/cnext/src/ZipLib.c \
    ../lib/cnext/src/miniz.c \
    ../src/DbClientLib.c \
    ../src/SqlClientLib.c \
    ../src/SqliteLib.c \
    ../src/WebClientLib.c \
    ../src/WebServerLib.c \

INCLUDES := \
    -I../include \
    -I../lib/cnext/include \

LINKS := \
    -Wl,--no-as-needed \
    -l:librt.so.1 \
    -l:libpthread.so.0 \
    -l:libdl.so.2 \
    -Wl,--as-needed \
    -Wl,--gc-sections \
    -Wl,--wrap=__libc_start_main \
    -Wl,--wrap=dlsym \
    -Wl,--wrap=dlopen \
    -Wl,--wrap=dladdr \
    -Wl,--wrap=dlclose \
    -Wl,--wrap=dlerror \
    -Wl,--wrap=dlvsym \
    -Wl,--wrap=pthread_cancel \
    -Wl,--wrap=pthread_create \
    -Wl,--wrap=pthread_detach \
    -Wl,--wrap=pthread_getspecific \
    -Wl,--wrap=pthread_join \
    -Wl,--wrap=pthread_key_create \
    -Wl,--wrap=pthread_key_delete \
    -Wl,--wrap=pthread_mutexattr_destroy \
    -Wl,--wrap=pthread_mutexattr_init \
    -Wl,--wrap=pthread_mutexattr_settype \
    -Wl,--wrap=pthread_mutex_timedlock \
    -Wl,--wrap=pthread_mutex_trylock \
    -Wl,--wrap=pthread_once \
    -Wl,--wrap=pthread_rwlock_destroy \
    -Wl,--wrap=pthread_rwlock_init \
    -Wl,--wrap=pthread_rwlock_rdlock \
    -Wl,--wrap=pthread_rwlock_unlock \
    -Wl,--wrap=pthread_rwlock_wrlock \
    -Wl,--wrap=pthread_setspecific \
    -Wl,--wrap=clock_gettime \

DEFINES := \
    -DWsResponseObject=RedBlackTree \
    -DWsRequestObject=RedBlackTree \
    -DWsResponseNode=RedBlackNode \
    -DWsRequestNode=RedBlackNode \
    -DwsResponseObjectDestroy=rbTreeDestroy \
    -DwsResponseObjectToJson=rbTreeToJson \

all: $(BUILDDIR)/sqlite3.o
	$(MKDIR) $(BUILDDIR)
	$(CXX) $(FLAGS) $(INCLUDES) $(DEFINES) $(BUILDDIR)/sqlite3.o $(SRC_FILES) ExampleService.c $(LINKS) -o $(BUILDDIR)/web-service

$(BUILDDIR)/sqlite3.o: ../src/sqlite3.c
	$(MKDIR) $(BUILDDIR)
	$(CC) -c ../src/sqlite3.c -o $(BUILDDIR)/sqlite3.o

32:
	$(MKDIR) $(BUILDDIR)
	$(CC32) $(FLAGS) $(INCLUDES) $(DEFINES) ../src/sqlite3.c $(SRC_FILES) ExampleService.c $(LINKS) -o $(BUILDDIR)/web-service

