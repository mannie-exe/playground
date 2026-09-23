# Project settings, user preferences, and files

The public API is typed C++; TOML is a storage format, not a dependency exposed
to apps/UI nodes. Declarations are in `include/platform/{Settings,FileStore}.hpp`.
See [WINDOWING.md](WINDOWING.md) for the presentation values they configure.

## Files and precedence

| Source | Location / role |
|---|---|
| AppInfo | Compiled app baseline and resize policy |
| `project.toml` | Read-only defaults beside the executable; installed from `config/project.toml` |
| `settings.toml` | User-authored overrides in SDL_GetPrefPath("Playground", "Playground") |
| `session.toml` | Machine-written normal bounds/display-name hint per app in that same preference directory |

Resolve app baseline -> project defaults -> project app overrides -> user defaults
-> user app overrides. Session state does not override settings; it is consulted
only for RestorePrevious sizing. Keys are stable strings: `menu`, `demo`,
`minesweeper`, `rock-paper-scissors`, `snake`. Unknown app keys can be retained for
future apps; this does not register/implement those apps.

No current-working-directory dependence, hard-coded home directory, registry key,
or PATH change is required. Install prepares runtime files. Running directly from
the build tree need not find `project.toml`; absent files retain compiled defaults.
The project root is not treated as a writable preferences directory.

All documents require integer `schema_version = 1`. Missing files mean no overrides;
malformed/unreadable files are errors, not silently replaced defaults. Unknown
sections, fields and enum strings are rejected to expose spelling/schema errors.
Future schema migrations need explicit readers; there is no automatic version repair.

## Settings schema

Both project and user files use this shape; every field inside the tables is optional:

```toml
schema_version = 1

[defaults]
mode = "windowed"
display = "current"
decorated = true
center = true
initial_sizing = "preferred"
viewport_mode = "reflow"
ui_scale = 1.0
follow_system_scale = true
resolution_scale = 1.0

[apps.demo]
initial_sizing = "fit-content"

[apps.minesweeper]
display = "primary"
```

| Field | Accepted values / units |
|---|---|
| `mode` | windowed, maximized, desktop-fullscreen, exclusive-fullscreen, borderless-display, borderless-work-area |
| `display` | primary, current, named |
| `display_name` | String hint; required after merging when selection is named |
| `decorated`, `center` | Booleans; full/borderless modes may override decorations |
| `exclusive_size` | Two positive integers: requested display-mode pixel dimensions |
| `refresh_rate` | Finite nonnegative number; zero chooses no explicit Hz preference |
| `initial_sizing` | preferred, fit-content, restore-previous |
| `viewport_mode` | reflow, fixed-canvas |
| `viewport_fit` | contain, cover, stretch |
| `canvas_size` | Two finite positive logical extents, width then height |
| `alignment` | Two finite factors in [0,1], horizontal then vertical |
| `ui_scale` | Finite positive Reflow zoom |
| `follow_system_scale` | Boolean |
| `resolution_scale` | Finite value in (0,4], whole-frame raster multiplier |

The shipped project file specifies shared presentation defaults, not a duplicate
Demo sizing policy. App configs declare FitContent; an explicit per-app user/project
override can still change it.

Unused mode-specific fields remain valid for a later mode change. Numeric values
are range-validated even when currently unused. Resizability, preferred window size, minimum size, app
title, asset paths and game rules are deliberately not part of this schema.
This is extensible presentation infrastructure, not an untyped global configuration bag.

`SettingsPatch` uses optional fields: absent means inherit, false/zero remain values.
Remove a persisted override to reset it to the next layer; this is not UI's
Keep/Set/Reset patch protocol. `SettingsDocument::apply` merges values only;
`SettingsStore::resolve` validates the completed result before publishing outputs.
`resolveWithUser` previews replacement user settings against the original baseline
and project values, not against yesterday's already-merged state.

## Publication and persistence

SettingsStore borrows two FileStores; both must outlive it. Its API is synchronous,
owning-thread only. `reload()` reads/parses all three documents into candidates
before replacing the in-memory documents. Cross-layer conditions, such as a named
display needing a name, are checked during resolution. A successfully parsed
document is not proof that every possible app-baseline combination is valid.

`setUser(document, persist)` validates serialized values and, if requested, writes
before publishing the replacement user document. AppHost previews the active app's
merged settings first. Disk failures preserve the previous published document.
An explicit save rewrites TOML canonically: comments/formatting are not retained.
Runtime-only changes do not touch `settings.toml`. App switches/settings reloads
resolve values again; explicit runtime presentation requests do not edit preferences.

Normal window geometry is saved per app on app switch and orderly run-loop exit.
It uses signed desktop coordinates (negative positions are legitimate) and a
display-name hint, never native display IDs. Session write failure is logged and
does not block switching; parse/configuration failures propagate. Abrupt process
termination can lose recent unsaved geometry. OS maximize/fullscreen gestures do
not automatically rewrite the authored mode preference.

```toml
schema_version = 1
[apps.demo]
size = [798, 978]
position = [-1200, 40]
display_name = "External display"
```

## Filesystem boundary

`FileStore::read(name)` returns optional UTF-8 document text: nullopt means absent;
exceptions mean access/read/validation failure. `replace(name, text)` replaces one
complete document. Test implementations can provide an in-memory store.

`DirectoryStore(root, writable)` resolves only leaf names under one absolute root;
it rejects separators, parent traversal, Windows alternate-stream syntax and NUL.
It is not a security sandbox against symlinks, hostile filesystem races, or an
untrusted process. It limits documents to 1 MiB and distinguishes read-only project
stores from writable user stores.

Writes create a uniquely named, exclusively opened temporary file in the same
directory, check writing/closing, then use SDL_RenamePath to replace the destination.
Temporary files are cleaned on ordinary exception paths. This avoids exposing a
partially written final document, but promises neither fsync/power-loss durability
nor interprocess locking/conflict resolution. Concurrent writers are last-writer-wins.
No directory enumeration, arbitrary deletion, filesystem watching, asynchronous IO,
or general asset-management API is implied by this small document store.

`preferenceDirectory` and `executableDirectory` centralize native path discovery.
Public paths use std::filesystem::path; conversion at SDL boundaries is UTF-8.
AppHost constructs these after SDL initialization and owns stores longer than the
settings service. UI callbacks request changes through AppContext; they do not write
files during paint/layout or retain a temporary AppContext.

## Dependencies and extension boundaries

toml++ 3.4.0 parses/formats TOML privately in Settings.cpp; CPM builds it without
examples/tests and install includes its license. The public headers expose no TOML
nodes. JSONC support would be a different serializer, not a second runtime authority.
Build-time CMake options remain separate from these runtime project settings.

Before adding new persisted settings, specify units, layer precedence, validation,
whether they apply immediately/on entry/only after window recreation, and recovery
behavior. Keep transient observed state out of user preferences. Disk/parse errors
are std::exception-compatible and the current app entry point reports them; no
interactive preferences/error UI or background file watcher is implemented.

References: [SDL preference paths](https://wiki.libsdl.org/SDL3/SDL_GetPrefPath),
[SDL executable base path](https://wiki.libsdl.org/SDL3/SDL_GetBasePath),
[replacement rename](https://wiki.libsdl.org/SDL3/SDL_RenamePath),
[toml++ release](https://github.com/marzer/tomlplusplus/releases/tag/v3.4.0).
