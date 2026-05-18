#pragma once
/* ============================================================================
 * vsl_manifest.h — Data structures and parsing for vsl.toml / vsl.lock.
 *
 * vsl.toml (package manifest) schema (v1):
 *
 *   [package]
 *   name        = "my-pkg"
 *   version     = "1.2.3"        # semver, required
 *   description = "..."          # optional
 *   license     = "MIT"          # optional (SPDX id)
 *   authors     = ["Alice <a@x>"]# optional
 *   keywords    = ["math","util"] # optional, max 5
 *   entry       = "my_pkg.vsl"   # optional, default: <name>.vsl
 *
 *   [dependencies]
 *   # alias = { git = "url", version = "1.0.0" }
 *   # alias = { path = "../locallib" }
 *   json = { git = "https://github.com/alice/visuall-json", version = "0.1.0" }
 *   mylib = { path = "../mylib" }
 *
 * vsl.lock schema (generated, do not edit by hand):
 *
 *   [[package]]
 *   alias   = "json"
 *   url     = "https://github.com/alice/visuall-json"
 *   version = "0.1.0"
 *   dir     = "C:/Users/alice/.visuall/packages/json@0.1.0"
 * ============================================================================ */

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <stdexcept>

namespace visuall {

// ── Package metadata (from [package]) ────────────────────────────────────

struct ManifestPackage {
    std::string name;
    std::string version;          // semver string, e.g. "1.0.0"
    std::string description;      // optional
    std::string license;          // optional SPDX id
    std::string entry;            // optional; default: name + ".vsl"
    std::vector<std::string> authors;
    std::vector<std::string> keywords;  // max 5
};

// ── Dependency kinds (from [dependencies]) ────────────────────────────────

/// Git-hosted dependency (fetched from a remote tag).
struct GitDep {
    std::string url;      ///< Full git URL
    std::string version;  ///< Semver tag name (without the leading "v")
};

/// Local path dependency (relative to the manifest file; not portable).
struct PathDep {
    std::string path;  ///< Relative or absolute filesystem path
};

using Dependency = std::variant<GitDep, PathDep>;

// ── Full manifest ─────────────────────────────────────────────────────────

struct VslManifest {
    ManifestPackage package;
    /// Map from import alias (the key in [dependencies]) to its descriptor.
    std::unordered_map<std::string, Dependency> dependencies;
};

// ── Lock-file entry ───────────────────────────────────────────────────────

struct LockEntry {
    std::string alias;    ///< Import alias (key from [dependencies])
    std::string url;      ///< Git URL (empty for path deps — they are not locked)
    std::string version;  ///< Resolved semver version
    std::string dir;      ///< Absolute path to the installed package directory
};

struct VslLock {
    /// All locked packages (git deps only; path deps are always live).
    std::vector<LockEntry> packages;
};

// ── Parsing / serialisation helpers ──────────────────────────────────────

/// Parse vsl.toml from file on disk.
/// Throws PackageError on parse failures or schema violations.
VslManifest parseManifest(const std::string& tomlPath);

/// Parse vsl.lock from file on disk.
/// Returns an empty VslLock (not an error) if the file does not exist.
/// Throws PackageError on parse failures.
VslLock parseLock(const std::string& lockPath);

/// Write vsl.lock to disk (overwrites any existing file).
/// Throws PackageError on I/O failure.
void writeLock(const VslLock& lock, const std::string& lockPath);

} // namespace visuall
