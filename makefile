BUILD_DIR       := build
OBJ_DIR         := $(BUILD_DIR)/obj64
EXE_DIR         := $(BUILD_DIR)/exe64
CC              := gcc
CXX             := g++
MAKEFILE        := makefile
CNEXT_OBJ_DIR   := lib/cnext/build/obj64

include include.mk

clean:
	$(REMOVE) $(OBJ_DIR)
	$(MAKE) -C lib/cnext -f makefile clean

.PHONY: $(CNEXT_OBJ_DIR)/Cnext.a
$(CNEXT_OBJ_DIR)/Cnext.a:
	$(MAKE) -C lib/cnext -f makefile COMMAND_LINE_DEFINES="$(DEFINES)"

