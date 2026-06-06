# Visuall Compiler (`visuallc`)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![CI](https://github.com/dogalesz/visuall/actions/workflows/ci.yml/badge.svg)](https://github.com/dogalesz/visuall/actions/workflows/ci.yml)

A compiled programming language with Python-like syntax, built with C++17 and LLVM. Visuall compiles directly to native machine code through LLVM IR, with a mark-and-sweep garbage collector, a growing standard library, and a built-in package manager (`vslpkg`).

```
Source (.vsl) → Lexer → Parser → Type Checker → LLVM IR → O2 Optimization → Native Binary
```

## Features

- **Native compilation** via LLVM (O2 optimization, targets host CPU)
- **Garbage collection** — thread-safe mark-and-sweep GC with conservative stack scanning, free-list pooling, and O(1) interior pointer resolution via chunk-indexed hash table
- **Rich syntax** — classes, closures/lambdas, f-strings, list/dict/tuple literals, list comprehensions, slicing, tuple unpacking, chained comparisons, match statements, generators, enums, decorators, walrus operator, `const` declarations, typed variable declarations (`x: int = 42`), collection generics (`list[int]`, `dict[str, float]`), pointer types in `@extern` (`int*`, `void*`), nullable types, generics, isinstance
- **Concurrency** — goroutines (`go`), channels (`chan`), `make_chan`, send (`<-`) and receive (`<-`) with continuation-passing semantics
- **Zero-overhead C FFI** — `@extern("libname")` decorator for calling C libraries directly via C calling convention
- **Escape analysis** — stack allocation for non-escaping lists, dicts, tuples, and lambda environments, bringing recursive data structure performance closer to C++
- **Module system** — `import` / `from ... import` with multi-file compilation
- **Standard library** — math, string, collections, I/O, random, datetime, json, network, and system modules
- **Error handling** — `try` / `catch` / `finally` / `throw` with typed exceptions
- **Package manager** — `vslpkg` for declaring, installing, and sharing packages via Git or local paths
- **LSP server** — `visuall-lsp` with diagnostics, completion, hover, go-to-definition, references, document symbols, formatting, inlay hints, semantic tokens, signature help, code actions, and rename

## Performance

Benchmarked against equivalent C++ compiled with `g++ -O2` and Python 3.14 (best of 3 runs). C++ functions are marked `noinline` to prevent constant-folding and ensure a fair cross-language comparison.

| Test | C++ (ms) | Visuall (ms) | Python (ms) | Visuall/C++ | Py/C++ |
|------|----------|--------------|-------------|-------------|--------|
| Primes (trial div, 100K ×3) | 6.9 | 12.7 | 241.6 | 1.8x | 35x |
| TreeSum (recursive, depth 22) | 8.2 | 35.6 | 645.9 | 4.3x | 79x |
| Collatz (1..100K) | 15.5 | 17.3 | 822.8 | 1.1x | 53x |
| Strings (200K f-strings) | 32.3 | 66.4 | 36.7 | 2.1x | 1.1x |
| Pi (Leibniz, 10M terms) | 10.7 | 15.1 | 781.0 | 1.4x | 73x |
| Nested loops (2000×2000) | 3.2 | 9.1 | 236.0 | 2.8x | 74x |
| Ackermann (3,11) | 851.6 | 1,192 | 26,918.0 | 1.4x | 32x |
| GCD sum (1..2000) | 45.8 | 51.1 | 863.8 | 1.1x | 19x |
| Fibonacci (fib(35) ×500K) | 5.7 | 7.3 | 728.4 | 1.3x | 128x |
| Float distance (1M) | 1.8 | 7.7 | 198.2 | 4.3x | 110x |

Visuall is within **1.1–2.8x** of C++ on integer compute and loops, and **1.4x** on deeply recursive code (Ackermann). The full benchmark suite runs **~1.9x** slower than C++ overall — 20–50x faster than Python on numeric workloads. The escape analysis pass avoids GC heap allocation for non-escaping list, dict, tuple, and closure objects, eliminating GC overhead on allocation-heavy paths.

