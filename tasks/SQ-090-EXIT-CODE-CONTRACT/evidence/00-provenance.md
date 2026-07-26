# 00-provenance.md
## Build Provenance
- **Branch:** 0.9.0
- **HEAD:** 7f871b75e917725dcdf46111fab88fb3be5663f2
- **Build dir:** /tmp/saqut-sq090-exitcode-validation-2DHSAm
- **Binary:** /tmp/saqut-sq090-exitcode-validation-2DHSAm/saqut
- **Binary SHA-256:** 98c0acdedfb9eddac2c96c8b15aeadc37e977c725746c8bf8bd5a989580ee5a2
- **Binary size:** 5498984 bytes
- **Toolchain:** GNU 16.1.1 (GCC), CMake 4.3.4, Ninja 1.13.2
- **Build type:** Release
- **Timestamp:** 1784982835 (epoch)

## Changed files (from HEAD)
### git diff --stat HEAD -- src/cli/exit_codes.hpp src/cli/commands/run.hpp src/cli/commands/check.hpp src/cli/commands/ir.hpp
 src/cli/commands/check.hpp | 3 ++-
 src/cli/commands/ir.hpp    | 3 ++-
 src/cli/commands/run.hpp   | 9 +++++----
 3 files changed, 9 insertions(+), 6 deletions(-)

### git status --short -- same files
 M src/cli/commands/check.hpp
 M src/cli/commands/ir.hpp
 M src/cli/commands/run.hpp
?? src/cli/exit_codes.hpp
