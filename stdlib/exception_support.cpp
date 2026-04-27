/* ════════════════════════════════════════════════════════════════════════════
 * Visuall Exception Support (C++ ABI layer)
 *
 * Provides the VisualException type and the thin helpers that the compiler
 * emits calls to when generating throw / landingpad code:
 *
 *   __visuall_exception_new(msg)       → allocates an exception via __cxa_*
 *   __visuall_exception_msg(exc_obj)   → extracts the message char*
 *   __visuall_get_exception_typeinfo() → returns &typeid(VisualException)
 *
 * On MinGW/Windows, VISUALL_PERSONALITY_SEH is defined (see CMakeLists.txt).
 * ════════════════════════════════════════════════════════════════════════════ */

#include <cxxabi.h>
#include <typeinfo>
#include <new>
#include <cstddef>

// ---------------------------------------------------------------------------
// The exception object thrown by every Visuall 'throw' statement.
// It carries a single string message (char* pointing to a Visuall string).
// ---------------------------------------------------------------------------
struct VisualException {
    const char* msg;
};

// ---------------------------------------------------------------------------
// __visuall_exception_new(const char* msg) → void*
//
// Allocates an exception buffer via __cxa_allocate_exception and initialises
// it with the message pointer.  The returned pointer is passed directly to
// __cxa_throw as the first argument.
// ---------------------------------------------------------------------------
extern "C" void* __visuall_exception_new(const char* msg) {
    void* buf = abi::__cxa_allocate_exception(sizeof(VisualException));
    VisualException* exc = new (buf) VisualException;
    exc->msg = msg ? msg : "";
    return buf;
}

// ---------------------------------------------------------------------------
// __visuall_exception_msg(void* exc_obj) → const char*
//
// Called from the catch handler after __cxa_begin_catch() has returned the
// exception object pointer.  Extracts the message field.
// ---------------------------------------------------------------------------
extern "C" const char* __visuall_exception_msg(void* exc_obj) {
    if (!exc_obj) return "(null exception)";
    return static_cast<const VisualException*>(exc_obj)->msg;
}

// ---------------------------------------------------------------------------
// __visuall_get_exception_typeinfo() → const void*
//
// Returns a pointer to the std::type_info for VisualException.  Passed as
// the second argument to __cxa_throw so the EH runtime can match catch
// clauses by type (we use catch-all in the landingpad IR, but the type_info
// must still be valid for the ABI).
// ---------------------------------------------------------------------------
extern "C" const void* __visuall_get_exception_typeinfo() {
    return static_cast<const void*>(&typeid(VisualException));
}
