#!/usr/bin/env python3
"""Reference interpreter for a deterministic subset of Visuall.

Used for differential fuzzing: compares interpreter output against the
compiled binary's output to detect miscompilations.

Supported subset (matches gen_vsl.py output):
  - Literals: int, float, string, bool, null
  - Expressions: arithmetic, comparison, boolean, unary, function calls
  - Statements: assignment, expr-stmt, pass, continue, break
  - Control flow: if/elsif/else, while, for-in-range
  - Functions: define with parameters and return
  - Builtins: print, range

Not supported:
  - Classes, objects, methods, f-strings, try/catch/finally
  - Lambdas, closures, lists, dicts, tuples, imports
  - Goroutines, channels, match, with, del, assert

Usage:
    python interpreter.py program.vsl
    echo "x = 1\ny = 2\nprint(x + y)" | python interpreter.py -
"""

import re
import sys
from typing import Any, Dict, List, Optional, Tuple, Union


# ════════════════════════════════════════════════════════════════════════════
# Tokenizer
# ════════════════════════════════════════════════════════════════════════════

class Token:
    __slots__ = ("kind", "value", "line")
    def __init__(self, kind: str, value: str, line: int):
        self.kind = kind
        self.value = value
        self.line = line
    def __repr__(self):
        return f"Token({self.kind}, {self.value!r})"


# Word-boundary anchor for keywords — prevents "in" matching "int".
_KW = r"(?![a-zA-Z0-9_])"

_TOKEN_SPEC = [
    ("STRING",   r'"[^"]*"'),
    ("FLOAT",    r"\d+\.\d*"),
    ("INT",      r"\d+"),
    ("BOOL",     r"true" + _KW + r"|false" + _KW),
    ("NULL",     r"null" + _KW),
    ("DEFINE",   r"define" + _KW),
    ("CLASS",    r"class" + _KW),
    ("INIT",     r"init" + _KW),
    ("RETURN",   r"return" + _KW),
    ("IF",       r"if" + _KW),
    ("ELSIF",    r"elsif" + _KW),
    ("ELSE",     r"else" + _KW),
    ("WHILE",    r"while" + _KW),
    ("FOR",      r"for" + _KW),
    ("IN",       r"in" + _KW),
    ("RANGE",    r"range" + _KW),
    ("BREAK",    r"break" + _KW),
    ("CONTINUE", r"continue" + _KW),
    ("PASS",     r"pass" + _KW),
    ("PRINT",    r"print" + _KW),
    ("AND",      r"and" + _KW),
    ("OR",       r"or" + _KW),
    ("NOT",      r"not" + _KW),
    ("IDENT",    r"[a-zA-Z_][a-zA-Z0-9_]*"),
    ("OP",       r"==|!=|<=|>=|->|\*\*|//|[+\-*/%=<>!()\[\]{},:.]"),
    ("COMMENT",  r"##[^\n]*"),
    ("SPACE",    r"[ ]+"),
    ("MISMATCH", r"."),
]

_TOKEN_RE = re.compile("|".join(f"(?P<{n}>{p})" for n, p in _TOKEN_SPEC))


