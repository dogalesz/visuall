# Changelog

All notable changes to the Visuall language are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


## [1.3.4] - 2026-06-26

### Fixed
- **== and != type validation**: the type checker now rejects equality comparisons
  between values of incompatible types.
- **Cross-function variable access**: variables defined in one function body are no
  longer visible inside another function's body.
- **Fuzzer crash (oversized integer literal)**: `std::stoll` exceptions from
  over-large integer/float literals are now caught and reported as `ParseError`
  instead of crashing the compiler.
- **Windows installer linker dependency chain**: `libxml2-16.dll` (transitive
  dependency of `libLLVM-20.dll`) and `libgcc.a`/`libgcc_eh.a` (in GCC-versioned
  subdirectory) are now included in the installer so `ld.lld` can link user
  programs without missing-library errors.
- **Dangling StringRef in linker argument builder**: CRT path strings and
  linker flags declared inside `if` blocks were going out of scope before
  `ExecuteAndWait()` ran, corrupting the linker command line and producing
  `undefined symbol: mainCRTStartup` errors on Windows. All argument strings
  are now hoisted to function scope.
- Remove `-static-libgcc -static-libstdc++` from `visuall_tests` link flags to
  prevent CI segfault (follow-up to the same fix applied to `visuallc`).

### Changed
- LSP server version string updated from `1.3.0` to `1.3.4` to match the
  compiler version.
- CI now verifies Windows installer contents, runs end-to-end smoke tests, and
  automatically creates GitHub Releases on tag push.

## [1.3.3] - 2026-06-17

### Added
- Self-contained Windows toolchain: the installer and zip archive now include
  `ld.lld` (LLVM linker) and MinGW CRT startup objects plus system import
  libraries in a `mingw_libs/` directory.  `visuallc` auto-detects the bundled
  linker next to itself at runtime — end users need no MSYS2 or MinGW
  installation to compile `.vsl` programs.
- Regression test suite (`tests/security_test.cpp`) for runtime security and
  correctness fixes.

### Fixed
- Shell injection in package manager: `vslpkg` now uses platform-native process
  execution (CreateProcess / fork+exec) with argument arrays instead of
  `popen()` with shell string interpolation.  Git URLs and tags from `vsl.toml`
  can no longer inject shell commands.
- Data races in goroutine scheduler: the ready queue is now protected by a
  mutex (SRWLOCK on Windows, pthread_mutex_t on POSIX).  Per-channel mutexes
  (already allocated) are now acquired in `chan_send`/`chan_recv`/`chan_close`,
  fixing races on the ring buffer and blocked-task queues.
- Channel direct-handoff: when a sender pairs with a waiting receiver, the
  value is now correctly delivered via a `recv_value` field — previously the
  receiver woke up with a stale or zero value.
- Channel close: blocked senders and receivers are now re-enqueued onto the
  ready queue so they can observe the closed flag and clean up, instead of
  being silently discarded (goroutine leak).
- Integer overflow guards in runtime collections (`list_push`, `dict_set`,
  `set_add`) — capacity doubling now checks against `INT64_MAX / 2`.
- Integer overflow guard in `string_repeat` — `len * n` multiplication now
  checks against `(SIZE_MAX - 1) / len` before the multiply, preventing
  undersized allocation followed by heap buffer overflow.
- GC: allocations exceeding `UINT32_MAX` bytes are now rejected with a clear
  error instead of having their size silently truncated in the header.
- GC: `calloc` return value checked in `ptr_map` init/grow/rebuild to prevent
  null-pointer dereference under memory pressure.
- GC: lazy-init the GC lock so `__visuall_alloc` is safe to call after
  `__visuall_gc_shutdown`.  Previously the lock was deleted during shutdown but
  re-entered by later allocations (e.g. between GC test fixtures and channel
  tests), corrupting internal CRITICAL_SECTION state and causing an exit-time
  segfault on Windows.

### Changed
- Windows install guide updated to reflect self-contained toolchain — the
  "Install a C linker (MinGW)" section has been removed.
- CONTRIBUTING.md updated: fixed a filename typo, added LLVM to Windows build
  dependencies, and clarified code style for C++ vs `.vsl` indentation.
- Stop tracking `.vscode/` in git (was already in `.gitignore` but remained
  indexed).
- Add `DEEPSEEK.md` and `bughunting/` to `.gitignore`.

