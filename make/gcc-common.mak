CXXFLAGS += -std=c++23 -Werror -Wall -Wextra -Wno-trigraphs -iquote cpp -fno-diagnostics-color
AR = $(SILENT)ar
CXX = $(SILENT)g++
LD = $(SILENT)g++

# Create C++ object: $(1)=input.cpp $(2)=output.o $(3)=output.d
compile = $(CXX) $(CXXFLAGS) -MMD -c $(1) -o $(2) -MT $(3)

# Create library: $(1)=inputs.o $(2)=output.a
archive = $(AR) $(ARFLAGS) $(2) $(1)

# Create executable: $(1)=inputs.{o,lib} $(2)=output.exe
link = $(LD) $(LDFLAGS) -o $(2) $(1) $(LDEXTRA)
