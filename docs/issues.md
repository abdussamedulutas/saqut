# Complete saQut Compiler Issue List (English)

Copy each issue's title and body directly into Gitea.
Issues are ordered by stage, from most urgent to long-term.

---

## Aşama 0: Metadata and Location Tracking

### Issue 0.1 — SourceFile and SourceLocation implementation

**Title:** Aşama 0.1 — Implement SourceFile and SourceLocation classes

**Body:**

**Goal:** Create the foundational metadata system so every token and AST node knows its exact origin (file, line, column, offset).

**Files to create/modify:**
- `src/core/location.hpp` — new file
- `src/core/sourcefile.hpp` — new file

**Requirements:**
- `SourceLocation` struct with fields: `filePath` (string), `line` (int), `column` (int), `offset` (int).
- `SourceFile` class that stores the full source text and a precomputed vector of line-start offsets. Provides `offsetToLocation(int offset) -> SourceLocation` using binary search (O(log n)).
- `SourceFile` constructor takes file path and source text; computes line offsets in one pass (O(n)).
- Both classes live in the `src/core/` directory.

**Success criteria:**
- Given a source string and offset, `offsetToLocation` returns correct line and column.
- Binary search is used, not linear scan.

---

### Issue 0.2 — Add location tracking to Lexer

**Title:** Aşama 0.2 — Add line/column tracking to Lexer

**Body:**

**Goal:** Lexer updates current line and column on every `nextChar()` call.

**Files to modify:**
- `src/lexer/lexer.hpp`

**Requirements:**
- Add `int currentLine`, `int currentColumn` private fields to `Lexer`.
- On `nextChar()`: if character is `\n`, increment line and reset column; otherwise increment column.
- Add `SourceLocation getLocation()` method returning current position.
- Initialize line=1, column=1 in `setText()`.
- Modify `INumber` struct to include `SourceLocation startLoc` and `SourceLocation endLoc` (or keep start/end offsets and add location separately — prefer using `SourceLocation` fields).

**Success criteria:**
- After lexing any source, calling `getLocation()` returns correct line and column.
- `INumber` carries source location info.

---

### Issue 0.3 — Add SourceLocation to Token base class

**Title:** Aşama 0.3 — Add SourceLocation to all Token types

**Body:**

**Goal:** Every token produced by the Tokenizer carries its SourceLocation.

**Files to modify:**
- `src/tokenizer/token.hpp`

**Requirements:**
- Add `SourceLocation loc` field to the base `Token` class.
- Remove or deprecate `int start, int end` fields (replace with `loc` data).
- Update `StringToken`, `NumberToken`, `IdentifierToken` if they reference start/end directly.
- Ensure all token constructors initialize `loc`.

**Success criteria:**
- After tokenizing, every token has a valid SourceLocation.
- Old start/end offsets are derivable from SourceLocation if needed (but location is primary).

---

### Issue 0.4 — Add SourceLocation to ASTNode base class

**Title:** Aşama 0.4 — Add SourceLocation to all AST nodes

**Body:**

**Goal:** Every AST node knows its originating source location.

**Files to modify:**
- `src/parser/ast.hpp`

**Requirements:**
- Add `SourceLocation loc` field to `ASTNode` base class.
- Optionally add `SourceLocation endLoc` for range.
- Parser must set `loc` when creating AST nodes from tokens.
- Update `toJson()` and `log()` methods to include location info.

**Success criteria:**
- JSON output includes `"location": {"file": "...", "line": N, "column": M}` for every node.
- Log output shows location when available.

---

## Aşama 1: CLI and REPL Mode

### Issue 1.1 — Implement REPL mode with readline support

**Title:** Aşama 1.1 — Implement REPL mode (`saqut` without arguments)

**Body:**

**Goal:** Running `saqut` without arguments enters an interactive REPL loop.

**Files to create/modify:**
- `src/cli/repl.hpp` — new file
- `src/main.cpp` — modify to detect no-args and launch REPL

