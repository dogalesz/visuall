#pragma once
/* ============================================================================
 * vsl_resolver.h — Minimal Version Selection (MVS) dependency resolver.
 *
 * Algorithm (Go-style MVS):
 *   - Walk the dependency graph breadth-first starting from the root manifest.
 *   - For each (gitUrl, version) pair seen, track maxMinVersion[gitUrl].
 *   - If the same URL appears with two different MAJOR versions → hard error.
 *   - The resolved version for each URL is maxMinVersion[gitUrl].
 *   - Aliases: root manifest's aliases take precedence; transitive deps that
 *     are not explicitly aliased in the root get their package.name as alias;
 *     conflict between a generated alias and an explicit root alias → error.
 *
 * Note: path deps bypass the resolver entirely (they're always live).
 * ============================================================================ */

#include "vsl_manifest.h"
#include <functional>
#include <string>
#include <vector>

namespace visuall {

/// Callback used by the resolver to fetch the manifest of a remote package
/// at a specific version.  vslpkg wires this to the fetcher; unit tests can
/// stub it.
///
/// @param gitUrl   Full git URL of the dependency.
/// @param version  Semver version string (no leading "v").
/// @returns The parsed VslManifest for that package/version.
/// @throws PackageError on network or parse failure.
using ManifestFetcher =
    std::function<VslManifest(const std::string& gitUrl,
                              const std::string& version)>;

/// Run MVS resolution starting from @p root.
///
/// Only git deps are processed; path deps in @p root are copied verbatim into
/// the returned lock (with empty url/version, dir = resolved absolute path
/// relative to @p manifestDir).
///
/// @param root         Parsed root vsl.toml.
/// @param manifestDir  Absolute directory that contains the root vsl.toml
///                     (used to resolve relative path deps).
/// @param fetcher      Callback to retrieve a transitive manifest.
/// @returns A fully-populated VslLock (git entries only; callers should call
///          the fetcher separately to populate 'dir' fields).
/// @throws PackageError on version conflicts, alias conflicts, or fetch errors.
VslLock resolveDeps(const VslManifest& root,
                    const std::string& manifestDir,
                    const ManifestFetcher& fetcher);

} // namespace visuall
