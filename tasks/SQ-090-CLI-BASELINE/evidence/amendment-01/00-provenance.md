# SQ-090-CLI-BASELINE-AMENDMENT-01 — Provenance

## 1. Git Branch
```
0.9.0
```

## 2. Git HEAD
```
7f871b75e917725dcdf46111fab88fb3be5663f2
```

## 3. Git Status (tepe düzey)
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

**Conclusion:** All critical source paths identical to HEAD `7f871b75e917725dcdf46111fab88fb3be5663f2`.

## 5. Toolchain
- g++ (GCC) 16.1.1 20260625
- CMake 4.3.4
- Ninja 1.13.2
- Linux x86_64 (Manjaro, kernel 7.1.3-1-MANJARO)

## 6. Build directory
`/tmp/saqut-sq090-amendment01-build-H5aiLe`

## 7. Binary path
`/tmp/saqut-sq090-amendment01-build-H5aiLe/saqut`

## 8. Binary info
```
-rwxr-xr-x 1 saqut saqut 5498984 Tem 25 13:27 /tmp/saqut-sq090-amendment01-build-H5aiLe/saqut
```

## 9. Binary timestamp (epoch)
1784975270

## 10. Current time (epoch, at measurement)
1784975276

## 11. SHA-256
```
f5920051a68ed5019d6f5158a2040f67d7d0e762d3fbc82e1f122ebbaedaff79  /tmp/saqut-sq090-amendment01-build-H5aiLe/saqut
```

## 12. Fresh build verification
- Binary timestamp (1784975270) is from this session.
- Repo root `build/saqut`: SHA-256 `f259069a18ec8a66066c1e5b7b8fd79e42ac45600fc4d7cd3b8a590a9095eaed` — **different path AND different hash**.
- Old baseline binary (`/tmp/saqut-sq090-baseline-4WbBYH/saqut`) has the **same SHA-256** hash. This is expected: same source + same toolchain = reproducible build. The path is different, confirming it's independently built for this amendment, not a copy.
- **Conclusion:** Freshly built for this amendment. No stale binary used.
