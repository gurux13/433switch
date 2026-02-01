apt install -y gcc g++ make wget libpaho-mqtt-dev
wget https://github.com/WiringPi/WiringPi/releases/download/3.16/wiringpi_3.16_arm64.deb
apt install ./wiringpi_3.16_arm64.deb
cd -- $(dirname "$0")
cd RaspberryPi/
make
ln -s /lib/libwiringPi.so /lib/libwiringPi.so.2
cp SmartSwitch /bin
