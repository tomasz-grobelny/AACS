# Overview
This document describes how to install AACS and Anbox on Odroid N2. Note that this is mainly for development purposes, not end-user usage.

# Prerequisites
The following hardware items are needed to follow this document:
1. Odroid N2
2. MicroSDHC card reader
3. MicroSDHC card (I used 32GB)

# Steps
Follow these steps to get AACS and Anbox running on Odroid N2:
1. Download image from http://de.eu.odroid.in/ubuntu_20.04lts/n2/ubuntu-20.04-4.9-minimal-odroid-n2-20200715.img.xz
1. unxz ubuntu-20.04-4.9-minimal-odroid-n2-20200715.img.xz && partx -v -a ubuntu-20.04-4.9-minimal-odroid-n2-20200715.img
1. mkdir image && mount /dev/loopXp2 image && rm image/.first_boot && umount image
1. dd if=ubuntu-20.04-4.9-minimal-odroid-n2-20200715.img of=/dev/sdX bs=1M
1. Insert card into Odroid N2 and start it.
1. Find out the IP address of your Odroid N2 (eg. from your router or using 'nmap -sT 192.168.0.* -p 22 -P0') and ssh to it (user: root, password: odroid)
1. Resize image somewhat if needed. Not the full size yet, just enough to complete following instructions.
1. apt upgrade && apt install git && apt install libboost1.67-all-dev && apt install libssl-dev && apt install libprotobuf-dev && apt install protobuf-compiler && apt install libgstreamer1.0-dev && apt install libconfig-dev && apt install libusb-1.0-0-dev && apt install libegl-dev &&apt install libgles2-mesa-dev && apt install libsdl2-dev
1. apt remove libsdl2-2.0-0 && apt install libsdl2-dev && apt install libsdl2-image-dev && apt install liblxc-dev && apt install libproperties-cpp-dev && apt install libsystemd-dev && apt install libcap-dev && apt install libgmock-dev
1. apt remove python3-distupgrade ubuntu-release-upgrader-core && apt install ubuntu-desktop-minimal
1. Follow steps below (based on https://forum.odroid.com/viewtopic.php?f=176&t=33993&p=261833#p261833) to install 5.9.x+ kernel.
1. git clone https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git
1. cd linux && wget https://github.com/tomasz-grobelny/AACS/raw/master/doc/kernel_config -O .config && wget https://github.com/tomasz-grobelny/AACS/raw/master/doc/compile_kernel.sh
1. wget https://github.com/tomasz-grobelny/AACS/raw/master/doc/tmp_usb_fix.patch && patch -p1 < tmp_usb_fix.patch (note that this step might not be necessary with kernels 5.10+)
1. chmod +x ./compile_kernel.sh && ./compile_kernel.sh
1. reboot
1. mkdir AA && cd AA
1. git clone https://github.com/tomasz-grobelny/AACS.git && git clone https://github.com/tomasz-grobelny/AAVideoSink.git && git clone https://github.com/libusbgx/libusbgx.git
1. cd libusbgx && autoreconf -i && ./configure --prefix=/usr && make && make install && cd ..
1. cd AACS && mkdir build && cd build && cmake .. && make && cd ../..
1. cd AAVideoSink && mkdir build && cd build && cmake .. && make && cd ../..
1. cd ~/ && git clone https://github.com/anbox/anbox.git && cd anbox && mkdir build && cd build && cmake .. && make
1. More steps to follow...
1. Stay tuned...
1. Turn off Odroid N2 and move card to card reader.
1. dd if=/dev/sdX of=myimage.img bs=1M count=<size of both partitions> && partx -v -a myimage.img
1. mkdir image && mount /dev/loopXp2 image && touch image/.first_boot && umount image
1. myimage.img is now ready for distribution

# Development tools
Development tools used:
* cmake
* clang
* Visual Studio Code, plugins:
    * Remote-SSH
    * Clang-Format
    * CMake
    * CMake Tools
    * Native Debug
    * vscode-clangd
    * Prettier - Code formatter

Client Visual Studio Code can run both Windows and Linux.
