# LUT Switching Notes

This note captures the current Pi-side LUT switching status so the work is easy to resume later without redoing the investigation.

## Current Pi-side Status

The Pi-side HyperHDR implementation already has a dedicated `lut-switching` API path and dedicated LUT slots, so applying a different LUT at runtime is the straightforward part of the workflow.

The current constraint is the source device. Our main media box is the Ugoos/CoreELEC path, and real-world testing showed it is not a reliable platform for HEVC RExt `4:2:2` and `4:4:4` playback. Because of that, Pi-side LUT switching has been intentionally trimmed down to `4:2:0` chroma-specific slots only.

This is not a statement that `4:2:2` or `4:4:4` are never useful in theory. It is a practical rollback based on the playback path we actually use.

## What Was Removed

The Pi-side LUT switching config/API no longer treats `444` and `422` as valid chroma switching targets.

Only `420` remains as the active chroma-specific switching branch for per-input LUT overrides.

Legacy stored `444` and `422` per-input chroma keys are pruned when the LUT switching config is rebuilt, so old settings do not keep resurfacing in API responses.

## Why This Was Deferred Instead of Replaced Immediately

The likely long-term replacement for those retired chroma slots is not more static chroma variants. The better candidate is Dolby Vision dynamic-metadata-aware switching.

That work is intentionally deferred until the new calibration source videos finish encoding and we verify that they materially improve the LUT quality. If those new calibration assets do not move results enough, there is no reason to spend time building scene-driven switching on top.

## Dolby Vision Metadata Findings

The useful scene/process data the custom Kodi build shows in `PlayerProcessInfo` is real structured data, not just overlay text.

The custom skin exposes the following labels:

- `Player.Process(video.dovi.l1.min.nits)`
- `Player.Process(video.dovi.l1.max.nits)`
- `Player.Process(video.dovi.l1.avg.nits)`
- `Player.Process(video.dovi.l6.max.cll)`
- `Player.Process(video.dovi.l6.max.fall)`

Those labels are backed by the custom CPM-A14 Kodi fork:

- The skin binds them in `DialogPlayerProcessInfo.xml`.
- `GUIInfoManager.cpp` registers the custom `Player.Process(video.dovi...)` labels.
- `PlayerGUIInfo.cpp` resolves them from `DataCacheCore`.
- `DataCacheCore` stores per-frame DoVi metadata keyed by render PTS.
- `BitstreamConverter.cpp` parses the Dolby Vision RPU metadata and populates that cache.

The practical takeaway is that current-scene Dolby Vision brightness information is already available in the player stack and should be pullable programmatically.

## PQ Versus Nits Verification

Follow-up verification against the live CPM-A14 Kodi JSON-RPC endpoint confirmed that the custom `Player.Process(video.dovi.l1.*.pq)` labels are not a separate Dolby Vision signal family.

They track the same live Dolby Vision `l1` scene brightness values exposed through `Player.Process(video.dovi.l1.*.nits)`, but in ST.2084 PQ code space instead of luminance space.

Observed verification points:

1. The custom skin exposes both `video.dovi.l1.*.nits` and `video.dovi.l1.*.pq` labels side by side in `DialogPlayerProcessInfo.xml`.
2. Live `XBMC.GetInfoLabels` polling returned matching `l1` nits and PQ triplets for the same playback frame.
3. The reported `max` values matched ST.2084 conversion almost exactly, for example `257 nits -> 2480 PQ`, `405 nits -> 2678 PQ`, `1011 nits -> 3084 PQ`, and `3770 nits -> 3670 PQ`.
4. The reported `avg` values also map correctly once account is taken for the nits label being rounded more coarsely than the PQ label.

Practical takeaway:

1. PQ does not add new Dolby Vision scene information beyond the live `l1` brightness signal.
2. PQ may still be a better runtime switching trigger than the integer nits label because it appears to preserve finer precision and is already in a perceptual domain.
3. Current HyperHDR Dolby Vision runtime bucket logic still uses `fllAvgCdM2`, `l1AvgNits`, `fllUpperCdM2`, and `l1MaxNits`; PQ is not yet part of the decision path.

## Likely Future Direction

If the new calibration videos prove worthwhile, the next incremental step should be an information-sweep prototype rather than a full switching implementation.

The cleanest candidate path is to poll Kodi over JSON-RPC using `XBMC.GetInfoLabels` for the custom `Player.Process(video.dovi...)` labels, then relay that scene metadata to the Pi-side controller logic.

That would let us test low/mid/high Dolby Vision scene buckets and decide whether those buckets justify repurposing the retired `422` and `444` switching space for dynamic-metadata-driven LUT selection.

## Calibration Video Findings So Far

Initial testing of the newer generated Dolby Vision calibration clips has been positive enough to keep pursuing this path.

Observed behavior so far:

