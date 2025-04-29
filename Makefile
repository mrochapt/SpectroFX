PLUGIN_NAME := SpectroFX
RACK_DIR ?= ../Rack-SDK

SOURCES += $(wildcard src/*.cpp)

include $(RACK_DIR)/plugin.mk
LDFLAGS += -lfftw3
