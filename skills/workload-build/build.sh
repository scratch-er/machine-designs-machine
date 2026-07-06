#!/bin/sh
# Build helper for the workload-build skill.
# Usage: build.sh <source.S|source.c> [output-stem]

set -e

# Resolve repository root from script location (skills/workload-build/build.sh).
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(cd "$script_dir/../.." && pwd)
cd "$repo_root"

# Load configuration; fall back to built-in defaults if it is missing.
config="data/workload-build/config.sh"
if [ -f "$config" ]; then
    # shellcheck source=/dev/null
    . "$config"
else
    echo "warning: config not found at $config, using built-in defaults" >&2
fi

# Tool defaults (overridden by the sourced config or the caller's environment).
: "${CC:=clang}"
: "${AS:=llvm-mc}"
: "${LD:=ld.lld}"
: "${OBJCOPY:=llvm-objcopy}"
: "${OBJDUMP:=llvm-objdump}"

# Flag defaults.
: "${WORKLOAD_ARCH:=rv32e_zicsr}"
: "${WORKLOAD_ABI:=ilp32e}"
: "${AS_FLAGS:=-triple=riscv32 -mattr=+e -filetype=obj}"
: "${CC_FLAGS:=--target=riscv32 -march=${WORKLOAD_ARCH} -mabi=${WORKLOAD_ABI} -nostdlib -static -mno-relax}"
: "${LD_FLAGS:=}"
: "${OBJCOPY_FLAGS:=}"
: "${OBJDUMP_FLAGS:=-d -M no-aliases}"

# Extra flags appended after the defaults.
: "${CC_EXTRA_FLAGS:=}"
: "${AS_EXTRA_FLAGS:=}"
: "${LD_EXTRA_FLAGS:=}"
: "${OBJCOPY_EXTRA_FLAGS:=}"
: "${OBJDUMP_EXTRA_FLAGS:=}"

# Linker script location.
: "${WORKLOAD_BUILD_LINK_SCRIPT:=skills/workload-build/link.ld}"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <source.S|source.c> [output-stem]" >&2
    exit 1
fi

src=$1
out=${2:-${src%.*}}

mkdir -p "$(dirname "$out")"

case "$src" in
    *.S|*.s)
        $AS $AS_FLAGS $AS_EXTRA_FLAGS "$src" -o "$out.o"
        $OBJCOPY -O binary $OBJCOPY_FLAGS $OBJCOPY_EXTRA_FLAGS "$out.o" "$out.bin"
        $OBJDUMP $OBJDUMP_FLAGS $OBJDUMP_EXTRA_FLAGS "$out.o" > "$out.lst"
        echo "Built $out.bin"
        ;;
    *.c)
        $AS $AS_FLAGS $AS_EXTRA_FLAGS "$script_dir/crt0.S" -o "$out.crt0.o"
        $CC $CC_FLAGS $CC_EXTRA_FLAGS -c "$src" -o "$out.o"
        $CC $CC_FLAGS $CC_EXTRA_FLAGS "$out.crt0.o" "$out.o" \
            -o "$out.elf" \
            -fuse-ld="${LD}" \
            -T "$WORKLOAD_BUILD_LINK_SCRIPT" \
            -Wl,--build-id=none $LD_FLAGS $LD_EXTRA_FLAGS
        $OBJCOPY -O binary $OBJCOPY_FLAGS $OBJCOPY_EXTRA_FLAGS "$out.elf" "$out.bin"
        $OBJDUMP $OBJDUMP_FLAGS $OBJDUMP_EXTRA_FLAGS "$out.elf" > "$out.lst"
        echo "Built $out.bin"
        ;;
    *)
        echo "error: unsupported source extension: $src" >&2
        exit 1
        ;;
esac
