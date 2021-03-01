#!/bin/bash -e

# Download latest release
echo 'Downloading HyperHDR.........................'
cd /tmp

if [ "$DEPLOY_DIR" != "${DEPLOY_DIR/v7/}" ]; then
    echo 'Building ARM v7!!!!!!!!!!!!!!!'
    wget https://github.com/awawa-dev/HyperHDR/releases/download/v15.0.0.0/HyperHDR-15.0.0.0-Linux-armv7l.deb
    apt install ./HyperHDR-15.0.0.0-Linux-armv7l.deb
    rm HyperHDR-15.0.0.0-Linux-armv7l.deb
else
    echo 'Building ARM v6!!!!!!!!!!!!!!!'
    wget https://github.com/awawa-dev/HyperHDR/releases/download/v15.0.0.0/HyperHDR-15.0.0.0-Linux-armv6l.deb
    apt install ./HyperHDR-15.0.0.0-Linux-armv6l.deb
    rm HyperHDR-15.0.0.0-Linux-armv6l.deb
fi

mv /etc/systemd/system/hyperhdr@.service /etc/systemd/system/hyperhdr.service
#sudo sed -i 's/%i/pi/g' /etc/systemd/system/hyperhdr.service
#systemctl -q enable hyperhdr.service
#rm -f /etc/systemd/system/hyperhdr@.service
echo 'Registering HyperHdr........................'
systemctl -q enable hyperhdr.service
