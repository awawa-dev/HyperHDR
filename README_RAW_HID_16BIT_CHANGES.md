# HyperHDR RawHID + Adalight16 Change Notes

This document summarizes the current host-side RawHID, Adalight16, and related ICE RGBW parity changes carried by this repository.

It is intended as a technical handoff for the BFI branch and as a quick reference when merging new upstream HyperHDR changes.

## Scope

These notes cover the HyperHDR-side transport, schema, API, and web UI changes related to:

- Linux RawHID output
- Adalight16 transport updates
- ICE RGBW parity updates for additional built-in drivers
- host-owned transfer and calibration header handling
- host-owned RGBW and solver-aware highlight policy behavior

Firmware-side Teensy sketches, USB descriptors, and device-side solver logic are out of tree and documented separately.

## Current HyperHDR-side feature set

### 1) Adalight16 write timeout support

Adalight16 now has a configurable `writeTimeout` exposed through:

- `sources/led-drivers/schemas/schema-adalight16.json`
- `sources/led-drivers/serial/ProviderSerial.cpp`
- `include/led-drivers/serial/ProviderSerial.h`

Behavior:

- `writeTimeout > 0`: blocking serial write wait
- `writeTimeout = 0`: non-blocking serial write path

This is implemented at the shared serial-provider level, so it affects Adalight16 without changing unrelated network drivers.

### 2) Linux RawHID LED driver

The repository carries a custom Linux-only `rawhid` LED output driver:

- `include/led-drivers/other/DriverOtherRawHid.h`
- `sources/led-drivers/other/DriverOtherRawHid.cpp`

Capabilities:

- explicit `hidraw` device selection
- auto-discovery by VID/PID using `/sys/class/hidraw/*/device/uevent`
- configurable report size and write timeout
- optional sideband log reads from the device
- support for 16-bit RGB and packed 12-bit carry payload modes

The host transport still uses framed reports, including the host-to-device LED payload path and the optional device-to-host sideband log path.

### 3) RawHID schema now reflects host-owned pipeline controls

`sources/led-drivers/schemas/schema-rawhid.json` is no longer just a minimal transport schema. It currently exposes several groups of settings:

Transport and device selection:

- `output`
- `VID`
- `PID`
- `reportSize`
- `writeTimeout`
- `delayAfterConnect`
- `readLogChannel`
- `logPollIntervalFrames`
- `logReadBudget`
- `payloadMode`

Host-owned transfer and calibration:

- `transferCurveOwner`
- `transferCurveProfile`
- `calibrationHeaderOwner`
- `calibrationHeaderProfile`
- `calibrationTransferOrder`

Host policy / highlight path:

- `scenePolicyMode`
- `scenePolicyEnableHighlight`
- `highlightShadowPercent`
- `highlightShadowMinPeakDelta`
- `highlightShadowUniformSpreadMax`
- `highlightShadowTriggerMargin`
- `highlightShadowMinSceneMedian`
- `highlightShadowMinSceneAvg`
- `highlightShadowDimUniformMedian`
- `highlightShadowQ16Threshold`

Host-owned RGBW and legacy white-channel controls:

- `enable_ice_rgbw`
- `ice_white_mixer_threshold`
- `ice_white_led_intensity`
- `ice_white_temperatur_red/green/blue`
- `white_channel_calibration`
- `white_channel_limit`
- `white_channel_red`
- `white_channel_green`
- `white_channel_blue`

Runtime compatibility note:

- RawHID still accepts the older `ice_white_temperatur_r/g/b` names as a fallback so older configs keep working

### 4) Host-owned transfer headers and calibration headers

This branch adds HyperHDR-side storage and management for custom transfer and calibration headers.

Backend/API integration:

- `sources/api/HyperAPI.cpp`
- `sources/api/JSONRPC_schema/schema-transfer-headers.json`
- `sources/api/JSONRPC_schema/schema-calibration-headers.json`
- `sources/api/JSONRPC_schemas.qrc`
- `sources/base/GrabberWrapper.cpp`

