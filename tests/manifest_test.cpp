// ════════════════════════════════════════════════════════════════════════════
// tests/manifest_test.cpp — Unit tests for vsl.toml / vsl.lock parsing.
//
// Test cases:
//   1. Parse a valid vsl.toml → correct struct fields
//   2. Missing [package] section → PackageError
//   3. Invalid name pattern → PackageError
//   4. Invalid version format → PackageError
//   5. Parse git + path dependencies → correct Dependency variants
//   6. Parse vsl.lock (git entries) → correct LockEntry fields
//   7. Parse vsl.lock (path entries, no url/version) → correct dir field
//   8. writeLock round-trip → parseLock reproduces original entries
//   9. parseLock on missing file → empty VslLock (not an error)
//  10. Dependency missing both 'git' and 'path' keys → PackageError
// ════════════════════════════════════════════════════════════════════════════

#include "vsl_manifest.h"
#include "diagnostic.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace visuall;

static int failures = 0;

static void expect(bool condition, const std::string& name) {
    if (!condition) {
        std::cerr << "  FAIL: " << name << "\n";
        ++failures;
    } else {
        std::cout << "  PASS: " << name << "\n";
    }
}

// Write @p content to a temp file, return its path.
static std::string writeTmp(const std::string& content,
                             const std::string& suffix = ".toml") {
    // Use a fixed name in the build dir; unique per test via suffix.
    static int counter = 0;
    std::string path = "manifest_test_tmp_" + std::to_string(++counter) + suffix;
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    f << content;
    return path;
}

static void removeTmp(const std::string& path) {
    std::remove(path.c_str());
}

// ── Test 1: valid vsl.toml ────────────────────────────────────────────────

static void test_validManifest() {
    std::string path = writeTmp(R"toml(
[package]
name        = "my-lib"
version     = "1.2.3"
description = "A test library"
license     = "MIT"
authors     = ["Alice", "Bob"]
keywords    = ["math", "util"]

[dependencies]
json  = { git = "https://github.com/alice/visuall-json", version = "0.1.0" }
utils = { path = "../utils" }
)toml");

    bool threw = false;
    VslManifest m;
    try {
        m = parseManifest(path);
    } catch (...) {
        threw = true;
    }
    removeTmp(path);

    expect(!threw,                               "1a. Valid manifest: no exception");
    expect(m.package.name        == "my-lib",    "1b. name correct");
    expect(m.package.version     == "1.2.3",     "1c. version correct");
    expect(m.package.description == "A test library", "1d. description correct");
    expect(m.package.license     == "MIT",        "1e. license correct");
    expect(m.package.authors.size() == 2,        "1f. authors count");
    expect(m.package.keywords.size() == 2,       "1g. keywords count");
    expect(m.dependencies.size() == 2,           "1h. two dependencies");
    expect(m.dependencies.count("json"),         "1i. 'json' dep present");
    expect(m.dependencies.count("utils"),        "1j. 'utils' dep present");
}

// ── Test 2: missing [package] ─────────────────────────────────────────────

static void test_missingPackageSection() {
    std::string path = writeTmp("[dependencies]\nfoo = { path = \"../foo\" }\n");
    bool got = false;
    try {
        parseManifest(path);
    } catch (const PackageError&) {
        got = true;
    }
    removeTmp(path);
    expect(got, "2. Missing [package] → PackageError");
}

// ── Test 3: invalid name ──────────────────────────────────────────────────

static void test_invalidName() {
    std::string path = writeTmp("[package]\nname = \"MyLib\"\nversion = \"1.0.0\"\n");
    bool got = false;
    std::string msg;
    try {
        parseManifest(path);
    } catch (const PackageError& e) {
        got = true;
        msg = e.message;
    }
    removeTmp(path);
    expect(got, "3a. Invalid name → PackageError");
    expect(msg.find("invalid") != std::string::npos, "3b. Error mentions 'invalid'");
}

// ── Test 4: invalid version ───────────────────────────────────────────────

static void test_invalidVersion() {
    std::string path = writeTmp("[package]\nname = \"mylib\"\nversion = \"v1.0\"\n");
    bool got = false;
    try {
        parseManifest(path);
    } catch (const PackageError&) {
        got = true;
    }
    removeTmp(path);
    expect(got, "4. Invalid version format → PackageError");
}

// ── Test 5: dependency variants ──────────────────────────────────────────

static void test_depVariants() {
    std::string path = writeTmp(R"toml(
[package]
name    = "app"
version = "0.1.0"
[dependencies]
json  = { git = "https://github.com/alice/json", version = "2.0.0" }
local = { path = "./local-lib" }
)toml");
    VslManifest m;
    try {
        m = parseManifest(path);
    } catch (...) {}
    removeTmp(path);

    bool jsonIsGit = false, localIsPath = false;
    if (m.dependencies.count("json")) {
        jsonIsGit = std::holds_alternative<GitDep>(m.dependencies.at("json"));
    }
    if (m.dependencies.count("local")) {
        localIsPath = std::holds_alternative<PathDep>(m.dependencies.at("local"));
    }
    expect(jsonIsGit,   "5a. 'json' parsed as GitDep");
    expect(localIsPath, "5b. 'local' parsed as PathDep");
    if (jsonIsGit) {
        const auto& gd = std::get<GitDep>(m.dependencies.at("json"));
        expect(gd.url     == "https://github.com/alice/json", "5c. git url correct");
        expect(gd.version == "2.0.0",                         "5d. git version correct");
    }
    if (localIsPath) {
        const auto& pd = std::get<PathDep>(m.dependencies.at("local"));
        expect(pd.path == "./local-lib", "5e. path dep path correct");
    }
}

