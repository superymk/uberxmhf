FROM debian:11
RUN apt-get update && \
	apt-get install \
					openssh-server tmux vim git aptitude \
					pbuilder texinfo ruby build-essential autoconf libtool \
					-y
RUN useradd lxy && \
	echo lxy ALL=NOPASSWD:ALL | sudo tee -a /etc/sudoers
USER lxy

RUN sudo apt-get install crossbuild-essential-i386 -y
RUN sudo apt-get install pkg-config -y
# Problem: Docker is 64-bit, but 32-bit libgcc.a is installed in
# /usr/lib/gcc-cross/i686-linux-gnu/10/libgcc.a, expected /usr/lib/gcc
# Ideal way is to use i686-linux-gnu-gcc etc.

