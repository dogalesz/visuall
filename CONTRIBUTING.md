# Contributing to Visuall

Thanks for your interest in contributing! Visuall is a compiled programming language with Python-like syntax, built with C++17 and LLVM. This guide covers how to get set up and submit changes.

## Code of Conduct

This project follows the [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code. Report unacceptable behavior to @nslvcha on Discord.

## Getting Started

### Prerequisites

- **CMake** 3.20 or later
- **Ninja** (recommended) or Make
- **LLVM 17** or later (development packages)
- **C++17 compiler**: GCC 12+, Clang 15+, or MSVC 2022+

### Building from Source

**Linux (Ubuntu/Debian):**

```bash
# Install dependencies
sudo apt-get install -y llvm-17-dev ninja-build cmake g++

# Build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Build with LSP
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_LSP=ON
cmake --build build --parallel
```

**Windows (MSYS2/MinGW64):**

```bash
# Install dependencies
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gcc

# Build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

**macOS:**

```bash
# Install dependencies
brew install llvm@17 ninja cmake

# Build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_DIR=$(brew --prefix llvm@17)/lib/cmake/llvm
cmake --build build --parallel
```

### Running Tests

```bash
cd build
ctest --output-on-failure
```

All tests must pass before a PR can be merged.

## Development Workflow

1. **Fork** the repository and clone it locally
2. **Create a branch** from `main` for your change:
   ```bash
   git checkout -b feat/my-feature
   ```
3. **Make your changes** and add tests as needed
4. **Run the test suite** to verify nothing is broken:
   ```bash
   cd build && ctest --output-on-failure
   ```
5. **Commit** using Conventional Commits (see below)
6. **Push** your branch and open a pull request against `main`

### Commit Conventions

We use [Conventional Commits](https://www.conventionalcommits.org/):

| Prefix | Use for |
|--------|---------|
| `feat:` | New features |
| `fix:` | Bug fixes |
| `docs:` | Documentation changes |
| `test:` | Adding or updating tests |
| `refactor:` | Code restructuring without behavior change |
| `perf:` | Performance improvements |
| `chore:` | Build, CI, or tooling changes |

Examples:
```
feat: add pattern matching exhaustiveness checking
fix: resolve GC race condition in allocation path
docs: update LSP setup guide for Neovim
```

Important: Always include a clear description of what changed and why in your commit message.

## Code Style

- **C++17** standard, so no C++20 features
- **4-space indentation** (no tabs)
- **LF line endings**, UTF-8 encoding
- **Warning flags**: `/W4` on MSVC, `-Wall -Wextra -Wpedantic` on GCC/Clang
- Include order: own header first, then standard library, then project headers
- Prefer clear names over comments: let the code speak for itself

An `.editorconfig` file is provided at the repo root; enable EditorConfig support in your IDE.

## Project Structure

```
visuall/
├── include/         # Public headers (AST, parser, codegen, typechecker, etc.)
├── src/             # Compiler implementation
├── tools/
│   ├── vslpkg/      # Package manager CLI
│   └── visuall-lsp/ # LSP server
├── stdlib/          # Runtime (C) and standard library (.vsl files)
├── tests/           # C++ test suite
├── benchmarks/      # Performance benchmarks (Visuall, C++, Python)
└── examples/        # Example .vsl programs
```

The core compilation pipeline is: **Lexer → Parser → Capture Analyzer → Class Analyzer → Type Checker → Codegen (LLVM IR) → Linker → Native Binary**.

Key architectural notes:
- `include/ast.h` - The AST node hierarchy. Every expression and statement type is defined here.
- `include/ast_visi0tor.h` - The visitor interface. Codegen and type checking both use this pattern.
- `include/diagnostic.h` - All compiler errors/warnings use clang-style source range formatting.
- `src/main.cpp` - CLI entry point. Loads `vsl.lock`, registers package aliases, orchestrates compilation.
- The type checker preserves scope history for LSP queries (symbol table uses a "Historical Registry Pattern").

## Pull Request Process

1. Fill out the PR template — describe what changed and why
2. Ensure all tests pass (`ctest --output-on-failure`)
3. Add tests for new functionality
4. Keep compiler warnings clean
5. A maintainer will review your PR — be responsive to feedback
6. Once approved, a maintainer (most likely me, lottieyael) will merge it

## Questions?

If you're unsure about something, open an issue or start a discussion. I'm happy to help!
