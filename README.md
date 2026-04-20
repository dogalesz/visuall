# Visuall Compiler (`visuallc`)

A compiled programming language with Python-like syntax, built with C++17 and LLVM. Visuall compiles directly to native machine code through LLVM IR, with a mark-and-sweep garbage collector and a growing standard library.

```
Source (.vsl) → Lexer → Parser → Type Checker → LLVM IR → O2 Optimization → Native Binary
```

## Features

- **Native compilation** via LLVM (O2 optimization, targets host CPU)
- **Garbage collection** — mark-and-sweep GC with conservative stack scanning, free-list pooling, and O(1) pointer lookup
- **Rich syntax** — classes, closures/lambdas, f-strings, list/dict/tuple literals, list comprehensions, slicing, tuple unpacking, chained comparisons
- **Module system** — `import` / `from ... import` with multi-file compilation
- **Standard library** — math, string, collections, I/O, and system modules
- **Error handling** — `try` / `catch` / `finally` / `throw`

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

## Prerequisites

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
│   ├── builtins.h           # Runtime function declarations
│   ├── capture_analyzer.h   # Closure variable capture analysis
│   ├── codegen.h            # LLVM IR code generation
│   ├── lexer.h              # Tokenizer
│   ├── linker.h             # Native linker interface
│   ├── module_loader.h      # Multi-file module resolution
│   ├── parser.h             # Recursive descent parser
│   ├── token.h              # Token types
│   └── typechecker.h        # Static type checker
├── src/
│   ├── main.cpp             # CLI entry point
│   ├── lexer.cpp            # Lexer implementation
│   ├── parser.cpp           # Parser implementation
│   ├── typechecker.cpp      # Type checker implementation
│   ├── codegen.cpp          # LLVM IR generation
│   ├── builtins.cpp         # Runtime function prototypes
│   ├── capture_analyzer.cpp # Closure capture analysis
│   ├── module_loader.cpp    # Module loader
│   ├── linker.cpp           # Object file linking
│   └── ast_printer.cpp      # AST printer
├── stdlib/
│   ├── runtime.c            # C runtime (print, string ops, list/dict/tuple ops)
│   ├── gc.c / gc.h          # Mark-and-sweep garbage collector
│   ├── math.vsl             # Math functions (sqrt, sin, cos, log, etc.)
│   ├── string.vsl           # String manipulation (split, join, replace, etc.)
│   ├── collections.vsl      # Stack, Queue, Set
│   ├── io.vsl               # File I/O (read, write, append, list_dir)
│   └── sys.vsl              # System utils (args, exit, env, time)
├── tests/
│   ├── test_main.cpp        # Test runner (Google Test)
│   ├── test_lexer.cpp       # Lexer tests
│   ├── test_parser.cpp      # Parser tests
│   ├── syntax_test.cpp      # Syntax integration tests
│   ├── codegen_test.cpp     # Code generation tests
│   ├── typechecker_test.cpp # Type checker tests
│   ├── typesystem_test.cpp  # Type system tests
│   ├── closure_test.cpp     # Closure/lambda tests
│   ├── operator_test.cpp    # Operator tests
│   ├── gc_test.cpp          # GC tests
│   ├── module_loader_test.cpp # Module tests
│   └── module_test/         # Multi-file module test fixtures
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
