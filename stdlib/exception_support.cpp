/* ════════════════════════════════════════════════════════════════════════════
 * Visuall Exception Support (C++ ABI layer)
 *
 * Provides the VisualException type and the thin helpers that the compiler
 * emits calls to when generating throw / landingpad code:
 *
 *   __visuall_exception_new(msg)              → allocates a plain exception
 *   __visuall_exception_new_typed(msg, cls)   → allocates a typed exception
 *   __visuall_exception_msg(exc_obj)          → extracts the message char*
 *   __visuall_exception_class(exc_obj)        → extracts the class name char*
 *   __visuall_get_exception_typeinfo()        → returns &typeid(VisualException)
 *
 * On MinGW/Windows, VISUALL_PERSONALITY_SEH is defined (see CMakeLists.txt).
 * ════════════════════════════════════════════════════════════════════════════ */

#include <cxxabi.h>
#include <typeinfo>
#include <new>
#include <cstddef>

// ---------------------------------------------------------------------------
// The exception object thrown by every Visuall 'throw' statement.
// It carries a message string and an optional class name for typed exceptions.
// ---------------------------------------------------------------------------
struct VisualException {
    const char* msg;
    const char* className; // empty string if untyped
};

// ---------------------------------------------------------------------------
// __visuall_exception_new(const char* msg) → void*
// ---------------------------------------------------------------------------
extern "C" void* __visuall_exception_new(const char* msg) {
    void* buf = abi::__cxa_allocate_exception(sizeof(VisualException));
    VisualException* exc = new (buf) VisualException;
    exc->msg       = msg ? msg : "";
    exc->className = "";
    return buf;
}

// ---------------------------------------------------------------------------
// __visuall_exception_new_typed(const char* msg, const char* className) → void*
// ---------------------------------------------------------------------------
extern "C" void* __visuall_exception_new_typed(const char* msg, const char* className) {
    void* buf = abi::__cxa_allocate_exception(sizeof(VisualException));
    VisualException* exc = new (buf) VisualException;
    exc->msg       = msg ? msg : "";
    exc->className = className ? className : "";
    return buf;
}

// ---------------------------------------------------------------------------
// __visuall_exception_msg(void* exc_obj) → const char*
// ---------------------------------------------------------------------------
extern "C" const char* __visuall_exception_msg(void* exc_obj) {
    if (!exc_obj) return "(null exception)";
    return static_cast<const VisualException*>(exc_obj)->msg;
}

// ---------------------------------------------------------------------------
// __visuall_exception_class(void* exc_obj) → const char*
// ---------------------------------------------------------------------------
extern "C" const char* __visuall_exception_class(void* exc_obj) {
    if (!exc_obj) return "";
    return static_cast<const VisualException*>(exc_obj)->className;
}

// ---------------------------------------------------------------------------
// __visuall_get_exception_typeinfo() → const void*
// ---------------------------------------------------------------------------
extern "C" const void* __visuall_get_exception_typeinfo() {
    return static_cast<const void*>(&typeid(VisualException));
}
