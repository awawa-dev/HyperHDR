# HyperHDR Dynamic LUT Switching Improvements (Dolby Vision)

## Overview

This document outlines improvements to the current LUT switching system used in HyperHDR for Dolby Vision (DV) content. The goal is to increase perceptual accuracy, reduce artifacts (e.g., over-darkening, oversaturation), and improve temporal stability without significantly increasing system complexity.

The current implementation relies on discrete LUT selection using either L1 Max or L1 Average thresholds. While functional, this approach introduces inaccuracies due to the reduction of rich scene metadata into a single scalar and abrupt transitions between LUTs.

The following sections describe incremental improvements to address these limitations.

---

## 1. Scene Brightness Index (Combine L1 Max + Avg)

### Problem

* L1 Max is highly sensitive to small bright highlights (e.g., sparks, lightning), causing overly aggressive tonemapping.
* L1 Avg is too conservative and does not adequately reflect highlight intensity.

### Solution

Compute a **scene brightness index** that blends L1 Avg and L1 Max:

```
scene_nits = L1_avg + k * (L1_max - L1_avg)
```

Where:

* `k` is a weighting factor (recommended range: `0.15 – 0.35`)

### Behavior

* Anchors exposure to average scene brightness
* Partially incorporates highlight intensity
* Reduces overreaction to transient peaks

### Notes

* `k` can be tuned globally or dynamically (future work: tie to highlight ratio)

---

## 2. Temporal Stability (Filtering)

### Problem

* Per-frame LUT switching leads to instability, flicker, and visible brightness shifts
* Existing hysteresis helps, but does not address continuous signal noise

### Solution

Apply temporal filtering to the computed scene brightness:

#### Option A: Simple exponential smoothing

```
filtered_nits = lerp(prev_nits, scene_nits, alpha)
```

* `alpha ≈ 0.05 – 0.2`

#### Option B: Asymmetric response (recommended)

```
if (scene_nits > prev_nits)
    filtered_nits = lerp(prev_nits, scene_nits, 0.25)   // fast attack
else
    filtered_nits = lerp(prev_nits, scene_nits, 0.05)   // slow decay
```

### Behavior

* Rapid response to brighter scenes
* Gradual adaptation to darker scenes
* Reduces flicker and LUT thrashing

### Notes

* Complements existing hysteresis rather than replacing it

---

## 3. L6 Metadata as Constraint

### Current State

* Mastering display luminance (MDL1000 vs MDL4000) is already correctly selected using metadata from Kodi

### Enhancement

Use L6 (when available) as a **bounding constraint** on scene brightness:

```
scene_nits = min(scene_nits, MaxCLL)
scene_nits = max(scene_nits, MaxFALL * 0.5)
```

### Behavior

* Prevents unrealistic brightness spikes
* Keeps scene interpretation within mastering intent

### Notes

* This is not a trigger for switching, only a clamp
* Safe to skip if metadata is unreliable

---

## 4. LUT Interpolation (Major Improvement)

### Problem

* Current system uses discrete LUT buckets
* Causes:

  * Brightness discontinuities
  * Color/saturation shifts
  * “Almost correct” rendering

### Solution

Blend between adjacent LUTs instead of selecting one:

```
t = (scene_nits - lower_bound) / (upper_bound - lower_bound)
output = (1 - t) * LUT_lower(pixel) + t * LUT_upper(pixel)
```

### Example

If scene_nits = 450:

* Blend LUT_400 and LUT_500
* `t = 0.5`

### Implementation Notes

* Blend **LUT outputs**, not LUT tables (simpler and faster)
* Can be implemented per-pixel after LUT lookup

### Benefits

* Smooth transitions across brightness levels
* Reduces oversaturation artifacts
* Eliminates threshold discontinuities

---

## 5. Highlight Gating (Future Work)

### Problem

* Small bright elements can distort scene brightness estimation

### Solution

Detect when highlights are not representative:

```
highlight_ratio = (L1_max - L1_avg) / max(1, L1_avg)
```

If ratio exceeds threshold (e.g., `3–5`):

* Reduce influence of L1 Max

Example adjustment:

```
k = base_k * clamp(1.0 - highlight_ratio * 0.1, 0.05, 1.0)
```

### Behavior

* Suppresses impact of specular highlights
* Improves stability in dark scenes with bright accents

### Notes

* Not required for initial implementation
* Recommended after interpolation is in place

---

## 6. Soft Zones (Replacing Hard Thresholds)

### Problem

* Hard thresholds create abrupt LUT switching boundaries

### Solution

Define **blending zones** instead of discrete cutoffs:

Example:

```
300–500 nits → blend region between LUT_300 and LUT_500
```

Instead of:

```
if (nits > 400) → LUT_400
```

### Behavior

* Continuous transitions instead of step changes
* Works naturally with LUT interpolation

### Notes

* This is effectively a structural extension of interpolation
* Threshold tables become range definitions instead of discrete triggers

---

## Recommended Implementation Order

To maximize impact while minimizing complexity:

1. **Scene Brightness Index (L1 Avg + Max)**
2. **Temporal Filtering**
3. **LUT Interpolation**
4. **Soft Zones (integrated with interpolation)**
5. **L6 Constraints (optional refinement)**
6. **Highlight Gating (experimental)**

---

## Expected Outcomes

After implementing the first three items:

* Reduced “too dark” scenes
* Improved highlight handling without overcompression
* Elimination of abrupt brightness/color shifts
* More consistent perceptual output across varied content

Interpolation in particular is expected to provide the most noticeable improvement.

---

## Closing Notes

This approach shifts LUT selection from a **threshold-based system** to a **continuous, signal-driven model**, better aligning with how Dolby Vision content is authored and how displays behave perceptually.

The system remains lightweight and compatible with existing LUT assets while significantly improving output quality.
