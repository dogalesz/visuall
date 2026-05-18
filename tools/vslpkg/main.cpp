/* ============================================================================
 * vslpkg — Visuall package manager CLI
 *
 * Subcommands:
 *   vslpkg init [name]   — scaffold a vsl.toml in the current directory
 *   vslpkg install       — resolve deps and write vsl.lock
 *   vslpkg install --force — wipe and re-install all packages
 *   vslpkg help          — show help
 *
 * Does NOT link LLVM. Pure C++17 + toml11 + vsl_manifest/vsl_paths.
 * ============================================================================ */

#include "vsl_manifest.h"
#include "vsl_paths.h"
#include "vsl_resolver.h"
#include "diagnostic.h"
#include "fetcher.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ── helpers ───────────────────────────────────────────────────────────────

static std::string normalizePathSep(const std::string& p) {
    std::string out = p;
    for (char& c : out) {
        if (c == '\\') c = '/';
    }
    return out;
}

static bool fileExists(const std::string& path) {
    std::error_code ec;
    return fs::exists(fs::path(path), ec) && !ec;
}

static bool dirExists(const std::string& path) {
    std::error_code ec;
    return fs::is_directory(fs::path(path), ec) && !ec;
}

// ── vslpkg init ───────────────────────────────────────────────────────────

static int cmd_init(const std::string& name) {
    const std::string tomlPath = "vsl.toml";
    if (fileExists(tomlPath)) {
        std::cerr << "error: vsl.toml already exists in the current directory\n";
        return 1;
    }

    std::string pkgName = name.empty() ? fs::current_path().filename().string() : name;
    // Normalise: lowercase, replace spaces/underscores with hyphens.
    for (char& c : pkgName) {
        if (c == ' ' || c == '_') c = '-';
        else c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }

    std::ofstream f(tomlPath);
    if (!f) {
        std::cerr << "error: cannot create vsl.toml\n";
        return 1;
    }

    f << "[package]\n";
    f << "name        = \"" << pkgName << "\"  # lowercase letters, digits, hyphens\n";
    f << "version     = \"0.1.0\"             # semver: MAJOR.MINOR.PATCH\n";
    f << "description = \"\"\n";
    f << "license     = \"\"                  # SPDX identifier, e.g. \"MIT\"\n";
    f << "\n";
    f << "# Import aliases used in your .vsl source files:\n";
    f << "#   alias = { git = \"https://github.com/user/repo\", version = \"1.0.0\" }\n";
    f << "#   alias = { path = \"../local-lib\" }\n";
    f << "[dependencies]\n";

    if (!f) {
        std::cerr << "error: I/O failure writing vsl.toml\n";
        return 1;
    }

    std::cout << "Created vsl.toml\n";
    return 0;
}

// ── vslpkg install ────────────────────────────────────────────────────────