def tokenize(source: str) -> List[Token]:
    """Tokenize Visuall source, emitting INDENT/DEDENT for tab-based blocks."""
    tokens: List[Token] = []
    indent_stack: List[int] = [0]
    line_no = 1
    line_start = True
    line_tokens: List[Token] = []

    def flush_line():
        nonlocal line_start, line_tokens
        if not line_tokens:
            line_start = True
            return

        # Compute indent from leading TAB count.
        indent = 0
        idx = 0
        while idx < len(line_tokens) and line_tokens[idx].kind == "TAB":
            indent += 1
            idx += 1
        content = line_tokens[idx:]

        if not content:
            line_tokens = []
            line_start = True
            return

        # Emit NEWLINE (except before the very first logical line).
        if tokens:
            tokens.append(Token("NEWLINE", "\n", content[0].line))

        # Emit INDENT/DEDENT.
        current = indent_stack[-1]
        if indent > current:
            indent_stack.append(indent)
            tokens.append(Token("INDENT", "", content[0].line))
        elif indent < current:
            while indent < indent_stack[-1]:
                indent_stack.pop()
                tokens.append(Token("DEDENT", "", content[0].line))
            if indent != indent_stack[-1]:
                raise SyntaxError(
                    f"Line {content[0].line}: indentation mismatch "
                    f"(indent {indent}, expected {indent_stack[-1]})")

        tokens.extend(content)
        line_tokens = []
        line_start = True

    pos = 0
    while pos < len(source):
        ch = source[pos]

        # Handle newlines before regex — re.DOTALL would let . match \n,
        # but doing it explicitly is clearer and avoids side-effects.
        if ch == "\n":
            flush_line()
            line_no += 1
            line_start = True
            pos += 1
            continue
        elif ch == "\t":
            if line_start:
                line_tokens.append(Token("TAB", "\t", line_no))
            pos += 1
            continue

        m = _TOKEN_RE.match(source, pos)
        if m is None:
            pos += 1
            continue
        kind = m.lastgroup
        value = m.group()
        pos = m.end()

        if kind in ("SPACE", "COMMENT"):
            continue
        elif kind == "MISMATCH":
            raise SyntaxError(
                f"Unexpected character {value!r} at line {line_no}")
        else:
            line_tokens.append(Token(kind, value, line_no))
            line_start = False

    flush_line()
    while len(indent_stack) > 1:
        indent_stack.pop()
        tokens.append(Token("DEDENT", "", line_no))
    tokens.append(Token("NEWLINE", "\n", line_no))
    tokens.append(Token("EOF", "", line_no))
    return tokens


# ════════════════════════════════════════════════════════════════════════════
# Parser  (recursive-descent, returns AST as nested tuples)
# ════════════════════════════════════════════════════════════════════════════

