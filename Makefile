BUILD_MODE = RELEASE
CXXFLAGS = -Wall -std=c++11
CXX = g++

PROJECT_ROOT = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

DEPS = include/sensors.h sensors/sensors.cpp
INCLUDES = $(PROJECT_ROOT)/include
LIBS = -lrpc -lpthread

SENSORS_EXE = sensors
GATEWAY_EXE = gateway
DEVICES_EXE = devices

# Setup build mode
ifeq ($(BUILD_MODE),DEBUG)
	CXXFLAGS += -g3 -O0 
else ifeq ($(BUILD_MODE),RELEASE)
	CXXFLAGS += -O3
else
	$(error Build mode $(BUILD_MODE) not supported by this Makefile)
endif

all: sensors gateway devices
	$(CXX) $(CXXFLAGS) $(LIBPATHS) -Wl,--start-group $(LIBS) -Wl,--end-group -o $(EXECUTEABLE) -Wl,-Map=$(EXECUTEABLE).map

sensors: $(DEPS)
	$(CXX) $(CXXFLAGS) sensors/sensors.cpp -I$(INCLUDES) -o $@

gateway: $(DEPS)

devices: $(DEPS)

clean:
	rm -rf $(SENSORS_EXE) $(GATEWAY_EXE) $(DEVICES_EXE)

.PHONY: all sensors gateway devices
