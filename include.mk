MKDIR       := mkdir -p
SRC_DIR     := src
INCLUDE_DIR := include
FLAGS       := -rdynamic
REMOVE      := rm -rf
ARCHIVE     := ar rcs

CNEXT_OBJ_FILES := \
    $(CNEXT_OBJ_DIR)/CThreadsMessages.o \
    $(CNEXT_OBJ_DIR)/Coroutines.o \
    $(CNEXT_OBJ_DIR)/DataTypes.o \
    $(CNEXT_OBJ_DIR)/Dictionary.o \
    $(CNEXT_OBJ_DIR)/DirectoryLib.o \
    $(CNEXT_OBJ_DIR)/Glibc27CompatLib.o \
    $(CNEXT_OBJ_DIR)/HashTable.o \
    $(CNEXT_OBJ_DIR)/List.o \
    $(CNEXT_OBJ_DIR)/Messages.o \
    $(CNEXT_OBJ_DIR)/LinuxProcessSync.o \
    $(CNEXT_OBJ_DIR)/LoggingLib.o \
    $(CNEXT_OBJ_DIR)/PosixCThreads.o \
    $(CNEXT_OBJ_DIR)/PosixProcesses.o \
    $(CNEXT_OBJ_DIR)/Queue.o \
    $(CNEXT_OBJ_DIR)/RandomLib.o \
    $(CNEXT_OBJ_DIR)/RedBlackTree.o \
    $(CNEXT_OBJ_DIR)/RsaLib.o \
    $(CNEXT_OBJ_DIR)/Scope.o \
    $(CNEXT_OBJ_DIR)/Sockets.o \
    $(CNEXT_OBJ_DIR)/SslCertificate.o \
    $(CNEXT_OBJ_DIR)/SslKey.o \
    $(CNEXT_OBJ_DIR)/Stack.o \
    $(CNEXT_OBJ_DIR)/StringLib.o \
    $(CNEXT_OBJ_DIR)/TimeUtils.o \
    $(CNEXT_OBJ_DIR)/Trie.o \
    $(CNEXT_OBJ_DIR)/Vector.o \
    $(CNEXT_OBJ_DIR)/WinCThreads.o \
    $(CNEXT_OBJ_DIR)/WinProcesses.o \
    $(CNEXT_OBJ_DIR)/ZipLib.o \
    $(CNEXT_OBJ_DIR)/miniz.o \

OBJ_FILES := \
    $(OBJ_DIR)/DbClientLib.o \
    $(OBJ_DIR)/MariaDbLib.o \
    $(OBJ_DIR)/SqlClientLib.o \
    $(OBJ_DIR)/SqliteLib.o \
    $(OBJ_DIR)/WebClientLib.o \
    $(OBJ_DIR)/WebServerLib.o \

INCLUDES := \
    -Iinclude \
    -Ilib/cnext/include \

LINKS := \
    -L$(OBJ_DIR) \
    -l:RestServer.a \
    -lm \
    -lz \
    -ldl \
    -Wl,--no-as-needed \
    -l:librt.so.1 \
    -l:libpthread.so.0 \
    -l:libdl.so.2 \
    -Wl,--as-needed \
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
    $(COMMAND_LINE_LINKS) \

WARNINGS := \
    -Wall \
    -Wextra \
    -Werror \

include defines.mk

all: $(OBJ_DIR)/RestServer.a $(EXE_DIR)/sqlite-client

$(OBJ_DIR)/RestServer.a: $(CNEXT_OBJ_DIR)/Cnext.a $(OBJ_FILES) $(MAKEFILE) include.mk $(OBJ_DIR)/sqlite3.o
	$(ARCHIVE) $(OBJ_DIR)/RestServer.a $(OBJ_FILES) $(OBJ_DIR)/sqlite3.o $(CNEXT_OBJ_FILES)

$(EXE_DIR)/sqlite-client: $(SRC_DIR)/SqliteClient.c $(OBJ_DIR)/RestServer.a
	$(MKDIR) $(EXE_DIR)
	$(CXX) $(FLAGS) $(INCLUDES) $(DEFINES) $(WARNINGS) $< $(LINKS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INCLUDE_DIR)/%.h $(MAKEFILE) include.mk
	$(MKDIR) $(OBJ_DIR)
	$(CXX) $(FLAGS) $(INCLUDES) $(DEFINES) $(WARNINGS) -c $< -o $@

$(OBJ_DIR)/sqlite3.o: $(SRC_DIR)/sqlite3.c
	$(MKDIR) $(OBJ_DIR)
	$(CC) -c $(SRC_DIR)/sqlite3.c -o $(OBJ_DIR)/sqlite3.o

cleanall:
	$(REMOVE) $(BUILD_DIR)
	$(MAKE) -C lib/cnext -f makefile cleanall

