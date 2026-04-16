# Visuall Compiler (`visuallc`)

A compiler for the **Visuall** programming language, built with C++17 and LLVM.

```
Source (.vsl) → Lexer → Parser → Type Checker → LLVM Codegen → Native Binary
```

## Prerequisites

### LLVM 17+

**Linux (Ubuntu/Debian):**
```bash
# Add the LLVM apt repository
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 17

# Install development headers
sudo apt install llvm-17-dev
```

**macOS (Homebrew):**
```bash
brew install llvm@17
# Add to PATH:
export PATH="/opt/homebrew/opt/llvm@17/bin:$PATH"
export LLVM_DIR="/opt/homebrew/opt/llvm@17/lib/cmake/llvm"
```

**Windows (vcpkg or prebuilt):**
```powershell
# Option 1: vcpkg
vcpkg install llvm:x64-windows

# Option 2: Download prebuilt binaries from https://releases.llvm.org/
# Set LLVM_DIR to the install path's lib/cmake/llvm directory
$env:LLVM_DIR = "C:\Program Files\LLVM\lib\cmake\llvm"
```

### Build tools

- CMake 3.20+
- A C++17 compiler: GCC 12+, Clang 15+, or MSVC 2022+

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

If LLVM is in a non-standard location, pass `-DLLVM_DIR=<path>`:
```bash
cmake .. -DLLVM_DIR=/opt/homebrew/opt/llvm@17/lib/cmake/llvm
```

The compiler binary `visuallc` will be in the `build/` directory.

## Usage

### Compile a `.vsl` file to a native binary

```bash
./visuallc examples/hello.vsl -o hello
./hello
```

### Debug flags

**Dump the token stream:**
```bash
./visuallc --tokens examples/hello.vsl
```
Output shows each token with its type, lexeme, line, and column:
```
KW_IMPORT 'import' 3:1
IDENTIFIER 'math' 3:8
NEWLINE '\n' 3:12
...
```

**Dump the AST:**
```bash
./visuallc --ast examples/hello.vsl
```
Output shows the parsed abstract syntax tree:
```
Program
  Import: math
  FromImport: math [sqrt]
  ClassDef: Greeter
    InitDef
      ...
```

**Emit LLVM IR to stdout:**
```bash
./visuallc --emit-ir examples/hello.vsl
```

### Running tests

```bash
cd build
ctest --output-on-failure
```

## Project Structure

```
visuall/
├── CMakeLists.txt          # Build system
├── README.md
├── include/
│   ├── token.h             # Token types and Token struct
│   ├── lexer.h             # Lexer interface
│   ├── ast.h               # AST node hierarchy
│   ├── parser.h            # Parser interface
│   └── codegen.h           # LLVM codegen interface
├── src/
│   ├── main.cpp            # Entry point (CLI)
│   ├── lexer.cpp           # Full lexer implementation
│   ├── parser.cpp          # Recursive descent parser
│   └── codegen.cpp         # LLVM IR generation
├── tests/
│   ├── test_main.cpp       # Test runner
│   ├── test_lexer.cpp      # Lexer tests
│   └── test_parser.cpp     # Parser tests
└── examples/
    └── hello.vsl           # Example Visuall program
```

## Language Quick Reference

```python
## Single-line comment
### Multi-line
    comment ###

import math
from math import sqrt

class MyClass:
    init(x: int):
        this.x = x

    define double() -> int:
        return this.x * 2

define add(a: int, b: int) -> int:
    return a + b

square = x -> x ** 2

if 18 <= age <= 65:
    status = "working age"
elsif age > 65:
    status = "retired"
else:
    status = "young"

for item in [1, 2, 3]:
    ...

while running:
    break

try:
    risky()
catch Error as e:
    handle(e)
finally:
    cleanup()
```
