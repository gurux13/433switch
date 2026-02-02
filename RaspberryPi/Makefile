CXXFLAGS=-DRPI -DWITH_LOCKS -g
LDFLAGS=-lwiringPi -lwiringPiDev -lcrypt -lpaho-mqtt3c

SmartSwitch: SmartSwitch.cpp ../rc-switch/RCSwitch.cpp ../rc-switch/RCSwitch.h
	$(CXX) $(CXXFLAGS) SmartSwitch.cpp ../rc-switch/RCSwitch.cpp -o $@ $(LDFLAGS)
