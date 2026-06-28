/*
* Copyright (C) 2010 - 2025 Eluna Lua Engine <https://elunaluaengine.github.io/>
* This program is free software licensed under GPL version 3
* Please see the included DOCS/LICENSE.md for more information
*/

#include "ALEManifest.h"
#include "ALEUtility.h"

#include <fkYAML/node.hpp>

#include <boost/filesystem.hpp>
#include <algorithm>
#include <cstdio>
#include <functional>

namespace fs = boost::filesystem;

bool ALEManifest::HasRootManifest(std::string const& scriptPath)
{
    fs::path manifestPath = fs::path(scriptPath) / "manifest.json";
    return fs::exists(manifestPath) && fs::is_regular_file(manifestPath);
}

std::vector<ModuleManifest> ALEManifest::LoadRootManifest(
    std::string const& scriptPath,
    std::string& requirePath,
    std::string& requireCPath)
{
    std::vector<ModuleManifest> modules;

    fs::path manifestFile = fs::path(scriptPath) / "manifest.json";

    FILE* file = std::fopen(manifestFile.string().c_str(), "r");
    if (!file)
    {
        ALE_LOG_ERROR("[ALE Manifest]: Failed to open root manifest `{}`", manifestFile.string());
        return modules;
    }

    fkyaml::node root;
    try
    {
        root = fkyaml::node::deserialize(file);
    }
    catch (const std::exception& e)
    {
        ALE_LOG_ERROR("[ALE Manifest]: Failed to parse root manifest: {}", e.what());
        std::fclose(file);
        return modules;
    }

    std::fclose(file);

    if (!root.contains("modules"))
    {
        ALE_LOG_ERROR("[ALE Manifest]: Root manifest is missing `modules` array");
        return modules;
    }

    fkyaml::node modulesNode = root["modules"];
    if (!modulesNode.is_sequence())
    {
        ALE_LOG_ERROR("[ALE Manifest]: `modules` must be an array in root manifest");
        return modules;
    }

    // Add root directory to require path
    requirePath +=
        scriptPath + "/?.lua;" +
        scriptPath + "/?.moon;" +
        scriptPath + "/?.out;";
    requireCPath +=
        scriptPath + "/?.dll;" +
        scriptPath + "/?.so;";

    for (auto const& moduleEntry : modulesNode.as_seq())
    {
        std::string moduleName = moduleEntry.get_value<std::string>();

        fs::path moduleDir = fs::path(scriptPath) / moduleName;

        if (!fs::exists(moduleDir) || !fs::is_directory(moduleDir))
        {
            ALE_LOG_ERROR("[ALE Manifest]: Module directory `{}` not found, skipping", moduleDir.string());
            continue;
        }

        std::string modulePath = moduleDir.generic_string();

        ALE_LOG_INFO("[ALE Manifest]: Loading module `{}`", moduleName);

        ModuleManifest mod;
        mod.name = moduleName;
        mod.fullPath = modulePath;
        mod.scripts = LoadModuleManifest(modulePath, moduleName, requirePath, requireCPath);

        modules.push_back(std::move(mod));
    }

    ALE_LOG_INFO("[ALE Manifest]: Loaded {} module(s) from root manifest", modules.size());
    return modules;
}