**Requirements:**
- REPL prompt: `> `
- Each line is parsed, evaluated, and the result printed.
- `.ast` command prints the AST of the last expression.
- `.tokens` command prints the token list of the last expression.
- `.symbols` command prints the current symbol table.
- `.exit` or `.quit` exits the REPL.
- Multi-line input support: when a block is started (`{`), keep reading until `}`.

**Success criteria:**
- `./saqut` launches REPL.
- `.ast`, `.tokens`, `.symbols` work correctly.
- Multi-line input accumulates until balanced braces.

---

### Issue 1.2 — Implement stdin mode (`saqut -`)

**Title:** Aşama 1.2 — Implement stdin reading mode

**Body:**

**Goal:** `saqut -` reads source code from standard input.

**Files to modify:**
- `src/cli/args.hpp`

**Requirements:**
- When `-` is passed as positional argument, read all stdin until EOF.
- Works with all commands: `saqut run -`, `saqut tokens -`, etc.
- Remove the current "TODO" stub and implement fully.

**Success criteria:**
- `echo "int main() { return 42; }" | ./saqut run -` works.
- `cat file.sqt | ./saqut tokens -` works.

---

### Issue 1.3 — Implement output file support for all commands

**Title:** Aşama 1.3 — Support `-o/--output` flag for all commands

**Body:**

**Goal:** All CLI commands respect `-o outputfile` to write results to a file instead of stdout.

**Files to modify:**
- `src/cli/commands/run.hpp`
- `src/cli/commands/tokens.hpp`
- `src/cli/commands/symbols.hpp`

**Requirements:**
- `cmdRun`, `cmdTokens`, `cmdSymbols` already partially support `-o`; ensure all do.
- If `-o` is provided, write output to the specified file; otherwise stdout.
- Handle file open errors gracefully.

**Success criteria:**
- `saqut tokens source.sqt -o tokens.txt` writes to file.
- `saqut symbols source.sqt --output=symbols.json` writes JSON to file.

---

## Aşama 2: AST — Memory Monster

### Issue 2.1 — Migrate to unique_ptr for AST-owned tokens

**Title:** Aşama 2.1 — Use std::unique_ptr for token ownership in AST

**Body:**

**Goal:** AST nodes own their tokens via `std::unique_ptr`, eliminating memory leaks.

**Files to modify:**
- `src/parser/ast.hpp`
- `src/parser/parser.hpp`
- `src/parser/token.hpp`

**Requirements:**
- `ParserToken::token` changes from `Token*` to `std::unique_ptr<Token>`.
- All AST nodes that store a `ParserToken` or `Token*` must use `std::unique_ptr`.
- Parser transfers ownership when creating nodes.
- Remove manual `delete` calls on tokens (they are now owned by AST nodes).

**Success criteria:**
- No memory leaks when parsing and deleting AST.
- Valgrind/ASan reports zero leaks on test cases.

---

### Issue 2.2 — Implement ASTNode::getSourceText()

**Title:** Aşama 2.2 — Add getSourceText() and getSourceRange() to ASTNode

**Body:**

**Goal:** Given any AST node, retrieve the exact source code substring it represents.

**Files to modify:**
- `src/parser/ast.hpp`
- `src/core/sourcefile.hpp`

**Requirements:**
- `ASTNode::getSourceText()` returns `std::string` — the original source code for this node.
- `ASTNode::getSourceRange()` returns `std::pair<SourceLocation, SourceLocation>` (start and end).
- Requires AST nodes to store a reference to the `SourceFile` (or the full source text).
- This powers future rich error messages with `^^^^` source highlighting.

**Success criteria:**
- For a BinaryExpression node representing `a + b`, `getSourceText()` returns `"a + b"`.
- For an IfStatement, returns the entire `if (...) { ... }` block text.

---

### Issue 2.3 — Implement Graphviz DOT format output

**Title:** Aşama 2.3 — Add --format=dot for AST visualization

**Body:**

**Goal:** Export the AST as a Graphviz DOT file for graphical visualization.

**Files to create/modify:**
- `src/format/dot.hpp` — new file
- `src/cli/commands/ast.hpp` — modify to support `--format=dot`

