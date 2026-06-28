/*
* Copyright (C) 2010 - 2025 Eluna Lua Engine <https://elunaluaengine.github.io/>
* This program is free software licensed under GPL version 3
* Please see the included DOCS/LICENSE.md for more information
*/

#ifndef ALE_MANIFEST_H
#define ALE_MANIFEST_H

#include "LuaEngine.h"
#include <string>
#include <vector>

/*
 * Manifest-based script loading system, inspired by FiveM's fxmanifest.lua.
 *
 * Root manifest (lua_scripts/manifest.json):
 * {
 *     "modules": [
 *         "my_module",
 *         "another_module/subdir"
 *     ]
 * }
 *
 * Module manifest (lua_scripts/my_module/manifest.json):
 * {
 *     "files": [
 *         "init.lua",
 *         "handlers/player_handler.lua",
 *         "handlers/creature_handler.lua"
 *     ]
 * }
 *
 * When manifest mode is enabled:
 *   - Only modules listed in the root manifest are loaded, in the specified order.
 *   - Only files listed in each module's manifest are loaded, in the specified order.
 *   - .ext extension files are NOT loaded (unlike legacy mode).
 *   - If no root manifest exists, falls back to legacy recursive scanning.
 */

struct ModuleManifest
{
    std::string name;           // Module directory name (relative to lua_scripts/)
    std::string fullPath;       // Absolute path to the module directory
    std::vector<LuaScript> scripts; // Ordered list of scripts to load
};

class ALEManifest
{
public:
    // Returns true if a root manifest exists at the given script path.
    static bool HasRootManifest(std::string const& scriptPath);

    // Loads the root manifest and all module manifests.
    // Returns a vector of ModuleManifest in the order specified by the root manifest.
    // Also populates requirePath/requireCPath for Lua's require() function.
    static std::vector<ModuleManifest> LoadRootManifest(
        std::string const& scriptPath,
        std::string& requirePath,
        std::string& requireCPath);

private:
    // Parses a single module's manifest.json and returns the ordered list of scripts.
    static std::vector<LuaScript> LoadModuleManifest(
        std::string const& modulePath,
        std::string const& moduleName,
        std::string& requirePath,
        std::string& requireCPath);

    // Builds a LuaScript entry from a file path relative to the module directory.
    static LuaScript BuildScriptEntry(
        std::string const& relativePath,
        std::string const& modulePath);

    // Checks if a file extension is supported in manifest mode.
    // In manifest mode, .ext files are excluded.
    static bool IsSupportedExtension(std::string const& ext);
};

#endif // ALE_MANIFEST_H