static int cmd_install(bool force) {
    const std::string tomlPath = "vsl.toml";
    if (!fileExists(tomlPath)) {
        std::cerr << "error: vsl.toml not found in the current directory\n"
                  << "       Run 'vslpkg init' to create one.\n";
        return 1;
    }

    visuall::VslManifest manifest;
    try {
        manifest = visuall::parseManifest(tomlPath);
    } catch (const visuall::PackageError& e) {
        std::cerr << "error: " << e.message << "\n";
        return 1;
    }

    if (manifest.dependencies.empty()) {
        std::cout << "No dependencies declared in vsl.toml\n";
        visuall::VslLock emptyLock;
        try {
            visuall::writeLock(emptyLock, "vsl.lock");
        } catch (const visuall::PackageError& e) {
            std::cerr << "error: " << e.message << "\n";
            return 1;
        }
        std::cout << "Wrote vsl.lock (no packages)\n";
        return 0;
    }

    fs::path manifestDir = fs::absolute(fs::path(tomlPath)).parent_path();

    // ── Resolve git deps via MVS ─────────────────────────────────────────
    // The ManifestFetcher callback shells out to git for transitive deps.
    visuall::ManifestFetcher fetcher = [](const std::string& url,
                                          const std::string& ver) {
        return vslpkg::fetchManifest(url, ver);
    };

    visuall::VslLock lock;
    try {
        lock = visuall::resolveDeps(manifest, manifestDir.string(), fetcher);
    } catch (const visuall::PackageError& e) {
        std::cerr << "error: " << e.message << "\n";
        return 1;
    }

    // ── Fetch (checkout) each git dep into the package store ─────────────
    for (auto& entry : lock.packages) {
        if (entry.url.empty()) continue;  // path dep — dir already set
        std::cout << "Fetching " << entry.alias
                  << " (" << entry.url << " @ " << entry.version << ")...\n";
        try {
            visuall::LockEntry fetched = vslpkg::fetchPackage(
                entry.alias, entry.url, entry.version, force);
            entry.dir = fetched.dir;
        } catch (const visuall::PackageError& e) {
            std::cerr << "error: " << e.message << "\n";
            return 1;
        }
    }

    // ── Append path dep entries to the lock ──────────────────────────────
    for (const auto& [alias, dep] : manifest.dependencies) {
        if (!std::holds_alternative<visuall::PathDep>(dep)) continue;
        const auto& pd = std::get<visuall::PathDep>(dep);

        fs::path depPath = fs::path(pd.path);
        if (depPath.is_relative()) depPath = manifestDir / depPath;
        std::error_code ec;
        depPath = fs::weakly_canonical(depPath, ec);
        if (ec) {
            std::cerr << "error: path dep '" << alias
                      << "': cannot resolve '" << pd.path
                      << "': " << ec.message() << "\n";
            return 1;
        }
        std::string depDir = normalizePathSep(depPath.string());
        if (!dirExists(depDir)) {
            std::cerr << "error: path dep '" << alias
                      << "': directory not found: " << depDir << "\n";
            return 1;
        }
        std::string depToml = depDir + "/vsl.toml";
        if (!fileExists(depToml)) {
            std::cerr << "error: path dep '" << alias
                      << "': no vsl.toml in " << depDir << "\n";
            return 1;
        }
        visuall::LockEntry entry;
        entry.alias   = alias;
        entry.url     = "";
        entry.version = "";
        entry.dir     = depDir;
        lock.packages.push_back(std::move(entry));
    }

    try {
        visuall::writeLock(lock, "vsl.lock");
    } catch (const visuall::PackageError& e) {
        std::cerr << "error: " << e.message << "\n";
        return 1;
    }

    int total = static_cast<int>(lock.packages.size());
    if (total == 1) std::cout << "Locked 1 dependency\n";
    else            std::cout << "Locked " << total << " dependencies\n";

    return 0;
}

// ── usage ─────────────────────────────────────────────────────────────────

static void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " <command> [options]\n"
              << "\n"
              << "Commands:\n"
              << "  init [name]    Create a vsl.toml scaffold in the current directory\n"
              << "  install        Install/lock all dependencies declared in vsl.toml\n"
              << "  install --force  Re-install all packages (wipes existing store entries)\n"
              << "  help           Show this message\n"
              << "\n"
              << "Examples:\n"
              << "  vslpkg init my-lib\n"
              << "  vslpkg install\n";
}

// ── main ──────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 0;
    }

    std::string cmd = argv[1];

    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        printUsage(argv[0]);
        return 0;
    }

    if (cmd == "init") {
        std::string name = (argc >= 3) ? argv[2] : "";
        return cmd_init(name);
    }

    if (cmd == "install") {
        bool force = false;
        for (int i = 2; i < argc; ++i) {
            if (std::string(argv[i]) == "--force") force = true;
        }
        return cmd_install(force);
    }

    std::cerr << "error: unknown command '" << cmd << "'\n\n";
    printUsage(argv[0]);
    return 1;
}
