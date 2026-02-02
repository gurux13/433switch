CXXFLAGS=-DRPI -DWITH_LOCKS -g
LDFLAGS=-lwiringPi -lwiringPiDev -lcrypt -lpaho-mqtt3c

SmartSwitch: SmartSwitch.cpp
	$(CXX) $(CXXFLAGS) SmartSwitch.cpp -o $@ $(LDFLAGS)
