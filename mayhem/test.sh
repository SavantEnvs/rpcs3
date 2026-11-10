#!/usr/bin/env bash
#
# mayhem/test.sh — RUN upstream's gtest unit tests plus known-answer probes of the fuzzed parsers
# (both built by mayhem/build.sh).
# exit 0 = pass. PATCH-grade oracle: after an agent patches the source, the grader rebuilds (build.sh)
# then runs this.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# Upstream's own unit tests (rpcs3/tests): gtest prints "[  PASSED  ] N tests." only when every test passed.
gtest_passed=0; gtest_failed=0
if [ ! -x "$SRC/rpcs3_test" ]; then echo "missing $SRC/rpcs3_test (build.sh bug)" >&2; gtest_failed=1
else
  out=$("$SRC/rpcs3_test" 2>&1); echo "$out" | tail -n 15
  p=$(echo "$out" | sed -n 's/^\[  PASSED  \] \([0-9]\+\) tests\?\.$/\1/p' | tail -n1)
  f=$(echo "$out" | sed -n 's/^\[  FAILED  \] \([0-9]\+\) tests\?, listed below:$/\1/p' | tail -n1)
  gtest_passed=${p:-0}; gtest_failed=${f:-0}
  # The runner must have really run: a stubbed-out binary prints nothing, which would otherwise count as zero failures.
  if [ "$gtest_passed" -lt 100 ] && [ "$gtest_failed" -eq 0 ]; then gtest_failed=1; echo "gtest reported only $gtest_passed passed tests" >&2; fi
fi

# Known-answer probes: PSF save/load round trip, SHA-256("abc"), TAR header parsing, each with fixed expected values.
kat_expected=10; kat_ok=0; kat_failed=0
if [ ! -x "$SRC/rpcs3_kat" ]; then echo "missing $SRC/rpcs3_kat (build.sh bug)" >&2; kat_failed=1
else
  out=$("$SRC/rpcs3_kat" 2>&1); echo "$out" | grep '^KAT '
  kat_ok=$(echo "$out" | grep -c '^KAT .* ok$')
  kat_failed=$(echo "$out" | grep -c '^KAT .* FAILED$')
  if [ "$kat_ok" -lt "$kat_expected" ] && [ "$kat_failed" -eq 0 ]; then kat_failed=$(( kat_expected - kat_ok )); fi
fi

emit_ctrf "gtest+kat" $(( gtest_passed + kat_ok )) $(( gtest_failed + kat_failed )) 0
