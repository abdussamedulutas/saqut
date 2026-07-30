# saQut Performance Benchmarks
## 1. DCE (Dead Code Elimination)
| Test | Mode | Time |
|---|---|---|
DCE|no-opt|s
DCE|opt(--optimized)|s

## 2. Constant Folding
| Test | Mode | Time |
|---|---|---|
ConstFold|no-opt|s
ConstFold|opt(--optimized)|s

## 3. Algorithmic: string concat vs. push
| Test | Mode | Time |
|---|---|---|
String concat (KOTU)|no-opt|s
String[] push (IYI)|no-opt|s

## 4. Loop Invariant: inside vs outside
| Test | Mode | Time |
|---|---|---|
Loop invariant (KOTU)|no-opt|s
Loop invariant (IYI)|no-opt|s

## 5. Algoritmik: O(n) vs O(n²)
| Test | Mode | Time |
|---|---|---|
O(n^2) reverse-all (KOTU)|no-opt|s
O(n) push-only (IYI)|no-opt|s

## 6. Crypto 64KB
| Test | Mode | Time |
|---|---|---|
| Crypto 64KB | no-opt | s |
| Crypto 64KB | opt | s |
