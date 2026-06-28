<div align="center">

# 📦 ALE Manifest System

*Manifest-based script loading inspired by FiveM's resource system*

[![Discord](https://img.shields.io/badge/Discord-Join%20Us-7289DA?style=for-the-badge&logo=discord&logoColor=white)](https://discord.com/invite/ZKSVREE7)
[![AzerothCore](https://img.shields.io/badge/AzerothCore-Integrated-darkgreen?style=for-the-badge)](http://www.azerothcore.org/)

---
</div>

> [!IMPORTANT]
> The manifest system is **auto-detected**. If a `manifest.json` file exists in your script folder, ALE automatically switches to manifest mode. No config option needed.

## 📋 Table of Contents

- [Overview](#-overview)
- [How It Works](#-how-it-works)
- [Root Manifest](#-root-manifest)
- [Module Manifest](#-module-manifest)
- [Load Order](#-load-order)
- [Retrocompatibility](#-retrocompatibility)
- [Differences from Legacy Mode](#-differences-from-legacy-mode)
- [Examples](#-examples)

## 🚀 Overview

ALE supports two script loading modes:

| Mode | Trigger | Behavior |
|------|---------|----------|
| **Manifest mode** | `manifest.json` exists in script folder | Loads only modules and files specified in manifests, in the defined order |
| **Legacy mode** | No `manifest.json` found | Recursively scans all folders, loads all `.lua`/`.ext`/`.moon` files, sorted alphabetically |

The manifest system is inspired by [FiveM's `fxmanifest.lua`](https://docs.fivem.net/docs/scripting-reference/resource-manifest/) resource system. It gives you explicit control over:

- **Which modules** to load (not everything in the folder)
- **In what order** modules are loaded
- **Which files** within each module to load
- **In what order** files are loaded

This solves a common problem: in legacy mode, file loading order is determined by alphabetical sorting of file paths, which means a file in `handlers/` loads before `init.lua` (because `h` < `i`). With manifests, you control the order explicitly.

## ⚙️ How It Works

```
lua_scripts/
├── manifest.json          ← Root manifest (lists modules to load, in order)
├── my_module/
│   ├── manifest.json      ← Module manifest (lists files to load, in order)
│   ├── init.lua
│   ├── utils/
│   │   └── helpers.lua
│   └── handlers/
│       ├── player.lua
│       └── creature.lua
└── another_module/
    ├── manifest.json
    └── main.lua
```

1. ALE checks if `manifest.json` exists in the script folder (configured via `ALE.ScriptPath`, default: `lua_scripts`)
2. If found, manifest mode is activated
3. The root manifest is parsed to get the ordered list of modules
4. For each module, its `manifest.json` is parsed to get the ordered list of files
5. Files are loaded in the exact order specified

## 📄 Root Manifest

The root manifest (`lua_scripts/manifest.json`) lists the modules to load, in order.

```json
{
    "modules": [
        "my_module",
        "another_module",
        "subfolder/third_module"
    ]
}
```

### Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `modules` | `string[]` | Yes | Ordered list of module directory names (relative to the script folder) |

### Module Names

- Module names are **relative paths** from the script folder
- Nested paths are supported: `"utils/library"` refers to `lua_scripts/utils/library/`
- Order matters: modules are loaded top-to-bottom

## 📦 Module Manifest

Each module can have its own `manifest.json` listing the files to load, in order.

```json
{
    "files": [
        "init.lua",
        "utils/helpers.lua",
        "handlers/player.lua",
        "handlers/creature.lua"
    ]
}
```

### Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `files` | `string[]` | Yes | Ordered list of file paths (relative to the module directory) |

### File Paths

- File paths are **relative to the module directory**
- Nested paths are supported: `"handlers/player.lua"` refers to `my_module/handlers/player.lua`
- Only the files listed are loaded - other files in the module directory are ignored
- Order matters: files are loaded top-to-bottom

### No Module Manifest (Fallback)

If a module directory does **not** contain a `manifest.json`, ALE falls back to recursive scanning within that module. This means:

- All supported files (`.lua`, `.moon`, `.out`) in the module and its subdirectories are loaded
- Files are NOT sorted (they are loaded in directory iteration order)
- This is useful for simple modules with a single file or when order doesn't matter

> [!TIP]
> For modules with dependencies between files (e.g., `init.lua` must load before handlers), always use a module manifest to guarantee load order.

## 🔢 Load Order

The load order is fully deterministic and controlled by the manifests:

```
Root manifest modules[0] → module manifest files[0], files[1], ...
Root manifest modules[1] → module manifest files[0], files[1], ...
...
```

### Example

With this root manifest:
```json
{ "modules": ["combat", "quests"] }
```

And `combat/manifest.json`:
```json
{ "files": ["init.lua", "handlers/player.lua"] }
```

The load order is:
1. `combat/init.lua`
2. `combat/handlers/player.lua`
3. `quests/...` (first file from quests module)

### Why Load Order Matters

In legacy mode, files are sorted alphabetically by full path. This means:

| Legacy order | Manifest order |
|---|---|
| `combat/handlers/player.lua` (h) | `combat/init.lua` (1st) |
| `combat/init.lua` (i) | `combat/handlers/player.lua` (2nd) |

With legacy mode, `handlers/player.lua` loads **before** `init.lua`, which breaks if `player.lua` depends on variables set up by `init.lua`. The manifest system solves this.

## 🔄 Retrocompatibility

The manifest system is fully retrocompatible:

- **No `manifest.json`** → Legacy mode is used (recursive scan, alphabetical sort, `.ext` files loaded)
- **`manifest.json` present** → Manifest mode is used automatically
- **Module without `manifest.json`** → That module falls back to recursive scanning

You can migrate gradually:
1. Start with legacy mode (no manifest)
2. Create a root `manifest.json` listing your existing folders as modules
3. Add module manifests one by one to control file order within each module

## ⚠️ Differences from Legacy Mode

| Feature | Legacy Mode | Manifest Mode |
|---------|-------------|---------------|
| File discovery | Recursive scan of all folders | Only files listed in manifests |
| Load order | Alphabetical by file path | Order defined in manifests |
| `.ext` files | Loaded (before `.lua` files) | **NOT loaded** |
| `.lua` files | Loaded | Loaded |
| `.moon` files | Loaded | Loaded |
| `.out` files | Loaded | Loaded |
| `.dll`/`.so` files | Loaded | Loaded |
| Hidden files/dirs | Skipped | Skipped |
| `require()` paths | All subdirectories added | Only module directories and file subdirectories added |
| Auto-reload | Watches `.lua`/`.ext`/`.moon` | Watches `.lua`/`.ext`/`.moon`/`.json` |

> [!WARNING]
> If you are using `.ext` extension files and want to switch to manifest mode, convert them to regular `.lua` files and list them in your module manifest instead.

## 📝 Examples

### Single Module

**`lua_scripts/manifest.json`:**
```json
{
    "modules": ["my_scripts"]
}
```

**`lua_scripts/my_scripts/manifest.json`:**
```json
{
    "files": [
        "config.lua",
        "main.lua"
    ]
}
```

### Multiple Modules with Dependencies

**`lua_scripts/manifest.json`:**
```json
{
    "modules": [
        "core_library",
        "combat_system",
        "quest_system",
        "economy"
    ]
}
```

This ensures `core_library` loads first, then `combat_system`, etc.

### Module with Nested Directories

**`lua_scripts/combat_system/manifest.json`:**
```json
{
    "files": [
        "init.lua",
        "utils/damage_calculator.lua",
        "handlers/player_handler.lua",
        "handlers/creature_handler.lua"
    ]
}
```

### Module Without Manifest (Fallback Scanning)

If `lua_scripts/simple_scripts/` has no `manifest.json`, all supported files in that directory and its subdirectories will be loaded automatically.

### Gradual Migration

1. **Before** (legacy mode - everything loads automatically):
```
lua_scripts/
├── lib/
│   └── utils.lua
├── combat/
│   ├── init.lua
│   └── handlers.lua
└── quests/
    └── main.lua
```

2. **Step 1** - Add root manifest (modules load in order, but files within each module still scan recursively):
```json
{
    "modules": ["lib", "combat", "quests"]
}
```

3. **Step 2** - Add module manifest to `combat/` to control file order:
```json
{
    "files": ["init.lua", "handlers.lua"]
}
```

---

<div align="center">
<sub>Developed with ❤️ by the AzerothCore and ALE community</sub>

[⬆ Back to Top](#-ale-manifest-system)
</div>
