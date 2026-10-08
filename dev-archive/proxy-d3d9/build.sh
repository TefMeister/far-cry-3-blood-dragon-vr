#!/bin/bash
# Builds the 32-bit d3d9.dll camera logger for Blood Dragon (FC3.dll is PE32/i386)
# and runs the constant-table test. Run from Git Bash. Needs llvm-mingw
# (i686-w64-mingw32-clang) on PATH or at the dev PC's WinGet path.
set -e
cd "$(dirname "$0")"

if command -v i686-w64-mingw32-clang >/dev/null 2>&1; then
    TOOLCHAIN="$(dirname "$(command -v i686-w64-mingw32-clang)")"
else
    TOOLCHAIN="$(ls -d "$LOCALAPPDATA"/Microsoft/WinGet/Packages/MartinStorsjo.LLVM-MinGW.*/llvm-mingw-*/bin 2>/dev/null | head -1)"
fi
CC="$TOOLCHAIN/i686-w64-mingw32-clang.exe"
MH=third_party/minhook

mkdir -p build
# -Wl,--no-insert-timestamp: identical source gives an identical file, so a
# rebuild's hash can be compared with what is installed.
"$CC" -shared -O2 -Wall -Wextra \
    -I"$MH/include" -Isrc \
    -o build/d3d9.dll \
    src/proxy.c src/ctab.c src/stereo.c src/stereo_math.c \
    "$MH/src/hook.c" "$MH/src/buffer.c" "$MH/src/trampoline.c" "$MH/src/hde/hde32.c" \
    src/d3d9.def \
    -Wl,--no-insert-timestamp \
    -luser32 -lkernel32
echo "Built build/d3d9.dll"
"$TOOLCHAIN/i686-w64-mingw32-objdump" -p build/d3d9.dll | sed -n '/Ordinal\/Name Pointer/,/^$/p'

# Host test of the constant-table reader against fxc-compiled shaders.
gcc -O2 -Wall -Wextra -Wpedantic -o build/ctab_test.exe tools/ctab_test.c src/ctab.c
./build/ctab_test.exe tools/vs30.cso tools/vs20.cso

# Host test of the per-eye shift against a camera actually moved sideways.
gcc -O2 -Wall -Wextra -Wpedantic -o build/stereo_test.exe tools/stereo_test.c src/stereo_math.c -lm
./build/stereo_test.exe
sha256sum build/d3d9.dll
