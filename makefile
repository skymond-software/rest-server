BUILD_DIR       := build
OBJ_DIR         := $(BUILD_DIR)/obj64
CC              := gcc
CXX             := g++
EXTRA_OBJ_FILES := lib/cnext/build/obj64/Cnext.a

include include.mk

lib/cnext/build/obj64/Cnext.a:
	$(MAKE) -C lib/cnext -f makefile

clean:
	$(REMOVE) $(OBJ_DIR)
	$(MAKE) -C lib/cnext -f makefile clean

