# Overview
This document describes how to install AACS and Anbox on Odroid N2. Note that this is mainly for development purposes, not end-user usage.

# Prerequisites
The following hardware items are needed to follow this document:
1. Odroid N2
2. MicroSDHC card reader
3. MicroSDHC card (I used 32GB)

# Steps
Follow these steps to get AACS and Anbox running on Odroid N2:
1. wget http://de.eu.odroid.in/ubuntu_20.04lts/n2/ubuntu-20.04-4.9-minimal-odroid-n2-20200715.img.xz
1. unxz ubuntu-20.04-4.9-minimal-odroid-n2-20200715.img.xz && partx -v -a ubuntu-20.04-4.9-minimal-odroid-n2-20200715.img
1. mkdir image && mount /dev/loopXp2 image && rm -f image/.first_boot && cat image/aafirstboot |sed "s#start)#start)\n\t\tdpkg-reconfigure openssh-server#" > image/aafirstboot.new && rm -f image/aafirstboot && mv image/aafirstboot.new image/aafirstboot && chmod 755 image/aafirstboot && umount image && rmdir image
1. dd if=ubuntu-20.04-4.9-minimal-odroid-n2-20200715.img of=/dev/sdX bs=1M
1. Insert card into Odroid N2 and start it.
1. Find out the IP address of your Odroid N2 (eg. from your router or using 'nmap -sT 192.168.0.* -p 22 -P0|grep -B 4 open|grep "scan report"') and ssh to it (user: root, password: odroid)
1. echo -en "d\n2\nn\np\n2\n264192\n25165823\np\nw\n"|fdisk /dev/mmcblk1 (Resize image somewhat, just enough to complete following instructions.)
1. resize2fs /dev/mmcblk1p2
1. apt update && apt -yq upgrade
1. apt -yq install git u-boot-tools build-essential libncurses5-dev bison flex bc libboost1.67-all-dev libssl-dev libprotobuf-dev protobuf-compiler libgstreamer1.0-dev libconfig-dev libusb-1.0-0-dev libegl-dev libgles2-mesa-dev libglm-dev
1. apt -yq remove libsdl2-2.0-0 python3-distupgrade && rm -f /etc/pulse/default.pa && DEBIAN_FRONTEND=noninteractive apt -yq install libsdl2-dev libsdl2-image-dev liblxc-dev libproperties-cpp-dev libsystemd-dev libcap-dev libgmock-dev python3-distupgrade ubuntu-release-upgrader-core ubuntu-desktop-minimal libxtst-dev adb gpsd gpsd-clients lightdm gstreamer1.0-plugins-ugly gstreamer1.0-plugins-bad gstreamer1.0-libav tigervnc-scraping-server lxc-utils xfce4 libdw-dev mc clangd clang gpiod
1. Follow steps below (based on https://forum.odroid.com/viewtopic.php?f=176&t=33993&p=261833#p261833) to install 5.9.x+ kernel.
1. git clone https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git
1. cd linux && git checkout linux-5.9.y && wget https://github.com/tomasz-grobelny/AACS/raw/master/doc/kernel_config -O .config && wget https://github.com/tomasz-grobelny/AACS/raw/master/doc/compile_kernel.sh
1. wget https://github.com/tomasz-grobelny/AACS/raw/master/doc/tmp_usb_fix.patch && patch -p1 < tmp_usb_fix.patch (note that this step might not be necessary with kernels 5.10+)
1. chmod +x ./compile_kernel.sh && make olddefconfig && ./compile_kernel.sh
1. reboot
1. git clone https://github.com/libusbgx/libusbgx.git && cd libusbgx && autoreconf -i && ./configure --prefix=/usr && make && make install && cd ..
1. git clone https://github.com/anbox/anbox.git && cd anbox && git submodule init && git submodule update && mkdir build && cd build && cmake .. && make -j 4 && make install && cd ../..
1. wget http://anbox.postmarketos.org/android-7.1.2_r39-anbox_arm64-userdebug.img -O android.img
1. mkdir AA && cd AA
1. git clone --recurse-submodules https://github.com/tomasz-grobelny/AACS.git && cd AACS && mkdir build && cd build && cmake .. && make && cd ../..
1. git clone https://github.com/tomasz-grobelny/AAVideoSink.git && cd AAVideoSink && mkdir build && cd build && cmake .. && make && cd ../..
1. cd
1. apt -yq remove gdm3 && dpkg-reconfigure lightdm
1. ./AA/AACS/scripts/setup
1. reboot
1. wget https://f-droid.org/repo/com.menny.android.anysoftkeyboard_6279.apk && adb install com.menny.android.anysoftkeyboard_6279.apk
1. wget https://download.osmand.net/releases/net.osmand-3.8.3-383.apk && adb install net.osmand-3.8.3-383.apk
1. sed -i "s#DEVICES=\"\"#DEVICES=\"/dev/ttyACM0\"#" /etc/default/gpsd
1. Connect via vncviewer to your Odroid N2.
1. Click on AnySoftKeyboard app icon, click on "Start setup..." link, click on "Go to your Language&Input settings..." link, enable AnySoftKeyboard, click on in dialog box twice, press Esc twice.
1. dd if=/dev/zero of=/empty_file ; rm /empty_file
1. halt
1. Turn off Odroid N2 and move card to card reader.
1. dd if=/dev/sdX of=aacs_image.img bs=1M count=12288 && partx -v -a aacs_image.img
1. mkdir aacs_image && mount /dev/loopXp2 aacs_image && touch aacs_image/.first_boot && rm -f aacs_image/etc/ssh/ssh_host_* && umount aacs_image && rmdir aacs_image
1. xz -T 8 aacs_image.img
1. aacs_image.img.xz is now ready for distribution

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
