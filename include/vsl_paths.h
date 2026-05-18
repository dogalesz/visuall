#pragma once
/* ============================================================================
 * vsl_paths.h — Platform-specific path helpers for the Visuall package store.
 * ============================================================================ */

#include <string>

namespace visuall {

/// Return the per-user Visuall data directory.
///
/// Windows: %APPDATA%\visuall  (via getenv("APPDATA"); falls back to USERPROFILE)
/// macOS/Linux: $HOME/.visuall (via getenv("HOME"); falls back to getpwuid(3))
///
/// Note (POSIX): getpwuid(3) may block on LDAP/NIS; this is a known
///               limitation for environments with network-backed passwd DBs.
///
/// Throws PackageError if the directory cannot be determined.
std::string getVisuallDataDir();

/// Canonical location of the git-bare-clone cache.
/// = getVisuallDataDir() + "/cache"
std::string getPackageCacheDir();

/// Canonical location of installed package trees.
/// = getVisuallDataDir() + "/packages"
std::string getPackageStoreDir();

/// Convert a git URL into a safe filesystem component for use as a cache key.
/// Algorithm:
///   1. Strip scheme (e.g. "https://")
///   2. Replace any character outside [a-zA-Z0-9._-] with '_'
///   3. Truncate to 200 characters
///   4. Append ".git"
/// Example: "https://github.com/alice/visuall-json"
///        → "github.com_alice_visuall-json.git"
std::string sanitizeUrlForCache(const std::string& url);

} // namespace visuall