1. Changing the generated `npl` value does materially change playback brightness.
2. The newer clips already show positive improvement in highly saturated scenes.
3. The current generated clips are still a poor fit for full-length real Dolby Vision titles with scene-varying metadata.
4. A dark scene shortly after a bright test scene can become much too dim in capture, which matches the expectation that a static calibration clip cannot track a title whose Dolby metadata changes scene to scene.

Important comparison points:

1. The imported `test_DV_yuv420_low_quality.mp4` remains a useful playback reference, but its exact generation flags are unknown.
2. The older locally generated `test_HDR_yuv420_low_quality_p8_generated.mp4` is a better internal comparison point for our own generation path.
3. Real titles like `Finding Dory` show wider and more dynamic Dolby Vision behavior than either of those static test clips.

Practical takeaway:

The new calibration videos are already good enough to justify continued investigation, but they currently validate static or semi-static brightness behavior better than true scene-by-scene Dolby Vision adaptation.

An important emerging signal is that Kodi/CoreELEC-reported FLL Min/Max appears to be a better runtime switching indicator than the static file-level Dolby Vision summary alone.

Observed direction so far:

1. At lower reported FLL values, the older LUT calibration remains reasonably acceptable.
2. Around the time reported FLL rises into the roughly `69 cd/m^2` range, the newer `npl1000` LUT starts to become the more acceptable match.

This suggests runtime Dolby Vision switching should pay particular attention to the Kodi-reported `FLL Min/Max lower | upper lightvalue cd/m^2` line and treat it as a primary candidate signal when defining low/mid/high Dolby Vision buckets.

## Why Kodi/CoreELEC Polling Now Looks Viable

This testing was one of the gates before attempting runtime Dolby Vision LUT switching.

That gate is now partially cleared:

1. The new calibration clips can positively influence results.
2. The remaining mismatch appears to be strongly tied to scene-varying Dolby Vision metadata rather than the idea of alternate LUTs itself.
3. Because the custom Kodi/CoreELEC stack already exposes per-scene Dolby Vision process labels, runtime polling now looks like a viable next experiment.

This does not mean runtime switching should be implemented immediately. It means the idea has now earned a prototype stage.

## Staged Plan From Here

Use the next stages in order:

1. Finish validating the current static calibration clips on a small set of known-problem scenes.
	Focus on whether the new clips improve saturated scenes enough to justify further work.

2. Log real Dolby Vision process data during playback.
	Use Kodi `Player.Process(video.dovi...)` labels to capture scene-by-scene `l1` and `l6` behavior from titles like `Finding Dory`.

3. Define practical runtime buckets.
	Start with a small low/mid/high Dolby Vision bucket model driven by observed scene metadata rather than by static file metadata alone.
	Prioritize Kodi-reported `FLL Min/Max` as the first bucket candidate signal, then compare it against `l1` and `l6` fields only as supporting context.

4. Prototype metadata-driven runtime LUT switching without changing the storage model first.
	Reuse the existing Pi-side per-input LUT switching architecture and only prove that polling plus bucket selection is stable.

5. Only then decide whether to generate more DV calibration families.
	If runtime bucketing shows promise, new calibration generation should be driven by the observed bucket boundaries rather than by the original static planning grid alone.

## End-to-End Prototype Plan

The first implementation should be intentionally conservative.

The current Pi-side `lut-switching` API already supports applying an explicit file override, and the existing `vrroom_hyperhdr_sync.py` script already drives runtime LUT application. That means the first Dolby Vision runtime-threshold prototype should keep the bucket-selection logic in the polling script rather than immediately pushing new threshold logic into HyperHDR core.

### 1. Switching model

Use ordered `cd/m^2` threshold values rather than fixed low/mid/high labels.

Recommended semantics:

1. A bucket model is defined by an ordered array of upper-bound thresholds in `cd/m^2`.
2. Bucket count is therefore `threshold_count + 1`.
3. The first test should still target the equivalent of `low/mid/high`, which means `3` buckets and therefore `2` interior threshold values.
4. Dolby Vision bucket decisions should be based primarily on Kodi/CoreELEC-reported `FLL Min/Max lower | upper lightvalue cd/m^2`.
5. `l1` and `l6` data should be logged alongside FLL, but treated as secondary context during the first prototype.

For HDR, keep the model simpler:

1. Start with `2` buckets or at most `3`.
2. HDR does not need scene-by-scene switching in the same way Dolby Vision does.
3. HDR bucket choice can continue to use static stream metadata such as reported HDR nits or mastering envelope rather than Kodi scene polling.

### 2. Prototype architecture

For the first end-to-end test:

1. `vrroom_hyperhdr_sync.py` remains the single runtime controller.
2. Add Kodi JSON-RPC polling there rather than introducing a second long-running service.
3. When playback is SDR or HDR-only, continue using the current VRROOM-driven path.
4. When playback is Dolby Vision/LLDV on a DV-capable input, query Kodi for current Dolby Vision process labels.
5. Use the current scene FLL value to select a configured DV bucket.
6. Resolve the corresponding per-input LUT file in the script.
7. Apply that exact LUT file through the existing HyperHDR `lut-switching` API using the `file` override path.

