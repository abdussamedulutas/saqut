#!/usr/bin/env bash
# #175 SQ-090-PROCESS-SYS-SURFACE — dynamic process host checks.
set -eu

binary=$1
tmp_root=$(mktemp -d "${TMPDIR:-/tmp}/saqut-process-surface.XXXXXX")
cleanup() {
    rm -rf "$tmp_root"
}
trap cleanup EXIT

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

run_prog() {
    local source=$1
    local stdout_file=$2
    local stderr_file=$3
    set +e
    "$binary" run --allow-sys "file:$source" >"$stdout_file" 2>"$stderr_file"
    RUN_RC=$?
    set -e
    return 0
}

cat >"$tmp_root/probe.sqt" <<'SQT'
import {pid, cwd, executable} from process;

void main() {
    print("PID=");
    print(pid());
    print("\nCWD=");
    print(cwd());
    print("\nEXE=");
    print(executable());
}
SQT

probe_out="$tmp_root/probe.out"
probe_err="$tmp_root/probe.err"
run_prog "$tmp_root/probe.sqt" "$probe_out" "$probe_err"
if [ "$RUN_RC" -ne 0 ]; then
    cat "$probe_err" >&2
    fail "pid/cwd/executable probe failed"
fi

pid_value=$(sed -n 's/^PID=//p' "$probe_out")
cwd_value=$(sed -n 's/^CWD=//p' "$probe_out")
exe_value=$(sed -n 's/^EXE=//p' "$probe_out")

case "$pid_value" in
    ''|*[!0-9]*) fail "pid is not numeric: '$pid_value'" ;;
esac
[ "$pid_value" -gt 0 ] || fail "pid is not positive: '$pid_value'"
[ -n "$cwd_value" ] || fail "cwd is empty"
[ -n "$exe_value" ] || fail "executable is empty"

mkdir "$tmp_root/target"
cat >"$tmp_root/chdir.sqt" <<SQT
import {cwd, chdir} from process;

void main() {
    chdir("$tmp_root/target");
    print(cwd());
}
SQT

chdir_out="$tmp_root/chdir.out"
chdir_err="$tmp_root/chdir.err"
run_prog "$tmp_root/chdir.sqt" "$chdir_out" "$chdir_err"
if [ "$RUN_RC" -ne 0 ]; then
    cat "$chdir_err" >&2
    fail "valid chdir probe failed"
fi

expected_cwd=$(cd "$tmp_root/target" && pwd -P)
actual_cwd=$(cd "$(cat "$chdir_out")" && pwd -P)
[ "$actual_cwd" = "$expected_cwd" ] || fail "cwd after chdir: expected '$expected_cwd', got '$actual_cwd'"

cat >"$tmp_root/chdir_invalid.sqt" <<SQT
import {chdir} from process;

void main() {
    chdir("$tmp_root/missing");
    print("after-invalid-chdir");
}
SQT

invalid_out="$tmp_root/chdir_invalid.out"
invalid_err="$tmp_root/chdir_invalid.err"
run_prog "$tmp_root/chdir_invalid.sqt" "$invalid_out" "$invalid_err"
invalid_rc=$RUN_RC
[ "$invalid_rc" -eq 70 ] || fail "invalid chdir expected exit 70, got $invalid_rc"
grep -q "chdir:" "$invalid_err" || fail "invalid chdir stderr missing chdir class"
! grep -q "after-invalid-chdir" "$invalid_out" || fail "invalid chdir continued after error"

cat >"$tmp_root/exit0.sqt" <<'SQT'
import {exit} from process;

void main() {
    print("before-exit0");
    exit(0);
    print("after-exit0");
}
SQT

exit0_out="$tmp_root/exit0.out"
exit0_err="$tmp_root/exit0.err"
run_prog "$tmp_root/exit0.sqt" "$exit0_out" "$exit0_err"
exit0_rc=$RUN_RC
[ "$exit0_rc" -eq 0 ] || fail "exit(0) expected exit 0, got $exit0_rc"
grep -q "before-exit0" "$exit0_out" || fail "exit(0) missing pre-exit marker"
! grep -q "after-exit0" "$exit0_out" || fail "exit(0) continued after exit"

cat >"$tmp_root/exit7.sqt" <<'SQT'
import {exit} from process;

void main() {
    print("before-exit7");
    exit(7);
    print("after-exit7");
}
SQT

exit7_out="$tmp_root/exit7.out"
exit7_err="$tmp_root/exit7.err"
run_prog "$tmp_root/exit7.sqt" "$exit7_out" "$exit7_err"
exit7_rc=$RUN_RC
[ "$exit7_rc" -eq 7 ] || fail "exit(7) expected exit 7, got $exit7_rc"
grep -q "before-exit7" "$exit7_out" || fail "exit(7) missing pre-exit marker"
! grep -q "after-exit7" "$exit7_out" || fail "exit(7) continued after exit"

trap - EXIT
cleanup
[ ! -e "$tmp_root" ] || fail "temp root residue remains: $tmp_root"
