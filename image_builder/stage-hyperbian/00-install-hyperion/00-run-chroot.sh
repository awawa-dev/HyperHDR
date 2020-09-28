#!/bin/bash -e

# Download latest release
echo '           Download HyperHDR'
cd /tmp
wget https://github.com/awawa-dev/HyperHDR/releases/download/v8.2.0.8A/Hyperion-8.2.0.8A-Linux-armv7l.deb
apt install ./Hyperion-8.2.0.8A-Linux-armv7l.deb
rm Hyperion-8.2.0.8A-Linux-armv7l.deb


