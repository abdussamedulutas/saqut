# SQ-090-CLI-BASELINE — Provenance

## 1. Git Branch
```
0.9.0
```

## 2. Git HEAD
```
7f871b75e917725dcdf46111fab88fb3be5663f2
```

## 3. Git Status (full)
```
 D .claude/agents/architect.md
 D .claude/agents/coder.md
 D .claude/agents/project-manager.md
 D .claude/agents/tester.md
 D .claude/hooks/role-guard.sh
 D .claude/settings.json
 M .gitignore
 M CLAUDE.md
 M docs/adr/ADR-032-mir-jit-gomulu-runtime-aot.md
 M docs/adr/ADR-038-determinizm-surum-uyumlulugu.md
 M docs/adr/ADR-041-self-hosted-stdlib-thin-runtime.md
 M docs/architecture.md
 D organization.md
 D screenshots/.directory
 D screenshots/dap.png
 D screenshots/lsp.png
 D target.txt
 D wiki/arrays.md
 D wiki/cli-commands.md
 D wiki/compound-assignment.md
 D wiki/control-flow.md
 D wiki/error-handling.md
 D wiki/functions.md
 D wiki/getting-started.md
 D wiki/globals.md
 D wiki/home.md
 D wiki/literals.md
 D wiki/operators.md
 D wiki/optimization.md
 D wiki/pipeline.md
 D wiki/strings.md
 D wiki/structs.md
 D wiki/variables-types.md
?? AGENTS.md
?? docs/adr/ADR-042-v1-feedback-mvp-ve-surumleme.md
?? docs/v0.9-v1.0-yol-haritasi.md
?? docs/v1.0-issue-disposition.md
?? docs/v1.0-kapsam-bildirgesi.md
?? docs/yerel-agent-kullanim-rehberi.md
?? knowledge-base/
?? prompts/
?? tasks/
?? tests/general/
```

## 4. Critical source path drift from HEAD

`git diff --stat HEAD -- CMakeLists.txt cmake/ src/ build-release.sh build-debug.sh`: **No output** (no changes).

`git status --short -- CMakeLists.txt cmake/ src/ build-release.sh build-debug.sh`: **No output** (no changes).

**Conclusion:** `CMakeLists.txt`, `cmake/`, `src/`, `build-release.sh`, `build-debug.sh` are identical to HEAD commit `7f871b75e917725dcdf46111fab88fb3be5663f2`. No unstaged or staged modifications exist on these paths.

## 5. Toolchain

### g++ --version
```
g++ (GCC) 16.1.1 20260625
Copyright (C) 2026 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```

### cmake --version
```
cmake version 4.3.4

CMake suite maintained and supported by Kitware (kitware.com/cmake).
```

### ninja --version
```
1.13.2
```

### uname -a
```
Linux ManjaroMonsterAbra 7.1.3-1-MANJARO #1 SMP PREEMPT_DYNAMIC Sat, 04 Jul 2026 20:54:12 +0000 x86_64 GNU/Linux
```

## 6. Binary path (updated after §2 build)

### Exact path
```
/tmp/saqut-sq090-baseline-4WbBYH/saqut
```

### Binary info
```
-rwxr-xr-x 1 saqut saqut 5498984 Tem 25 13:02 /tmp/saqut-sq090-baseline-4WbBYH/saqut
```

### Timestamp (epoch seconds)
1784973763

### Current time (epoch seconds, at measurement)
1784973769

### MD5 hash
```
475cfe6be7dcff69cc1784d96bfc0562  /tmp/saqut-sq090-baseline-4WbBYH/saqut
```

### Fresh build verification

- Binary timestamp (1784973763) is recent and from this session.
- Existing `build/saqut` at repo root has a different path and different hash:
  - Path: `/home/saqut/Masaüstü/saqutcompiler/build/saqut`
  - MD5: `4d3082ba378a8511777dc9e648b9532e` (different hash)
  - Timestamp: Tem 18 20:04 (older, different day)
- **Conclusion:** Binary is a fresh build from the current HEAD. No stale binary was used.