// ── Test 6: parse vsl.lock (git entries) ─────────────────────────────────

static void test_parseLockGit() {
    std::string path = writeTmp(R"toml(
[[package]]
alias   = "json"
url     = "https://github.com/alice/json"
version = "2.0.0"
dir     = "/home/user/.visuall/packages/json@2.0.0"

[[package]]
alias   = "utils"
url     = "https://github.com/alice/utils"
version = "1.0.0"
dir     = "/home/user/.visuall/packages/utils@1.0.0"
)toml", ".lock");
    VslLock lock;
    try {
        lock = parseLock(path);
    } catch (...) {}
    removeTmp(path);

    expect(lock.packages.size() == 2, "6a. Two lock entries");
    if (lock.packages.size() >= 2) {
        // entries might be in any order — find by alias
        const LockEntry* je = nullptr;
        for (const auto& e : lock.packages)
            if (e.alias == "json") { je = &e; break; }
        expect(je != nullptr,                       "6b. 'json' entry present");
        if (je) {
            expect(je->url     == "https://github.com/alice/json", "6c. url correct");
            expect(je->version == "2.0.0",                         "6d. version correct");
            expect(je->dir     == "/home/user/.visuall/packages/json@2.0.0",
                   "6e. dir correct");
        }
    }
}

// ── Test 7: parse vsl.lock (path entry, no url/version) ──────────────────

static void test_parseLockPath() {
    std::string path = writeTmp(R"toml(
[[package]]
alias = "mylib"
dir   = "C:/projects/mylib"
)toml", ".lock");
    VslLock lock;
    bool threw = false;
    try {
        lock = parseLock(path);
    } catch (...) {
        threw = true;
    }
    removeTmp(path);

    expect(!threw,                               "7a. Path entry: no exception");
    expect(lock.packages.size() == 1,            "7b. One lock entry");
    if (!lock.packages.empty()) {
        expect(lock.packages[0].alias == "mylib",       "7c. alias correct");
        expect(lock.packages[0].url.empty(),             "7d. url empty");
        expect(lock.packages[0].version.empty(),         "7e. version empty");
        expect(lock.packages[0].dir == "C:/projects/mylib", "7f. dir correct");
    }
}

// ── Test 8: writeLock round-trip ──────────────────────────────────────────

static void test_writeLockRoundTrip() {
    VslLock original;
    LockEntry e1;
    e1.alias = "json"; e1.url = "https://example.com/json";
    e1.version = "1.0.0"; e1.dir = "/pkg/json@1.0.0";
    original.packages.push_back(e1);
    LockEntry e2;
    e2.alias = "mylib"; e2.url = ""; e2.version = ""; e2.dir = "/local/mylib";
    original.packages.push_back(e2);

    std::string path = "manifest_test_roundtrip.lock";
    try {
        writeLock(original, path);
    } catch (...) {}

    VslLock loaded;
    try {
        loaded = parseLock(path);
    } catch (...) {}
    std::remove(path.c_str());

    expect(loaded.packages.size() == 2, "8a. Round-trip: same entry count");
    const LockEntry* le1 = nullptr;
    const LockEntry* le2 = nullptr;
    for (const auto& e : loaded.packages) {
        if (e.alias == "json")   le1 = &e;
        if (e.alias == "mylib")  le2 = &e;
    }
    expect(le1 != nullptr,                         "8b. git entry present");
    expect(le2 != nullptr,                         "8c. path entry present");
    if (le1) {
        expect(le1->url     == "https://example.com/json", "8d. git url preserved");
        expect(le1->version == "1.0.0",                    "8e. git version preserved");
        expect(le1->dir     == "/pkg/json@1.0.0",          "8f. git dir preserved");
    }
    if (le2) {
        expect(le2->url.empty(),    "8g. path url empty");
        expect(le2->version.empty(), "8h. path version empty");
        expect(le2->dir == "/local/mylib", "8i. path dir preserved");
    }
}

// ── Test 9: parseLock missing file → empty (not an error) ─────────────────

static void test_parseLockMissing() {
    bool threw = false;
    VslLock lock;
    try {
        lock = parseLock("manifest_test_nonexistent_XXXXXXXXXX.lock");
    } catch (...) {
        threw = true;
    }
    expect(!threw,                     "9a. Missing lock: no exception");
    expect(lock.packages.empty(),      "9b. Missing lock: empty result");
}

// ── Test 10: dep with neither 'git' nor 'path' → PackageError ─────────────

static void test_invalidDepSchema() {
    std::string path = writeTmp(R"toml(
[package]
name    = "app"
version = "0.1.0"
[dependencies]
bad = { registry = "some-registry" }
)toml");
    bool got = false;
    try {
        parseManifest(path);
    } catch (const PackageError&) {
        got = true;
    }
    removeTmp(path);
    expect(got, "10. Dep with unknown schema → PackageError");
}

// ── Entry point ───────────────────────────────────────────────────────────

int runManifestTests() {
    failures = 0;
    test_validManifest();
    test_missingPackageSection();
    test_invalidName();
    test_invalidVersion();
    test_depVariants();
    test_parseLockGit();
    test_parseLockPath();
    test_writeLockRoundTrip();
    test_parseLockMissing();
    test_invalidDepSchema();
    return failures;
}
