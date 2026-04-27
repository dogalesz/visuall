#pragma once

#include <string>
#include <stdexcept>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// Diagnostic — unified compiler error/warning with clang-style formatting.
//
// All compiler error classes (LexError, ParseError, TypeError, CodegenError,
// ImportError) inherit from this struct so that a single
//   catch (const Diagnostic& d)
// handler in main.cpp can display every error in a consistent way.
//
// Output format (clang-style):
//   filename:line:col: error: message
//   source_line              (if source_line is non-empty)
//    ^                       (caret at col-1 spaces)
//   hint: suggestion         (if hint is non-empty)
// ════════════════════════════════════════════════════════════════════════════
struct Diagnostic : std::exception {
    enum class Severity { Error, Warning };

    Severity    severity    = Severity::Error;
    std::string message;
    std::string hint;
    std::string filename;
    int         line        = 0;
    int         col         = 0;
    std::string source_line;

    Diagnostic() = default;

    Diagnostic(Severity sev, std::string msg, std::string hnt,
               std::string file, int ln, int c, std::string src = "")
        : severity(sev)
        , message(std::move(msg))
        , hint(std::move(hnt))
        , filename(std::move(file))
        , line(ln)
        , col(c)
        , source_line(std::move(src))
    {
        what_ = buildWhat();
    }

    // Produce the full clang-style formatted string.
    std::string format() const {
        std::string sev_str = (severity == Severity::Error) ? "error" : "warning";
        std::string result = filename
                           + ":" + std::to_string(line)
                           + ":" + std::to_string(col)
                           + ": " + sev_str + ": " + message;
        if (!source_line.empty()) {
            result += "\n" + source_line;
            int nspaces = (col > 1 ? col - 1 : 0);
            result += "\n" + std::string(static_cast<size_t>(nspaces), ' ') + "^";
        }
        if (!hint.empty()) {
            result += "\nhint: " + hint;
        }
        return result;
    }

    const char* what() const noexcept override { return what_.c_str(); }

    // ── Factory helpers ───────────────────────────────────────────────────

    // Throw a bare Diagnostic (not a subclass).
    static void emit(Severity sev, const std::string& msg,
                     const std::string& hnt,
                     const std::string& file, int ln, int c,
                     const std::string& src_line = "") {
        throw Diagnostic(sev, msg, hnt, file, ln, c, src_line);
    }

    // Convenience: throw an error-severity Diagnostic.
    static void error(const std::string& msg, const std::string& hnt,
                      const std::string& file, int ln, int c,
                      const std::string& src_line = "") {
        throw Diagnostic(Severity::Error, msg, hnt, file, ln, c, src_line);
    }

protected:
    std::string what_;
    std::string buildWhat() const { return format(); }
};

} // namespace visuall
