/* ============================================================================
 * fetcher.cpp — git-based package fetch/checkout for vslpkg.
 *
 * Uses OS-native process execution (CreateProcess / fork+exec) — NO shell,
 * so git URLs, tags, and paths can never be interpreted as shell commands.
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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace vslpkg {

// ── Platform-specific process execution (NO shell) ─────────────────────────

/// Execute @p program with argument vector @p args.
/// Returns {exitCode, combined stdout+stderr}.
/// Does NOT invoke a shell — every argument is passed verbatim.
static std::pair<int, std::string> execProcess(
    const std::string& program,
    const std::vector<std::string>& args) {

#ifdef _WIN32
    // ── Windows: CreateProcess with pipes ─────────────────────────────────
    // Build a CommandLine string.  Each argument is quoted and double-quotes
    // are escaped so CreateProcess receives them verbatim.
    std::string cmdLine = "\"" + program + "\"";
    for (const auto& arg : args) {
        cmdLine += " \"";
        for (char c : arg) {
            if (c == '"')
                cmdLine += "\\\"";
            else
                cmdLine += c;
        }
        cmdLine += "\"";
    }

    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
        throw visuall::PackageError("Failed to create pipe for: " + program);
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    PROCESS_INFORMATION pi = {};
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.hStdOutput = hWritePipe;
    si.hStdError  = hWritePipe;
    si.dwFlags   |= STARTF_USESTDHANDLES;

    // CreateProcessA needs a mutable buffer.
    std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');

    BOOL ok = CreateProcessA(
        NULL,                 // application name (derived from command line)
        cmdBuf.data(),        // command line
        NULL, NULL,           // process/thread security
        TRUE,                 // inherit handles (for the pipe)
        0,                    // creation flags
        NULL, NULL,           // environment / current directory
        &si, &pi);

    CloseHandle(hWritePipe);   // child will write; parent doesn't need it

    if (!ok) {
        CloseHandle(hReadPipe);
        throw visuall::PackageError("Failed to execute: " + program);
    }
    CloseHandle(pi.hThread);

    // Read all output.
    std::string output;
    char buf[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hReadPipe, buf, sizeof(buf), &bytesRead, NULL) && bytesRead > 0)
        output.append(buf, bytesRead);
    CloseHandle(hReadPipe);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);

    return { static_cast<int>(exitCode), output };

#else
    // ── POSIX: fork + execvp with pipe ────────────────────────────────────
    int pipefd[2];
    if (pipe(pipefd) != 0)
        throw visuall::PackageError("Failed to create pipe for: " + program);

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]); close(pipefd[1]);
        throw visuall::PackageError("Failed to fork for: " + program);
    }

    if (pid == 0) {
        // Child
        close(pipefd[0]);                         // close read end
        dup2(pipefd[1], STDOUT_FILENO);           // stdout → pipe
        dup2(pipefd[1], STDERR_FILENO);           // stderr → pipe
        close(pipefd[1]);

        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(program.c_str()));
        for (const auto& a : args)
            argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);

        execvp(program.c_str(), argv.data());
        _exit(127);  // exec failed
    }

    // Parent
    close(pipefd[1]);                             // close write end
    std::string output;
    char buf[4096];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0)
        output.append(buf, n);
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    return { exitCode, output };
#endif
}

// ── Git-specific wrappers ──────────────────────────────────────────────────

/// Run git with @p args (no shell).  Returns {exitCode, combined stdout+stderr}.
static std::pair<int, std::string> runGit(const std::vector<std::string>& args) {
    return execProcess("git", args);
}

/// Like runGit, but throws PackageError on non-zero exit.
static std::string mustRunGit(const std::vector<std::string>& args,
                              const std::string& errContext) {
    auto [code, output] = runGit(args);
    if (code != 0)
        throw visuall::PackageError(errContext + ":\n" + output);
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
        mustRunGit({"-C", cachePath, "fetch", "--tags", "--prune"},
                   "Failed to update cache for " + gitUrl);
        return;
    }

    // First time: treeless clone.
    fs::create_directories(fs::path(cachePath).parent_path(), ec);
    mustRunGit({"clone", "--filter=blob:none", "--no-checkout",
                gitUrl, cachePath},
               "Failed to clone " + gitUrl);
}

// ── tag existence check ───────────────────────────────────────────────────

static void assertTagExists(const std::string& cachePath,
                             const std::string& tag,
                             const std::string& gitUrl) {
    auto [code, found] = runGit({"-C", cachePath, "tag", "-l", tag});
    if (code != 0 || found.find(tag) == std::string::npos) {
        // List available tags to provide a "did you mean" hint.
        auto [tc, tags] = runGit({"-C", cachePath, "tag", "--list", "v*"});
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

    // Extract vsl.toml from the tag — capture to stdout.
    std::string ref = tag + ":vsl.toml";
    auto [code, content] = runGit({"-C", cache, "show", ref});
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
    std::string commit = mustRunGit(
        {"-C", cache, "rev-parse", tag},
        "Failed to resolve tag " + tag + " in " + gitUrl);
    // Trim trailing newline / CR.
    while (!commit.empty() &&
           (commit.back() == '\n' || commit.back() == '\r'))
        commit.pop_back();

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
        mustRunGit({"-C", cache, "--work-tree=" + pkgDir,
                    "checkout", tag, "--", "."},
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