Web UI integration:

- `www/content/light_source.html`
- `www/js/hyperhdr.js`
- `www/js/light_source.js`

Current behavior:

- transfer headers are stored in HyperHDR's data directory under `transfer_headers`
- calibration headers are stored separately under `calibration_headers`
- the UI can list, save, and delete these profiles
- RawHID can be configured so HyperHDR, rather than the Teensy, owns the transfer curve and/or calibration header stage

### 5) Current RawHID host policy behavior

The current repository no longer carries the older scene-offset/scene-aware experiments.

The supported host policy modes are now:

- `disabled`
- `solverAwareV2`
- `solverAwareV3`

Current behavior:

- `solverAwareV2` keeps the older legacy energy model for highlight analysis
- `solverAwareV3` uses the direct solver emitted-output energy model for highlight analysis
- solver-aware mode no longer rewrites emitted values through the solver ladder; it uses solver-derived output only for analysis and optional non-highlight capping
- custom solver profiles can be uploaded and selected, with the temporal runtime solver header `.h` treated as the preferred source of truth

The main user-facing control for the non-highlight ceiling is:

- `highlightShadowQ16Threshold`

This is the current branch behavior and should be treated as the authoritative description, rather than any older notes referring to scene-aware offset logic.

### 6) 16-bit output path and host-owned RGBW output

Both Adalight16 and RawHID carry custom high-precision output handling:

- `sources/led-drivers/serial/DriverSerialAdalight16.cpp`
- `sources/led-drivers/other/DriverOtherRawHid.cpp`

Relevant current behavior:

- the branch keeps the 16-bit-oriented path instead of falling back immediately to 8-bit quantization
- Adalight16 supports host ICE RGBW output as well as the older white-channel calibration trailer path
- RawHID supports host ICE RGBW output and can also combine that path with the current solver-aware highlight policy behavior
- the host-owned RawHID ICE RGBW infinite path now stays in float/Q16 through highlight analysis and output preparation instead of dropping to 8-bit in the highlight-selection stage
- Adalight and Adalight16 runtime parsing now tolerate both the modern `ice_white_temperatur_red/green/blue` keys and the older `r/g/b` keys

### 7) Additional ICE RGBW parity now present in this tree

The branch now also carries ICE RGBW parity work for several built-in drivers beyond RawHID and Adalight16.

Backends added or updated:

- DDP
- ArtNet
- RPi PIO
- HyperSPI
- Adalight

This parity work includes backend `writeInfiniteColors(...)` support and matching schema exposure where applicable.

## Current operational constraints

### Linux-only RawHID transport

The RawHID driver currently reports itself as Linux-only. It expects `hidraw` access and Linux-style device discovery.

### Linux permissions

RawHID opens `/dev/hidraw*` with read/write access.

If HyperHDR runs without permission to open the device, startup will fail. Use appropriate udev or group rules for the runtime user.

### Sideband logging overhead

RawHID sideband log reads are optional. For the quietest or lowest-overhead host path, disable `readLogChannel`.

### USB link speed still matters

For high-throughput experiments, the USB link speed and firmware buffering still matter. This document only describes the host-side repository state; it does not guarantee equivalent performance across all firmware/device configurations.

## Sync guidance for future upstream merges

Use a dedicated branch workflow so upstream syncs do not overwrite custom transport and schema work.

Recommended conflict hotspots:

