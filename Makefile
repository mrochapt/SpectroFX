PLUGIN_NAME := SpectroFX
RACK_DIR ?= ../..

# Add .cpp files to the build
SOURCES += $(wildcard src/*.cpp)

# Include the Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk
