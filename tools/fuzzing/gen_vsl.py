#!/usr/bin/env python3
"""Type-safe random Visuall program generator for effective fuzzing.

Generates programs that compile ~70%+ of the time by tracking variable types
and generating only type-compatible expressions.

Usage:
    python tools/fuzzing/gen_vsl.py --seed 42 --stmts 20 > program.vsl
"""

import argparse
import random
import sys
from typing import Dict, List, Optional, Set, Tuple


# ── Mini type system ─────────────────────────────────────────────────────

class T:
    I = "int"
    F = "float"
    S = "str"
    B = "bool"
    V = "void"


# Which types are legal for which operators
_ARITH_TYPES = (T.I, T.F)
_NUMERIC_TYPES = (T.I, T.F)
_ALL_TYPES = (T.I, T.F, T.S, T.B)

# Operators that work only on numerics (int/float compatible)
_NUMERIC_OPS = ["+", "-", "*", "//", "%", "**"]
# Operators that work on numerics for ordering
_CMP_OPS = ["<", ">", "<=", ">="]
# Equality works on same type or int/float cross
_EQ_OPS = ["==", "!="]
# Boolean ops work on anything
_BOOL_OPS = ["and", "or"]


# ════════════════════════════════════════════════════════════════════════════
# Generator
# ════════════════════════════════════════════════════════════════════════════

