#!/bin/sh
cd "$(dirname "$0")"
# Path to hyperhdr!?
cd ../Resources/bin
exec ./hyperhdr "$@"
