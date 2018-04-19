CXXFLAGS += -std=c++14 -Werror -Wextra -iquote ./cpp
AR = @ar
CXX = @g++
LD = @g++

# Create C++ object: $(1)=input.cpp $(2)=output.o $(3)=output.d
compile = $(CXX) $(CXXFLAGS) -MMD -MT "$(2) $(3)" -c -o $(2) $(1)

# Create library: $(1)=inputs.o $(2)=output.a
archive = $(AR) $(ARFLAGS) $(2) $(1)

# Create executable: $(1)=inputs.{o,lib} $(2)=output.exe
link = $(LD) $(LDFLAGS) -o $(2) $(1)