## [1.3.2] - 2026-06-06

### Changed
- Lazy chunk table build: the GC interior-pointer chunk table is now built from the
  heap-linked list at collection start instead of eagerly on every allocation,
  eliminating ~16% allocation-time overhead and recovering full string-benchmark
  performance while keeping O(1) interior pointer resolution.

## [1.3.1] - 2026-06-04

### Added
- `const` keyword for compile-time constant declarations (`const NAME [: Type] = expr`)
- Typed variable declarations: `x: int = 42` syntax with type annotation validation
- Collection type generics: `list[int]`, `dict[str, int]`, `tuple[int, str]` in type annotations
- Bare collection types (`list`, `dict`, `tuple`) as wildcard type annotations
- `len()` builtin type-checked in the type system
- Pointer types in `@extern` declarations: `int*`, `void*`, `char*`, and arbitrary pointer suffixes
- New string escape sequences: `\xNN` (hex byte), `\a` (bell), `\b` (backspace), `\f` (form feed), `\v` (vertical tab), `\e` (ESC)
- 4-space indentation normalization: spaces are automatically converted to tabs before lexing
- `print()` now dispatches via GC tag for readable list and tuple output

### Changed
- Extern declarations (`@extern`) now validated to be at module scope (not inside functions)
- Functions that return a value now require explicit `-> ReturnType` annotation
- Return type checking now uses `isAssignableTo` instead of strict `typeEquals` for better collection compatibility
- `getLLVMType()` handles parameterized, nullable, and pointer type suffixes
- Class field LLVM types now preserved through `classFieldTypes_` for correct pointer/double round-tripping

### Fixed
- **VSL-001**: Use-after-free risk: `escapeInfo_` raw pointer replaced with `std::shared_ptr`
- **VSL-002**: Path traversal in module loader: `..` and `/` components in module names now rejected, null bytes stripped, `normalizePath()` resolves `.`/`..` components
- **VSL-003**: Parser out-of-bounds read on empty or comment-only source files
- **VSL-004/005**: Unicode validation: surrogate codepoints (U+D800–U+DFFF) and codepoints above U+10FFFF now rejected with clear errors
- **VSL-006**: Missing `__cxa_throw` fallback changed from `unreachable` to `abort()` — prevents undefined behavior on throw in compiled programs
- **VSL-008**: `externLibName` now validated against allowed characters (alphanumeric, `_`, `.`, `-`); path separators and leading dashes rejected
- **VSL-009**: Integer overflow in allocation size multiplication — added `safeMul8()` with compile-time overflow check
- **VSL-010**: Library search paths canonicalized via `llvm::sys::fs::real_path()` before linker flag construction
- **VSL-012**: C variadic `...` now enforced as the last parameter in a function signature
- **VSL-014**: Added `std::abort()` guard after `error()` in `expect()` for safety if `[[noreturn]]` is violated
- **VSL-015**: `tokenTypeToString()` now has a `default:` case instead of returning `"UNKNOWN"` outside the switch
- **VSL-017**: Added assertion guard in lexer main loop to catch infinite-loop regressions in debug builds

## [1.3.0] - 2026-06-03

### Added
- Cross-compilation: `--target`, `--cpu`, `--linker`, `--linker-prefix`, `--sysroot`, `--features`, and `--runtime-lib-dir` CLI flags for compiling to non-host architectures
- `TargetSpec` struct carrying target triple, CPU, features, linker path, sysroot, and runtime library directory through the compilation pipeline
- CPU safety: cross builds default to `"generic"` CPU (not host CPU) to avoid emitting instructions the target cannot execute
- Triple normalization (`llvm::Triple::normalize()`) for runtime library directory names
- Pre-link existence checks for runtime and yyjson libraries with user-friendly error messages
- Deterministic runtime library lookup order with fallback from normalized to user-supplied triple directory

### Changed
- `Linker::optimize()`, `Linker::emitObjectFile()`, `Linker::linkToBinary()`, and `Linker::compileAndLink()` accept an optional `TargetSpec` parameter (defaults to native host)
- `Linker::optimize()` now throws on null target lookup (was silently ignored)
- LLVM target initialization centralized into `initializeTargets()` / `ensureTargetsReady()` helpers
- TargetMachine creation factored into shared `createTargetMachine()` helper
- `-lws2_32` is now conditionally added only when targeting Windows (not unconditionally)
- Linker executable configurable via `--linker <path>` or `--linker-prefix <prefix>`