class Parser:
    def __init__(self, tokens: List[Token]):
        self.tokens = tokens
        self.pos = 0
        self._skip_nl()

    def _skip_nl(self):
        while self.pos < len(self.tokens) and self.tokens[self.pos].kind == "NEWLINE":
            self.pos += 1

    def _peek(self) -> Token:
        return self.tokens[self.pos]

    def _check(self, kind: str) -> bool:
        return self._peek().kind == kind

    def _match(self, kind: str) -> Optional[Token]:
        if self._check(kind):
            t = self._peek()
            self.pos += 1
            self._skip_nl()
            return t
        return None

    def _expect(self, kind: str) -> Token:
        t = self._match(kind)
        if t is None:
            actual = self._peek()
            raise SyntaxError(
                f"Line {actual.line}: expected {kind}, "
                f"got {actual.kind}({actual.value!r})")
        return t

    # ── Top level ───────────────────────────────────────────────────────

    def parse_program(self) -> List:
        stmts = []
        while not self._check("EOF"):
            self._skip_nl()
            if self._check("EOF") or self._check("DEDENT"):
                break
            stmts.append(self._parse_stmt())
            self._skip_nl()
        return stmts

    # ── Block parsing ───────────────────────────────────────────────────

    def _parse_block(self) -> List:
        """INDENT stmt* DEDENT"""
        stmts: List = []
        self._expect("INDENT")
        while not self._check("DEDENT") and not self._check("EOF"):
            self._skip_nl()
            if self._check("DEDENT") or self._check("EOF"):
                break
            stmts.append(self._parse_stmt())
            self._skip_nl()
        self._expect("DEDENT")
        return stmts

    # ── Statements ──────────────────────────────────────────────────────

    def _parse_stmt(self) -> Tuple:
        t = self._peek()
        kind = t.kind

        if kind == "DEFINE":
            return self._parse_funcdef()
        elif kind == "IF":
            return self._parse_if()
        elif kind == "WHILE":
            return self._parse_while()
        elif kind == "FOR":
            return self._parse_for()
        elif kind == "RETURN":
            self._match("RETURN")
            expr = self._parse_expr()
            return ("return", expr)
        elif kind == "BREAK":
            self._match("BREAK")
            return ("break",)
        elif kind == "CONTINUE":
            self._match("CONTINUE")
            return ("continue",)
        elif kind == "PASS":
            self._match("PASS")
            return ("pass",)
        elif kind == "CLASS":
            return self._parse_classdef()
        elif kind == "IDENT":
            saved = self.pos
            self.pos += 1
            peek = self._peek()
            self.pos = saved
            if peek.kind == "OP" and peek.value == "=":
                return self._parse_assign()
            else:
                return ("expr_stmt", self._parse_expr())
        elif kind in ("INT", "FLOAT", "STRING", "BOOL", "NULL",
                       "LPAREN", "OP", "NOT", "PRINT"):
            return ("expr_stmt", self._parse_expr())
        else:
            raise SyntaxError(
                f"Line {t.line}: unexpected token "
                f"{kind}({t.value!r}) in statement")

    def _parse_assign(self) -> Tuple:
        name = self._expect("IDENT").value
        self._expect("OP")  # '='
        expr = self._parse_expr()
        return ("assign", name, expr)

    def _parse_funcdef(self) -> Tuple:
        self._match("DEFINE")
        name = self._expect("IDENT").value
        self._expect("OP")  # '('
        params: List[str] = []
        if not (self._check("OP") and self._peek().value == ")"):
            while True:
                pname = self._expect("IDENT").value
                params.append(pname)
                if self._check("OP") and self._peek().value == ":":
                    self._match("OP")
                    self._match("IDENT")  # type annotation
                if self._check("OP") and self._peek().value == ",":
                    self._match("OP")
                else:
                    break
        self._expect("OP")  # ')'
        if self._check("OP") and self._peek().value == "->":
            self._match("OP")
            self._match("IDENT")  # return type
        self._expect("OP")  # ':'
        body = self._parse_block()
        return ("funcdef", name, params, body)

    def _parse_classdef(self) -> Tuple:
        self._match("CLASS")
        name = self._expect("IDENT").value
        if self._check("IDENT") and self._peek().value == "extends":
            self._match("IDENT")
            self._match("IDENT")
        self._expect("OP")  # ':'
        body = self._parse_block()
        return ("classdef", name, body)

    def _parse_if(self) -> Tuple:
        self._match("IF")
        cond = self._parse_expr()
        self._expect("OP")  # ':'
        body = self._parse_block()
        elifs = []
        self._skip_nl()
        while self._check("ELSIF"):
            self._match("ELSIF")
            elif_cond = self._parse_expr()
            self._expect("OP")
            elif_body = self._parse_block()
            elifs.append(("elsif", elif_cond, elif_body))
            self._skip_nl()
        else_body = []
        self._skip_nl()
        if self._check("ELSE"):
            self._match("ELSE")
            self._expect("OP")  # ':'
            else_body = self._parse_block()
        return ("if", cond, body, elifs, else_body)

    def _parse_while(self) -> Tuple:
        self._match("WHILE")
        cond = self._parse_expr()
        self._expect("OP")  # ':'
        body = self._parse_block()
        return ("while", cond, body)

    def _parse_for(self) -> Tuple:
        self._match("FOR")
        var = self._expect("IDENT").value
        self._match("IN")
        self._match("RANGE")
        self._expect("OP")  # '('
        lo = self._parse_expr()
        hi = None
        if self._check("OP") and self._peek().value == ",":
            self._match("OP")
            hi = self._parse_expr()
        self._expect("OP")  # ')'
        self._expect("OP")  # ':'
        body = self._parse_block()
        return ("for", var, lo, hi, body)

    # ── Expressions (precedence climbing) ────────────────────────────────

    def _parse_expr(self) -> Any:
        return self._parse_or()

    def _parse_or(self) -> Any:
        left = self._parse_and()
        while self._check("OR"):
            self._match("OR")
            right = self._parse_and()
            left = ("or", left, right)
        return left

    def _parse_and(self) -> Any:
        left = self._parse_not()
        while self._check("AND"):
            self._match("AND")
            right = self._parse_not()
            left = ("and", left, right)
        return left

    def _parse_not(self) -> Any:
        if self._check("NOT"):
            self._match("NOT")
            return ("not", self._parse_not())
        return self._parse_compare()

    def _parse_compare(self) -> Any:
        left = self._parse_add()
        if self._check("OP") and self._peek().value in (
                "==", "!=", "<", ">", "<=", ">="):
            op = self._match("OP").value
            right = self._parse_add()
            return (op, left, right)
        return left

    def _parse_add(self) -> Any:
        left = self._parse_mul()
        while self._check("OP") and self._peek().value in ("+", "-"):
            op = self._match("OP").value
            right = self._parse_mul()
            left = (op, left, right)
        return left

    def _parse_mul(self) -> Any:
        left = self._parse_unary()
        while self._check("OP") and self._peek().value in ("*", "//", "%", "**"):
            op = self._match("OP").value
            right = self._parse_unary()
            left = (op, left, right)
        return left

    def _parse_unary(self) -> Any:
        if self._check("OP") and self._peek().value == "-":
            self._match("OP")
            return ("neg", self._parse_unary())
        return self._parse_call()

    def _parse_call(self) -> Any:
        expr = self._parse_atom()
        if self._check("OP") and self._peek().value == "(":
            self._match("OP")
            args: List = []
            if not (self._check("OP") and self._peek().value == ")"):
                while True:
                    args.append(self._parse_expr())
                    if self._check("OP") and self._peek().value == ",":
                        self._match("OP")
                    else:
                        break
            self._expect("OP")  # ')'
            return ("call", expr, args)
        return expr

    def _parse_atom(self) -> Any:
        t = self._peek()
        if t.kind == "INT":
            self._match("INT")
            return ("int", int(t.value))
        elif t.kind == "FLOAT":
            self._match("FLOAT")
            return ("float", float(t.value))
        elif t.kind == "STRING":
            self._match("STRING")
            return ("string", t.value[1:-1])
        elif t.kind == "BOOL":
            self._match("BOOL")
            return ("bool", t.value == "true")
        elif t.kind == "NULL":
            self._match("NULL")
            return ("null",)
        elif t.kind == "IDENT":
            self._match("IDENT")
            return ("ident", t.value)
        elif t.kind == "PRINT":
            self._match("PRINT")
            self._expect("OP")  # '('
            args: List = []
            if not (self._check("OP") and self._peek().value == ")"):
                while True:
                    args.append(self._parse_expr())
                    if self._check("OP") and self._peek().value == ",":
                        self._match("OP")
                    else:
                        break
            self._expect("OP")  # ')'
            return ("call", ("ident", "print"), args)
        elif t.kind == "OP" and t.value == "(":
            self._match("OP")
            expr = self._parse_expr()
            self._expect("OP")  # ')'
            return expr
        else:
            raise SyntaxError(
                f"Line {t.line}: unexpected token "
                f"{t.kind}({t.value!r}) in expression")