- `sources/led-drivers/other/DriverOtherRawHid.cpp`
- `include/led-drivers/other/DriverOtherRawHid.h`
- `sources/led-drivers/schemas/schema-rawhid.json`
- `include/led-drivers/net/DriverNetDDP.h`
- `sources/led-drivers/net/DriverNetDDP.cpp`
- `include/led-drivers/net/DriverNetUdpArtNet.h`
- `sources/led-drivers/net/DriverNetUdpArtNet.cpp`
- `include/led-drivers/pwm/rpi_pio/DriverRpiPio.h`
- `sources/led-drivers/pwm/rpi_pio/DriverRpiPio.cpp`
- `include/led-drivers/spi/DriverSpiHyperSPI.h`
- `sources/led-drivers/spi/DriverSpiHyperSPI.cpp`
- `include/led-drivers/serial/DriverSerialAdalight.h`
- `sources/led-drivers/serial/DriverSerialAdalight.cpp`
- `sources/led-drivers/serial/ProviderSerial.cpp`
- `include/led-drivers/serial/ProviderSerial.h`
- `include/led-drivers/serial/DriverSerialAdalight16.h`
- `sources/led-drivers/serial/DriverSerialAdalight16.cpp`
- `sources/led-drivers/schemas/schema-adalight.json`
- `sources/led-drivers/schemas/schema-adalight16.json`
- `sources/led-drivers/schemas/schema-ddp.json`
- `sources/led-drivers/schemas/schema-artnet.json`
- `sources/led-drivers/schemas/schema-rpi_pio.json`
- `sources/led-drivers/schemas/schema-hyperspi.json`
- `include/api/HyperAPI.h`
- `sources/api/HyperAPI.cpp`
- `sources/api/JSONRPC_schema/schema-solver-profiles.json`
- `sources/api/JSONRPC_schema/schema.json`
- `sources/api/JSONRPC_schemas.qrc`
- `www/js/hyperhdr.js`
- `www/js/light_source.js`
- `www/content/light_source.html`

When conflicts occur, prefer preserving behavior rather than preserving line history. This branch has custom transport, policy, and UI/API integration that upstream does not fully mirror.

## Files to pull into the separate git repository

If the goal is to reproduce the full current feature set in another repository, copy files as grouped sets rather than one-by-one.

### A) RawHID core driver and solver-aware behavior

- `include/led-drivers/other/DriverOtherRawHid.h`
- `sources/led-drivers/other/DriverOtherRawHid.cpp`
- `sources/led-drivers/other/RawHidSolverLaddersRgbw.h`
- `sources/led-drivers/other/RawHidTransferCurve3_4New.h`
- `sources/led-drivers/schemas/schema-rawhid.json`

These should be copied together.

### B) Solver profile API and LED Hardware UI

- `include/api/HyperAPI.h`
- `sources/api/HyperAPI.cpp`
- `sources/api/JSONRPC_schema/schema-solver-profiles.json`
- `sources/api/JSONRPC_schema/schema.json`
- `sources/api/JSONRPC_schemas.qrc`
- `www/js/hyperhdr.js`
- `www/js/light_source.js`
- `www/content/light_source.html`

Copy this set together with the RawHID files above if you want custom solver profile upload, selection, and management to work.

### C) Transfer and calibration header support

- `sources/api/JSONRPC_schema/schema-transfer-headers.json`
- `sources/api/JSONRPC_schema/schema-calibration-headers.json`
- `sources/base/GrabberWrapper.cpp`
- `sources/api/HyperAPI.cpp`
- `sources/api/JSONRPC_schemas.qrc`
- `www/js/hyperhdr.js`
- `www/js/light_source.js`
- `www/content/light_source.html`

This set is coupled to the HyperAPI and LED Hardware page changes.

### D) Shared serial support used by Adalight16

- `sources/led-drivers/serial/ProviderSerial.cpp`
- `include/led-drivers/serial/ProviderSerial.h`

This is required for the shared serial `writeTimeout` support used by Adalight16.

### E) Adalight and Adalight16

- `include/led-drivers/serial/DriverSerialAdalight.h`
- `sources/led-drivers/serial/DriverSerialAdalight.cpp`
- `sources/led-drivers/schemas/schema-adalight.json`
- `include/led-drivers/serial/DriverSerialAdalight16.h`
- `sources/led-drivers/serial/DriverSerialAdalight16.cpp`
- `sources/led-drivers/schemas/schema-adalight16.json`

Notes:

