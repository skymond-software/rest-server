MKDIR       := mkdir -p
SRC_DIR     := src
INCLUDE_DIR := include
FLAGS       := -rdynamic
REMOVE      := rm -rf
ARCHIVE     := ar rcs

CNEXT_OBJ_FILES := \
    $(CNEXT_OBJ_DIR)/Coroutines.o \
    $(CNEXT_OBJ_DIR)/DataTypes.o \
    $(CNEXT_OBJ_DIR)/Dictionary.o \
    $(CNEXT_OBJ_DIR)/DirectoryLib.o \
    $(CNEXT_OBJ_DIR)/Glibc27CompatLib.o \
    $(CNEXT_OBJ_DIR)/HashTable.o \
    $(CNEXT_OBJ_DIR)/List.o \
    $(CNEXT_OBJ_DIR)/LoggingLib.o \
    $(CNEXT_OBJ_DIR)/PosixCThreads.o \
    $(CNEXT_OBJ_DIR)/PosixProcesses.o \
    $(CNEXT_OBJ_DIR)/Queue.o \
    $(CNEXT_OBJ_DIR)/RandomLib.o \
    $(CNEXT_OBJ_DIR)/RedBlackTree.o \
    $(CNEXT_OBJ_DIR)/Scope.o \
    $(CNEXT_OBJ_DIR)/Sockets.o \
    $(CNEXT_OBJ_DIR)/Stack.o \
    $(CNEXT_OBJ_DIR)/StringLib.o \
    $(CNEXT_OBJ_DIR)/TimeUtils.o \
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

include defines.mk

WARNINGS := \
    -Wall \
    -Wextra \
    -Werror \

all: $(OBJ_DIR)/RestServer.a $(EXE_DIR)/sqlite-client

$(OBJ_DIR)/RestServer.a: $(CNEXT_OBJ_DIR)/Cnext.a $(OBJ_FILES) $(MAKEFILE) include.mk $(OBJ_DIR)/sqlite3.o
	$(ARCHIVE) $(OBJ_DIR)/RestServer.a $(OBJ_FILES) $(OBJ_DIR)/sqlite3.o $(CNEXT_OBJ_FILES)

$(EXE_DIR)/sqlite-client: $(OBJ_DIR)/RestServer.a
	$(MKDIR) $(EXE_DIR)
	$(CXX) $(FLAGS) $(INCLUDES) $(DEFINES) $(WARNINGS) -L$(OBJ_DIR) $(SRC_DIR)/SqliteClient.c -l:RestServer.a -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INCLUDE_DIR)/%.h $(MAKEFILE) include.mk
	$(MKDIR) $(OBJ_DIR)
	$(CXX) $(FLAGS) $(INCLUDES) $(DEFINES) $(WARNINGS) -c $< -o $@

.PHONY: $(CNEXT_OBJ_DIR)/Cnext.a
$(CNEXT_OBJ_DIR)/Cnext.a:
	$(MAKE) -C lib/cnext -f makefile

$(OBJ_DIR)/sqlite3.o: $(SRC_DIR)/sqlite3.c
	$(MKDIR) $(OBJ_DIR)
	$(CC) -c $(SRC_DIR)/sqlite3.c -o $(OBJ_DIR)/sqlite3.o

cleanall:
	$(REMOVE) $(BUILD_DIR)
	$(MAKE) -C lib/cnext -f makefile cleanall