This keeps HyperHDR core as the apply/reload engine and puts the experimental threshold logic in one script that is easy to back up and revert.

### 3. Kodi polling work

Add a small Kodi polling block to `vrroom_hyperhdr_sync.py` with these responsibilities:

1. Configure Kodi host, port, and optional credentials.
2. Poll `XBMC.GetInfoLabels` on a short interval only when the current mode is `lldv`.
3. Request the custom CPM-A14 labels for:
	- `Player.Process(video.dovi.l1.min.nits)`
	- `Player.Process(video.dovi.l1.max.nits)`
	- `Player.Process(video.dovi.l1.avg.nits)`
	- `Player.Process(video.dovi.l6.max.cll)`
	- `Player.Process(video.dovi.l6.max.fall)`
	- the `FLL Min/Max lower | upper lightvalue cd/m^2` line if available through the same process info path
4. Parse numeric values robustly, accepting missing or malformed labels without crashing the loop.
5. Log raw and parsed values during the prototype phase.
6. Debounce bucket changes so LUTs do not flap on noisy or momentary metadata transitions.
7. Apply hysteresis or a minimum dwell time before changing DV buckets.

### 4. DV bucket configuration in the script

Keep the initial DV bucket configuration script-local and explicit.

Suggested shape:

1. `DV_FLL_THRESHOLD_VALUES_CD_M2 = [...]`
2. `DV_BUCKET_LUTS = { input_name: [bucket0, bucket1, ...] }`
3. Each active DV-capable input must provide a LUT file per configured bucket.
4. Steam Deck should not participate in DV bucket logic because it does not currently present a DV path.
5. `rx3` remains undefined until a real device is installed and measured.

The exact threshold values should remain easy to edit without touching HyperHDR core.

### 5. HDR bucket configuration in the script

HDR should stay simpler than Dolby Vision.

Suggested shape:

1. `HDR_THRESHOLD_VALUES_NITS = [...]`
2. `HDR_BUCKET_LUTS = { input_name: [bucket0, bucket1, ...] }`
3. Start with `2` buckets by default and only expand to `3` if testing justifies it.
4. Continue sourcing HDR bucket input from the existing VRROOM/HyperHDR-reported stream metadata rather than Kodi scene polling.

### 6. HyperHDR repository changes for the prototype

For the first prototype, HyperHDR core changes should be kept to the minimum possible.

Required repo changes for the prototype:

1. Documentation updates only.
2. Optional small API/status improvements only if the script proves it needs more runtime state from HyperHDR.

Not required for the first prototype:

1. Replacing the current fixed low/mid/high transfer automation model.
2. Adding dynamic threshold arrays to HyperHDR settings.
3. Expanding `lut-switching` config keys for arbitrary bucket counts.

Those should be second-stage productization tasks only if the script-driven prototype proves useful.

### 7. HyperHDR repository changes for later productization

If the prototype works, the second stage should move threshold configuration into HyperHDR settings.

That would likely require changes to:

1. `sources/api/HyperAPI.cpp`
   Add structured settings parsing and validation for threshold arrays.
2. `sources/api/JSONRPC_schema/schema-lut-switching.json`
   Allow new threshold-array and bucket-mapping properties.
3. Any UI/settings surfaces that should expose those thresholds later.
4. Possibly the runtime response payload so the script and UI can inspect the active threshold model.

The productized model should prefer ordered arrays over fixed `Low/Mid/High` fields.

### 8. Backup plan before implementation

Before changing runtime files, make timestamped backups in-place so the real Pi-side fork always has a reliable local rollback point.

Back up at minimum:

1. `\\hyperhdr.local\pi\vrroom_hyperhdr_sync.py`
2. `\\hyperhdr.local\pi\hyperhdr-master\hyperhdr\sources\api\HyperAPI.cpp`
3. `\\hyperhdr.local\pi\hyperhdr-master\hyperhdr\sources\api\JSONRPC_schema\schema-lut-switching.json`
4. `\\hyperhdr.local\pi\hyperhdr-master\hyperhdr\README_LUT_SWITCHING_NOTES.md`

Recommended naming pattern:

1. `.bak-YYYYMMDD-HHMM-description`

Also capture the current active HyperHDR LUT-switching settings response before any changes so there is a settings-level rollback reference in addition to file backups.

### 9. Acceptance criteria for moving from prototype to implementation

Proceed with the prototype only if these remain true:

1. The current new calibration clips continue to show visible improvement on at least some known-problem Dolby Vision scenes.
2. Logged Kodi FLL values correlate with which LUT looks more acceptable.
3. Debounced bucket switching does not cause obvious flapping or reload instability.
4. The bucket model stays small enough that per-input LUT storage remains manageable.

## Recommended Next Step

Keep testing the current generated calibration clips on representative scenes, then build a lightweight Dolby Vision metadata poller/logger first.

Only after those logs confirm useful bucket boundaries should we decide whether fast LUT switching on scene metadata is worth the added complexity.