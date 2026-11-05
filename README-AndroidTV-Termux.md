# HyperHDR v24.0.0-beta.0 — Android TV APK & Termux Headless Server

Built from the official [`HyperHDR-24.0.0-beta.0-Linux-aarch64.deb`](https://github.com/awawa-dev/HyperHDR/releases) package (`24.0.0-beta.0`, Qt `6.8.4`), bundling all **226 Web UI files** (`web_resources.rcc`) and **123 embedded JSON configuration schemas** (`res1`–`res4`, including new HID drivers `lightpack`, `paintpack`, `rawhid`, `synclight` under `leds_group_6_HID` and `sp616e` serial driver) with multi-backend Android TV screen grabbing (`MediaProjection`, `screencap`, `FlatBuffers :19400`, `Protobuf :19445`, and `JSON-RPC image :19444`), the Infinite Color Engine, and WLED UDP (`21324` DRGB/DNRGB & `4048` DDP) + LAN discovery.

## Screen Capture Architecture & Fixes (v24.0.0-beta.0)

* **Android TV APK (`HyperHDR-v24.0.0-beta.0-AndroidTV.apk`)**:
  * **Automatic `MediaProjection` Consent & Multi-Flag VirtualDisplay**: Automatically prompts for Android TV `MediaProjection` permission on launch (or via the **Start TV Capture** button) and creates the `VirtualDisplay` with `VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR | VIRTUAL_DISPLAY_FLAG_PUBLIC | VIRTUAL_DISPLAY_FLAG_PRESENTATION` (with automatic fallback) so all TV surfaces mirror reliably.
  * **Opaque Pixel Packing (`0xFF000000`)**: Forces `alpha = 0xFF` when reading `RGBA_8888` planes from `ImageReader` (`SurfaceFlinger` hardware video overlays on Android TV often write `alpha = 0`, which previously caused `Bitmap.compress(JPEG)` to render black frames in the Web UI Live Video preview).
  * **Native `screencap` Fallback + External Network Grabbers**: Includes a background `/system/bin/screencap` raw frame fallback if `MediaProjection` is idle, plus full TCP listeners for **FlatBuffers (`:19400`)**, **Protobuf (`:19445`)**, and **JSON-RPC `image` (`:19444` / `:8090`)**.
* **Termux Headless Server (`hyperhdr-termux-v24.0.0-beta.0.tar.gz`)**:
  * **Real-Time `TermuxScreenGrabber`**: Implements real screen capture in `hyperhdr_termux_daemon.py` across 5 automatic backends:
    1. Android `/system/bin/screencap` (with `LD_PRELOAD` cleared and `/system/lib64` path set so Android's native `screencap` runs inside Termux)
    2. Root `su -c /system/bin/screencap`
    3. ADB loopback (`adb exec-out screencap`)
    4. Linux Framebuffer (`/dev/graphics/fb0` or `/dev/fb0`) & Termux:X11 (`DISPLAY` via `ffmpeg`)
    5. External **FlatBuffers (`:19400`)**, **Protobuf (`:19445`)**, and **JSON-RPC `image` (`:19444`)** streams
  * **Pure-Python Live Video Stream Encoder**: Encodes captured `192x108` RGB frames into PNG binary WebSocket frames (`opcode 0x2`) with zero external Python library dependencies so the Web UI **Live Video** preview works out-of-the-box in Termux.
  * **Updated Native ELF Glibc Runner (`v24.0.0-beta.0`)**: Updated library paths from `/usr/lib/aarch64-linux-gnu/hyperhdr` to `/usr/lib/hyperhdr` (matching `v24.0.0-beta.0`'s `RPATH`), added a custom `libdbus-1.so.3` (`LIBDBUS_1_3`) shim for `HelperDBus` / `SystemSuspendDBus` / `SessionMonitorDBus`, and included a background `screencap` -> JSON-RPC bridge when running the native ELF binary in `--glibc` mode.

## Default Password & Authentication Behavior

* **Default First-Time Password**: `hyperhdr`
* **First-Time Login Flow**:
  * On a fresh installation (while the password is still the default `hyperhdr`), `authorize-newPasswordRequired` returns `true`. Just like desktop Linux HyperHDR, the Web UI automatically logs in using `hyperhdr`, displays the warning banner prompting you to change the default password, and pre-fills `hyperhdr` as the current password in the **Change Password** dialog.
* **After Changing the Password**:
  * Change the password via the sidebar **Change Password** button (minimum 8 characters).
  * Once changed, `defaultPasswordSet` becomes `false` and all previous session tokens are immediately invalidated.
  * Every new browser session, page reload after locking (`Lock UI`), or unauthenticated WebSocket connection is required to sign in on the **Login** screen with your new password before any configuration or server commands (`config`, `serverinfo`, `instance`, `componentstate`, etc.) are allowed.
* **Resetting Forgotten Password**:
  * **Android TV App**: Clear app data in Android TV Settings (`adb shell pm clear org.hyperhdr.androidtv`) or delete `/data/data/org.hyperhdr.androidtv/files/hyperhdr_db.json`.
  * **Termux**: Delete `~/.hyperhdr/hyperhdr_db.json` and restart `hyperhdr`.

## 1. Android TV APK (`HyperHDR-v24.0.0-beta.0-AndroidTV.apk`)

* **Signed APK**: `HyperHDR-v24.0.0-beta.0-AndroidTV.apk` (also copied to `HyperHDR-v22.0.0.0-AndroidTV.apk` and `app/build/outputs/apk/debug/app-debug.apk`)
* **Rebuild APK**:
  ```bash
  ./build_apk.sh
  ```
* **Install via ADB**:
  ```bash
  adb connect <tv-ip>:5555
  adb install -r HyperHDR-v24.0.0-beta.0-AndroidTV.apk
  ```

## 2. Termux Headless Server (`hyperhdr-termux-v24.0.0-beta.0.tar.gz`)

* **Archive**: `hyperhdr-termux-v24.0.0-beta.0.tar.gz` (also copied to `hyperhdr-termux-v22.0.0.0.tar.gz`)
* **Push to Device via ADB**:
  ```bash
  ./termux/adb-push-to-termux.sh <tv-ip>:5555
  ```
* **Install & Run in Termux**:
  ```bash
  tar -xzf /sdcard/Download/hyperhdr-termux-v24.0.0-beta.0.tar.gz -C ~
  ~/termux/install-in-termux.sh
  hyperhdr start
  ```
* **CLI Commands**:
  * `hyperhdr -d` — Run headless in the foreground with debug console logs
  * `hyperhdr start` — Start headless in the background with `termux-wake-lock`
  * `hyperhdr status` — Check daemon PID and recent logs
  * `hyperhdr logs` — Follow `~/.hyperhdr/hyperhdr.log`
  * `hyperhdr stop` — Stop background daemon

## 3. Native ADB Screencap Bridge (Zero-APK Grabbing)

If you run the headless server in Termux without installing any Android APK, standard Android permissions prevent Termux from capturing the screen. The native `screencap_bridge` runs under UID 2000 (`shell`) in `/data/local/tmp`, captures frames in RAM via `/system/bin/screencap`, downscales to 160x90, and streams them via loopback UDP (`:19446`) directly into HyperHDR:

* Source: [`termux/shim/hyperhdr_screencap_bridge.c`](termux/shim/hyperhdr_screencap_bridge.c)
* Host controller: [`scripts/screencap_bridge.sh`](scripts/screencap_bridge.sh)
  ```bash
  ./scripts/screencap_bridge.sh start    # Start background grabber on TV
  ./scripts/screencap_bridge.sh status   # Check grabber status
  ./scripts/screencap_bridge.sh logs     # View live capture FPS & metrics
  ./scripts/screencap_bridge.sh stop     # Stop bridge
  ```

