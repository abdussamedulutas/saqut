#!/usr/bin/env bash
# #176 SQ-090-STDIO-SURFACE — stdin/stdout/stderr black-box checks.
set -eu

binary=$1
tmp_root=$(mktemp -d "${TMPDIR:-/tmp}/saqut-stdio-XXXXXX")
cleanup() {
    rm -rf "$tmp_root"
}
trap cleanup EXIT

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

run_with_input() {
    local source=$1
    local input=$2
    local stdout_file=$3
    local stderr_file=$4
    set +e
    "$binary" run --allow-sys "file:$source" <"$input" >"$stdout_file" 2>"$stderr_file"
    RUN_RC=$?
    set -e
    return 0
}

cat >"$tmp_root/read_all_stdout.sqt" <<'SQT'
import {readAll} from stdin;
import {write} from stdout;

void main() {
    string text = readAll();
    write("BEGIN>");
    write(text);
    write("<END");
}
SQT
printf 'alpha\nbeta' >"$tmp_root/read_all.in"
printf 'BEGIN>alpha\nbeta<END' >"$tmp_root/read_all.expected"
run_with_input "$tmp_root/read_all_stdout.sqt" "$tmp_root/read_all.in" \
    "$tmp_root/read_all.out" "$tmp_root/read_all.err"
[ "$RUN_RC" -eq 0 ] || fail "readAll/stdout.write exited $RUN_RC"
cmp -s "$tmp_root/read_all.expected" "$tmp_root/read_all.out" || fail "readAll/stdout.write stdout mismatch"
[ ! -s "$tmp_root/read_all.err" ] || fail "readAll/stdout.write wrote stderr"

cat >"$tmp_root/read_line_eof.sqt" <<'SQT'
import {readLine} from stdin;
import {write} from stdout;

void main() {
    string? first = readLine();
    string? second = readLine();
    string? third = readLine();
    if (first != null) { write(first); }
    write("|");
    if (second != null) { write(second); }
    write("|");
    if (third == null) { write("EOF"); }
}
SQT
printf 'one\ntwo' >"$tmp_root/read_line.in"
printf 'one|two|EOF' >"$tmp_root/read_line.expected"
run_with_input "$tmp_root/read_line_eof.sqt" "$tmp_root/read_line.in" \
    "$tmp_root/read_line.out" "$tmp_root/read_line.err"
[ "$RUN_RC" -eq 0 ] || fail "readLine EOF exited $RUN_RC"
cmp -s "$tmp_root/read_line.expected" "$tmp_root/read_line.out" || fail "readLine EOF stdout mismatch"
[ ! -s "$tmp_root/read_line.err" ] || fail "readLine EOF wrote stderr"

cat >"$tmp_root/read_bytes_stdout.sqt" <<'SQT'
import {readBytes} from stdin;
import {writeBytes} from stdout;

void main() {
    byte[] data = readBytes();
    writeBytes(data);
}
SQT
printf '\000A\377\nZ' >"$tmp_root/read_bytes.in"
cp "$tmp_root/read_bytes.in" "$tmp_root/read_bytes.expected"
run_with_input "$tmp_root/read_bytes_stdout.sqt" "$tmp_root/read_bytes.in" \
    "$tmp_root/read_bytes.out" "$tmp_root/read_bytes.err"
[ "$RUN_RC" -eq 0 ] || fail "readBytes/stdout.writeBytes exited $RUN_RC"
cmp -s "$tmp_root/read_bytes.expected" "$tmp_root/read_bytes.out" || fail "readBytes/stdout.writeBytes stdout mismatch"
[ ! -s "$tmp_root/read_bytes.err" ] || fail "readBytes/stdout.writeBytes wrote stderr"

cat >"$tmp_root/stderr_write.sqt" <<'SQT'
import {write} from stderr;

void main() {
    write("ERR>");
    write("text");
    write("<ERR");
}
SQT
: >"$tmp_root/empty.in"
printf 'ERR>text<ERR' >"$tmp_root/stderr_write.expected"
run_with_input "$tmp_root/stderr_write.sqt" "$tmp_root/empty.in" \
    "$tmp_root/stderr_write.out" "$tmp_root/stderr_write.err"
[ "$RUN_RC" -eq 0 ] || fail "stderr.write exited $RUN_RC"
[ ! -s "$tmp_root/stderr_write.out" ] || fail "stderr.write wrote stdout"
cmp -s "$tmp_root/stderr_write.expected" "$tmp_root/stderr_write.err" || fail "stderr.write stderr mismatch"

cat >"$tmp_root/stderr_write_bytes.sqt" <<'SQT'
import {writeBytes} from stderr;

void main() {
    byte[] data = [69, 0, 82, 255];
    writeBytes(data);
}
SQT
printf '\105\000\122\377' >"$tmp_root/stderr_bytes.expected"
run_with_input "$tmp_root/stderr_write_bytes.sqt" "$tmp_root/empty.in" \
    "$tmp_root/stderr_bytes.out" "$tmp_root/stderr_bytes.err"
[ "$RUN_RC" -eq 0 ] || fail "stderr.writeBytes exited $RUN_RC"
[ ! -s "$tmp_root/stderr_bytes.out" ] || fail "stderr.writeBytes wrote stdout"
cmp -s "$tmp_root/stderr_bytes.expected" "$tmp_root/stderr_bytes.err" || fail "stderr.writeBytes stderr mismatch"

trap - EXIT
cleanup
[ ! -e "$tmp_root" ] || fail "temp root residue remains: $tmp_root"
