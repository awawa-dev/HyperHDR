# Color Space And Transfer Reference

This note is a practical reference for later work on transfer-curve selection.

It separates three things that are often mixed together:

1. Color primaries or gamut container
2. Transfer function or display gamma
3. Metadata or profile conventions used by delivery formats

The important rule is that color space and gamma are related, but they are not the same thing. A source tagged as Rec.2020 does not automatically imply PQ or HLG. Dolby Vision almost always implies PQ for the Dolby layer, but compatibility layers can be SDR, HDR10, or HLG depending on profile.

## Fast Summary

| Family | Common gamut / primaries | Common transfer / display target | Notes |
| --- | --- | --- | --- |
| SDR desktop / web | sRGB | sRGB TRC, commonly treated like gamma 2.2 | Same primaries and white point as Rec.709, different TRC |
| SDR broadcast HD | Rec.709 | Rec.709 OETF, typically viewed on BT.1886 / gamma 2.4 displays | Most common SDR TV pipeline |
| SDR legacy SD | Rec.601 / SMPTE-C / EBU / BT.470 | Historically CRT-style gamma, often ends up approximated around gamma 2.2 to 2.4 in modern chains | Legacy content needs caution |
| SDR wide gamut UHD | Rec.2020 SDR | Rec.2020 OETF, typically displayed with BT.1886-like gamma 2.4 | Exists, but less common than HDR Rec.2020 |
| HDR10 / HDR10+ | Rec.2020 container | PQ / ST 2084 | HDR10+ adds dynamic metadata, same base transfer family |
| Broadcast HDR | Rec.2020 container | HLG | Backward-oriented broadcast HDR, no static mastering metadata required |
| Dolby Vision | Rec.2020 container, often P3 mastering inside that container | PQ / ST 2084 for Dolby Vision image representation | Dynamic metadata; some profiles carry compatibility layers |
| Digital cinema SDR | DCI-P3 | Gamma 2.6 | Cinema, not home TV |
| Consumer wide-gamut displays | Display P3 / P3-D65 | Usually sRGB-like TRC or platform-specific display pipeline | Common on Apple-class displays, not a broadcast standard |

## SDR Reference

### Rec.601 / BT.470 / SMPTE-C / EBU-era SDR

These are legacy SD television families.

Typical associations:

| Signal family | Typical primaries | Typical transfer / display expectation |
| --- | --- | --- |
| NTSC-derived SD | SMPTE-C or older NTSC-family primaries | CRT-style behavior, often approximated today around gamma 2.2 to 2.4 |
| PAL / SECAM-derived SD | EBU / BT.470 primaries | CRT-style behavior, often approximated today around gamma 2.4 |
| Rec.601 digital SD | Matrix / coding standard more than a single modern display gamut | Usually lands in modern SDR playback as a Rec.709-like or BT.1886-like display rendering |

Practical note:

- Old SDR material is often not reliable enough at the metadata level to drive automatic gamma switching.
- For modern playback, these sources are usually tone-shaped into a standard SDR display response rather than preserved as an exact historic CRT behavior.

### Rec.709 SDR

Rec.709 is the dominant HD SDR video standard.

Typical associations:

| Item | Typical value |
| --- | --- |
| Primaries | Rec.709 |
| White point | D65 |
| Camera / encoding transfer | Rec.709 OETF |
| Display target in grading / reference monitoring | BT.1886 EOTF, effectively close to gamma 2.4 |
| Consumer shorthand | "gamma 2.4 SDR" |

Practical note:

- For TV and movie SDR, gamma 2.4 or BT.1886 is the most common reference assumption.
- For brighter-room or computer-style viewing, people often approximate with gamma 2.2 instead.

### sRGB SDR

sRGB shares Rec.709 primaries and D65 white point, but not the same transfer curve.

Typical associations:

| Item | Typical value |
| --- | --- |
| Primaries | Same as Rec.709 |
| White point | D65 |
| Transfer | sRGB piecewise TRC |
| Common shorthand | "roughly gamma 2.2" |

Practical note:

- sRGB is the default assumption for desktop graphics, UI, and web assets.
- It should not be treated as identical to broadcast Rec.709 even though the primaries match.

### Rec.2020 SDR

Rec.2020 is not HDR by itself. It can carry SDR wide-gamut content.

Typical associations:

| Item | Typical value |
| --- | --- |
| Primaries | Rec.2020 |
| White point | D65 |
| Transfer | Rec.2020 SDR OETF, derived from Rec.709-style behavior |
| Display reference expectation | Usually BT.1886-like or gamma 2.4 appearance |

Practical note:

- If you see Rec.2020 without PQ or HLG, that does not necessarily mean HDR.
- Some UHD SDR workflows use Rec.2020 gamut with an SDR transfer function.

### DCI-P3 And Display P3

These matter because a lot of real displays and masters sit in P3 even when delivery uses another container.

Typical associations:

| Variant | White point | Typical transfer / display target | Notes |
| --- | --- | --- | --- |
| DCI-P3 | DCI white point in cinema workflows | Gamma 2.6 | Digital cinema reference |
| P3-D65 / Display P3 | D65 | Usually sRGB-like TRC in consumer platforms | Common for phones, tablets, monitors |

Practical note:

- Many HDR masters are effectively graded to P3 gamut inside a Rec.2020 signal container.
- That is a mastering / display volume fact, not the same as transport metadata saying the signal itself is P3.

