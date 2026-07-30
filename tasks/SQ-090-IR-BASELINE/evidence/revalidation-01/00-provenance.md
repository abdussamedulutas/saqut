## branch
0.9.0
## HEAD
eabc01ae44c04c211edae90ca69a6e10c4b95a97
## status --short (full)
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
 M src/cli/commands/check.hpp
 M src/cli/commands/ir.hpp
 M src/cli/commands/run.hpp
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
?? .codewhale/
?? AGENTS.md
?? docs/adr/ADR-042-v1-feedback-mvp-ve-surumleme.md
?? docs/v0.9-v1.0-yol-haritasi.md
?? docs/v1.0-issue-disposition.md
?? docs/v1.0-kapsam-bildirgesi.md
?? docs/yerel-agent-kullanim-rehberi.md
?? knowledge-base/
?? prompts/
?? src/cli/exit_codes.hpp
?? stoponword/
?? tasks/SQ-090-IR-BASELINE/evidence/revalidation-01/
?? tests/general/
## diff --stat HEAD -- five paths
 src/cli/commands/check.hpp | 3 ++-
 src/cli/commands/ir.hpp    | 3 ++-
 src/cli/commands/run.hpp   | 9 +++++----
 3 files changed, 9 insertions(+), 6 deletions(-)
## status --short -- five paths
 M src/cli/commands/check.hpp
 M src/cli/commands/ir.hpp
 M src/cli/commands/run.hpp
?? src/cli/exit_codes.hpp
?? tests/general/
## diff HEAD -- five paths | sha256sum
1ed62153aa730fb8030915fe5d34204204ef9c8b9fb888cc46fafe904dbd2eb9  -
## sha256 of 4 active-source files
85df0e518f8febd2ca73de8ce1cd31d55625525c400e304bdbcbcf9a0ed6acb3  /home/saqut/Masaüstü/saqutcompiler/src/cli/commands/run.hpp
0f59a3e10a4e8f4db5111545425ffe24ec98e23b19eb7e9c4029e4e907b47ddf  /home/saqut/Masaüstü/saqutcompiler/src/cli/commands/check.hpp
8506fceaff34f227ee0765861fc03fddf167c4d89786a352e89d7d9f9ece02ce  /home/saqut/Masaüstü/saqutcompiler/src/cli/commands/ir.hpp
c67dd60af429f0fe96fc4d1fb91bfe7a93620f8cc19760a4a71167ef4166824b  /home/saqut/Masaüstü/saqutcompiler/src/cli/exit_codes.hpp
## toolchain
cmake version 4.3.4

CMake suite maintained and supported by Kitware (kitware.com/cmake).
g++ (GCC) 16.1.1 20260625
Copyright © 2026 Free Software Foundation, Inc.
Bu bir serbest yazılımdır; kopyalama koşulları için kaynak koduna bakınız.
Hiçbir garantisi yoktur; hatta SATILABiLiRLiĞi veya ŞAHSi KULLANIMINIZA
UYGUNLUĞU için bile garanti verilmez.
ctest version 4.3.4

CMake suite maintained and supported by Kitware (kitware.com/cmake).
