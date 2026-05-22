# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Hoard & Seek** is a Guild Wars 2 addon (Windows DLL) for the [Raidcore Nexus](https://raidcore.gg/) addon platform. It provides account-wide item searching across up to 16 GW2 accounts and exposes a cross-addon event API so other addons can query item counts, wallet data, achievements, and more without their own GW2 API integration.

The project is cross-compiled from Linux to Windows (x86-64 MinGW), producing `HoardAndSeek.dll`.

## Build

```bash
# One-time setup: download ImGui and nlohmann/json into lib/
chmod +x scripts/setup.sh && ./scripts/setup.sh

# Build
mkdir build && cd build
cmake ..
make
```

Output: `build/HoardAndSeek.dll`

**Always rebuild the DLL after making code changes.** Run `cd build && make` automatically — do not leave it to the user.

**Never bump the addon version** unless the user explicitly asks. Plans and implementations should leave the version constant alone; the user decides when to bump.

There is no test suite. Verification is done by loading the DLL into GW2 via Nexus.

## Architecture

The addon has four source files with clear separation of concerns:

### [src/dllmain.cpp](src/dllmain.cpp) — Entry Point & UI
- `DllMain` and Nexus `AddonLoad`/`AddonUnload` lifecycle
- All ImGui rendering: search window, settings panel, permissions panel, quick-access icon
- Nexus keybind and event registration
- Bridges the cross-addon API events to `GW2API` calls
- Owns search result state and triggers re-searches on data updates

### [src/GW2API.h](src/GW2API.h) / [src/GW2API.cpp](src/GW2API.cpp) — Data & Networking
- `AccountManager` singleton manages up to 16 accounts (add/remove/validate/fetch)
- Fetches item data from ~15 GW2 API v2 endpoints per account, stores results to disk (`accounts.json`, `account_data.json`)
- `Search()` does substring matching across all cached item data and returns `SearchResult` with per-location breakdowns
- Proxy query support: other addons can pass arbitrary authenticated GW2 API requests through H&S

### [src/IconManager.h](src/IconManager.h) / [src/IconManager.cpp](src/IconManager.cpp) — Async Icons
- Background worker thread downloads icon PNGs from GW2 CDN URLs
- Rate-limited (100ms between requests), with retry backoff on failure
- Two-level cache: disk (PNG files) → Nexus GPU texture

### [src/PermissionManager.h](src/PermissionManager.h) / [src/PermissionManager.cpp](src/PermissionManager.cpp) — Consent System
- Before fulfilling cross-addon API requests, checks if the requesting addon has permission
- First request shows a popup; user grants/denies per event type; saved to `permissions.json`
- This is a UX transparency mechanism, not a security boundary

### [include/HoardAndSeekAPI.h](include/HoardAndSeekAPI.h) — Cross-Addon Public API
- Header other addons include to query H&S without their own GW2 API integration
- Event-based (Nexus `Events_Raise` / `Events_Subscribe`); no linking required
- Synchronous dispatch: responses are delivered (and must be `delete`d) before `Events_Raise` returns
- Current API version: 3

## Key Dependencies

All vendored under `lib/` (downloaded by `setup.sh`):
- **Dear ImGui v1.80** — immediate-mode UI
- **nlohmann/json v3.11.3** — JSON serialization
- **Nexus API** (`include/nexus/Nexus.h`) — addon framework: events, keybinds, textures, quick-access

Linked Windows system libs: `gdi32`, `wininet`.

## Important Patterns

**Threading:** Account fetches and icon downloads run on background threads. UI state accessed from both threads must be protected by mutexes. `dllmain.cpp` uses atomic flags (`pending_results`, `results_ready`) to hand off search results without blocking the render thread.

**Nexus events as IPC:** The cross-addon API works entirely through named Nexus events. Structs in `HoardAndSeekAPI.h` define request/response shapes. Responses are heap-allocated by H&S and ownership transfers to the caller.

**No hot-reload:** There is no live reload. Changes require recompiling the DLL and restarting GW2 (or reloading addons via Nexus).

**Settings persistence:** User settings, account list, and permissions are stored as JSON in the GW2 appdata directory (path resolved via Nexus `PATHS_GetAddonDir`).