**Requirements:**
- Implement `astToDot(ASTNode*) -> std::string` function.
- Each node becomes a labeled box.
- Parent-child relationships become directed edges.
- Node labels show kind and name (e.g., `BinaryExpression +`).
- Output is valid DOT format, renderable by `dot -Tpng -o ast.png ast.dot`.

**Success criteria:**
- `saqut ast source.sqt --format=dot -o ast.dot` produces a valid DOT file.
- The resulting image shows a readable tree.

---

## Aşama 3: Symbol Table

### Issue 3.1 — Implement Symbol and SymbolTable classes

**Title:** Aşama 3.1 — Implement Symbol struct and SymbolTable class with nested scopes

**Body:**

**Goal:** Build a full symbol table with nested scope support.

**Files to create:**
- `src/symbol/symbol.hpp` — new file
- `src/symbol/symbol_table.hpp` — new file

**Requirements:**

`Symbol` struct:
- `name` (string)
- `kind` (enum: Variable, Function, Parameter, Type, Struct)
- `type` (Type* or string for now)
- `definitionLoc` (SourceLocation)
- `references` (vector of SourceLocation)
- `scope` (pointer to parent scope or scope level)
- `metadata` (optional map<string,string>)

`SymbolTable` class:
- Nested scope stack: `enterScope()`, `exitScope()`.
- `define(Symbol) -> bool` (returns false on duplicate in same scope).
- `resolve(name) -> Symbol*` (searches innermost to outermost).
- `addReference(name, location)` (appends to symbol's reference list).
- `getAllSymbols() -> vector<Symbol*>` (flat list of all symbols in all scopes).
- `toJson() -> string` for serialization.

**Success criteria:**
- Nested scopes work: variable in inner scope shadows outer.
- Duplicate definition in same scope returns false.
- resolve finds symbols across scope boundaries.

---

### Issue 3.2 — Implement SymbolCollector AST walker

**Title:** Aşama 3.2 — Implement SymbolCollector that populates SymbolTable from AST

**Body:**

**Goal:** Walk the AST and populate the SymbolTable with all definitions and references.

**Files to create/modify:**
- `src/symbol/symbol_collector.hpp` — new file
- Replace or refactor the simple `collectSymbolsRecursive` in `src/json.hpp`

**Requirements:**
- `SymbolCollector` class with method `collect(ASTNode* root, SymbolTable* table)`.
- Walks all AST node types (Program, FunctionDecl, VariableDecl, Block, etc.).
- Calls `table->define()` for declarations.
- Calls `table->addReference()` for identifier usages.
- Handles all AST node types currently in `ast.hpp` (19 types).
- Replaces the ad-hoc `SymbolEntry` vector with proper SymbolTable population.

**Success criteria:**
- After parsing `source.sqt`, SymbolTable contains all functions and variables.
- References are collected for each symbol.
- `saqut symbols source.sqt` shows the enriched data.

---

### Issue 3.3 — Semantic error: undefined variable

**Title:** Aşama 3.3 — Report "undefined variable" errors using SymbolTable

**Body:**

**Goal:** When a variable is used before definition, report a clear error with location.

**Files to modify:**
- `src/symbol/symbol_collector.hpp`
- `src/core/diagnostic.hpp` — new file (or extend existing error reporting)

**Requirements:**
- During symbol collection, when an identifier reference has no matching `resolve()`, emit a diagnostic.
- Diagnostic includes: error level, SourceLocation, message, optional hint.
- `"Variable 'x' is not defined. Did you mean 'xy'?"` if a close match exists (Levenshtein distance < 3).
- Diagnostic system supports multiple errors (don't stop at first).

**Success criteria:**
- `int main() { return x; }` reports: `Error: 'x' is not defined at line 1 column 19`.
- Typos suggest close matches.

---

### Issue 3.4 — Semantic error: duplicate definition

**Title:** Aşama 3.4 — Report "duplicate definition" errors

**Body:**

**Goal:** When a symbol is defined twice in the same scope, report an error.

**Files to modify:**
- `src/symbol/symbol_table.hpp`
- `src/symbol/symbol_collector.hpp`

**Requirements:**
- `SymbolTable::define()` returns false on duplicate; collector emits diagnostic.
- Error message: `"Function 'main' is already defined. Previous definition at line X."`
- Works for variables, functions, structs.

**Success criteria:**
- Two `int main()` definitions produce an error.
- Two `int x` in the same block produce an error.
- Shadowing in nested scopes is allowed (not an error).

---

## Aşama 4: Feature Toggle System

### Issue 4.1 — Implement CompilerConfig struct and flag parsing

**Title:** Aşama 4.1 — Implement CompilerConfig struct and --disable-* flags

**Body:**

**Goal:** Create a configuration system that controls language features at compile time.

**Files to create/modify:**
- `src/core/config.hpp` — new file
- `src/cli/args.hpp` — extend to parse feature flags

**Requirements:**

`CompilerConfig` struct with boolean fields:
- `enableWhile`, `enableFor`, `enableDoWhile`, `enableSwitch`
- `enableClass`, `enableInterface`, `enableEnum`
- `enableTernary`, `enablePostfix`, `enableUnary`
- `optConstantFolding`, `optDeadCodeElim`
- `outputFormat` (text/json/dot)
- `mode` (run/tokens/ast/symbols/compile/transpile)

CLI flags:
- `--disable-while` sets `enableWhile = false`
- `--disable-for` sets `enableFor = false`
- `--opt-all` enables all optimizations
- `--opt-none` disables all optimizations

**Success criteria:**
- `--disable-while` flag is parsed into CompilerConfig.
- Config is passed through to Tokenizer and Parser.

---

### Issue 4.2 — Implement keyword toggling in Tokenizer

**Title:** Aşama 4.2 — Disable keywords based on CompilerConfig

**Body:**

**Goal:** When a keyword is disabled in config, the Tokenizer treats it as an identifier.

**Files to modify:**
- `src/tokenizer/tokenizer.hpp`

**Requirements:**
- Tokenizer receives a `CompilerConfig` reference (or copy).
- Before matching keywords, check config flags.
- Disabled keywords are skipped in keyword matching; they fall through to identifier.
- Example: `--disable-while` means `while` becomes a regular identifier.

**Success criteria:**
- With `--disable-while`, `while (true) {}` tokenizes `while` as identifier.
- Parser then does not parse it as a while statement (falls through to expression).

---

### Issue 4.3 — Implement optimization pass interface

**Title:** Aşama 4.3 — Implement OptimizationPass interface and OptimizationManager

**Body:**

**Goal:** Create a framework for pluggable optimization passes.

**Files to create:**
- `src/opt/optimization_pass.hpp` — new file
- `src/opt/optimization_manager.hpp` — new file

**Requirements:**
- `OptimizationPass` abstract class with `run(ASTNode* root, SymbolTable* table) -> bool` method.
- `OptimizationManager` holds a list of passes, runs them in order based on CompilerConfig.
- Initially, two passes: `ConstantFoldingPass` and `DeadCodeEliminationPass` (empty implementations for now, will be filled in Aşama 6).
- `--skip-constant-folding` flag skips that pass.

**Success criteria:**
- OptimizationManager runs (even if passes are no-ops for now).
- Feature flags control which passes execute.

---

## Aşama 5: Backend — Execution

### Issue 5.1 — Strengthen IR with control flow and function opcodes

**Title:** Aşama 5.1 — Extend IR with control flow, function, and memory opcodes

**Body:**

**Goal:** IR must support control flow (branch, jump, compare), function calls, and memory operations before any backend can work.

**Files to modify:**
- `src/ir/ir.hpp`

**Requirements:**

New opcodes:
- Control flow: `cmp`, `br`, `br_eq`, `br_lt`, `br_gt`, `jmp`
- Function: `call`, `ret`, `param`
- Memory: `load`, `store`, `alloca`

Update `IROpData` if needed to support new parameter types (labels for jump targets, function indices).
Add a `label` field or a separate `IRLabel` structure for branch targets.

**Success criteria:**
- All opcodes are defined and documented.
- IR can represent a simple if-else and a while loop.
- IR can represent a function definition with parameters and return.

---

### Issue 5.2 — Implement C Transpile Backend

**Title:** Aşama 5.2 — Implement C transpile backend (`saqut transpile`)

**Body:**

**Goal:** Convert saQut IR (or AST) to compilable C source code.

**Files to create:**
- `src/backend/c_transpile.hpp` — new file

**Requirements:**
- Reads IR (or AST) and generates equivalent C code.
- Handles: variable declarations, binary expressions, if/for/while/do-while, function definitions, return.
- Generates readable C with reasonable indentation.
- Embed `#line` directives so GCC/Clang error messages point to original `.sqt` files.
- `saqut transpile source.sqt -o output.c` command.

**Success criteria:**
- Given `int main() { return 42; }`, generates compilable C code that returns 42.
- Generated C compiles with `gcc -Wall -Werror` without warnings.
- `saqut compile source.sqt output:prog` compiles and runs correctly.

---

### Issue 5.3 — Implement Interpreter (Tree-walk VM)

**Title:** Aşama 5.3 — Implement interpreter VM (`saqut run` execution)

**Body:**

**Goal:** Execute saQut programs by walking the AST or interpreting IR directly.

**Files to create:**
- `src/backend/interpreter.hpp` — new file

**Requirements:**
- `Interpreter` class that walks AST and executes.
- Supports: variable declaration and assignment, binary/unary expressions, if/else, while/for/do-while, break/continue, return, function calls (after function parameters are implemented).
- Stack-based or register-based value storage.
- `saqut run source.sqt` executes and prints program result.
- REPL mode (from Issue 1.1) uses the interpreter for evaluation.

**Success criteria:**
- `saqut run source.sqt` executes correctly for arithmetic and control flow.
- Variables hold values across statements.
- Return value propagates correctly.

---

## Aşama 6: Optimization

### Issue 6.1 — Implement Constant Folding pass

**Title:** Aşama 6.1 — Implement Constant Folding optimization

**Body:**

**Goal:** Evaluate constant expressions at compile time.

**Files to create:**
- `src/opt/constant_folding.hpp` — new file

**Requirements:**
- Walk AST, find `BinaryExpression` nodes where both operands are Literals.
- Compute the result, replace the subtree with a single Literal node.
- Handles: `+`, `-`, `*`, `/`, `%` for integers and floats.
- Handles unary `-` on literals.
- Guard against division by zero (emit warning, skip folding).

**Success criteria:**
- `4 + 5` becomes `9` in the optimized AST.
- `x + 0` is NOT folded (x is not constant).
- `1 / 0` emits warning, AST unchanged.

---

### Issue 6.2 — Implement Dead Code Elimination pass

**Title:** Aşama 6.2 — Implement Dead Code Elimination

**Body:**

**Goal:** Remove code that is provably unreachable.

**Files to create:**
- `src/opt/dead_code_elim.hpp` — new file

**Requirements:**
- Remove statements after `return`, `break`, `continue` within the same block.
- Remove `if (false)` branches.
- Remove `while (false)` bodies.
- Remove unused variable declarations (requires SymbolTable reference counting).

**Success criteria:**
- `return; x = 5;` — assignment is removed.
- `if (false) { ... }` — entire block removed.
- Unused variable `int y = 10;` removed when y has zero references.

---

### Issue 6.3 — Implement Null Check and Type Check Elimination

**Title:** Aşama 6.3 — Implement Null/Type Check Elimination

**Body:**

**Goal:** Remove redundant null checks and type checks when the compiler can prove they are unnecessary.

**Files to create:**
- `src/opt/null_check_elim.hpp` — new file
- `src/opt/type_check_elim.hpp` — new file

**Requirements:**
- Track which variables have been checked for null in the current path.
- If a variable was already null-checked (and not reassigned), skip subsequent checks.
- Similarly for type checks (`is` expressions).
- Requires dataflow analysis within a function body.

**Success criteria:**
- Two consecutive `if (x != null)` — second check eliminated.
- `if (x is int) { ... if (x is int) { ... } }` — inner check eliminated.

---

## Aşama 7: Test and Performance

### Issue 7.1 — Set up unit test framework (Google Test)

**Title:** Aşama 7.1 — Set up Google Test framework and write initial tests

**Body:**

**Goal:** Create a proper testing infrastructure.

**Files to create/modify:**
- `tests/` directory
- `tests/lexer_test.cpp`
- `tests/tokenizer_test.cpp`
- `tests/parser_test.cpp`
- `tests/symbol_test.cpp`
- `CMakeLists.txt` or `Makefile` with test target

**Requirements:**
- Use Google Test (download during build or include as submodule).
- Initial tests: lexing numbers, tokenizing keywords, parsing simple expressions, symbol collection.
- `make test` or `cmake --build . --target test` runs all tests.

**Success criteria:**
- At least 10 passing tests.
- CI-ready: tests can be run from command line.
- Failures show expected vs actual.

---

### Issue 7.2 — Write snapshot tests for AST output

**Title:** Aşama 7.2 — Snapshot testing for AST/IR/symbol output

**Body:**

**Goal:** Ensure compiler output is stable across changes.

**Files to create:**
- `tests/snapshots/` directory
- Script or C++ test that runs `saqut ast` and compares to stored JSON.

**Requirements:**
- Store known-good JSON output for `source.sqt`, `Final.sqt`.
- Test compares current output to snapshot; fails on difference.
- Snapshot update mode to regenerate expected files.

**Success criteria:**
- Changes to parser that affect AST structure are caught.
- False positives (formatting changes) are manageable.

---

### Issue 7.3 — Implement benchmark suite

**Title:** Aşama 7.3 — Implement benchmark infrastructure (`saqut bench`)

**Body:**

**Goal:** Measure compiler performance on large inputs.

**Files to create/modify:**
- `src/cli/commands/bench.hpp` — new file
- `benchmarks/` directory with test files

**Requirements:**
- `saqut bench` runs a set of benchmark files and reports parse time, token throughput, memory usage.
- Warm-up phase to reduce noise.
- Output in machine-readable format (JSON) for tracking over time.

**Success criteria:**
- Parse a 10K-line file and report tokens/second.
- Memory usage reported in KB/MB.

---

## Aşama 8: Advanced Type System

### Issue 8.1 — Implement Struct type (user-defined types)

**Title:** Aşama 8.1 — Full struct support: definition, instantiation, field access

**Body:**

**Goal:** Users can define and use `struct` types.

**Files to modify:**
- `src/parser/parser.hpp`
- `src/parser/ast.hpp`
- `src/symbol/`
- `src/backend/`

**Requirements:**
- `struct Point { int x; int y; }` defines a type.
- `Point p;` declares a variable of that type.
- `p.x` accesses a field (already partially supported via MemberAccess).
- Struct type checking: field must exist, type must match on assignment.
- C transpile and interpreter both support structs.

**Success criteria:**
- Struct definition, instantiation, and field access work end-to-end.
- Accessing nonexistent field produces clear error.

---

### Issue 8.2 — Implement Array and Pointer types

**Title:** Aşama 8.2 — Implement array and pointer type support

**Body:**

**Goal:** Support `int[]`, `int*`, array indexing, and pointer arithmetic.

**Files to modify:**
- `src/parser/parser.hpp`
- `src/parser/ast.hpp`
- `src/symbol/`
- `src/backend/`

**Requirements:**
- `int arr[10];` array declaration.
- `arr[i]` indexing (already partially supported).
- `int* p;` pointer declaration.
- `*p` dereference (unary `*` operator).
- `&x` address-of operator.
- Type checking for pointer/array operations.

**Success criteria:**
- Array declaration and indexing work.
- Pointer declaration, assignment, dereference work.
- Pointer arithmetic (`p + 1`) works.

---

### Issue 8.3 — Implement standard library foundation (lib/std.sqt)

**Title:** Aşama 8.3 — Create standard library with basic data structures

**Body:**

**Goal:** Provide built-in data structures: List, Map, Set, Buffer, String utilities.

**Files to create:**
- `lib/std.sqt` — standard library source file
- `lib/collections.sqt`, `lib/io.sqt`, `lib/encoding.sqt` — optional modules

**Requirements:**
- `List<T>` — dynamic array with `add`, `get`, `remove`, `size`.
- `Map<K,V>` — hash map with `put`, `get`, `contains`, `remove`.
- `Set<T>` — hash set.
- `Buffer` — byte buffer for binary data.
- `String` methods: `split`, `replace`, `substring`, `toUpper`, `toLower`.
- All implemented in saQut itself (or native functions exposed to saQut).

**Success criteria:**
- `import std;` makes these types available.
- Basic operations work without crashes.

---

## Aşama 9: Ecosystem

### Issue 9.1 — Implement project initialization (`saqut init`)

**Title:** Aşama 9.1 — Implement `saqut init` project scaffolding

**Body:**

**Goal:** `saqut init my-project` creates a standard project directory.

**Files to create/modify:**
- `src/cli/commands/init.hpp` — new file

**Requirements:**
- Creates directory with:
  - `project.saqut` manifest file (TOML or JSON format).
  - `src/` directory with `main.sqt`.
  - `.gitignore` file.
- Manifest contains: project name, version, description, author, dependencies (empty initially).

**Success criteria:**
- `saqut init testproj` creates the expected structure.
- `saqut run` inside the project directory finds and runs `src/main.sqt`.

---

### Issue 9.2 — Implement built-in test framework (`saqut test`)

**Title:** Aşama 9.2 — Implement built-in test framework

**Body:**

**Goal:** `saqut test` discovers and runs test functions in the project.

**Files to create/modify:**
- `src/cli/commands/test.hpp` — new file
- `lib/test.sqt` — test framework library

**Requirements:**
- Functions annotated with `#[test]` or named `test_*` are test functions.
- `saqut test` runs all tests, reports pass/fail.
- `assert(condition)` and `assert_eq(a, b)` built-in functions.
- Output in TAP or JUnit XML format for CI integration.

**Success criteria:**
- `saqut test` in a project with test functions runs them.
- Failures report file and line number.
- Exit code 0 for all pass, non-zero for any failure.

---

## Aşama 10: Package Manager

### Issue 10.1 — Implement package manager foundation (`saqut add`)

**Title:** Aşama 10.1 — Implement `saqut add` package manager

**Body:**

**Goal:** `saqut add <package>` downloads and installs a package dependency.

**Files to create/modify:**
- `src/cli/commands/add.hpp` — new file
- `src/package/registry.hpp` — new file
- `src/package/resolver.hpp` — new file

**Requirements:**
- Package registry: a central Git repository or simple HTTP server listing available packages.
- `saqut add json` adds the `json` package to `project.saqut` dependencies.
- Downloads package source into `packages/` directory (or a cache).
- `import json;` in source code finds the installed package.
- Semantic versioning support (major.minor.patch).

**Success criteria:**
- `saqut add` adds dependency to manifest.
- `import` of installed package works.
- Version constraints are enforced.

---

## Aşama 11: Language Specification

### Issue 11.1 — Write language specification document

**Title:** Aşama 11.1 — Write comprehensive language specification

**Body:**

**Goal:** Create `docs/lang_spec.md` that fully defines the saQut language.

**Files to create:**
- `docs/lang_spec.md`

**Requirements:**
- Syntax: full grammar in EBNF or similar notation.
- Type system: all built-in types, conversion rules, type inference.
- Control flow: semantics of if/else, for, while, do-while, break, continue, return.
- Memory model: stack vs heap, pointer rules, array layout.
- Standard library: function signatures and contracts for all `lib/std.sqt` functions.
- Error handling: exception-like or error-return semantics.

**Success criteria:**
- A developer can implement a saQut compiler from only this document.
- All implemented features are documented.
- Unimplemented features are clearly marked as "planned" or "future."

---

> **Total issues: 30**
>
> **Order:** Start from 0.1 and work sequentially. Each issue is a milestone toward
> the next. Dependencies are explicit: later stages require earlier stages complete.