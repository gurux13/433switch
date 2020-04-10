CXXFLAGS=-DRPI
LDFLAGS=-lwiringPi -lwiringPiDev -lcrypt -lpaho-mqtt3c

SmartSwitch:
    $(CXX) $(CXXFLAGS) SmartSwitch.cpp ../rc-switch/RCSwitch.cpp -o $@ $(LDFLAGS)