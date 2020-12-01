#!/bin/bash -e

# Download latest release
echo '           Download HyperHDR'
cd /tmp

if [ "$DEPLOY_DIR" != "${DEPLOY_DIR/v7/}" ]; then
    echo 'Building ARM v7!!!!!!!!!!!!!!!'
    wget https://github.com/awawa-dev/HyperHDR/releases/download/v11.2.0.8A/HyperHDR-11.2.0.8A-Linux-armv7l.deb
    apt install ./HyperHDR-11.2.0.8A-Linux-armv7l.deb
    rm HyperHDR-11.2.0.8A-Linux-armv7l.deb
else
    echo 'Building ARM v6!!!!!!!!!!!!!!!'
    wget https://github.com/awawa-dev/HyperHDR/releases/download/v11.2.0.8A/HyperHDR-11.2.0.8A-Linux-armv6l.deb
    apt install ./HyperHDR-11.2.0.8A-Linux-armv6l.deb
    rm HyperHDR-11.2.0.8A-Linux-armv6l.deb
fi

mv /etc/systemd/system/hyperiond@.service /etc/systemd/system/hyperiond@pi.service
sudo sed -i 's/%i/pi/g' /etc/systemd/system/hyperiond@pi.service
systemctl -q enable hyperiond@pi.service


#if [ -d /home/pi ]; then
#    mkdir /home/pi/.hyperion
#    chown pi /home/pi/.hyperion
#else
#    mkdir /home/pi
#    chown pi /home/pi
#    mkdir /home/pi/.hyperion
#    chown pi /home/pi/.hyperion
#fi




