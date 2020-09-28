#!/bin/bash -e

# Download latest release
echo '           Download HyperHDR'
cd /tmp

if [ "$DEPLOY_DIR" != "${DEPLOY_DIR/v7/}" ]; then
    echo 'ARM v7!!!!!!!!!!!!!!!'
    wget https://github.com/awawa-dev/HyperHDR/releases/download/v8.2.0.8A2/HyperHDR-8.2.0.8A2-Linux-armv7l.deb
    apt install ./HyperHDR-8.2.0.8A2-Linux-armv7l.deb
    rm HyperHDR-8.2.0.8A2-Linux-armv7l.deb
else
    echo 'ARM v6!!!!!!!!!!!!!!!'
    wget https://github.com/awawa-dev/HyperHDR/releases/download/v8.2.0.8A2/HyperHDR-8.2.0.8A2-Linux-armv6l.deb
    apt install ./HyperHDR-8.2.0.8A2-Linux-armv6l.deb
    rm HyperHDR-8.2.0.8A2-Linux-armv6l.deb
fi