- Adalight is parity-aligned on ICE RGBW naming/defaults and runtime handling.
- Adalight16 is local/custom and should be treated as a branch-owned extension.
- `schema-adalight16.json` still exposes the older `ice_white_temperatur_r/g/b` field names, while the runtime accepts both old and new names.

### F) Additional ICE RGBW parity drivers

- `include/led-drivers/net/DriverNetDDP.h`
- `sources/led-drivers/net/DriverNetDDP.cpp`
- `sources/led-drivers/schemas/schema-ddp.json`
- `include/led-drivers/net/DriverNetUdpArtNet.h`
- `sources/led-drivers/net/DriverNetUdpArtNet.cpp`
- `sources/led-drivers/schemas/schema-artnet.json`
- `include/led-drivers/pwm/rpi_pio/DriverRpiPio.h`
- `sources/led-drivers/pwm/rpi_pio/DriverRpiPio.cpp`
- `sources/led-drivers/schemas/schema-rpi_pio.json`
- `include/led-drivers/spi/DriverSpiHyperSPI.h`
- `sources/led-drivers/spi/DriverSpiHyperSPI.cpp`
- `sources/led-drivers/schemas/schema-hyperspi.json`

Copy each backend together with its schema.

### G) Optional documentation handoff files

- `README_RAW_HID_16BIT_CHANGES.md`
- `CHANGELOG_RAWHID_SOLVERAWARE_V3_2026-03-29.md`

These are not required for runtime behavior, but they are useful when rebasing or reapplying the branch work elsewhere.

## File index

Core RawHID files:

- `include/led-drivers/other/DriverOtherRawHid.h`
- `sources/led-drivers/other/DriverOtherRawHid.cpp`
- `sources/led-drivers/other/RawHidSolverLaddersRgbw.h`
- `sources/led-drivers/other/RawHidTransferCurve3_4New.h`
- `sources/led-drivers/schemas/schema-rawhid.json`

Adalight / Adalight16 / serial support:

- `include/led-drivers/serial/DriverSerialAdalight.h`
- `sources/led-drivers/serial/DriverSerialAdalight.cpp`
- `sources/led-drivers/schemas/schema-adalight.json`
- `include/led-drivers/serial/DriverSerialAdalight16.h`
- `sources/led-drivers/serial/DriverSerialAdalight16.cpp`
- `sources/led-drivers/serial/ProviderSerial.cpp`
- `include/led-drivers/serial/ProviderSerial.h`
- `sources/led-drivers/schemas/schema-adalight16.json`

Additional ICE RGBW parity drivers:

- `include/led-drivers/net/DriverNetDDP.h`
- `sources/led-drivers/net/DriverNetDDP.cpp`
- `sources/led-drivers/schemas/schema-ddp.json`
- `include/led-drivers/net/DriverNetUdpArtNet.h`
- `sources/led-drivers/net/DriverNetUdpArtNet.cpp`
- `sources/led-drivers/schemas/schema-artnet.json`
- `include/led-drivers/pwm/rpi_pio/DriverRpiPio.h`
- `sources/led-drivers/pwm/rpi_pio/DriverRpiPio.cpp`
- `sources/led-drivers/schemas/schema-rpi_pio.json`
- `include/led-drivers/spi/DriverSpiHyperSPI.h`
- `sources/led-drivers/spi/DriverSpiHyperSPI.cpp`
- `sources/led-drivers/schemas/schema-hyperspi.json`

Transfer/calibration header support:

- `include/api/HyperAPI.h`
- `sources/api/HyperAPI.cpp`
- `sources/api/JSONRPC_schema/schema-solver-profiles.json`
- `sources/api/JSONRPC_schema/schema.json`
- `sources/api/JSONRPC_schema/schema-transfer-headers.json`
- `sources/api/JSONRPC_schema/schema-calibration-headers.json`
- `sources/api/JSONRPC_schemas.qrc`
- `sources/base/GrabberWrapper.cpp`
- `www/content/light_source.html`
- `www/js/hyperhdr.js`
- `www/js/light_source.js`