std::vector<LuaScript> ALEManifest::LoadModuleManifest(
    std::string const& modulePath,
    std::string const& moduleName,
    std::string& requirePath,
    std::string& requireCPath)
{
    std::vector<LuaScript> scripts;

    fs::path moduleManifestFile = fs::path(modulePath) / "manifest.json";

    // Add module directory to require path
    requirePath +=
        modulePath + "/?.lua;" +
        modulePath + "/?.moon;" +
        modulePath + "/?.out;";
    requireCPath +=
        modulePath + "/?.dll;" +
        modulePath + "/?.so;";

    if (!fs::exists(moduleManifestFile) || !fs::is_regular_file(moduleManifestFile))
    {
        // No module manifest — load all supported files from this directory recursively (legacy behavior within module)
        ALE_LOG_DEBUG("[ALE Manifest]: No manifest.json in module `{}`, scanning directory recursively", moduleName);

        // Recursively scan the module directory for supported files
        std::function<void(fs::path const&)> scanDir = [&](fs::path const& dir)
        {
            fs::directory_iterator endIter;
            for (fs::directory_iterator it(dir); it != endIter; ++it)
            {
                std::string fullpath = it->path().generic_string();

                // Skip hidden files/dirs
                std::string name = it->path().filename().generic_string();
                if (!name.empty() && name[0] == '.')
                    continue;

                if (fs::is_directory(it->status()))
                {
                    // Add subdirectory to require path
                    requirePath += fullpath + "/?.lua;" + fullpath + "/?.moon;" + fullpath + "/?.out;";
                    requireCPath += fullpath + "/?.dll;" + fullpath + "/?.so;";
                    scanDir(it->path());
                }
                else if (fs::is_regular_file(it->status()))
                {
                    std::string filename = it->path().filename().generic_string();
                    std::size_t extDot = filename.find_last_of('.');
                    if (extDot == std::string::npos)
                        continue;

                    std::string ext = filename.substr(extDot);
                    if (IsSupportedExtension(ext))
                    {
                        // Compute path relative to modulePath to preserve subdirectory information
                        fs::path relPath = fs::relative(it->path(), fs::path(modulePath));
                        scripts.push_back(BuildScriptEntry(relPath.generic_string(), modulePath));
                    }
                }
            }
        };

        scanDir(fs::path(modulePath));

        // Sort to match legacy alphabetical stability
        std::sort(scripts.begin(), scripts.end(),
            [](LuaScript const& a, LuaScript const& b) {
                return a.filepath < b.filepath;
            });

        return scripts;
    }

    // Parse the module manifest
    FILE* file = std::fopen(moduleManifestFile.string().c_str(), "r");
    if (!file)
    {
        ALE_LOG_ERROR("[ALE Manifest]: Failed to open module manifest `{}`", moduleManifestFile.string());
        return scripts;
    }

    fkyaml::node root;
    try
    {
        root = fkyaml::node::deserialize(file);
    }
    catch (const std::exception& e)
    {
        ALE_LOG_ERROR("[ALE Manifest]: Failed to parse module manifest `{}`: {}", moduleManifestFile.string(), e.what());
        std::fclose(file);
        return scripts;
    }

    std::fclose(file);

    if (!root.contains("files"))
    {
        ALE_LOG_ERROR("[ALE Manifest]: Module manifest `{}` is missing `files` array", moduleName);
        return scripts;
    }

    fkyaml::node filesNode = root["files"];
    if (!filesNode.is_sequence())
    {
        ALE_LOG_ERROR("[ALE Manifest]: `files` must be an array in module manifest `{}`", moduleName);
        return scripts;
    }

    for (auto const& fileEntry : filesNode.as_seq())
    {
        std::string relativePath = fileEntry.get_value<std::string>();

        // Validate extension
        std::size_t extDot = relativePath.find_last_of('.');
        if (extDot == std::string::npos)
        {
            ALE_LOG_ERROR("[ALE Manifest]: File `{}` in module `{}` has no extension, skipping", relativePath, moduleName);
            continue;
        }

        std::string ext = relativePath.substr(extDot);
        if (!IsSupportedExtension(ext))
        {
            ALE_LOG_ERROR("[ALE Manifest]: File `{}` in module `{}` has unsupported extension `{}`, skipping", relativePath, moduleName, ext);
            continue;
        }

        // Check that the file actually exists
        fs::path fullPath = fs::path(modulePath) / relativePath;
        if (!fs::exists(fullPath) || !fs::is_regular_file(fullPath))
        {
            ALE_LOG_ERROR("[ALE Manifest]: File `{}` not found in module `{}`, skipping", relativePath, moduleName);
            continue;
        }

        // Add subdirectory paths to require path if the file is in a subdirectory
        fs::path fileDir = fullPath.parent_path();
        if (fileDir != fs::path(modulePath))
        {
            std::string dirPath = fileDir.generic_string();
            requirePath += dirPath + "/?.lua;" + dirPath + "/?.moon;" + dirPath + "/?.out;";
            requireCPath += dirPath + "/?.dll;" + dirPath + "/?.so;";
        }

        scripts.push_back(BuildScriptEntry(relativePath, modulePath));
    }

    ALE_LOG_DEBUG("[ALE Manifest]: Loaded {} file(s) from module `{}`", scripts.size(), moduleName);
    return scripts;
}

LuaScript ALEManifest::BuildScriptEntry(
    std::string const& relativePath,
    std::string const& modulePath)
{
    LuaScript script;

    // Extract extension from basename
    fs::path filePath(relativePath);
    std::string filename = filePath.filename().generic_string();

    std::size_t extDot = filename.find_last_of('.');
    if (extDot != std::string::npos)
    {
        script.fileext = filename.substr(extDot);
    }
    else
    {
        script.fileext = "";
    }

    // Use relative path (without extension) as script name to avoid collisions
    // when multiple files share the same basename in different subdirectories.
    // e.g., "config/main" and "logging/main" instead of "main" and "main"
    std::string relWithoutExt = relativePath;
    std::size_t relExtDot = relWithoutExt.find_last_of('.');
    if (relExtDot != std::string::npos)
    {
        script.filename = relWithoutExt.substr(0, relExtDot);
    }
    else
    {
        script.filename = relWithoutExt;
    }

    // Full absolute path
    fs::path fullPath = fs::path(modulePath) / relativePath;
    script.filepath = fullPath.generic_string();

    // Module path (directory containing the file)
    script.modulepath = fullPath.parent_path().generic_string();

    return script;
}

bool ALEManifest::IsSupportedExtension(std::string const& ext)
{
    // In manifest mode, .ext files are NOT supported
    // .dll and .so are native libraries that should only be discovered
    // through lua_requirecpath, not loaded as startup scripts via RunScripts().
    return ext == ".lua" || ext == ".moon" || ext == ".out";
}
