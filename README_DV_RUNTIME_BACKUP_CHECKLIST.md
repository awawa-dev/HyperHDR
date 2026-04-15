# Dolby Vision Runtime Switching Backup Checklist

Use this checklist before making any runtime Dolby Vision switching changes.

## Goal

Preserve a known-good rollback point for the real Pi-hosted HyperHDR fork and the active VRROOM sync script before any prototype work touches runtime behavior.

## Files To Back Up First

Create timestamped backups for these files before making changes:

1. `\\hyperhdr.local\pi\vrroom_hyperhdr_sync.py`
2. `\\hyperhdr.local\pi\hyperhdr-master\hyperhdr\sources\api\HyperAPI.cpp`
3. `\\hyperhdr.local\pi\hyperhdr-master\hyperhdr\sources\api\JSONRPC_schema\schema-lut-switching.json`
4. `\\hyperhdr.local\pi\hyperhdr-master\hyperhdr\README_LUT_SWITCHING_NOTES.md`

Optional but recommended if touched during later productization:

1. `\\hyperhdr.local\pi\hyperhdr-master\hyperhdr\sources\api\JSONRPC_schema\schema-current-state.json`
2. any HyperHDR web UI or settings files changed to expose threshold arrays

## Backup Naming

Use this naming pattern:

1. `.bak-YYYYMMDD-HHMM-description`

Examples:

1. `vrroom_hyperhdr_sync.py.bak-20260403-0945-pre-kodi-dv-poller`
2. `HyperAPI.cpp.bak-20260403-0945-pre-dv-thresholds`

## Settings Snapshot

In addition to file backups, capture the current live HyperHDR LUT-switching settings response before any runtime changes.

Save at minimum:

1. current `lut-switching get` response JSON
2. current active LUT directory contents
3. current per-input file naming layout under the LUT directory

This gives a settings-level rollback point even if a file edit is later reverted incorrectly.

## Suggested Order

1. Back up `vrroom_hyperhdr_sync.py`
2. Back up `HyperAPI.cpp`
3. Back up `schema-lut-switching.json`
4. Back up `README_LUT_SWITCHING_NOTES.md`
5. Capture current `lut-switching get` response
6. Capture current LUT directory listing
7. Only then begin prototype edits

## First Prototype Scope

For the first prototype, keep all Dolby Vision threshold logic out of HyperHDR core.

That means the only runtime file that should need code changes in the first pass is:

1. `\\hyperhdr.local\pi\vrroom_hyperhdr_sync.py`

The HyperHDR C++ and schema files should remain unchanged until the local Kodi JSON-RPC testing produces useful and stable threshold results.

## Stop Conditions

Do not continue with runtime changes if any of these are true:

1. backup copies were not created successfully
2. current `lut-switching get` response was not captured
3. local Kodi polling results do not show stable, actionable FLL-based bucket behavior
4. the proposed bucket count would require an impractical number of retained per-input LUT files