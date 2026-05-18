/* ============================================================================
 * fetcher.cpp — git-based package fetch/checkout for vslpkg.
 * ============================================================================ */

#include "fetcher.h"
#include "vsl_paths.h"
#include "diagnostic.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#  define POPEN  _popen
#  define PCLOSE _pclose
#else
#  define POPEN  popen
#  define PCLOSE pclose
#endif

namespace fs = std::filesystem;

namespace vslpkg {

// ── subprocess helper ─────────────────────────────────────────────────────

/// Run @p cmd as a subprocess, capture combined stdout+stderr.
/// Returns {exitCode, output}.
static std::pair<int, std::string> runCmd(const std::string& cmd) {
    std::string output;
    std::array<char, 512> buf;
    FILE* pipe = POPEN(cmd.c_str(), "r");
    if (!pipe) {
        throw visuall::PackageError("Failed to run: " + cmd);
    }
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe)) {
        output += buf.data();
    }
    int rc = PCLOSE(pipe);
#ifdef _WIN32
    // _pclose returns the exit code directly on Windows.
    int exitCode = rc;
#else
    int exitCode = WEXITSTATUS(rc);
#endif
    return {exitCode, output};
}

/// Same as runCmd but throws PackageError on non-zero exit.
static std::string mustRunCmd(const std::string& cmd,
                              const std::string& errContext) {
    auto [code, output] = runCmd(cmd);
    if (code != 0) {
        throw visuall::PackageError(errContext + ":\n" + output);
    }
    return output;
}

// ── path helpers ──────────────────────────────────────────────────────────

static std::string normalizePathSep(const std::string& p) {
    std::string out = p;
    for (char& c : out) if (c == '\\') c = '/';
    return out;
}

static std::string cacheDir(const std::string& gitUrl) {
    return visuall::getPackageCacheDir() + "/" +
           visuall::sanitizeUrlForCache(gitUrl);
}

static std::string storeDir(const std::string& alias,
                             const std::string& version) {
    return visuall::getPackageStoreDir() + "/" + alias + "@" + version;
}

// ── ensure cache ──────────────────────────────────────────────────────────

static void ensureCache(const std::string& gitUrl,
                        const std::string& cachePath) {
    std::error_code ec;
    if (fs::is_directory(fs::path(cachePath), ec) && !ec) {
        // Cache exists — update tags.
        mustRunCmd("git -C \"" + cachePath + "\" fetch --tags --prune 2>&1",
                   "Failed to update cache for " + gitUrl);
        return;
    }

    // First time: treeless clone.
    fs::create_directories(fs::path(cachePath).parent_path(), ec);
    mustRunCmd(
        "git clone --filter=blob:none --no-checkout \"" + gitUrl +
        "\" \"" + cachePath + "\" 2>&1",
        "Failed to clone " + gitUrl);
}

// ── tag existence check ───────────────────────────────────────────────────

static void assertTagExists(const std::string& cachePath,
                             const std::string& tag,
                             const std::string& gitUrl) {
    auto [code, found] = runCmd(
        "git -C \"" + cachePath + "\" tag -l \"" + tag + "\" 2>&1");
    if (code != 0 || found.find(tag) == std::string::npos) {
        // List available tags to provide a "did you mean" hint.
        auto [tc, tags] = runCmd(
            "git -C \"" + cachePath + "\" tag --list \"v*\" 2>&1");
        std::string hint;
        if (!tags.empty()) {
            hint = "\nAvailable tags:\n" + tags;
        }
        throw visuall::PackageError(
            "Tag '" + tag + "' not found in " + gitUrl + hint);
    }
}

// ── fetchManifest ─────────────────────────────────────────────────────────

visuall::VslManifest fetchManifest(const std::string& gitUrl,
                                   const std::string& version) {
    const std::string tag   = "v" + version;
    const std::string cache = cacheDir(gitUrl);

    ensureCache(gitUrl, cache);
    assertTagExists(cache, tag, gitUrl);

    // Extract only vsl.toml from the tag, capture to stdout.
    auto [code, content] = runCmd(
        "git -C \"" + cache + "\" show \"" + tag + ":vsl.toml\" 2>&1");
    if (code != 0) {
        throw visuall::PackageError(
            "'" + gitUrl + "' @ " + version +
            " has no vsl.toml at its root:\n" + content);
    }

    // Write to a temp file so toml::parse (file-based) can read it.
    std::string tmpPath = cache + "/.vslpkg_tmp_manifest.toml";
    {
        std::ofstream f(tmpPath, std::ios::out | std::ios::trunc);
        if (!f) throw visuall::PackageError("Cannot write temp manifest");
        f << content;
    }
    visuall::VslManifest m = visuall::parseManifest(tmpPath);
    std::remove(tmpPath.c_str());
    return m;
}

// ── fetchPackage ──────────────────────────────────────────────────────────

visuall::LockEntry fetchPackage(const std::string& alias,
                                const std::string& gitUrl,
                                const std::string& version,
                                bool force) {
    const std::string tag       = "v" + version;
    const std::string cache     = cacheDir(gitUrl);
    const std::string pkgDir    = storeDir(alias, version);
    const std::string pkgDirN   = normalizePathSep(pkgDir);

    ensureCache(gitUrl, cache);
    assertTagExists(cache, tag, gitUrl);

    // Resolve commit SHA for the tag.
    std::string commit = mustRunCmd(
        "git -C \"" + cache + "\" rev-parse \"" + tag + "\" 2>&1",
        "Failed to resolve tag " + tag + " in " + gitUrl);
    // Trim trailing newline.
    while (!commit.empty() &&
           (commit.back() == '\n' || commit.back() == '\r')) {
        commit.pop_back();
    }

    std::error_code ec;
    if (force && fs::exists(fs::path(pkgDir), ec)) {
        fs::remove_all(fs::path(pkgDir), ec);
        if (ec) throw visuall::PackageError(
            "Failed to remove existing package dir: " + pkgDir);
    }

    if (!fs::exists(fs::path(pkgDir), ec) || ec) {
        fs::create_directories(fs::path(pkgDir), ec);
        if (ec) throw visuall::PackageError(
            "Failed to create package dir: " + pkgDir);

        // Sparse checkout of the tag into the package directory.
        mustRunCmd(
            "git -C \"" + cache + "\" --work-tree=\"" + pkgDir +
            "\" checkout \"" + tag + "\" -- . 2>&1",
            "Failed to checkout " + tag + " from " + gitUrl);
    }

    visuall::LockEntry entry;
    entry.alias   = alias;
    entry.url     = gitUrl;
    entry.version = version;
    entry.dir     = pkgDirN;
    return entry;
}

} // namespace vslpkg
