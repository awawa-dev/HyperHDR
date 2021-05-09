#!/bin/bash -e

CLEAN=1 ./build.sh -c config_v6
mv deploy/armv6/image*HyperHDR-lite.zip  deploy/armv6/SD-card-image-rpi012-armv6.zip