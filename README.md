# Visuall Compiler (`visuallc`)

A compiled programming language with Python-like syntax, built with C++17 and LLVM. Visuall compiles directly to native machine code through LLVM IR, with a mark-and-sweep garbage collector, a growing standard library, and a built-in package manager (`vslpkg`).

```
Source (.vsl) → Lexer → Parser → Type Checker → LLVM IR → O2 Optimization → Native Binary
```

## Features

- **Native compilation** via LLVM (O2 optimization, targets host CPU)
- **Garbage collection** — thread-safe mark-and-sweep GC with conservative stack scanning, free-list pooling, and O(1) pointer lookup
- **Rich syntax** — classes, closures/lambdas, f-strings, list/dict/tuple literals, list comprehensions, slicing, tuple unpacking, chained comparisons, match statements, generators, enums, decorators
- **Module system** — `import` / `from ... import` with multi-file compilation
- **Standard library** — math, string, collections, I/O, random, datetime, json, network, and system modules
- **Error handling** — `try` / `catch` / `finally` / `throw` with typed exceptions
- **Package manager** — `vslpkg` for declaring, installing, and sharing packages via Git or local paths

## Performance
Benchmarked against equivalent C++ compiled with `g++ -O2` and Python 3.14 (best of 3 runs):

| Test | C++ (ms) | Visuall (ms) | Python (ms) | Visuall/C++ | Py/C++ |
|------|----------|--------------|-------------|-------------|--------|
| Primes (sieve to 100K ×3) | 7.6 | 7.9 | 196.8 | 1.0x | 25.9x |
| Collatz (sequence ×5) | 16.7 | 17.5 | 630.6 | 1.0x | 37.8x |
| Strings (200K f-string builds) | 28.0 | 30.3 | 29.6 | 1.1x | 1.1x |
| GCD (Euclidean ×10M) | 46.5 | 55.1 | 434.0 | 1.2x | 9.3x |
| Pi (Leibniz 50M terms) | 11.1 | 16.0 | 581.1 | 1.4x | 52.4x |
| Nested loops (triple ×300) | 3.9 | 8.6 | 149.1 | 2.2x | 38.2x |
| Distance (sqrt ×1M) | 2.1 | 7.0 | 81.1 | 3.3x | 38.6x |
| Fibonacci (recursive fib(35)) | 1.4 | 5.1 | 374.7 | 3.6x | 267.6x |
| TreeSum (recursive depth 22) | 2.7 | 13.4 | 552.6 | 5.0x | 204.7x |
| Ackermann (3,12) | 175.7 | 888.8 | 17,990.7 | 5.1x | 102.4x |

Compute-bound integer work is within **1.1–1.2x** of C++. Recursion-heavy workloads are 4–5x due to GC stack scanning overhead.

## Download (Prebuilt Binary)

The easiest way to use Visuall is to download the prebuilt release — no need to install LLVM or build from source.

Go to the [Releases page](../../releases) and grab the asset for your platform.

---

### Windows (x86-64)

Download `visuall-v0.9.3-beta-windows-x86_64.zip` and extract it anywhere (e.g. `C:\visuall\`).

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

## Project Structure

```
visuall/
├── CMakeLists.txt
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
│   └── vslpkg/
│       ├── main.cpp         # vslpkg CLI (init, install)
│       ├── fetcher.h
│       └── fetcher.cpp      # Git subprocess wrapper (clone, fetch, checkout)
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
name = "Visuall"
age = 25
pi = 3.14159
active = true
nothing = null
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
