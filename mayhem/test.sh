#!/usr/bin/env bash
# mayhem/test.sh — RUN upstream's gtest unit tests (built by mayhem/build.sh with normal flags)
# for the code the fuzz targets cover: artifact version/manifest/manifest-sig/sha parsing,
# deployment logs, config parser, key=value parser, json, io, path. Emits CTRF.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

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

list=build/tests/mayhem-test-list
if [ ! -s "$list" ]; then
  echo "test list $list missing: mayhem/build.sh did not build the unit tests" >&2
  emit_ctrf gtest 0 1 0; exit 1
fi
passed=0; failed=0; skipped=0
while read -r t; do
  bin=$(find build/tests/tests -type f -name "$t" -perm -u+x | head -n1)
  if [ -z "$bin" ]; then echo "missing test binary $t" >&2; failed=$((failed + 1)); continue; fi
  out=$(cd "$(dirname "$bin")" && "./$t" 2>&1); rc=$?
  p=$(sed -n 's/^\[  PASSED  \] \([0-9]*\) test.*/\1/p' <<<"$out" | tail -n1)
  f=$(sed -n 's/^\[  FAILED  \] \([0-9]*\) test.*/\1/p' <<<"$out" | tail -n1)
  s=$(sed -n 's/^\[  SKIPPED \] \([0-9]*\) test.*/\1/p' <<<"$out" | tail -n1)
  p=${p:-0}; f=${f:-0}; s=${s:-0}
  # A binary that crashed/exited before printing a summary, or reported nothing, counts as failed.
  if [ "$rc" -ne 0 ] && [ "$f" -eq 0 ]; then f=1; fi
  if [ $((p + f + s)) -eq 0 ]; then f=1; fi
  echo "$t: passed=$p failed=$f skipped=$s (rc=$rc)"
  [ "$f" -eq 0 ] || grep -E '^\[  FAILED  \]|Failure' <<<"$out" | head -n 20
  passed=$((passed + p)); failed=$((failed + f)); skipped=$((skipped + s))
done < "$list"
emit_ctrf gtest "$passed" "$failed" "$skipped"