class VslGenerator:
    PRESETS = {
        "simple": {
            "assign": 0.45,
            "if": 0.12,
            "while": 0.10,
            "for": 0.10,
            "funcdef": 0.15,
            "print": 0.08,
        },
        "default": {
            "assign": 0.32,
            "if": 0.12,
            "while": 0.08,
            "for": 0.08,
            "funcdef": 0.18,
            "classdef": 0.07,
            "print": 0.10,
            "try": 0.05,
        },
        "all": {
            "assign": 0.25,
            "if": 0.12,
            "while": 0.08,
            "for": 0.08,
            "funcdef": 0.16,
            "classdef": 0.08,
            "print": 0.10,
            "try": 0.06,
            "match": 0.05,
            "throw": 0.02,
        },
    }

    def __init__(self, seed: Optional[int] = None, preset: str = "default"):
        if seed is not None:
            random.seed(seed)
        self.weights = dict(self.PRESETS[preset])
        self.indent = 0
        self.scopes: List[Dict[str, str]] = [{}]   # name -> type
        self.functions: Dict[str, Tuple[List[Tuple[str, str]], str]] = {}
        self.classes: List[str] = []
        self.next_id = 0
        self._loop_depth = 0
        self._func_depth = 0
        self._defining: Optional[str] = None  # function currently being defined
        self._module_var_names: Set[str] = set()

    # ── Scope ────────────────────────────────────────────────────────────

    def _push(self):
        self.scopes.append({})

    def _pop(self):
        self.scopes.pop()

    def _add_var(self, name: str, vtype: str):
        self.scopes[-1][name] = vtype

    def _find_var(self, name: str) -> Optional[str]:
        for s in reversed(self.scopes):
            if name in s:
                return s[name]
        return None

    def _vars_of_type(self, vtype: str) -> List[str]:
        seen = set()
        result = []
        for s in reversed(self.scopes):
            for n, t in s.items():
                if t == vtype and n not in seen:
                    seen.add(n)
                    result.append(n)
        return result

    def _any_var(self, types: Tuple[str, ...] = _ALL_TYPES) -> Optional[str]:
        pool = []
        seen = set()
        for s in reversed(self.scopes):
            for n, t in s.items():
                if t in types and n not in seen:
                    seen.add(n)
                    pool.append(n)
        return random.choice(pool) if pool else None

    def _fresh(self) -> str:
        self.next_id += 1
        return f"v{self.next_id}"

    def _ind(self) -> str:
        return "\t" * self.indent

    # ── Literals ─────────────────────────────────────────────────────────

    def _lit(self, t: str) -> str:
        if t == T.I:
            return str(random.randint(0, 100))
        elif t == T.F:
            return f"{random.uniform(0.0, 100.0):.1f}"
        elif t == T.S:
            cs = "abcdefghijklmnopqrstuvwxyz"
            s = "".join(random.choice(cs) for _ in range(random.randint(1, 6)))
            return f'"{s}"'
        elif t == T.B:
            return random.choice(["true", "false"])
        return "null"

    # ── Type-aware expressions ────────────────────────────────────────────

    def _expr(self, want: str, depth: int = 0) -> str:
        """Generate an expression of the requested type."""
        if depth > 4:
            return self._lit(want)

        r = random.random()

        # 1. Literal of the right type
        if r < 0.20:
            return self._lit(want)

        # 2. Variable of the right type
        elif r < 0.40:
            v = self._any_var((want,))
            if v:
                return v
            return self._lit(want)

        # 3. Binary / comparison / boolean expression that yields the wanted type
        elif r < 0.68:
            if want == T.B:
                return self._gen_bool_expr(depth + 1)
            elif want in (T.I, T.F):
                return self._gen_numeric_expr(want, depth + 1)
            elif want == T.S:
                return self._gen_str_expr(depth + 1)
            return self._lit(want)

        # 4. Parenthesized
        elif r < 0.80:
            return f"({self._expr(want, depth + 1)})"

        # 5. Function call returning the wanted type
        elif r < 0.92:
            fns = [(n, ret) for n, (_, ret) in self.functions.items()
                   if ret == want and n != self._defining]
            if fns:
                fn_name, _ = random.choice(fns)
                params = self.functions[fn_name][0]
                args = [self._expr(pt, depth + 1) for _, pt in params]
                return f"{fn_name}({', '.join(args)})"
            return self._lit(want)

        # 6. Unary: -numeric, not bool
        elif r < 0.98:
            if want in (T.I, T.F):
                return f"-{self._expr(want, depth + 1)}"
            elif want == T.B:
                return f"not {self._expr(T.B, depth + 1)}"
            return self._lit(want)

        # Fallback
        return self._lit(want)

    def _gen_bool_expr(self, depth: int) -> str:
        """Generate an expression that yields bool."""
        r = random.random()
        if r < 0.25:
            return self._lit(T.B)
        elif r < 0.45:
            v = self._any_var((T.B,))
            if v:
                return v
            return self._lit(T.B)
        elif r < 0.65:
            # Comparison
            op = random.choice(_CMP_OPS + _EQ_OPS)
            t = random.choice(_NUMERIC_TYPES)
            return f"{self._expr(t, depth)} {op} {self._expr(t, depth)}"
        elif r < 0.80:
            # Boolean op
            op = random.choice(_BOOL_OPS)
            # For and/or, Visuall allows any type
            return f"{self._expr(T.B, depth)} {op} {self._expr(T.B, depth)}"
        elif r < 0.90:
            return f"not {self._expr(T.B, depth)}"
        else:
            return f"({self._gen_bool_expr(depth)})"

    def _gen_numeric_expr(self, want: str, depth: int) -> str:
        """Generate an expression that yields int or float."""
        r = random.random()
        if r < 0.30:
            return self._lit(want)
        elif r < 0.50:
            v = self._any_var((want,))
            if v:
                return v
            return self._lit(want)
        elif r < 0.75:
            op = random.choice(_NUMERIC_OPS)
            t = random.choice(_NUMERIC_TYPES)
            return f"{self._expr(t, depth)} {op} {self._expr(t, depth)}"
        elif r < 0.85:
            return f"-{self._expr(want, depth)}"
        elif r < 0.95:
            fn_ret = [n for n, (_, ret) in self.functions.items()
                      if ret in _NUMERIC_TYPES and n != self._defining]
            if fn_ret:
                fn_name = random.choice(fn_ret)
                params = self.functions[fn_name][0]
                args = [self._expr(pt, depth) for _, pt in params]
                return f"{fn_name}({', '.join(args)})"
            return self._lit(want)
        else:
            return f"({self._gen_numeric_expr(want, depth)})"

    def _gen_str_expr(self, depth: int) -> str:
        r = random.random()
        if r < 0.35:
            return self._lit(T.S)
        elif r < 0.55:
            v = self._any_var((T.S,))
            if v:
                return v
            return self._lit(T.S)
        elif r < 0.75:
            # String concatenation
            return f"{self._expr(T.S, depth)} + {self._expr(T.S, depth)}"
        elif r < 0.90:
            fn_ret = [n for n, (_, ret) in self.functions.items()
                      if ret == T.S and n != self._defining]
            if fn_ret:
                fn_name = random.choice(fn_ret)
                params = self.functions[fn_name][0]
                args = [self._expr(pt, depth) for _, pt in params]
                return f"{fn_name}({', '.join(args)})"
            return self._lit(T.S)
        else:
            return f"({self._gen_str_expr(depth)})"

    # ── Statements ────────────────────────────────────────────────────────

    def _stmt_kind(self) -> str:
        kinds = list(self.weights.keys())
        w = list(self.weights.values())
        if self.indent >= 4:
            for kw in ("if", "while", "for", "funcdef", "classdef", "try", "match"):
                if kw in kinds:
                    w[kinds.index(kw)] *= 0.1
        # funcdef and classdef only at module level (Visuall scoping rule)
        if self.indent > 0:
            for kw in ("funcdef", "classdef"):
                if kw in kinds:
                    w[kinds.index(kw)] = 0
        if len(self.functions) >= 8 and "funcdef" in kinds:
            w[kinds.index("funcdef")] *= 0.15
        if len(self.classes) >= 3 and "classdef" in kinds:
            w[kinds.index("classdef")] *= 0.1
        total = sum(w)
        if total == 0:
            return "assign"
        return random.choices(kinds, weights=w, k=1)[0]

    def _gen_assign(self) -> str:
        t = random.choice(_ALL_TYPES)
        name = self._fresh()
        # Generate expression FIRST, THEN add variable to scope
        # (prevents self-references like v1 = not v1)
        expr = self._expr(t)
        self._add_var(name, t)
        if self.indent == 0:
            self._module_var_names.add(name)
        return f"{self._ind()}{name} = {expr}"

    def _gen_print(self) -> str:
        n = random.randint(0, 3)
        args = [self._expr(random.choice(_ALL_TYPES)) for _ in range(n)]
        return f"{self._ind()}print({', '.join(args)})"

    def _gen_if(self) -> str:
        lines = [f"{self._ind()}if {self._expr(T.B)}:"]
        self.indent += 1; self._push()
        lines.append(self._gen_stmt())
        self._pop(); self.indent -= 1

        if random.random() < 0.2:
            lines.append(f"{self._ind()}elsif {self._expr(T.B)}:")
            self.indent += 1; self._push()
            lines.append(self._gen_stmt())
            self._pop(); self.indent -= 1

        if random.random() < 0.2:
            lines.append(f"{self._ind()}else:")
            self.indent += 1; self._push()
            lines.append(self._gen_stmt())
            self._pop(); self.indent -= 1

        return "\n".join(lines)

    def _gen_while(self) -> str:
        lines = [f"{self._ind()}while {self._expr(T.B)}:"]
        self.indent += 1; self._push(); self._loop_depth += 1
        lines.append(self._gen_stmt())
        if random.random() < 0.12:
            lines.append(f"{self._ind()}break")
        self._loop_depth -= 1; self._pop(); self.indent -= 1
        return "\n".join(lines)

    def _gen_for(self) -> str:
        v = self._fresh()
        hi = random.randint(1, 10)
        lines = [f"{self._ind()}for {v} in range({hi}):"]
        self.indent += 1; self._push()
        self._add_var(v, T.I); self._loop_depth += 1
        lines.append(self._gen_stmt())
        self._loop_depth -= 1; self._pop(); self.indent -= 1
        return "\n".join(lines)

    def _gen_funcdef(self) -> str:
        name = f"func_{len(self.functions) + 1}"
        params: List[Tuple[str, str]] = []
        for _ in range(random.randint(0, 2)):
            params.append((self._fresh(), random.choice(_ALL_TYPES)))
        ret = random.choice([T.I, T.F, T.S, T.B, T.V])

        ret_str = f" -> {ret}" if ret != T.V else ""
        lines = [f'{self._ind()}define {name}({", ".join(f"{n}: {t}" for n, t in params)}){ret_str}:']
        self.functions[name] = (params, ret)

        prev_defining = self._defining
        self._defining = name
        self.indent += 1; self._push(); self._func_depth += 1
        for pn, pt in params:
            self._add_var(pn, pt)
        lines.append(self._gen_stmt())
        if ret != T.V:
            lines.append(f"{self._ind()}return {self._expr(ret)}")
        self._func_depth -= 1; self._pop(); self.indent -= 1
        self._defining = prev_defining
        return "\n".join(lines)

    def _gen_classdef(self) -> str:
        name = f"Class_{len(self.classes) + 1}"
        self.classes.append(name)
        lines = [f"{self._ind()}class {name}:"]
        self.indent += 1; self._push()

        lines.append(f"{self._ind()}init():")
        self.indent += 1; self._push(); self._func_depth += 1
        lines.append(f"{self._ind()}this.x = {self._expr(T.I)}")
        self._func_depth -= 1; self._pop(); self.indent -= 1

        if random.random() < 0.7:
            mname = f"m_{random.randint(1, 99)}"
            ret = random.choice([T.I, T.S, T.B])
            lines.append(f"{self._ind()}define {mname}() -> {ret}:")
            self.indent += 1; self._push(); self._func_depth += 1
            lines.append(f"{self._ind()}return {self._expr(ret)}")
            self._func_depth -= 1; self._pop(); self.indent -= 1

        self._pop(); self.indent -= 1
        return "\n".join(lines)

    def _gen_try(self) -> str:
        lines = [f"{self._ind()}try:"]
        self.indent += 1; self._push()
        lines.append(self._gen_stmt())
        self._pop(); self.indent -= 1

        lines.append(f"{self._ind()}catch Exception as e:")
        self.indent += 1; self._push()
        self._add_var("e", T.S)
        lines.append(self._gen_stmt())
        self._pop(); self.indent -= 1

        if random.random() < 0.3:
            lines.append(f"{self._ind()}finally:")
            self.indent += 1
            lines.append(self._gen_assign())
            self.indent -= 1
        return "\n".join(lines)

    def _gen_match(self) -> str:
        lines = [f"{self._ind()}match {self._expr(T.I)}:"]
        self.indent += 1
        for i in range(random.randint(1, 2)):
            lines.append(f"{self._ind()}case {i}:")
            self.indent += 1; self._push(); self._func_depth += 1
            lines.append(f"{self._ind()}return {i}")
            self._func_depth -= 1; self._pop(); self.indent -= 1
        lines.append(f"{self._ind()}case _:")
        self.indent += 1
        lines.append(self._gen_assign())
        self.indent -= 1
        self.indent -= 1
        return "\n".join(lines)

    def _gen_throw(self) -> str:
        return f'{self._ind()}throw {self._expr(T.S)}'

    def _gen_stmt(self) -> str:
        kind = self._stmt_kind()
        if kind == "assign":
            return self._gen_assign()
        elif kind == "print":
            return self._gen_print()
        elif kind == "if":
            return self._gen_if()
        elif kind == "while":
            return self._gen_while()
        elif kind == "for":
            return self._gen_for()
        elif kind == "funcdef":
            return self._gen_funcdef()
        elif kind == "classdef":
            return self._gen_classdef()
        elif kind == "try":
            return self._gen_try()
        elif kind == "match":
            return self._gen_match()
        elif kind == "throw":
            return self._gen_throw()
        return self._gen_assign()

    def generate(self, n: int = 20) -> str:
        lines = ["## Auto-generated Visuall program"]
        # Start with typed assignments
        for _ in range(min(4, n)):
            lines.append(self._gen_assign())
        for _ in range(max(0, n - 4)):
            if self.indent > 0 and random.random() < 0.25:
                self.indent = 0
            lines.append(self._gen_stmt())
        return "\n".join(lines)


def main():
    p = argparse.ArgumentParser(
        description="Type-safe Visuall program generator for fuzzing")
    p.add_argument("--seed", type=int, default=None)
    p.add_argument("--stmts", type=int, default=20)
    p.add_argument("--preset", choices=["simple", "default", "all"],
                   default="default")
    args = p.parse_args()
    gen = VslGenerator(seed=args.seed, preset=args.preset)
    print(gen.generate(args.stmts))


if __name__ == "__main__":
    main()
