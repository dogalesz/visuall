/* ============================================================================
 * vsl_paths.cpp — Platform-specific path helpers for the Visuall package store.
 * ============================================================================ */

#include "vsl_paths.h"
#include "diagnostic.h"

#include <algorithm>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>   // GetTempPath fallback only
#else
#  include <sys/types.h>
#  include <pwd.h>       // getpwuid
#  include <unistd.h>    // getuid
#endif

namespace visuall {

std::string getVisuallDataDir() {
#ifdef _WIN32
    // %APPDATA% is the standard per-user roaming application data directory
    // on Windows.  It resolves to the same path as
    // SHGetKnownFolderPath(FOLDERID_RoamingAppData) but avoids a Shell32/
    // ole32 runtime dependency.
    const char* appdata = std::getenv("APPDATA");
    if (appdata && appdata[0] != '\0') {
        std::string path(appdata);
        // Normalise to forward slashes.
        std::replace(path.begin(), path.end(), '\\', '/');
        return path + "/visuall";
    }
    // Last-resort fallback: use the user-profile directory.
    const char* userprofile = std::getenv("USERPROFILE");
    if (userprofile && userprofile[0] != '\0') {
        std::string path(userprofile);
        std::replace(path.begin(), path.end(), '\\', '/');
        return path + "/AppData/Roaming/visuall";
    }
    throw PackageError("Cannot determine %%APPDATA%% directory: "
                       "neither APPDATA nor USERPROFILE is set");
#else
    // Prefer $HOME; fall back to passwd entry.
    // Note: getpwuid(3) may block on LDAP/NIS — known limitation.
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return std::string(home) + "/.visuall";
    }
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir && pw->pw_dir[0] != '\0') {
        return std::string(pw->pw_dir) + "/.visuall";
    }
    throw PackageError("Cannot determine home directory for package store");
#endif
}

std::string getPackageCacheDir() {
    return getVisuallDataDir() + "/cache";
}

std::string getPackageStoreDir() {
    return getVisuallDataDir() + "/packages";
}

std::string sanitizeUrlForCache(const std::string& url) {
    // 1. Strip scheme (anything up to and including "://").
    std::string s = url;
    auto schemeEnd = s.find("://");
    if (schemeEnd != std::string::npos) {
        s = s.substr(schemeEnd + 3);
    }

    // 2. Replace any character outside [a-zA-Z0-9._-] with '_'.
    for (char& c : s) {
        bool safe = (c >= 'a' && c <= 'z') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') ||
                    c == '.' || c == '_' || c == '-';
        if (!safe) c = '_';
    }

    // 3. Truncate to 200 characters.
    if (s.size() > 200) {
        s.resize(200);
    }

    // 4. Append ".git".
    return s + ".git";
}

} // namespace visuall
