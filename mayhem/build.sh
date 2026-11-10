#!/usr/bin/env bash
#
# mayhem/build.sh — builds the RPCS3 fuzz targets, their standalone reproducers and the unit-test suite.
#
# RPCS3 is a full emulator (Qt, LLVM, Vulkan, ...) whose parsers are fuzzable on their own: the harnesses
# compile the parser sources straight from the checkout, with the sanitizers, and link mayhem/shim.cpp for the
# few thread/emulator symbols the sources reference at link time. Nothing of the emulator is patched.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

YAML_DIR=$(echo /opt/toolchains/deps/yaml-cpp-*)
PUGI_DIR=$(echo /opt/toolchains/deps/pugixml-*)
OUT="$SRC/build-mayhem"
mkdir -p "$OUT/fuzz" "$OUT/test"

SOURCES=(
  Utilities/File.cpp Utilities/StrFmt.cpp Utilities/mutex.cpp Utilities/cond.cpp Utilities/sema.cpp
  Utilities/version.cpp Utilities/Config.cpp Utilities/cheat_info.cpp
  rpcs3/util/logs.cpp rpcs3/util/sysinfo.cpp rpcs3/util/atomic.cpp rpcs3/util/yaml.cpp rpcs3/util/serialization_ext.cpp
  rpcs3/Emu/vfs_config.cpp
  rpcs3/Crypto/sha1.cpp rpcs3/Crypto/sha256.cpp rpcs3/Crypto/md5.cpp rpcs3/Crypto/utils.cpp rpcs3/Crypto/aes.cpp
  rpcs3/Crypto/aesni.cpp rpcs3/Crypto/key_vault.cpp rpcs3/Crypto/ec.cpp rpcs3/Crypto/lz.cpp
  rpcs3/Crypto/unself.cpp rpcs3/Crypto/unedat.cpp
  rpcs3/Loader/PSF.cpp rpcs3/Loader/PUP.cpp rpcs3/Loader/TRP.cpp rpcs3/Loader/TAR.cpp rpcs3/Loader/ELF.cpp
  mayhem/shim.cpp
)
HARNESSES=(fuzz_psf fuzz_pup fuzz_trp fuzz_tar fuzz_elf fuzz_self fuzz_edat)
TESTS=(
  test test_bit_set test_cctype test_fmt test_pair test_simple_array test_tuple test_types_util test_address_range
)

COMMON_FLAGS=(-std=gnu++23 -msse4.2 -maes -mcx16 -ffunction-sections -DZLIB_CONST=1
  -I"$SRC" -I"$SRC/rpcs3" -I"$YAML_DIR/src/include" -I"$PUGI_DIR/src/src")
LIBS=("$YAML_DIR/lib/libyaml-cpp.a" "$PUGI_DIR/lib/libpugixml.a" -lz -lzstd)

obj_of() { echo "$1" | tr / _; }

# compile <dir> <flags...> -- <sources...>  (parallel, one object per source)
compile_all() {
  local dir="$1"; shift
  local flags=()
  while [ "$1" != "--" ]; do flags+=("$1"); shift; done
  shift
  printf '%s\n' "$@" | xargs -P "$MAYHEM_JOBS" -I{} bash -c \
    "$CXX \"\$@\" -c \"$SRC/{}\" -o \"$dir/\$(echo {} | tr / _).o\"" _ "${flags[@]}"
}

FUZZ_FLAGS=($SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -O1 "${COMMON_FLAGS[@]}")
TEST_FLAGS=(-O1 $COVERAGE_FLAGS "${COMMON_FLAGS[@]}")

# 1) sanitized project sources + harnesses; each harness is linked twice (fuzzer, standalone reproducer)
HARNESS_SRCS=(); for h in "${HARNESSES[@]}"; do HARNESS_SRCS+=("mayhem/harnesses/$h.cpp"); done
compile_all "$OUT/fuzz" "${FUZZ_FLAGS[@]}" -- "${SOURCES[@]}" "${HARNESS_SRCS[@]}"

$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$OUT/fuzz/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/fuzz/standalone_main.o"

PROJECT_OBJS=(); for s in "${SOURCES[@]}"; do PROJECT_OBJS+=("$OUT/fuzz/$(obj_of "$s").o"); done
for h in "${HARNESSES[@]}"; do
  HOBJ="$OUT/fuzz/$(obj_of "mayhem/harnesses/$h.cpp").o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE -fuse-ld=lld -Wl,--gc-sections \
    "$HOBJ" "${PROJECT_OBJS[@]}" "$OUT/fuzz/lsan_off.o" "${LIBS[@]}" -o "$SRC/$h"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -fuse-ld=lld -Wl,--gc-sections \
    "$HOBJ" "$OUT/fuzz/standalone_main.o" "${PROJECT_OBJS[@]}" "$OUT/fuzz/lsan_off.o" "${LIBS[@]}" -o "$SRC/$h-standalone"
done

# 2) unit tests and known-answer probes with the project's normal flags (no sanitizers), run by test.sh
TEST_SRCS=(); for t in "${TESTS[@]}"; do TEST_SRCS+=("rpcs3/tests/$t.cpp"); done
compile_all "$OUT/test" "${TEST_FLAGS[@]}" -- "${SOURCES[@]}" "${TEST_SRCS[@]}" mayhem/kat.cpp

TEST_PROJECT_OBJS=(); for s in "${SOURCES[@]}"; do TEST_PROJECT_OBJS+=("$OUT/test/$(obj_of "$s").o"); done
TEST_OBJS=(); for t in "${TEST_SRCS[@]}"; do TEST_OBJS+=("$OUT/test/$(obj_of "$t").o"); done
$CXX $COVERAGE_FLAGS -fuse-ld=lld -Wl,--gc-sections "${TEST_OBJS[@]}" "${TEST_PROJECT_OBJS[@]}" "${LIBS[@]}" \
  -lgtest -lgmock -o "$SRC/rpcs3_test"
$CXX $COVERAGE_FLAGS -fuse-ld=lld -Wl,--gc-sections "$OUT/test/$(obj_of mayhem/kat.cpp).o" "${TEST_PROJECT_OBJS[@]}" "${LIBS[@]}" \
  -o "$SRC/rpcs3_kat"

echo "build.sh: done"
