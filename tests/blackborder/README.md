# Blackbar detector regression tests

Build and run with the Qt Core development package, without TV hardware:

```sh
cmake -S tests/blackborder -B build/blackborder-tests -DCMAKE_BUILD_TYPE=Release
cmake --build build/blackborder-tests
ctest --test-dir build/blackborder-tests --output-on-failure
```

The tests use HyperHDR's actual detector and image classes. Synthetic frames
reproduce [issue #821](https://github.com/awawa-dev/HyperHDR/issues/821), including
wide two-line captions that cross Letterbox mode's fixed bottom scan positions.
They also cover asymmetric bars, aspect-ratio changes, threshold boundaries,
completely black frames, bright picture content, known limits (text reaching a
bottom corner sample, overlays in the top bar) and small or empty images.

In **Image processing → Blackbar detector**, enable detection and select
**Subtitles**. The bar height is measured at the top across the full width and
assumed at the bottom, which is only sampled near its corners, so captions in the
bottom bar are ignored. While a crop is known, frames too dark to measure keep it.
Subtitles inside the active picture are still included in LED color sampling.
Existing modes and their default selection are unchanged.

These fixtures validate the detector, not real-world capture performance or
subtitle coverage across different players. TV playback remains a manual check.
