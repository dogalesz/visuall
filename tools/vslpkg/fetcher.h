#pragma once
/* ============================================================================
 * fetcher.h — git-based package fetch/checkout for vslpkg.
 *
 * Strategy:
 *   1. Treeless clone into ~/.visuall/cache/<sanitized_url>.git  (bare)
 *   2. On subsequent calls: git fetch --tags --prune in the cache
 *   3. Checkout the tag into ~/.visuall/packages/<alias>@<version>/
 *   4. Returns a LockEntry with alias, url, version, dir populated.
 *
 * --force: wipes the package store entry before checkout (NOT the cache).
 * ============================================================================ */

#include "vsl_manifest.h"
#include <string>

namespace vslpkg {

/// Fetch (or update the cache for) @p gitUrl and check out tag v@p version
/// into the package store.
///
/// @param alias    Import alias / key from [dependencies].
/// @param gitUrl   Full git URL (https).
/// @param version  Semver string without leading "v".
/// @param force    If true, wipe the existing package-store entry first.
/// @returns A LockEntry with dir pointing to the checked-out source tree.
/// @throws visuall::PackageError on any git or I/O failure.
visuall::LockEntry fetchPackage(const std::string& alias,
                                const std::string& gitUrl,
                                const std::string& version,
                                bool force = false);

/// Read the vsl.toml from a cached tag without fully checking it out.
/// Used by the MVS resolver's ManifestFetcher callback to inspect transitive
/// deps before committing to a full checkout.
///
/// @throws visuall::PackageError on git or parse failure.
visuall::VslManifest fetchManifest(const std::string& gitUrl,
                                   const std::string& version);

} // namespace vslpkg
