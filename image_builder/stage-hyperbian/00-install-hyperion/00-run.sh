#!/bin/bash -e

# Enable SPI and force HDMI output
sed -i "s/^#dtparam=spi=on.*/dtparam=spi=on/" ${ROOTFS_DIR}/boot/config.txt
sed -i "s/^#hdmi_force_hotplug=1.*/hdmi_force_hotplug=1/" ${ROOTFS_DIR}/boot/config.txt
