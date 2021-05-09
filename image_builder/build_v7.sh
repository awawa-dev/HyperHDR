#!/bin/bash -e

CLEAN=1 ./build.sh -c config_v7
mv deploy/armv7/image*HyperHDR-lite.zip  deploy/armv7/SD-card-image-rpi34-armv7.zip