# ════════════════════════════════════════════════════════════════════════════
# Interpreter
# ════════════════════════════════════════════════════════════════════════════

Value = Union[int, float, str, bool, None]


class ReturnSignal(Exception):
    def __init__(self, value: Value):
        self.value = value


class BreakSignal(Exception):
    pass


class ContinueSignal(Exception):
    pass


class InterpreterError(Exception):
    pass


class Interpreter:
    def __init__(self, source: str):
        self.source = source
        self.output_lines: List[str] = []
        self.globals: Dict[str, Any] = {}
        self.functions: Dict[str, Tuple[List[str], List]] = {}

    def run(self) -> str:
        tokens = tokenize(self.source)
        parser = Parser(tokens)
        stmts = parser.parse_program()
        self._exec_stmts(stmts, {})
        return "\n".join(self.output_lines)

    # ── Statement execution ─────────────────────────────────────────────

    def _exec_stmts(self, stmts: List, env: Dict[str, Value]) -> None:
        for stmt in stmts:
            try:
                self._exec_stmt(stmt, env)
            except ReturnSignal:
                raise
            except (BreakSignal, ContinueSignal):
                raise
            except InterpreterError:
                raise
            except Exception as e:
                raise InterpreterError(f"{type(e).__name__}: {e}") from e

    def _exec_stmt(self, stmt: Tuple, env: Dict[str, Value]) -> None:
        if not isinstance(stmt, tuple):
            return
        kind = stmt[0]

        if kind == "assign":
            _, name, expr = stmt
            value = self._eval(expr, env)
            env[name] = value
            if name not in self.globals and env is not self.globals:
                pass  # local scope

        elif kind == "expr_stmt":
            self._eval(stmt[1], env)

        elif kind == "if":
            _, cond, body, elifs, else_body = stmt
            if self._truthy(self._eval(cond, env)):
                self._exec_stmts(body, env)
            else:
                executed = False
                for _, elif_cond, elif_body in elifs:
                    if self._truthy(self._eval(elif_cond, env)):
                        self._exec_stmts(elif_body, env)
                        executed = True
                        break
                if not executed and else_body:
                    self._exec_stmts(else_body, env)

        elif kind == "while":
            _, cond, body = stmt
            while self._truthy(self._eval(cond, env)):
                try:
                    self._exec_stmts(body, env)
                except BreakSignal:
                    break
                except ContinueSignal:
                    continue

        elif kind == "for":
            _, var, lo, hi, body = stmt
            start_val = self._eval(lo, env)
            if not isinstance(start_val, int):
                raise InterpreterError("for loop range requires integer")
            if hi is not None:
                end_val = self._eval(hi, env)
                if not isinstance(end_val, int):
                    raise InterpreterError("for loop range requires integer")
                iter_range = range(start_val, end_val)
            else:
                iter_range = range(start_val)
            for i in iter_range:
                env[var] = i
                try:
                    self._exec_stmts(body, env)
                except BreakSignal:
                    break
                except ContinueSignal:
                    continue

        elif kind == "funcdef":
            _, name, params, body = stmt
            self.functions[name] = (params, body)

        elif kind == "classdef":
            pass  # Skip class definitions

        elif kind == "return":
            raise ReturnSignal(self._eval(stmt[1], env))

        elif kind == "break":
            raise BreakSignal()

        elif kind == "continue":
            raise ContinueSignal()

        elif kind == "pass":
            pass

    # ── Expression evaluation ────────────────────────────────────────────

    def _eval(self, expr: Any, env: Dict[str, Value]) -> Value:
        if not isinstance(expr, tuple):
            if isinstance(expr, str):
                if expr in env:
                    return env[expr]
                if expr in self.globals:
                    return self.globals[expr]
                raise InterpreterError(f"undefined variable: {expr}")
            return expr

        kind = expr[0]

        if kind == "int":
            return expr[1]
        elif kind == "float":
            return expr[1]
        elif kind == "string":
            return expr[1]
        elif kind == "bool":
            return expr[1]
        elif kind == "null":
            return None
        elif kind == "ident":
            name = expr[1]
            if name in env:
                return env[name]
            if name in self.globals:
                return self.globals[name]
            raise InterpreterError(f"undefined variable: {name}")

        elif kind in ("+", "-", "*", "//", "%", "**",
                       "==", "!=", "<", ">", "<=", ">="):
            left = self._eval(expr[1], env)
            right = self._eval(expr[2], env)
            return self._apply_binary(kind, left, right)

        elif kind == "and":
            left = self._eval(expr[1], env)
            if not self._truthy(left):
                return left
            return self._eval(expr[2], env)

        elif kind == "or":
            left = self._eval(expr[1], env)
            if self._truthy(left):
                return left
            return self._eval(expr[2], env)

        elif kind == "not":
            return not self._truthy(self._eval(expr[1], env))

        elif kind == "neg":
            val = self._eval(expr[1], env)
            if not isinstance(val, (int, float)):
                raise InterpreterError(f"cannot negate {type(val).__name__}")
            return -val

        elif kind == "call":
            _, callee, args = expr
            arg_vals = [self._eval(a, env) for a in args]

            # Resolve callee
            if isinstance(callee, tuple) and callee[0] == "ident":
                func_name = callee[1]
                if func_name == "print":
                    parts = [self._to_str(a) for a in arg_vals]
                    self.output_lines.append(" ".join(parts))
                    return None
                elif func_name in self.functions:
                    params, body = self.functions[func_name]
                    frame: Dict[str, Value] = {}
                    # Copy globals into frame
                    for k, v in self.globals.items():
                        frame[k] = v
                    for pname, pval in zip(params, arg_vals):
                        frame[pname] = pval
                    try:
                        self._exec_stmts(body, frame)
                    except ReturnSignal as ret:
                        return ret.value
                    return None
                else:
                    raise InterpreterError(f"undefined function: {func_name}")
            elif isinstance(callee, str):
                func_name = callee
                if func_name == "print":
                    parts = [self._to_str(a) for a in arg_vals]
                    self.output_lines.append(" ".join(parts))
                    return None
                elif func_name in self.functions:
                    params, body = self.functions[func_name]
                    frame: Dict[str, Value] = {}
                    for k, v in self.globals.items():
                        frame[k] = v
                    for pname, pval in zip(params, arg_vals):
                        frame[pname] = pval
                    try:
                        self._exec_stmts(body, frame)
                    except ReturnSignal as ret:
                        return ret.value
                    return None
                else:
                    raise InterpreterError(f"undefined function: {func_name}")

            raise InterpreterError(f"cannot call {callee}")

        else:
            raise InterpreterError(f"unknown expression: {kind}")

    # ── Binary operators ─────────────────────────────────────────────────

    def _apply_binary(self, op: str, left: Value, right: Value) -> Value:
        if op == "+":
            if isinstance(left, str) or isinstance(right, str):
                return self._to_str(left) + self._to_str(right)
            self._check_numeric(left, right, "+")
            return left + right  # type: ignore[operator]
        elif op == "-":
            self._check_numeric(left, right, "-")
            return left - right  # type: ignore[operator]
        elif op == "*":
            self._check_numeric(left, right, "*")
            return left * right  # type: ignore[operator]
        elif op == "//":
            self._check_numeric(left, right, "//")
            if right == 0:
                raise InterpreterError("division by zero")
            return int(left) // int(right)
        elif op == "%":
            self._check_numeric(left, right, "%")
            if right == 0:
                raise InterpreterError("modulo by zero")
            return int(left) % int(right)
        elif op == "**":
            self._check_numeric(left, right, "**")
            return left ** right  # type: ignore[operator]
        elif op == "==":
            return left == right
        elif op == "!=":
            return left != right
        elif op in ("<", ">", "<=", ">="):
            self._check_numeric(left, right, op)
            if op == "<":
                return left < right  # type: ignore[operator]
            elif op == ">":
                return left > right  # type: ignore[operator]
            elif op == "<=":
                return left <= right  # type: ignore[operator]
            elif op == ">=":
                return left >= right  # type: ignore[operator]
        raise InterpreterError(f"unknown operator: {op}")

    # ── Helpers ──────────────────────────────────────────────────────────

    def _truthy(self, val: Value) -> bool:
        if val is None:
            return False
        if isinstance(val, bool):
            return val
        if isinstance(val, int):
            return val != 0
        if isinstance(val, float):
            return val != 0.0
        if isinstance(val, str):
            return val != ""
        return bool(val)

    def _to_str(self, val: Value) -> str:
        if val is None:
            return "null"
        if isinstance(val, bool):
            return "true" if val else "false"
        if isinstance(val, float):
            if val == int(val) and abs(val) < 1e15:
                return f"{int(val)}.0"
            return f"{val}"
        return str(val)

    def _check_numeric(self, left: Value, right: Value, op: str) -> None:
        if not isinstance(left, (int, float)) or not isinstance(right, (int, float)):
            raise InterpreterError(
                f"non-numeric operand for {op}: "
                f"{type(left).__name__}, {type(right).__name__}")


# ════════════════════════════════════════════════════════════════════════════
# CLI
# ════════════════════════════════════════════════════════════════════════════

def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print("Usage: python interpreter.py <file.vsl>")
        print("       echo 'source' | python interpreter.py -")
        sys.exit(0)

    if sys.argv[1] == "-":
        source = sys.stdin.read()
    else:
        with open(sys.argv[1], "r", encoding="utf-8") as f:
            source = f.read()

    try:
        interp = Interpreter(source)
        output = interp.run()
        sys.stdout.write(output)
        if output and not output.endswith("\n"):
            sys.stdout.write("\n")
    except Exception as e:
        print(f"<interpreter error: {e}>", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
