BUILD_MODE = RELEASE
CXXFLAGS = -Wall -std=c++11 -Wno-unused-but-set-variable
CXX = g++

PROJECT_ROOT = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

DEPS = include/sensors.h sensors/sensors.cpp include/devices.h devices/devices.cpp gateway/gateway.cpp
INCLUDES = $(PROJECT_ROOT)include
LIBS = -lrpc -lpthread

SENSORS_EXE = sensors.exe
GATEWAY_EXE = gateway.exe
DEVICES_EXE = devices.exe

# Setup build mode
ifeq ($(BUILD_MODE),DEBUG)
	CXXFLAGS += -g3 -O0 
else ifeq ($(BUILD_MODE),RELEASE)
	CXXFLAGS += -O3
else
	$(error Build mode $(BUILD_MODE) not supported by this Makefile)
endif

all: sensors.exe gateway.exe devices.exe

sensors.exe: $(DEPS)
	$(CXX) $(CXXFLAGS) sensors/sensors.cpp -I$(INCLUDES) -Wl,--start-group $(LIBS) -Wl,--end-group -o $@

gateway.exe: $(DEPS)
	$(CXX) $(CXXFLAGS) gateway/gateway.cpp -I$(INCLUDES) -Wl,--start-group $(LIBS) -Wl,--end-group -o $@

devices.exe: $(DEPS)
	$(CXX) $(CXXFLAGS) devices/devices.cpp -I$(INCLUDES) -Wl,--start-group $(LIBS) -Wl,--end-group -o $@

clean:
	rm -rf $(SENSORS_EXE) $(GATEWAY_EXE) $(DEVICES_EXE)

.PHONY: all sensors gateway devices
