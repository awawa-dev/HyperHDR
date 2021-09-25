#!/bin/bash -e

CLEAN=1 ./build.sh -c config_v8
mv deploy/armv8/image*HyperHDR-lite.zip  deploy/armv8/SD-card-image-rpi234-aarch64.zip