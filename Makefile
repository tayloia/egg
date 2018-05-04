#############################################################################
# PARAMETERS
#############################################################################

override PLATFORM ?= linux
override TOOLCHAIN ?= gcc
override CONFIGURATION ?= release

include make/$(PLATFORM).mak

# We exclude these source files
EXCLUDE_SRCS = cpp/main.cpp

SILENT = @

#############################################################################
# FILE MACROS
#############################################################################

# These platform-specific actions need to be macros because of the quoting under Windows
# See https://stackoverflow.com/a/40751022
ifeq ($(findstring .exe,$(SHELL)),.exe)
	mkdir   = $(SILENT)mkdir "$(1)"
	rmdir   = $(SILENT)if exist "$(1)" rmdir /s /q "$(1)"
	noop    = $(SILENT):
else
	mkdir   = $(SILENT)mkdir -p $(1)
	rmdir   = $(SILENT)rm -rf $(1)
	noop    = $(SILENT)cd .
endif

# Search for various files
sources = $(filter-out $(EXCLUDE_SRCS),$(wildcard $(1)))
objects = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(1))
directories = $(patsubst %,%.,$(sort $(dir $(1))))

#############################################################################
# COMPUTED VALUES
#############################################################################

ifeq ($(OS),Windows_NT)
	RUNTEST = $(SILENT).\runtest.cmd
else
	RUNTEST = $(SILENT)./runtest.sh
endif

ECHO = @echo
SUBMAKE = $(SILENT)+$(MAKE) --no-print-directory

OBJ_ROOT = $(PLATFORM)/$(TOOLCHAIN)/obj
BIN_ROOT = $(PLATFORM)/$(TOOLCHAIN)/bin

OBJ_DIR = $(OBJ_ROOT)/$(CONFIGURATION)
BIN_DIR = $(BIN_ROOT)/$(CONFIGURATION)

EGG_SRCS = $(call sources,cpp/*.cpp)
TEST_SRCS = $(call sources,cpp/test/*.cpp)

EGG_OBJS = $(call objects,$(EGG_SRCS))
TEST_OBJS = $(call objects,$(TEST_SRCS))

ALL_OBJS = $(EGG_OBJS) $(TEST_OBJS)
ALL_DIRS = $(call directories,$(ALL_OBJS)) $(BIN_DIR)/.

EGG_EXE = $(BIN_DIR)/egg.exe
TEST_EXE = $(BIN_DIR)/egg-testsuite.exe

#############################################################################
# GENERIC RULES
#############################################################################

# This is the thing that is built when you just type 'make'
default: all

.PHONY: default bin test clean nuke release debug all rebuild valgrind

# We need to create certain directories or our toolchain fails
%/.:
	$(ECHO) Creating directory $*
	$(call mkdir,$*)

# We create dependency files (*.d) at the same time as compiling the source
$(OBJ_DIR)/%.o: %.cpp
	$(ECHO) Compiling $(PLATFORM) $(TOOLCHAIN) $(CONFIGURATION) $<
	$(call compile,$<,$(OBJ_DIR)/$*.o,$(OBJ_DIR)/$*.d)

# Include the generated dependencies
-include $(ALL_OBJS:.o,.d)

# Rule for creating libraries from object files
%.a:
	$(ECHO) Archiving $@
	$(call archive,$^,$@)

# Rule for creating executables from object files and libraries
%.exe:
	$(ECHO) Linking $@
	$(call link,$^,$@)

#############################################################################
# SPECIFIC RULES
#############################################################################

# These source files need additional include directories for Google Test
$(OBJ_DIR)/cpp/test/%: CXXFLAGS += -iquote ./thirdparty/googletest/include
$(OBJ_DIR)/cpp/test/gtest.%: CXXFLAGS += -iquote ./thirdparty/googletest

# Re-generate the object files if this makefile changes
# Make sure intermediate directories are created before generating object files
$(ALL_OBJS): Makefile | $(ALL_DIRS)

# Egg executable dependencies
$(EGG_EXE): $(EGG_OBJS)

# Testsuite dependencies
$(TEST_EXE): $(EGG_OBJS) $(TEST_OBJS)

#############################################################################
# PSEUDO-TARGETS
#############################################################################

# Pseudo-target to build the binaries
bin: $(TEST_EXE)
	$(call noop)

# Pseudo-target to build and run the test suite
test: $(TEST_EXE)
	$(ECHO) Running tests $<
	$(RUNTEST) $<

# Pseudo-target to clean the intermediates for the current configuration
clean:
	$(ECHO) Cleaning directory $(OBJ_DIR)
	$(call rmdir,$(OBJ_DIR))

# Pseudo-target to nuke all configurations
nuke:
	$(ECHO) Nuking directory $(OBJ_ROOT)
	$(call rmdir,$(OBJ_ROOT))
	$(ECHO) Nuking directory $(BIN_ROOT)
	$(call rmdir,$(BIN_ROOT))

#############################################################################
# SUBMAKES
#############################################################################

# Pseudo-target for 'release' binaries
release:
	$(SUBMAKE) CONFIGURATION=release bin

# Pseudo-target for 'debug' binaries
debug:
	$(SUBMAKE) CONFIGURATION=debug bin

# Pseudo-target for all binaries and tests (parallel-friendly)
all: release debug
	$(SUBMAKE) CONFIGURATION=release test
	$(SUBMAKE) CONFIGURATION=debug test

# Pseudo-target for everything from scratch (parallel-friendly)
rebuild: nuke
	$(SUBMAKE) all

valgrind: clean
	$(SUBMAKE) $(TEST_EXE)
	valgrind -v --leak-check=full --show-leak-kinds=all --track-origins=yes $(TEST_EXE)
