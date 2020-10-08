#!/bin/bash
set -e
O="0x1080000"
DT="$(date -Iminutes)"
DEV="$(find /dev/mmcblk? | tail -n 1)"
DIR="/boot/$DT"
make clean
date -Iseconds
make -j 4
date -Iseconds
make modules_install
R=$(make kernelrelease)
#cp -r "/lib/modules/$R" "$DIR"_modules
mkdir "$DIR"
mkimage -A arm64 -O linux -T kernel -C none -a $O -e $O -n "$R" -d arch/arm64/boot/Image "$DIR/uImage"
cp arch/arm64/boot/dts/amlogic/meson-g12b-odroid-n2.dtb "$DIR/n2.dtb"
echo "ODROIDN2-UBOOT-CONFIG
setenv bootargs \"root=${DEV}p2 rootwait rw clk_ignore_unused console=ttyAML0,115200 usb-storage.quirks=0bc2:ab38:,0bc2:3312:u\"
setenv dtb_loadaddr \"0x1000000\"
fatload mmc \${devno}:1 \${dtb_loadaddr} n2.dtb
fatload mmc \${devno}:1 0x01080000 uImage
bootm 0x1080000 - \${dtb_loadaddr}
" > "$DIR/boot.ini"
rm /media/boot/* || true
cp "$DIR"/* /media/boot/
