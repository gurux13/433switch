# 1. Use a slim Debian version as the base
FROM debian:bookworm-slim

# 2. Update packages and install a basic tool (like curl)
# We combine commands and clean up to keep the image small
RUN apt-get update
RUN apt-get install -y libpaho-mqtt-dev pkg-config gcc git cmake wget sudo 
RUN git clone --recurse-submodules https://github.com/gurux13/433switch.git #2
RUN chmod +x ./433switch/build_pi.sh
RUN ./433switch/build_pi.sh
RUN rm -rf /var/lib/apt/lists/*

# 3. Set a default command to run when the container starts
CMD ["SmartSwitch"]
