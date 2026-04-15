# HyperHDR Display Calibration Plan

This document captures the current design direction for adding display-side calibration to HyperHDR without changing runtime behavior yet.

## Goal

The existing LUT calibration solves the source and capture path. It does not model the TV panel's own processing, so modes like LG G4 HDR or Dolby Vision Filmmaker can make the on-screen image diverge from the LED result even when the grabber LUT is otherwise correct.

The new display calibration stage should use real probe measurements from the display and feed that information back into the same 3D LUT methodology HyperHDR already uses.

## Core Direction

Display calibration should not be treated as part of transfer-header tuning.

Transfer headers remain brightness and transfer-behavior controls. Display calibration is a separate measurement workflow whose output is used to correct the final LUT.

The intended composition is:

1. Build the normal grabber LUT from source-side captures.
2. Measure the TV output with a display probe using probe-oriented test patches.
3. Solve a display correction cube from those measurements.
4. Compose the display correction back into the final HyperHDR 3D LUT.

Conceptually:

- `L_grabber`: existing LUT calibration output
- `C_display`: display-measured correction cube
- `L_final = C_display(L_grabber(...))`

The runtime artifact should still be the normal `lut_lin_tables*.3d` file family so the current grabber and LUT switching architecture stays intact.

## Why 3D LUT Composition

The display stage should stay aligned with HyperHDR's current grabber LUT methodology.

Reasons:

1. HyperHDR already consumes a single 3D LUT artifact at runtime.
2. A display-side correction needs more than a 1D transfer adjustment if saturation and hue shift with brightness.
3. Composing a display correction cube into the final LUT preserves the current pipeline instead of introducing a parallel runtime correction model.

## Measurement Model

Display calibration should use probe-oriented patches, not the full-frame board decoding workflow used by the current LUT calibrator.

Initial assumptions:

1. The measurement device is a real display probe such as DisplayProHL.
2. The patch should be a fixed window at a known location, centered by default.
3. The patch geometry and measurement conditions must be part of the saved profile metadata.

Suggested metadata to preserve:

1. Display make and model
2. Picture mode name
3. Dynamic range mode: SDR, HDR10, Dolby Vision
4. Patch size
5. Background type
6. Peak target and metadata assumptions for HDR10
7. Dwell time before read
8. Sample count and aggregation method

## Calibration Workflow

Planned workflow:

1. Run the existing source-side LUT calibration and generate the base LUT.
2. Launch a display calibration mode that presents single-patch probe targets.
3. Capture measured panel output for those targets using the display probe.
4. Import the measurement file into HyperHDR.
5. Fit a display correction cube.
6. Compose that correction into the base LUT and export a final corrected `lut_lin_tables*.3d`.

## Scope For First Version

The first useful version should still be 3D-LUT-native, but the measurement lattice can be intentionally sparse and biased toward the areas most likely to matter.

Priority areas:

1. Dense grayscale coverage
2. Primaries and secondaries at multiple luminance levels
3. Interior lattice points with emphasis on bright-region behavior
4. Near-black and near-peak validation points

This should be enough to capture hue and saturation drift at higher brightness levels, which is one of the main reasons a simple 1D transfer approach is insufficient.

## Runtime Position

No runtime architecture change is required if the display stage is applied offline.

The runtime system can continue to:

1. load a single active LUT file,
2. switch LUTs by mode and source as it already does,
3. remain independent from LED-driver-owned transfer or calibration behavior.

## Open Design Questions

1. What patch sequence format should drive the display probe run?
2. What import format should DisplayProHL measurements use?
3. Should the intermediate display correction cube be stored separately from the final composed LUT?
4. How should Dolby Vision mode be constrained given its additional TV-side processing complexity?
5. How dense does the first measurement lattice need to be to meaningfully improve bright HDR and LLDV color behavior?

## Current Working View

The display stage is best treated as a second calibration dataset that corrects the existing LUT, not as a separate transfer-profile mechanism.

That keeps the system aligned with HyperHDR's current grabber LUT methodology while letting real panel measurements influence the final LED result.