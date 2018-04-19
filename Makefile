#############################################################################
# PARAMETERS
#############################################################################

override TOOLCHAIN ?= gcc
override CONFIGURATION ?= release

include $(TOOLCHAIN)/$(TOOLCHAIN)-$(CONFIGURATION).mak

# We exclude these source files
EXCLUDE_SRCS = cpp/main.cpp

#############################################################################
# FILE MACROS
#############################################################################

# These platform-specific actions need to be macros because of the quoting under Windows
ifeq ($(OS),Windows_NT)
	mkdir = @mkdir "$(1)"
	rmdir = @if exist "$(1)" rmdir /s /q "$(1)"
else
	mkdir = @mkdir -p $(1)
	rmdir = @rm -rf $(1)
endif

# Search for various files
sources = $(filter-out $(EXCLUDE_SRCS),$(wildcard $(1)))
objects = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(1))
directories = $(patsubst %,%.,$(sort $(dir $(1))))

#############################################################################
# COMPUTED VALUES
#############################################################################

ECHO = @echo

OBJ_ROOT = $(TOOLCHAIN)/obj
BIN_ROOT = $(TOOLCHAIN)/bin

OBJ_DIR = $(OBJ_ROOT)/$(CONFIGURATION)
BIN_DIR = $(BIN_ROOT)/$(CONFIGURATION)

EGG_SRCS = $(call sources,cpp/*.cpp)
TEST_SRCS = $(call sources,cpp/test/*.cpp)

EGG_OBJS = $(call objects,$(EGG_SRCS))
TEST_OBJS = $(call objects,$(TEST_SRCS))

ALL_OBJS = $(EGG_OBJS) $(TEST_OBJS)
ALL_DIRS = $(call directories,$(ALL_OBJS)) $(BIN_DIR)/.

#############################################################################
# GENERIC RULES
#############################################################################

# This is the thing that is built when you just type 'make'
default: test

.PHONY: default test clean nuke

# We need to create certain directories or our toolchain fails
%/.:
	$(ECHO) Creating directory $*
	$(call mkdir,$*)

# We create dependency files (*.d) at the same time as compiling the source
$(OBJ_DIR)/%.o $(OBJ_DIR)/%.d: %.cpp
	$(ECHO) Compiling $<
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

# This source files needs additional include directories for Google Test
$(OBJ_DIR)/cpp/test/%: CXXFLAGS += -iquote ./thirdparty/googletest/include
$(OBJ_DIR)/cpp/test/gtest.%: CXXFLAGS += -iquote ./thirdparty/googletest

# Re-generate the object files if this makefile changes
# Make sure intermediate directories are created before generating object files
$(ALL_OBJS): Makefile | $(ALL_DIRS)

# Egg executable dependencies
$(BIN_DIR)/egg.exe: $(EGG_OBJS)

# Testsuite dependencies
$(BIN_DIR)/egg-testsuite.exe: $(EGG_OBJS) $(TEST_OBJS)

# Pseudo-target to build and run the test suite
test: $(BIN_DIR)/egg-testsuite.exe
	$(ECHO) Running tests $<
	@./$<

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
