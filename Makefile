#############################################################################
# PARAMETERS
#############################################################################

override PLATFORM ?= linux
override TOOLCHAIN ?= gcc
override CONFIGURATION ?= release

include make/$(PLATFORM).mak

# We exclude these source files
EXCLUDE_SRCS = cpp/ovum/os-windows.cpp

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

OBJ_ROOT = obj/$(PLATFORM)/$(TOOLCHAIN)
BIN_ROOT = bin/$(PLATFORM)/$(TOOLCHAIN)

OBJ_DIR = $(OBJ_ROOT)/$(CONFIGURATION)
BIN_DIR = $(BIN_ROOT)/$(CONFIGURATION)

EGG_SRCS = $(call sources,cpp/ovum/*.cpp cpp/yolk/*.cpp)
TEST_SRCS = $(call sources,cpp/ovum/test/*.cpp cpp/yolk/test/*.cpp)
STUB_SRCS = $(call sources,cpp/stub/*.cpp)

EGG_OBJS = $(call objects,$(EGG_SRCS))
TEST_OBJS = $(call objects,$(TEST_SRCS))
STUB_OBJS = $(call objects,$(STUB_SRCS))

ALL_OBJS = $(EGG_OBJS) $(TEST_OBJS) $(STUB_OBJS)
ALL_DIRS = $(call directories,$(ALL_OBJS)) $(BIN_DIR)/.

TEST_EXE = $(BIN_DIR)/egg-test.exe
STUB_EXE = $(BIN_DIR)/egg-stub.exe
EGG_EXE = $(BIN_DIR)/egg.exe

EGG_ZIP = obj/egg.zip

#############################################################################
# GENERIC RULES
#############################################################################

# This is the thing that is built when you just type 'make'
default: all

.PHONY: default bin test smoke-test clean nuke release debug all rebuild coverage fresh valgrind gdb test-coverage test-gdb version

# We need to create certain directories or our toolchain fails
%/.:
	$(ECHO) Creating directory $*
	$(call mkdir,$*)

# We create dependency files (*.d) at the same time as compiling the source
$(OBJ_DIR)/%.o: %.cpp
$(OBJ_DIR)/%.o: %.cpp $(OBJ_DIR)/%.d
	$(ECHO) Compiling $(PLATFORM) $(TOOLCHAIN) $(CONFIGURATION) $<
	$(call compile,$<,$(OBJ_DIR)/$*.o,$(OBJ_DIR)/$*.d)

# See https://make.mad-scientist.net/papers/advanced-auto-dependency-generation
$(ALL_OBJS:.o=.d):

# Include the generated dependencies
include $(ALL_OBJS:.o=.d)

# Rule for creating libraries from object files
%.a:
	$(ECHO) Archiving $@
	$(call archive,$^,$@)

#############################################################################
# SPECIFIC RULES
#############################################################################

# These source files need additional include directories for Google Test
$(OBJ_DIR)/cpp/ovum/test/% $(OBJ_DIR)/cpp/yolk/test/%: CXXFLAGS += -iquote ./thirdparty/googletest/include
$(OBJ_DIR)/cpp/ovum/test/gtest.%: CXXFLAGS += -iquote ./thirdparty/googletest

# These source files need additional include directories for miniz-cpp
$(OBJ_DIR)/cpp/ovum/os-zip.%: CXXFLAGS += -iquote ./thirdparty -fpermissive

# This source file ingests environmental data
$(OBJ_DIR)/cpp/ovum/version.%: CXXFLAGS += -DEGG_COMMIT=\"$(shell git rev-parse HEAD)\" -DEGG_TIMESTAMP=\"$(shell date -u +\"%Y-%m-%d\ %H:%M:%SZ\")\"

# Re-generate the object files if this makefile changes
# Make sure intermediate directories are created before generating object files
$(ALL_OBJS): Makefile | $(ALL_DIRS)

# Testsuite dependencies
$(TEST_EXE): $(EGG_OBJS) $(TEST_OBJS) | $(STUB_EXE) $(EGG_EXE)

# Command-line dependencies
$(STUB_EXE): $(EGG_OBJS) $(STUB_OBJS)

# Rule for creating executables from object files and libraries
$(TEST_EXE) $(STUB_EXE):
	$(ECHO) Linking $@
	$(call link,$^,$@)

# Rule for creating executables from stubs and zips
$(EGG_EXE): $(STUB_EXE) $(EGG_ZIP)
	$(ECHO) Sandwiching $@
	$(SILENT)$(STUB_EXE) sandwich make --target=$(EGG_EXE) --zip=$(EGG_ZIP)

# Rule for creating zips
$(EGG_ZIP): $(STUB_EXE)
	$(ECHO) Zipping $@
	$(SILENT)$(STUB_EXE) zip make --target=$(EGG_ZIP) --directory=box

#############################################################################
# PSEUDO-TARGETS
#############################################################################

# Pseudo-target to build the binaries
bin: $(TEST_EXE) $(STUB_EXE) $(EGG_EXE)
	$(call noop)

# Pseudo-target to build and run the test suite
test: $(TEST_EXE)
	$(ECHO) Running tests $<
	$(SILENT)./runtest.sh $<

# Pseudo-target to run valgrind
valgrind: $(TEST_EXE)
	$(ECHO) Grinding tests $<
	$(SILENT)./valgrind.sh $<

# Pseudo-target to build and run a smoke test
smoke-test: $(EGG_EXE)
	$(ECHO) Smoke testing $<
	$(SILENT)./valgrind.sh $< --profile smoke-test

# Pseudo-target to run test coverage
test-coverage: bin
	$(ECHO) Running test coverage $(TEST_EXE)
	$(SILENT)./coverage.sh $(TEST_EXE) $(OBJ_DIR) $(OBJ_ROOT)/html
	$(SILENT)cmd.exe /C start $(OBJ_ROOT)/html/index.html

# Pseudo-target to run gdb
test-gdb: bin
	$(ECHO) Debugging tests $(TEST_EXE)
	gdb --batch --eval-command=run --eval-command=where --args $(TEST_EXE)

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

# Pseudo-target to build and run coverage
coverage:
	$(SUBMAKE) CONFIGURATION=coverage test-coverage

# Pseudo-target to clean and build tests
fresh: clean
	$(SUBMAKE) test

# Pseudo-target to build and run gdb
gdb:
	$(SUBMAKE) CONFIGURATION=debug test-gdb

# Pseudo-target for all binaries and tests (parallel-friendly)
all: release debug
	$(SUBMAKE) CONFIGURATION=release test
	$(SUBMAKE) CONFIGURATION=debug test

# Pseudo-target for everything from scratch (parallel-friendly)
rebuild: nuke
	$(SUBMAKE) all

version:
	$(ECHO) PLATFORM=$(PLATFORM)
	$(ECHO) TOOLCHAIN=$(TOOLCHAIN)
	$(ECHO) CONFIGURATION=$(CONFIGURATION)
