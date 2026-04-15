# RawHID SolverAware V3 Changelog

Date: 2026-03-29

Purpose:
- Add HyperHDR-side Host solver-aware v3 mode.
- Remove the fixed 4096 host solver lookup assumption.
- Move host scene analysis off the old black-frame approximation and onto the solver's emitted-output model.
- Update the LED Hardware page text to reflect arbitrary transfer/calibration header sizes.
- Add file-backed solver profile storage, selection, and upload support for RawHID host solver analysis.

Files touched:
- include/api/HyperAPI.h
- include/led-drivers/other/DriverOtherRawHid.h
- sources/api/HyperAPI.cpp
- sources/api/JSONRPC_schema/schema-solver-profiles.json
- sources/led-drivers/other/DriverOtherRawHid.cpp
- sources/led-drivers/schemas/schema-rawhid.json
- www/js/hyperhdr.js
- www/js/light_source.js
- www/content/light_source.html

Summary by file:

include/api/HyperAPI.h
- Added `handleSolverProfilesCommand(...)` declaration.

include/led-drivers/other/DriverOtherRawHid.h
- Added `ScenePolicyMode::SolverAwareV3`.
- Added instance-owned solver profile and lookup-table structures so RawHID can use either the built-in ladders or a stored custom solver profile.

sources/api/HyperAPI.cpp
- Added `solver-profiles` JSON-RPC support with `list`, `save`, `delete`, `get-active`, and `set-active` subcommands.
- Added `serverinfo.grabbers.solver_profiles*` fields so the web UI can discover stored solver profiles.
- Stores custom solver profiles under `.hyperhdr/solver_profiles/`, with the uploaded temporal runtime solver header preserved as the source of truth when provided and a normalized JSON profile emitted alongside it.

sources/api/JSONRPC_schema/schema-solver-profiles.json
- Added request validation for the new `solver-profiles` API command.

sources/led-drivers/other/DriverOtherRawHid.cpp
- Replaced the fixed 4096-entry host solver lookup tables with per-channel lookup tables derived from the actual ladder counts.
- Added parser and logging support for `solverAwareV3`.
- Kept solverAwareV2 on the legacy black-frame-weighted energy model.
- Added solverAwareV3 on the direct solver emitted-output energy model so highlight detection follows the actual ladder-derived output rather than the old fixed approximation.
- Follow-up behavior fix: solver-aware mode no longer rewrites emitted pixel values through the solver ladder. It now uses solver-derived energy only for highlight analysis, and the highlight threshold acts purely as a cap on emitted Q16 values when a pixel is not highlighted.
- Added file-backed solver profile loading that prefers `.hyperhdr/solver_profiles/<slug>.h` temporal runtime solver headers, falls back to `.json`, and still supports split per-channel ladder JSON directories for manual tooling workflows.

sources/led-drivers/schemas/schema-rawhid.json
- Added `solverAwareV3` to the RawHID host policy enum and updated the description text.
- Added `solverProfile` so RawHID can select the built-in or a stored custom solver profile.

www/js/hyperhdr.js
- Added `requestSolverProfilesList`, `requestSolverProfileSave`, and `requestSolverProfileDelete` helpers.

www/js/light_source.js
- Kept the highlight threshold control visible for both solverAwareV2 and solverAwareV3.
- Added solver profile discovery, dropdown population, and upload/delete UI handling.

www/content/light_source.html
- Updated transfer-header help text to describe arbitrary positive `BUCKET_COUNT` support.
- Updated calibration-header help text to describe arbitrary consistent LUT sizes.
- Clarified that extra generated transfer arrays are ignored during storage.
- Added a solver profile upload and management section, with the temporal runtime solver header documented as the preferred custom profile input.

Copy notes for your separate repository:
- Copy the files listed above as a set.
- The driver, API/schema, and web UI changes are coupled; moving only one side will leave the option surface inconsistent.
- No transport frame-format changes were made in this pass.