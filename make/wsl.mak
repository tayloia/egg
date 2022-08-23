LDEXTRA = -pthread

# Stop GCC outputting UTF-8 quotes
export LANG=C

include make/$(TOOLCHAIN)-$(CONFIGURATION).mak
