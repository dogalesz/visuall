# Visuall Release Checklist

This document describes the process for cutting a new release.

## Pre-Release Checklist

Before tagging a new version (e.g., `v1.3.4`):

### 1. Code Quality
- [ ] All CI jobs green on `main` (Ubuntu build + test, Windows build + test, fuzzer smoke test)
- [ ] All tests pass locally: `ctest --test-dir build --output-on-failure`
- [ ] No known crash bugs without fixes
- [ ] Fuzzer artifacts reviewed (`tests/fuzzing/artifacts/`)

### 2. Version Consistency
- [ ] `CMakeLists.txt` — `project(visuall VERSION X.Y.Z)`
- [ ] `include/version.h` — `#define VISUALL_VERSION "X.Y.Z"`
- [ ] `installer.nsi` — `!define PRODUCT_VERSION "X.Y.Z"`
- [ ] `tools/visuall-lsp/src/handlers/initialize.cpp` — `{"version", "X.Y.Z"}`
- [ ] All four locations match the intended release version

### 3. Changelog
- [ ] `CHANGELOG.md` has a `## [X.Y.Z]` section
- [ ] All user-facing changes since the last release are documented
- [ ] Breaking changes are clearly marked
- [ ] Bug fixes reference the issue or PR number
- [ ] The changelog section is dated with the release date

### 4. Smoke Tests
- [ ] `hello.vsl` example compiles and runs correctly
- [ ] `visuallc --version` reports the correct version
- [ ] `vslpkg --help` works
- [ ] `visuall-lsp --help` works (if built)
- [ ] A simple program with `print()` compiles and runs
- [ ] A simple program with `define` and `return` compiles and runs

### 5. Installer (Windows)
- [ ] Run `makensis installer.nsi` to build the installer
- [ ] Extract installer and verify contents:
  - `visuallc.exe`, `vslpkg.exe`, `visuall-lsp.exe`
  - `libvisuall_runtime.a`
  - `_deps/yyjson-build/libyyjson.a`
  - `ld.lld.exe`
  - `mingw_libs/*` (crt2.o, crtbegin.o, crtend.o, *.a)
  - `stdlib/*.vsl` (all 9 files)
  - `README.md`, `LICENSE`, `CHANGELOG.md`
- [ ] Install on a clean machine (or VM) and verify compilation works

### 6. Package Manager
- [ ] `vslpkg install <pkg>` works for at least one package
- [ ] A program that imports the installed package compiles and runs

## Creating the Release

1. Ensure all pre-release checklist items above are checked
2. Commit any final changes and push to `main`
3. Verify CI is green on `main`
4. Create and push the tag:
   ```bash
   git tag -a vX.Y.Z -m "Visuall vX.Y.Z"
   git push origin vX.Y.Z
   ```
5. CI will automatically:
   - Build and test on Ubuntu + Windows
   - Run the fuzzer smoke test
   - Build and verify the Windows installer
   - Run the end-to-end smoke test
   - Check version consistency across all files
   - Create a GitHub Release with the installer and source tarball
6. Verify the GitHub Release at `https://github.com/<org>/visuall/releases`

## Post-Release

- [ ] Download and test the installer from the GitHub Release
- [ ] Announce the release in relevant channels
- [ ] Update any package distribution channels (Homebrew, etc.)
