FLAGS := -g $(EXTRA_FLAGS) $(COMMAND_LINE_FLAGS)

OBJ_FILES := \
    $(OBJ_DIR)/DbInterfaceUnitTest.o \
    $(OBJ_DIR)/WebServerUnitTest.o \

INCLUDES := \
    -I../lib/cnext/include \
    -I../include \

DEFINES := \
    -DTLS_SOCKETS_ENABLED \
    -DLOGGING_ENABLED \
    -DWsResponseObject=RedBlackTree \
    -DWsRequestObject=Dictionary \
    -DWsResponseNode=RedBlackNode \
    -DWsRequestNode=DictionaryEntry \
    -DwsResponseObjectDestroy=rbTreeDestroy \
    -DwsResponseObjectToJson=rbTreeToJson \
    -DMAX_SCOPE_VARS=128 \
    -DUSE_OPENSSL \
    $(commandLineDefines) \

WARNINGS := \
    -Wall \
    -Wextra \
    -Werror \
    -Wno-format-truncation \

all: $(OBJ_FILES)

cc:
	$(eval COMMAND_LINE_FLAGS := -fprofile-arcs -ftest-coverage)
	$(eval COMMAND_LINE_LINKS := -lgcov -fprofile-arcs)
	$(MAKE) clean
	$(MAKE) COMMAND_LINE_FLAGS="$(COMMAND_LINE_FLAGS)" COMMAND_LINE_LINKS="$(COMMAND_LINE_LINKS)"

$(OBJ_DIR)/%.o: %.c $(MAKEFILE) include.mk
	$(CXX) $(FLAGS) $(INCLUDES) $(DEFINES) $(WARNINGS) -c $< -o $@

clean:
	rm -f $(OBJ_FILES)

debugvars:
	@echo $(FLAGS)
	@echo $(OBJ_FILES)
	@echo $(INCLUDES)
	@echo $(DEFINES)
	@echo $(WARNINGS)

