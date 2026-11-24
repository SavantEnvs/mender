#!/usr/bin/env bash
# mayhem/build.sh — build mender's fuzz harnesses (sanitized project + harnesses + standalone
# reproducers) and the upstream gtest unit tests used by mayhem/test.sh (normal flags).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export CC CXX
cd "$SRC"

# 0) Upstream's git submodules: CI checks out without them, so fill each empty submodule path
#    from the copy the Dockerfile fetched at the pinned commit (idempotent, offline).
for pair in json lmdbxx tiny-process-library expected optional-lite yaml-cpp; do
  dst="src/common/vendor/$pair"
  if [ -z "$(ls -A "$dst" 2>/dev/null)" ]; then
    src=$(ls -d /opt/toolchains/deps/"$pair"-*/src | head -n1)
    mkdir -p "$dst"
    cp -a "$src"/. "$dst"/
  fi
done

FUZZ_LIBS=(artifact mender_deployments client_shared_config_parser common_key_value_parser
           mender_update_daemon common_http)

# 1) The project, sanitized + coverage-instrumented (build dirs live under the gitignored build/).
#    A custom build type keeps CMake's per-config flags (-g = DWARF 5, -Os) out of the way.
cmake -S . -B build/fuzz -DBUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Fuzz \
  -DCMAKE_C_FLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS -O1" \
  -DCMAKE_CXX_FLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS -O1"
cmake --build build/fuzz -j"$MAYHEM_JOBS" --target "${FUZZ_LIBS[@]}"

# 2) Harnesses: libFuzzer binary + standalone reproducer each.
INC=(-I"$SRC/src" -I"$SRC/build/fuzz/src"
     -I"$SRC/src/artifact/tar/platform" -I"$SRC/src/artifact/sha/platform"
     -I"$SRC/src/common/vendor/expected/include" -I"$SRC/src/common/vendor/optional-lite/include"
     -I"$SRC/src/common/vendor/json/include" -I"$SRC/src/common/vendor/lmdbxx/include"
     -I"$SRC/src/common/vendor/tiny-process-library" -I"$SRC/src/common/vendor/yaml-cpp/include"
     $(pkg-config --cflags dbus-1))
mapfile -t ARCHIVES < <(find "$SRC/build/fuzz" -name '*.a' | sort)
SYSLIBS=(-larchive -llmdb -lssl -lcrypto -lboost_log -lboost_log_setup -lboost_filesystem
         -lboost_thread -lboost_regex -ldbus-1 -lpthread -ldl)
CXXFLAGS_H=(-std=c++17 $SANITIZER_FLAGS $DEBUG_FLAGS -O1 -DBOOST_LOG_DYN_LINK -DBOOST_ALL_DYN_LINK)
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o build/fuzz/standalone_main.o
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o build/fuzz/lsan_off.o
# Each harness object is rebuilt every run so an edited harness is always picked up.
for h in fuzz_artifact fuzz_deployment_logs fuzz_config fuzz_key_value fuzz_deployment_response \
         fuzz_http_url; do
  $CXX "${CXXFLAGS_H[@]}" -fsanitize=fuzzer-no-link "${INC[@]}" -c "mayhem/harnesses/$h.cpp" -o "build/fuzz/$h.o"
  $CXX "${CXXFLAGS_H[@]}" $LIB_FUZZING_ENGINE "build/fuzz/$h.o" build/fuzz/lsan_off.o \
    -Wl,--start-group "${ARCHIVES[@]}" -Wl,--end-group "${SYSLIBS[@]}" -o "$SRC/$h"
  $CXX "${CXXFLAGS_H[@]}" "build/fuzz/$h.o" build/fuzz/standalone_main.o build/fuzz/lsan_off.o \
    -Wl,--start-group "${ARCHIVES[@]}" -Wl,--end-group "${SYSLIBS[@]}" -o "$SRC/$h-standalone"
done

# 3) Upstream unit tests (normal flags) for mayhem/test.sh: the suites for the fuzzed code that
#    need neither the mender-artifact tool nor network.
TESTS=(artifact_manifest_parser_test artifact_version_parser_test artifact_manifest_sig_parser_test
       sha_test deployments_test config_parser_test key_value_parser_test json_test io_test path_test)
cmake -S . -B build/tests -DBUILD_TESTS=ON -DMENDER_DOWNLOAD_GTEST=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/opt/toolchains/deps/googletest-1.12.1/src -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DCMAKE_BUILD_TYPE=Debug \
  -DMENDER_TEST_SKIP_NETWORK_BASED_TESTS=ON \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build build/tests -j"$MAYHEM_JOBS" --target "${TESTS[@]}"
printf '%s\n' "${TESTS[@]}" > build/tests/mayhem-test-list
