MKDIR       := mkdir -p
SRC_DIR     := src
INCLUDE_DIR := include
FLAGS       := -rdynamic
REMOVE      := rm -rf
ARCHIVE     := ar rcs

OBJ_FILES := \
    $(OBJ_DIR)/DbClientLib.o \
    $(OBJ_DIR)/LoggingLib.o \
    $(OBJ_DIR)/MariaDbLib.o \
    $(OBJ_DIR)/SqlClientLib.o \
    $(OBJ_DIR)/SqliteLib.o \
    $(OBJ_DIR)/WebClientLib.o \
    $(OBJ_DIR)/WebServerLib.o \
    $(EXTRA_OBJ_FILES) \

INCLUDES := \
    -Iinclude \
    -Ilib/cnext/include \

LINKS := \

DEFINES := \
    -DWsResponseObject=RedBlackTree \
    -DWsRequestObject=RedBlackTree \
    -DWsResponseNode=RedBlackNode \
    -DWsRequestNode=RedBlackNode \
    -DwsResponseObjectDestroy=rbTreeDestroy \
    -DwsResponseObjectToJson=rbTreeToJson \

WARNINGS := \
    -Wall \
    -Wextra \
    -Werror \

all: $(OBJ_FILES)
	$(ARCHIVE) $(OBJ_DIR)/RestServer.a $(OBJ_FILES)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INCLUDE_DIR)/%.h $(MAKEFILE) include.mk
	$(MKDIR) $(OBJ_DIR)
	$(CXX) $(FLAGS) $(INCLUDES) $(DEFINES) $(WARNINGS) -c $< -o $@

$(OBJ_DIR)/sqlite3.o: $(SRC_DIR)/sqlite3.c
	$(MKDIR) $(OBJ_DIR)
	$(CC) -c $(SRC_DIR)/sqlite3.c -o $(OBJ_DIR)/sqlite3.o

cleanall:
	$(REMOVE) $(BUILD_DIR)
	$(MAKE) -C lib/cnext -f makefile cleanall
