# Changelog

All notable changes to the Visuall language are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