### Fixed
- Command injection vulnerability: replaced `std::system()` with `llvm::sys::ExecuteAndWait()` — linker is now invoked via argument array with no shell
- `CodeGenFileType` version guard corrected to `LLVM_VERSION_MAJOR <= 17` (scoped enum boundary is LLVM 18)

### Removed
- Dead code: `Codegen::emitObjectFile()`, `Codegen::linkToBinary()`, and `Codegen::compileToNative()` methods (unused by the standard pipeline)

## [1.2.0] - 2026-05-31

### Added
- Escape analysis: stack allocation for non-escaping lists, dicts, tuples, and lambda environments via LLVM `alloca`
- Zero-overhead C FFI: `@extern("libname")` decorator for calling C libraries directly with C calling convention
- Concurrency: `go` keyword for goroutines, `chan` type, `make_chan` builtin, `<-` send/receive operators
- M:N scheduler with 2-thread worker pool for goroutine execution
- GC root tracing for scheduler ready-queue and channel blocked-queues

### Changed
- CMake build improved for cross-environment MSYS2 auto-detection (UCRT64/MINGW64)
- CI updated with Windows and Ubuntu build workflows
- Escape analysis inserted between ClassAnalyzer and TypeChecker in compilation pipeline

## [1.1.0] - 2026-05-24

### Added
- LSP server (`visuall-lsp`) with diagnostics, completion, hover, go-to-definition, references, document symbols, workspace symbols, formatting, inlay hints, semantic tokens, signature help, code actions, and rename
- Workspace symbol indexing with cross-file search
- Package manager (`vslpkg`) with `init` and `install` commands, MVS dependency resolution, Git-based package distribution, and `vsl.lock` lockfile support
- Enums with dot-access syntax (`Color.RED`)
- Generators (`yield` keyword)
- Typed exceptions (`catch TypeError`)
- Random stdlib module (`random.vsl`)
- DateTime stdlib module (`datetime.vsl`)
- JSON stdlib module (`json.vsl`)
- Network stdlib module (`network.vsl`) with socket and HTTP client support
- Stack traces on runtime exceptions
- Multiple inheritance (`extraBases`)
- Interface definitions (`interface` keyword, `implements` clause)
- `@property` decorator support

### Changed
- Module system enhanced with package alias registry from `vsl.lock`
- Type checker improvements and enhancements
- README restructured with full language reference, LSP documentation, and package manager guide
- Performance benchmarks added comparing against C++ and Python

### Fixed
- Memory leaks in garbage collector

## [1.0.0] - 2026-05-14

### Added
- Match statement guard support (`case x if x > 0`)
- Enhanced type checking

### Changed
- Stable release milestone

## [0.9.6-beta] - 2026-05-11

### Added
- vslpkg package manager (initial version)
- Enums, generators, typed exceptions
- Random stdlib module
- Stack traces on exceptions

### Changed
- Code cleanup and refactoring

### Fixed
- Memory leaks

## [0.9.5-beta] - 2026-05-09

### Changed
- README updates to reflect current features
- Type checker refinements

## [0.9.4-beta] - 2026-05-08

### Changed
- README update for v0.9.3 changes (community contribution)

## [0.9.3-beta] - 2026-05-07

### Changed
- README and documentation updates
- Type checker refinements

## [0.9.2-beta] - 2026-05-06

### Fixed
- Patches and bug fixes

## [0.9.1-beta] - 2026-05-05

### Added
- Membership tests (`in`, `not in`)
- Field discovery in class definitions

### Changed
- Beta release milestone with full compilation pipeline

## [0.9.0-beta] - 2026-05-03

### Added
- Module system with multi-file compilation
- Type checker
- Closure capture analysis
- Class field analysis
- Several new test suites

## [0.1.1-alpha] - 2026-05-01

### Fixed
- Bug fixes and minor improvements

## [0.1.0-alpha] - 2026-04-28

### Added
- Initial release with lexer, parser, and LLVM code generation
- Basic types: int, float, str, bool, void, null
- Functions, classes (single inheritance), closures/lambdas
- List, dict, tuple literals and list comprehensions
- Control flow: if/else, for, while, match
- Exception handling: try/catch/finally/throw
- Built-in garbage collector
- C runtime library for built-in operations
