#!/bin/bash -e

# Download latest release
echo '           Download HyperHDR'
cd /tmp

if [ "$DEPLOY_DIR" != "${DEPLOY_DIR/v7/}" ]; then
    echo 'Building ARM v7!!!!!!!!!!!!!!!'
    wget https://github.com/awawa-dev/HyperHDR/releases/download/v12.1.0.0/HyperHDR-12.1.0.0-Linux-armv7l.deb
    apt install ./HyperHDR-12.1.0.0-Linux-armv7l.deb
    rm HyperHDR-12.1.0.0-Linux-armv7l.deb
else
    echo 'Building ARM v6!!!!!!!!!!!!!!!'
    wget https://github.com/awawa-dev/HyperHDR/releases/download/v12.1.0.0/HyperHDR-12.1.0.0-Linux-armv6l.deb
    apt install ./HyperHDR-12.1.0.0-Linux-armv6l.deb
    rm HyperHDR-12.1.0.0-Linux-armv6l.deb
fi

mv /etc/systemd/system/hyperiond@.service /etc/systemd/system/hyperiond@pi.service
sudo sed -i 's/%i/pi/g' /etc/systemd/system/hyperiond@pi.service
systemctl -q enable hyperiond@pi.service
