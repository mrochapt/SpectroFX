PLUGIN_NAME := SpectroFX
RACK_DIR ?= ../Rack-SDK

SOURCES += $(wildcard src/*.cpp)

# INCLUDE PATHS
FLAGS += -IC:/msys64/mingw64/include/opencv4
FLAGS += -IC:/msys64/mingw64/include

# LIBRARY PATHS
LDFLAGS += -LC:/msys64/mingw64/lib

# LIBRARIES
LDFLAGS += -lopencv_core -lopencv_imgproc -lfftw3

FLAGS += -std=c++17

include $(RACK_DIR)/plugin.mk