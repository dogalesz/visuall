/* ============================================================================
 * vsl_manifest.cpp — Parsing and serialisation for vsl.toml / vsl.lock.
 *
 * Uses toml11 v4 (header-only). toml11 v4 uses toml::parse() and typed
 * accessors via toml::find / toml::find_or.
 * ============================================================================ */

#include "vsl_manifest.h"
#include "diagnostic.h"

#include <toml.hpp>

#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>

namespace visuall {

// ── Internal helpers ──────────────────────────────────────────────────────

static std::string requireString(const toml::value& tbl,
                                  const std::string& key,
                                  const std::string& context) {
    if (!tbl.contains(key)) {
        throw PackageError("Missing required key '" + key + "' in " + context);
    }
    const auto& v = tbl.at(key);
    if (!v.is_string()) {
        throw PackageError("Key '" + key + "' in " + context + " must be a string");
    }
    return v.as_string();
}

static std::vector<std::string> optStringArray(const toml::value& tbl,
                                                const std::string& key) {
    std::vector<std::string> result;
    if (!tbl.contains(key)) return result;
    const auto& v = tbl.at(key);
    if (!v.is_array()) return result;
    for (const auto& elem : v.as_array()) {
        if (elem.is_string()) {
            result.push_back(elem.as_string());
        }
    }
    return result;
}

// ── parseManifest ─────────────────────────────────────────────────────────

VslManifest parseManifest(const std::string& tomlPath) {
    toml::value root;
    try {
        root = toml::parse(tomlPath);
    } catch (const toml::exception& e) {
        throw PackageError("Failed to parse " + tomlPath + ": " + e.what());
    }

    // [package] section
    if (!root.contains("package")) {
        throw PackageError(tomlPath + ": missing [package] section");
    }
    const auto& pkgTbl = root.at("package");

    ManifestPackage pkg;
    pkg.name        = requireString(pkgTbl, "name",    "[package] in " + tomlPath);
    pkg.version     = requireString(pkgTbl, "version", "[package] in " + tomlPath);

    // Validate name: lowercase letters, digits, hyphens; must start with a letter.
    static const std::regex nameRe("^[a-z][a-z0-9-]*$");
    if (!std::regex_match(pkg.name, nameRe)) {
        throw PackageError(tomlPath + ": [package].name '" + pkg.name +
                           "' is invalid (must match [a-z][a-z0-9-]*)");
    }
    // Validate version: MAJOR.MINOR.PATCH (semver without pre-release).
    static const std::regex verRe(R"(^\d+\.\d+\.\d+$)");
    if (!std::regex_match(pkg.version, verRe)) {
        throw PackageError(tomlPath + ": [package].version '" + pkg.version +
                           "' is invalid (must be MAJOR.MINOR.PATCH, e.g. 1.0.0)");
    }

    pkg.description = toml::find_or(pkgTbl, "description", std::string{});
    pkg.license     = toml::find_or(pkgTbl, "license",     std::string{});
    pkg.entry       = toml::find_or(pkgTbl, "entry",       std::string{});
    if (pkg.entry.empty()) {
        pkg.entry = pkg.name + ".vsl";
    }
    pkg.authors  = optStringArray(pkgTbl, "authors");
    pkg.keywords = optStringArray(pkgTbl, "keywords");
    if (pkg.keywords.size() > 5) {
        pkg.keywords.resize(5);
    }

    // [dependencies] section (optional)
    VslManifest manifest;
    manifest.package = std::move(pkg);

    if (root.contains("dependencies")) {
        const auto& depTbl = root.at("dependencies");
        if (!depTbl.is_table()) {
            throw PackageError(tomlPath + ": [dependencies] must be a table");
        }
        for (const auto& [alias, depVal] : depTbl.as_table()) {
            if (!depVal.is_table()) {
                throw PackageError(tomlPath + ": dependency '" + alias +
                                   "' must be an inline table");
            }
            const auto& dep = depVal;
            if (dep.contains("git")) {
                GitDep gd;
                gd.url     = requireString(dep, "git",     "dependency '" + alias + "'");
                gd.version = requireString(dep, "version", "dependency '" + alias + "'");
                manifest.dependencies.emplace(alias, std::move(gd));
            } else if (dep.contains("path")) {
                PathDep pd;
                pd.path = requireString(dep, "path", "dependency '" + alias + "'");
                manifest.dependencies.emplace(alias, std::move(pd));
            } else {
                throw PackageError(tomlPath + ": dependency '" + alias +
                                   "' must have either 'git' or 'path' key");
            }
        }
    }

    return manifest;
}

// ── parseLock ─────────────────────────────────────────────────────────────

VslLock parseLock(const std::string& lockPath) {
    VslLock lock;

    // A missing lockfile is not an error here — callers decide what to do.
    std::ifstream f(lockPath);
    if (!f.good()) {
        return lock;
    }

    toml::value root;
    try {
        root = toml::parse(lockPath);
    } catch (const toml::exception& e) {
        throw PackageError("Failed to parse " + lockPath + ": " + e.what());
    }

    if (!root.contains("package")) {
        return lock;  // empty lockfile is fine
    }
    const auto& arr = root.at("package");
    if (!arr.is_array()) {
        throw PackageError(lockPath + ": 'package' must be an array of tables");
    }
    for (const auto& entry : arr.as_array()) {
        LockEntry le;
        le.alias   = requireString(entry, "alias", "lock entry in " + lockPath);
        le.url     = toml::find_or(entry, "url",     std::string{});
        le.version = toml::find_or(entry, "version", std::string{});
        le.dir     = requireString(entry, "dir",  "lock entry in " + lockPath);
        lock.packages.push_back(std::move(le));
    }

    return lock;
}

// ── writeLock ─────────────────────────────────────────────────────────────

void writeLock(const VslLock& lock, const std::string& lockPath) {
    std::ofstream f(lockPath, std::ios::out | std::ios::trunc);
    if (!f) {
        throw PackageError("Cannot write lock file: " + lockPath);
    }

    f << "# vsl.lock - generated by vslpkg. Do not edit by hand.\n\n";

    for (const auto& entry : lock.packages) {
        f << "[[package]]\n";
        f << "alias   = \"" << entry.alias << "\"\n";
        if (!entry.url.empty()) {
            f << "url     = \"" << entry.url     << "\"\n";
            f << "version = \"" << entry.version << "\"\n";
        }
        f << "dir     = \"" << entry.dir << "\"\n";
        f << "\n";
    }

    if (!f) {
        throw PackageError("I/O error while writing lock file: " + lockPath);
    }
}

} // namespace visuall
