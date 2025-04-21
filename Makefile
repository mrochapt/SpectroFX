RACK_DIR ?= ../..
PLUGIN_NAME := SpectroFX

# Include the Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk

# Add .cpp files to the build
SOURCES += $(wildcard src/*.cpp)

# FLAGS will be passed to both the C and C++ compiler
FLAGS += -I/mingw64/include/    # caminho para headers fftw3.h
LDFLAGS += -I/mingw64/include/ -lfftw3f        # link com fftw3f

# Add files to the ZIP package when running `make dist`
# The compiled plugin and "plugin.json" are automatically added.
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += $(wildcard presets)

