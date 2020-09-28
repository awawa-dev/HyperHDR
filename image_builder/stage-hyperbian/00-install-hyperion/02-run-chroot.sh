#!/bin/bash -e

echo '           Install Hyperion'
chmod +x -R /usr/share/hyperion/bin
ln -fs /usr/share/hyperion/bin/hyperiond /usr/bin/hyperiond
ln -fs /usr/share/hyperion/bin/hyperion-remote /usr/bin/hyperion-remote
ln -fs /usr/share/hyperion/bin/hyperion-v4l2 /usr/bin/hyperion-v4l2
ln -fs /usr/share/hyperion/bin/hyperion-framebuffer /usr/bin/hyperion-framebuffer 2>/dev/null
ln -fs /usr/share/hyperion/bin/hyperion-dispmanx /usr/bin/hyperion-dispmanx 2>/dev/null
ln -fs /usr/share/hyperion/bin/hyperion-qt /usr/bin/hyperion-qt 2>/dev/null
echo '           Register Hyperion'

mv /etc/systemd/system/hyperiond@.service /etc/systemd/system/hyperion.service
sudo sed -i 's/%i/pi/g' /etc/systemd/system/hyperion.service
systemctl -q enable hyperion.service

