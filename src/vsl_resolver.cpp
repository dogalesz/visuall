/* ============================================================================
 * vsl_resolver.cpp — MVS dependency resolver implementation.
 * ============================================================================ */

#include "vsl_resolver.h"
#include "diagnostic.h"

#include <algorithm>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace visuall {

// ── Semver helpers ─────────────────────────────────────────────────────────

struct Semver {
    int major = 0, minor = 0, patch = 0;
};

static bool parseSemver(const std::string& s, Semver& out) {
    // Accept "X.Y.Z" (no leading "v").
    int major = 0, minor = 0, patch = 0;
    char sep1 = 0, sep2 = 0;
    std::istringstream ss(s);
    ss >> major >> sep1 >> minor >> sep2 >> patch;
    if (ss.fail() || sep1 != '.' || sep2 != '.') return false;
    out = {major, minor, patch};
    return true;
}

static bool semverGreater(const Semver& a, const Semver& b) {
    if (a.major != b.major) return a.major > b.major;
    if (a.minor != b.minor) return a.minor > b.minor;
    return a.patch > b.patch;
}

// ── BFS state ─────────────────────────────────────────────────────────────

struct WorkItem {
    std::string gitUrl;
    std::string version;
    std::string requiredBy;  // human-readable, for error messages
};

// ── resolveDeps ───────────────────────────────────────────────────────────

VslLock resolveDeps(const VslManifest& root,
                    const std::string& /*manifestDir*/,
                    const ManifestFetcher& fetcher) {
    // Maps git URL → resolved version (the maximum of all minimums).
    std::unordered_map<std::string, std::string> resolvedVersion;
    // Maps git URL → alias in the root manifest (may be empty for transitives).
    std::unordered_map<std::string, std::string> urlToAlias;
    // Explicit aliases declared in the root manifest.
    std::unordered_set<std::string> rootAliases;

    // Collect root git deps.
    std::queue<WorkItem> queue;
    for (const auto& [alias, dep] : root.dependencies) {
        if (!std::holds_alternative<GitDep>(dep)) continue;
        const auto& gd = std::get<GitDep>(dep);
        rootAliases.insert(alias);
        urlToAlias[gd.url] = alias;
        queue.push({gd.url, gd.version, "root vsl.toml"});
    }

    // BFS — keep fetching manifests to discover transitive deps.
    // seenUrlVersion tracks (url, version) pairs already enqueued.
    std::unordered_set<std::string> seenUrlVersion;

    while (!queue.empty()) {
        WorkItem item = queue.front();
        queue.pop();

        const std::string& url = item.gitUrl;
        const std::string& ver = item.version;

        // Merge into resolvedVersion[url] using MVS (keep max).
        auto it = resolvedVersion.find(url);
        if (it == resolvedVersion.end()) {
            resolvedVersion[url] = ver;
        } else {
            Semver existing, incoming;
            bool okE = parseSemver(it->second, existing);
            bool okI = parseSemver(ver, incoming);
            if (!okE || !okI) {
                throw PackageError(
                    "Invalid semver '" + (okE ? ver : it->second) +
                    "' required by " + item.requiredBy);
            }
            // Major-version conflict check.
            if (existing.major != incoming.major) {
                std::ostringstream msg;
                msg << "Incompatible major version requirements for '"
                    << url << "': "
                    << ">= " << it->second << " and >= " << ver
                    << " (required by " << item.requiredBy << "). "
                    << "Use two aliases pointing to the same git URL with "
                       "different keys to force coexistence.";
                throw PackageError(msg.str());
            }
            if (semverGreater(incoming, existing)) {
                resolvedVersion[url] = ver;
            }
        }

        // Avoid re-fetching the same (url, version) twice.
        std::string key = url + "@" + ver;
        if (seenUrlVersion.count(key)) continue;
        seenUrlVersion.insert(key);

        // Fetch the manifest for this dep to discover ITS transitive deps.
        VslManifest depManifest;
        try {
            depManifest = fetcher(url, ver);
        } catch (const PackageError& e) {
            throw PackageError("While resolving '" + url + "' @ " + ver +
                               " (required by " + item.requiredBy + "): " +
                               e.message);
        }

        // Enqueue this dep's own git dependencies.
        for (const auto& [transAlias, transDepV] : depManifest.dependencies) {
            if (!std::holds_alternative<GitDep>(transDepV)) continue;
            const auto& tgd = std::get<GitDep>(transDepV);

            // If this URL has no alias yet, assign the package.name as a
            // generated alias (unless a root alias is already using that name).
            if (!urlToAlias.count(tgd.url)) {
                // Use the transitive package's declared name as alias.
                const std::string& generatedAlias = depManifest.package.name;
                if (rootAliases.count(generatedAlias)) {
                    // The root already has a different package under this name.
                    throw PackageError(
                        "Transitive package '" + generatedAlias +
                        "' conflicts with explicit alias '" + generatedAlias +
                        "' in your vsl.toml. Rename one of them.");
                }
                urlToAlias[tgd.url] = transAlias;
            }

            queue.push({tgd.url, tgd.version,
                        "'" + url + "' @ " + ver});
        }
    }

    // Build the VslLock (git deps only; dir is populated by the caller via
    // the fetcher after resolution).
    VslLock lock;
    for (const auto& [url, version] : resolvedVersion) {
        LockEntry entry;
        entry.alias   = urlToAlias.count(url) ? urlToAlias.at(url) : "";
        entry.url     = url;
        entry.version = version;
        entry.dir     = "";  // filled in by vslpkg after checkout
        lock.packages.push_back(std::move(entry));
    }

    // Deterministic order (alias alphabetical) so diffs are stable.
    std::sort(lock.packages.begin(), lock.packages.end(),
              [](const LockEntry& a, const LockEntry& b) {
                  return a.alias < b.alias;
              });

    return lock;
}

} // namespace visuall
