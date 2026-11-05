#!/usr/bin/env bash
# Helper script to control the Native ADB Screencap Bridge on Android TV
set -e

ADB="${ADB:-adb}"
DEVICE="${DEVICE:-}"
BRIDGE_BIN="/data/local/tmp/screencap_bridge"

adb_cmd() {
    if [ -n "$DEVICE" ]; then
        "$ADB" -s "$DEVICE" "$@"
    else
        "$ADB" "$@"
    fi
}

usage() {
    echo "Usage: $0 {start|stop|status|logs|restart}"
    echo "Controls the HyperHDR Screencap Bridge running as UID shell on the TV."
    echo "Optional environment variables: ADB=/path/to/adb DEVICE=192.168.x.x:5555"
    exit 1
}

check_device() {
    if ! adb_cmd get-state >/dev/null 2>&1; then
        if [ -n "$DEVICE" ]; then
            echo "Error: Device $DEVICE is not connected via ADB."
        else
            echo "Error: No ADB device connected."
        fi
        exit 1
    fi
}

case "$1" in
    start)
        check_device
        echo "Starting HyperHDR Screencap Bridge on TV..."
        adb_cmd shell "$BRIDGE_BIN -d"
        sleep 1
        adb_cmd shell "$BRIDGE_BIN -s"
        ;;
    stop)
        check_device
        echo "Stopping HyperHDR Screencap Bridge on TV..."
        adb_cmd shell "$BRIDGE_BIN -k"
        ;;
    status)
        check_device
        adb_cmd shell "$BRIDGE_BIN -s"
        ;;
    logs)
        check_device
        adb_cmd shell "cat /data/local/tmp/screencap_bridge.log | tail -n 25"
        ;;
    restart)
        check_device
        adb_cmd shell "$BRIDGE_BIN -k || true"
        sleep 1
        adb_cmd shell "$BRIDGE_BIN -d"
        sleep 1
        adb_cmd shell "$BRIDGE_BIN -s"
        ;;
    *)
        usage
        ;;
esac