Recent compiler optimizations in v1.3.1:
- **Generator lowering** — generator state-machine save/restore now emits direct LLVM `GetElementPtr` field access instead of C runtime function calls, letting the O2 optimizer inline and constant-fold across yield points
- **GC interior pointer resolution** — replaced the O(N) linear heap-list walk with an O(1) chunk-indexed hash table (4 KB chunks) that is built lazily at collection start and freed after sweep, so allocations carry zero bookkeeping overhead. Deep-stack GC pause times down **86–133×**, memory overhead ~2.5% of peak heap

### Micro-benchmarks

| Test | Iterations | C++ | Visuall | vs C++ | Python |
|------|-----------|-----|---------|--------|--------|
| Match dispatch | 1M | 7.5 ms | 6.6 ms | 0.9x | 269 ms |
| Function call | 1M | 4.8 ms | 11.0 ms | 2.3x | 252 ms |
| Tight loop sum | 100K | 5.9 ms | 11.0 ms | 1.9x | 144 ms |

## Download (Prebuilt Binary)

The easiest way to use Visuall is to download the prebuilt release — no need to install LLVM or build from source.

Go to the [Releases page](../../releases) and grab the asset for your platform.

---

### Windows (x86-64)

Download the latest version and extract it anywhere (e.g. `C:\visuall\`).

| File / Folder | Purpose |
|---------------|---------|
| `visuallc.exe` | The Visuall compiler |
| `stdlib/` | Standard library `.vsl` modules (math, string, io, sys, collections) |
| `hello.vsl` | Example program |
| `README.md` | This documentation |

**Install a C linker (MinGW)**

`visuallc` emits object files and calls `gcc` to link the final binary. You need **MinGW-W64**:

1. Download [WinLibs MinGW-W64](https://winlibs.com/) (UCRT or MSVCRT build, x86-64, POSIX threads)
2. Extract it (e.g. `C:\mingw64\`)
3. Add `C:\mingw64\bin` to your system **PATH**

Verify: open PowerShell and run `gcc --version`.

**Use the compiler**

```powershell
cd C:\visuall
.\visuallc.exe hello.vsl -o hello
.\hello.exe
```

---

### Linux (x86-64)

Download `visuallc-linux-x86_64.tar.gz` and extract it:

```bash
tar -xzf visuallc-linux-x86_64.tar.gz
cd visuallc-linux-x86_64
```

**Install a C linker**

`visuallc` calls `gcc` to produce the final binary. Install it if you don't have it:

```bash
# Ubuntu / Debian
sudo apt install gcc

# Fedora / RHEL
sudo dnf install gcc

# Arch
sudo pacman -S gcc
```

**Use the compiler**

```bash
./visuallc hello.vsl -o hello
./hello
```

To use the bundled stdlib from anywhere, pass `--module-path` pointing to the `stdlib/` folder:

```bash
./visuallc program.vsl --module-path ./stdlib -o program
```

---

## Prerequisites (Building from Source)

### LLVM 17+

**Linux (Ubuntu/Debian):**
```bash
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 17
sudo apt install llvm-17-dev
```

**macOS (Homebrew):**
```bash
brew install llvm@17
export PATH="/opt/homebrew/opt/llvm@17/bin:$PATH"
export LLVM_DIR="/opt/homebrew/opt/llvm@17/lib/cmake/llvm"
```

**Windows (prebuilt):**
```powershell
# Download prebuilt binaries from https://releases.llvm.org/
$env:LLVM_DIR = "C:\Program Files\LLVM\lib\cmake\llvm"
```

### Build tools

- CMake 3.20+
- A C++17 compiler: GCC 12+, Clang 15+, or MSVC 2022+

## Building

```bash
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

If LLVM is in a non-standard location:
```bash
cmake .. -DLLVM_DIR=/opt/homebrew/opt/llvm@17/lib/cmake/llvm
```

The compiler binary `visuallc` will be in the `build/` directory.

## Usage

### Compile and run

```bash
./visuallc examples/hello.vsl -o hello
./hello
```

### Debug flags

```bash
./visuallc --tokens examples/hello.vsl    # Dump token stream
./visuallc --ast examples/hello.vsl       # Dump parsed AST
./visuallc --emit-ir examples/hello.vsl   # Emit LLVM IR to stdout
./visuallc --dump-modules program.vsl     # Print resolved module paths
./visuallc --gc-stats program.vsl -o prog # Print GC statistics at exit
```

### Running tests

```bash
cd build
ctest --output-on-failure
```

### Cross-compilation

`visuallc` can cross-compile to any target LLVM supports via `--target`:

```bash
# Compile for ARM64 Linux from an x86-64 host
./visuallc hello.vsl -o hello --target aarch64-linux-gnu

# With CPU tuning and explicit linker
./visuallc hello.vsl -o hello --target aarch64-linux-gnu \
    --cpu cortex-a72 --linker aarch64-linux-gnu-gcc

# Or use --linker-prefix as a shorter alternative
./visuallc hello.vsl -o hello --target aarch64-linux-gnu \
    --linker-prefix aarch64-linux-gnu-
```

**Flags:**

| Flag | Description |
|------|-------------|
| `--target <triple>` | LLVM target triple (e.g. `aarch64-linux-gnu`, `x86_64-w64-mingw32`) |
| `--cpu <cpu>` | Target CPU model (e.g. `cortex-a72`). Default: host CPU for native builds, `generic` for cross builds |
| `--linker <path>` | Full path to linker executable. Takes priority over `--linker-prefix` if both are set |
| `--linker-prefix <p>` | Shorthand: prepends to `gcc` (e.g. `aarch64-linux-gnu-` → `aarch64-linux-gnu-gcc`) |
| `--runtime-lib-dir <d>` | Directory containing cross-compiled runtime libraries |

The linker must be GCC-compatible (accepts `-L`, `-l`, `-o`, `-lm` flags). On Windows this means MinGW GCC, not MSVC `link.exe`.

**Runtime libraries** for the target architecture must be built separately.
The compiler looks for them in this exact order:
1. `--runtime-lib-dir` if provided
2. `<compiler-dir>/runtime/<normalized-triple>/` (cross-compile only)
3. `<compiler-dir>/` (native only — cross builds never fall back here)

The `<normalized-triple>` is LLVM's canonical form of your `--target` triple
(e.g. `aarch64-linux-gnu` normalizes to `aarch64-unknown-linux-gnu`).

Build cross-compiled runtimes (use the **normalized** triple in directory paths —
run `llvm-config --host-target` or check the error message if unsure of the exact form):

```bash
# Example: cross-compile runtime for aarch64
TARGET=aarch64-unknown-linux-gnu   # normalized form of "aarch64-linux-gnu"
mkdir build-cross && cd build-cross
cmake .. -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
         -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
         -DCMAKE_BUILD_TYPE=Release
cmake --build . --target visuall_runtime yyjson
mkdir -p ../runtime/${TARGET}/_deps/yyjson-build/
cp libvisuall_runtime.a ../runtime/${TARGET}/
cp _deps/yyjson-build/libyyjson.a ../runtime/${TARGET}/_deps/yyjson-build/
```

Without `--target`, the compiler behaves exactly as before (host-only compilation).

## Project Structure

```
visuall/
├── CMakeLists.txt
├── LICENSE
├── CHANGELOG.md
├── CODE_OF_CONDUCT.md
├── CONTRIBUTING.md
├── .editorconfig
├── include/
│   ├── ast.h                # AST node hierarchy
│   ├── ast_printer.h        # AST pretty-printer
│   ├── ast_visitor.h        # Pure-virtual visitor interface for AST nodes
│   ├── builtins.h           # Runtime function declarations
│   ├── capture_analyzer.h   # Closure variable capture analysis
│   ├── class_analyzer.h     # Pre-pass to collect class field assignments
│   ├── codegen.h            # LLVM IR code generation
│   ├── diagnostic.h         # Unified compiler error/warning (clang-style)
│   ├── lexer.h              # Tokenizer
│   ├── linker.h             # Native linker interface
│   ├── module_loader.h      # Multi-file module resolution (with package alias registry)
│   ├── parser.h             # Recursive descent parser
│   ├── token.h              # Token types
│   ├── typechecker.h        # Static type checker
│   ├── vsl_manifest.h       # vsl.toml / vsl.lock schema structs and parsing
│   ├── vsl_paths.h          # Platform-specific package store paths
│   └── vsl_resolver.h       # MVS dependency resolver
├── src/
│   ├── main.cpp             # CLI entry point (loads vsl.lock and registers aliases)
│   ├── lexer.cpp
│   ├── parser.cpp
│   ├── typechecker.cpp
│   ├── codegen.cpp
│   ├── builtins.cpp
│   ├── capture_analyzer.cpp
│   ├── class_analyzer.cpp
│   ├── module_loader.cpp
│   ├── linker.cpp
│   ├── ast_printer.cpp
│   ├── vsl_manifest.cpp     # TOML parsing for manifests and lockfiles
│   ├── vsl_paths.cpp        # Package store path helpers
│   └── vsl_resolver.cpp     # Minimal Version Selection resolver
├── tools/
│   ├── vslpkg/
│   │   ├── main.cpp         # vslpkg CLI (init, install)
│   │   ├── fetcher.h
│   │   └── fetcher.cpp      # Git subprocess wrapper (clone, fetch, checkout)
│   └── visuall-lsp/
│       ├── CMakeLists.txt
│       ├── include/         # Server capabilities, document store, formatter, JSON-RPC
│       ├── src/             # LSP server, handlers, workspace index
│       └── tests/           # LSP integration tests
├── stdlib/
│   ├── runtime.c            # C runtime (print, string ops, list/dict/tuple/set ops)
│   ├── gc.c / gc.h          # Mark-and-sweep garbage collector
│   ├── exception_support.cpp # C++ ABI layer for throw/catch support
│   ├── math.vsl             # Math functions (sqrt, sin, cos, log, etc.)
│   ├── string.vsl           # String manipulation (split, join, replace, etc.)
│   ├── collections.vsl      # Stack, Queue, Set
│   ├── random.vsl           # Random number generation
│   ├── datetime.vsl         # Date/time manipulation and formatting
│   ├── json.vsl             # JSON parsing and stringifying
│   ├── network.vsl          # Socket programming and HTTP client
│   ├── io.vsl               # File I/O (read, write, append, list_dir)
│   └── sys.vsl              # System utils (args, exit, env, time)
├── tests/
│   ├── test_main.cpp        # Test runner
│   ├── test_lexer.cpp
│   ├── test_parser.cpp
│   ├── syntax_test.cpp
│   ├── codegen_test.cpp
│   ├── typechecker_test.cpp
│   ├── typesystem_test.cpp
│   ├── closure_test.cpp
│   ├── operator_test.cpp
│   ├── class_analyzer_test.cpp
│   ├── diagnostic_test.cpp
│   ├── gc_test.cpp
│   ├── manifest_test.cpp    # vsl.toml / vsl.lock parsing tests
│   ├── module_loader_test.cpp
│   ├── pkg_test/            # Integration fixture for package path-dep resolution
│   │   ├── app/             # Depends on mathlib via vsl.toml path dep
│   │   └── mathlib/
│   └── module_test/
└── examples/
    └── hello.vsl
```

## Language Quick Reference

### Basics

```python
## Single-line comment
### Multi-line
    comment ###

import math
from string import upper, split
```

### Variables and types

```python
## Regular variables
name = "Visuall"
age = 25
pi = 3.14159
active = true
nothing = null

## Typed declarations (type annotation checked at compile time)
count: int = 0
ratio: float = 0.5
prefix: str = ">>"

## Compile-time constants
const MAX_SIZE = 1024
const TIMEOUT: int = 30
```

### Functions and lambdas

```python
define add(a: int, b: int) -> int:
    return a + b

square = x -> x ** 2
transform = (x, y) -> x + y
```

### Classes

```python
class Point:
    init(x: int, y: int):
        this.x = x
        this.y = y

    define magnitude() -> float:
        return sqrt(this.x ** 2 + this.y ** 2)

class Point3D extends Point:
    init(x: int, y: int, z: int):
        super.init(x, y)
        this.z = z
```

### Control flow

```python
if 18 <= age <= 65:
    status = "working age"
elsif age > 65:
    status = "retired"
else:
    status = "young"

for item in [1, 2, 3]:
    print(item)

for i in range(0, 10, 2):
    print(i)

while running:
    if done:
        break
    continue
```

### Data structures

```python
## Lists
numbers = [1, 2, 3, 4, 5]
numbers[0] = 99

## Tuples
coords = (10, 20, 30)
x, y, z = coords

## Dictionaries
config = {"name": "app", "version": 1}

## List comprehensions
squares = [x * x for x in range(10)]
evens = [x for x in numbers if x % 2 == 0]

## Slicing
first_three = numbers[0:3]
every_other = numbers[0:5:2]
tail = numbers[1:]
```

### Strings and f-strings

```python
greeting = "Hello, World!"
message = f"Result: {value}, took {ms}ms"
multipart = f"{name} scored {score} / {total}"
```

### Error handling

```python
try:
    risky()
catch Error as e:
    print(f"Error: {e}")
finally:
    cleanup()
```

### Operators

```python
## Arithmetic
x = 2 ** 10        ## exponentiation
y = 17 // 3        ## integer division
z = 17 % 3         ## modulo

## Bitwise
a = x & 0xFF
b = x | mask
c = x ^ toggle
d = x << 4
e = x >> 2

## Logical
flag = true and not false
result = a or b

## Comparison (chained)
if 0 <= x <= 100:
    print("in range")
```

### Match statement

```python
match status:
    case 200:
        print("OK")
    case 404:
        print("Not Found")
    case _:
        print("other")

## Guards (conditional checks on case patterns)
match value:
    case x if x > 0:
        print("positive")
    case x if x < 0:
        print("negative")
    case _:
        print("zero")

## String and bool patterns work too
match name:
    case "alice":
        role = "admin"
    case _:
        role = "user"
```

### Generators (`yield`)

```python
define evens(n: int) -> int:
    i = 0
    while i < n:
        yield i
        i = i + 2

for v in evens(10):
    print(v)
```

A `yield` inside a function turns it into a generator: the function collects all yielded values into a list that is returned.

### Enums

```python
enum Color:
    RED
    GREEN
    BLUE

x = Color.RED
```

### Walrus operator (`:=`)

```python
## Assign and test in one expression
if (n := len(data)) > 10:
    print(f"too large: {n}")
```

### Decorators

```python
@cache
@log
define compute(x: int) -> int:
    return x * 2
```

### Context managers (`with`)

```python
with open("file.txt") as f:
    data = f.read()
```

Any object that defines `__enter__` and `__exit__` can be used as a context manager.

### Nullable types and generics

```python
define find(items: list, key: str) -> str?:
    ## returns str or null
    ...

define clamp<T: int>(x: T, lo: T, hi: T) -> T:
    ...
```

### `isinstance`

```python
if isinstance(obj, Animal):
    obj.speak()
```

### Date and time (`datetime`)

```python
import datetime

now = datetime.now()
print(now.format("%Y-%m-%d %H:%M:%S"))

tomorrow = now.add_days(1)
d = datetime.Date(2026, 5, 24)
```

### JSON (`json`)

```python
import json

obj = json.parse('{"key": [1, 2, 3]}')
s = json.stringify(obj)
```

### Networking (`network`)

```python
import network

## Low-level sockets
sock = network.Socket(network.AF_INET, network.SOCK_STREAM)
sock.connect("example.com", 80)
sock.send("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n")
response = sock.recv(4096)
sock.close()

## HTTP client
client = network.HTTPClient()
html = client.get("http://example.com/")
```

### Concurrency (`go`, `chan`)

```python
ch = make_chan[int](0)

define worker(id: int):
    ch <- id * id

go worker(1)
go worker(2)

x = <-ch
y = <-ch
print(x + y)
```

`go` spawns a goroutine that runs concurrently. `<-` sends a value into a channel, and `= <-ch` receives from one. `make_chan[T](capacity)` creates a buffered (capacity > 0) or unbuffered (capacity = 0) channel.

### C FFI (`@extern`)

```python
@extern("m")
define sqrt(x: float) -> float

@extern("c")
define printf(fmt: str) -> void

## Pointer types for C functions that take/return pointers
@extern("c")
define fopen(path: str, mode: str) -> void*

@extern("c")
define fclose(fp: void*) -> int

result = sqrt(16.0)
```

`@extern("libname")` declares an external C function — the compiler emits a native C calling-convention call with no marshaling overhead. Supported pointer types include `int*`, `float*`, `void*`, `char*`, and arbitrary pointer suffixes (`int**`, `my_struct*`, etc.).

---

## Package Manager (`vslpkg`)

`vslpkg` is the Visuall package manager. It reads `vsl.toml` to resolve and install dependencies, then writes a `vsl.lock` that the compiler consumes to wire import aliases.

### Package store layout

| Platform | Location |
|----------|----------|
| Windows  | `%APPDATA%\visuall\packages\<alias>@<version>\` |
| Linux / macOS | `~/.visuall/packages/<alias>@<version>/` |

A Git-based clone cache lives alongside it under `cache/`.

---

### Creating a package (`vslpkg init`)

```bash
cd my-project
vslpkg init my-project
```

This creates a `vsl.toml` scaffold:

```toml
[package]
name        = "my-project"   # lowercase letters, digits, hyphens
version     = "0.1.0"        # semver: MAJOR.MINOR.PATCH
description = ""
license     = ""             # SPDX identifier, e.g. "MIT"

[dependencies]
# alias = { git = "https://github.com/user/repo", version = "1.0.0" }
# alias = { path = "../local-lib" }
```

**Name rules:** must match `^[a-z][a-z0-9-]*$` — lowercase letters, digits, hyphens, starting with a letter.  
**Version rules:** semver `MAJOR.MINOR.PATCH` (e.g. `1.2.3`).

---

### Declaring dependencies

There are two dependency types:

**Git dependency** — fetched from a remote Git repository at a specific tag (`v<version>`):

```toml
[dependencies]
mathlib = { git = "https://github.com/alice/mathlib", version = "1.0.0" }
utils   = { git = "https://github.com/bob/vsl-utils",  version = "2.3.1" }
```

**Path dependency** — points to a local directory that contains its own `vsl.toml`:

```toml
[dependencies]
mathlib = { path = "../mathlib" }
shared  = { path = "./vendor/shared" }
```

The key on the left (`mathlib`, `utils`, etc.) is the **import alias** used in `.vsl` source files:

```python
import mathlib
result = mathlib.add(3, 4)
```

---

### Installing dependencies (`vslpkg install`)

```bash
vslpkg install
```

This will:

1. Parse `vsl.toml`
2. Resolve all transitive Git dependencies using **Minimal Version Selection (MVS)** — the highest minimum version required across all dependency chains is selected
3. Clone each Git dependency (treeless clone) into the local cache, then check out the exact tagged commit into the package store
4. Append path dependencies (validated locally — directory must exist and contain a `vsl.toml`)
5. Write `vsl.lock`

```
Fetching mathlib (https://github.com/alice/mathlib @ 1.0.0)...
Locked 1 dependency
```

To re-download and overwrite existing packages:

```bash
vslpkg install --force
```

**Requirements:** `git` must be on your `PATH`.

---

### The lockfile (`vsl.lock`)

After `vslpkg install`, a `vsl.lock` is written alongside `vsl.toml`. Commit this file to version control — it pins every dependency to an exact directory, ensuring reproducible builds.

Example `vsl.lock`:

```toml
# vsl.lock - generated by vslpkg - do not edit manually

[[package]]
alias   = "mathlib"
url     = "https://github.com/alice/mathlib"
version = "1.0.0"
dir     = "C:/Users/alice/AppData/Roaming/visuall/packages/mathlib@1.0.0"

[[package]]
alias   = "utils"
dir     = "C:/projects/myapp/vendor/utils"
```

> Git packages include `url`, `version`, and `dir`. Path packages only include `alias` and `dir`.

---

### Compiling with packages

Once `vsl.lock` exists in the same directory as your source file (or its parent), `visuallc` loads it automatically:

```bash
visuallc main.vsl -o main
```

No extra flags needed. The compiler reads `vsl.lock`, registers each alias in the module resolver, and resolves `import mathlib` to the correct directory.

---

### Publishing a package

`vslpkg` fetches packages directly from Git repositories — there is no central registry. To publish a package:

1. **Create a Git repository** (GitHub, GitLab, Gitea, or any host) containing your `.vsl` source files and a `vsl.toml` at the root.

2. **Tag a release** using the semver convention `v<MAJOR>.<MINOR>.<PATCH>`:

   ```bash
   git tag v1.0.0
   git push origin v1.0.0
   ```

3. **Tell users to add it** as a dependency:

   ```toml
   [dependencies]
   mylib = { git = "https://github.com/you/mylib", version = "1.0.0" }
   ```

**Required repository layout:**

```
mylib/
├── vsl.toml          # required — must be at the repository root
├── mylib.vsl         # your module source
└── ...
```

The `vsl.toml` at the repository root must declare a valid `[package]` with `name` and `version`.

**Versioning guidance:**

| Change | Version bump |
|--------|-------------|
| Bug fix, no API change | PATCH (`1.0.0` → `1.0.1`) |
| New backwards-compatible API | MINOR (`1.0.0` → `1.1.0`) |
| Breaking API change | MAJOR (`1.0.0` → `2.0.0`) |

MVS selects the **highest minimum** version across all consumers, so breaking changes (major bumps) are not automatically adopted by dependents.

---

## Editor Support (LSP)

`visuall-lsp` is a [Language Server Protocol](https://microsoft.github.io/language-server-protocol/) server that brings Visuall language support to any LSP-compatible editor (VS Code, Neovim, Helix, Emacs, etc.).

### Capabilities

| Feature | Description |
|---------|-------------|
| Diagnostics | Real-time error and warning underlines with range highlighting |
| Completion | Scope-aware autocomplete (trigger: `.`, `(`, `"`, `'`) |
| Hover | Type information and documentation on hover |
| Go to Definition | Jump to symbol definitions (cross-file via workspace index) |
| Find References | List all usages of a symbol across the workspace |
| Document Symbols | Breadcrumb outline of functions, classes, and scopes |
| Workspace Symbols | Fuzzy search for symbols across all project files |
| Formatting | Full-document formatting |
| Inlay Hints | Inline type annotations and parameter names |
| Semantic Tokens | Syntax highlighting (keywords, functions, classes, etc.) |
| Signature Help | Function parameter hints on `(` and `,` trigger |
| Code Actions | Quick fixes (missing return, typo suggestions) |
| Rename | Cross-file symbol renaming |
| Incremental Sync | Efficient range-based document sync for large files |

### Building

The LSP builds alongside the compiler when `BUILD_LSP=ON` (default):

```bash
cd build
cmake .. -DBUILD_LSP=ON
cmake --build . --target visuall-lsp
```

The binary `visuall-lsp` will be in the `build/tools/visuall-lsp/` directory.

### Editor Setup

**VS Code** — add to `settings.json`:

```json
"visuall.lsp.path": "/path/to/build/tools/visuall-lsp/visuall-lsp"
```

**Neovim** (with `nvim-lspconfig`):

```lua
local lspconfig = require('lspconfig')
lspconfig.visuall_lsp = {
  cmd = { '/path/to/visuall-lsp' },
  filetypes = { 'visuall' },
  root_dir = lspconfig.util.root_pattern('vsl.toml', '.git'),
}
```

The server discovers project structure by looking for a `vsl.toml` or `.git` directory at the workspace root, and indexes all `.vsl` files under it.

---

## Links

- [Website](https://visuall.dev) — official site
- [License](LICENSE) — MIT
- [Changelog](CHANGELOG.md) — release history
- [Contributing](CONTRIBUTING.md) — how to get involved
- [Code of Conduct](CODE_OF_CONDUCT.md)
