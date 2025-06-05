PLUGIN_NAME := SpectroFX
RACK_DIR ?= ../Rack-SDK

SOURCES += $(wildcard src/*.cpp)

LDFLAGS += -lfftw3
FLAGS += -IC:/msys64/mingw64/include/opencv4
LDFLAGS += -lopencv_core -lopencv_imgproc -lfftw3
FLAGS += -std=c++17

include $(RACK_DIR)/plugin.mk