## HDR Reference

### HDR10

Typical associations:

| Item | Typical value |
| --- | --- |
| Primaries / container | Rec.2020 |
| Transfer | PQ / SMPTE ST 2084 |
| Metadata | Static metadata such as mastering display info |
| Common shorthand | "Rec.2020 + PQ" |

Practical note:

- Most consumer HDR disc and streaming delivery that is not Dolby Vision or HLG is effectively this class.
- The mastering display is frequently P3-limited even though the bitstream container is Rec.2020.

### HDR10+

Typical associations:

| Item | Typical value |
| --- | --- |
| Primaries / container | Rec.2020 |
| Transfer | PQ / SMPTE ST 2084 |
| Metadata | Dynamic metadata |

Practical note:

- From a transfer-curve selection standpoint, HDR10 and HDR10+ usually belong in the same broad PQ-driven bucket unless you later decide to use metadata-driven refinement.

### HLG

Typical associations:

| Item | Typical value |
| --- | --- |
| Primaries / container | Usually Rec.2020 |
| Transfer | HLG |
| Metadata | Usually no static mastering metadata required |
| System gamma | Display-system dependent, often referenced around 1.2 at 1000 nits in Rec.2100 system behavior |

Practical note:

- HLG is common in broadcast HDR.
- It is not PQ and should not be grouped with HDR10 or Dolby Vision for transfer-curve decisions.

## Dolby Vision Reference

Dolby Vision is best thought of as a PQ-based HDR family with dynamic metadata and multiple profile layouts.

### Core Dolby Vision Characteristics

| Item | Typical value |
| --- | --- |
| Core transfer | PQ / SMPTE ST 2084 |
| Container gamut | Rec.2020 |
| Common mastering gamut | Often P3 inside Rec.2020 container |
| Metadata | Dynamic, scene or frame based |

Practical note:

- If the question is "what broad gamma family does Dolby Vision usually imply?" the answer is PQ.
- If the question is "what exact compatible layer is present?" the answer depends on profile.

### Important Dolby Vision Profile Buckets

| Profile family | Broad meaning | Typical transfer implication |
| --- | --- | --- |
| Profile 5 | Dolby Vision only | PQ |
| Profile 7 | UHD Blu-ray dual-layer Dolby Vision with HDR10 compatibility | PQ for Dolby Vision, HDR10-compatible base layer |
| Profile 8.x | Single-layer Dolby Vision derived from another base system | Depends on sub-profile: can align with HDR10, SDR, or HLG compatibility |
| Profile 8.1 | HDR10-compatible base layer | PQ-oriented |
| Profile 8.4 | HLG-derived compatibility behavior | HLG compatibility layer with Dolby metadata layer |
| Profile 9 | SDR compatibility emphasis | SDR-oriented fallback behavior |

Practical note:

- For future automation, a safe first-pass grouping is:
  - Dolby Vision PQ bucket: profiles like 5, 7, 8.1
  - Dolby Vision HLG-adjacent bucket: profile 8.4
  - Dolby Vision SDR-adjacent bucket: SDR compatibility profiles
- If the upstream metadata only says "Dolby Vision" with no profile detail, PQ is the most defensible default assumption.

## Practical Mapping For Future Curve Selection

This is not a standard. It is a practical grouping that is likely useful later.

| Incoming signal family | Safe first-pass transfer bucket |
| --- | --- |
| SDR Rec.709 | SDR 2.4 / BT.1886 bucket |
| SDR sRGB / desktop-origin | SDR 2.2 / sRGB bucket |
| SDR Rec.2020 | SDR wide-gamut bucket |
| HDR10 | PQ bucket |
| HDR10+ | PQ bucket |
| HLG | HLG bucket |
| Dolby Vision, unspecified | PQ bucket |
| Dolby Vision profile 8.4 | HLG-adjacent bucket |
| Dolby Vision with SDR compatibility signaling | SDR-adjacent bucket |

## Caveats

1. Metadata in the wild is often incomplete or wrong.
2. The mastering gamut is often P3 even when the transport container is Rec.2020.
3. SDR does not have one single universal gamma. The most common practical split is:
   - video / TV SDR: BT.1886 or gamma 2.4
   - computer / web SDR: sRGB or gamma 2.2-like
4. Dolby Vision profile detail matters if you want perfect automation. Without profile detail, treat Dolby Vision as PQ by default.
5. Colorspace tags, matrix coefficients, and transfer characteristics are separate metadata fields. They should not be collapsed into one decision variable.

## Good Defaults For Later Discussion

If later work needs a small number of buckets rather than a full standards matrix, these are sensible starting points:

1. SDR video bucket: Rec.709 + BT.1886 / gamma 2.4
2. SDR desktop bucket: sRGB / gamma 2.2-like
3. HDR PQ bucket: HDR10, HDR10+, most Dolby Vision
4. HDR HLG bucket: HLG and Dolby Vision profile 8.4-like cases
5. Optional SDR wide-gamut bucket: Rec.2020 SDR if you find real material that benefits from separate handling

## Reference Standards And Background

Standards and references worth checking later:

1. ITU-R BT.709
2. ITU-R BT.1886
3. ITU-R BT.2020
4. ITU-R BT.2100
5. SMPTE ST 2084 (PQ)
6. ARIB STD-B67 (HLG)
7. IEC 61966-2-1 (sRGB)
8. Dolby Vision white papers and profile documentation