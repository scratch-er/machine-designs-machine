#!/bin/sh
# Run every ISA test workload under workloads/isa-test.
# Usage: ./workloads/isa-test/run_tests.sh
#
# Builds each .S file with the workload-build skill, runs it in the emulator
# with UART output captured, and checks that the output is exactly "PASS\n".

set -e

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(cd "$script_dir/../.." && pwd)
cd "$repo_root"

# Load toolchain configuration.
config="data/workload-build/config.sh"
if [ -f "$config" ]; then
    # shellcheck source=/dev/null
    . "$config"
else
    echo "warning: config not found at $config, using built-in defaults" >&2
fi

build_helper="./skills/workload-build/build.sh"
emulator="./emulator/build/emulator"

if [ ! -x "$build_helper" ]; then
    echo "error: build helper not found: $build_helper" >&2
    exit 1
fi
if [ ! -x "$emulator" ]; then
    echo "error: emulator binary not found: $emulator" >&2
    echo "hint: cmake --build emulator/build -j" >&2
    exit 1
fi

pass=0
fail=0
failed_tests=""

run_test() {
    src=$1
    stem=${src%.S}
    name=$(echo "$src" | sed "s|^$script_dir/||")

    printf "  %-40s " "$name"

    if ! "$build_helper" "$src" "$stem" >"$stem.build.log" 2>&1; then
        echo "BUILD FAIL"
        fail=$((fail + 1))
        failed_tests="$failed_tests $name(build)"
        echo "  -> build log: $stem.build.log"
        return
    fi

    rm -f "$stem.out"
    uart_cmd=""
    if [ -f "$stem.uart" ]; then
        uart_cmd="uart input $stem.uart; "
    fi

    # Estimate an instruction budget from source size to avoid waiting on hangs.
    # The multiplier is a generous over-estimate of instructions per line.
    lines=$(wc -l < "$src" | tr -d ' ')
    budget=$((lines * 20))
    if [ "$budget" -lt 50000 ]; then
        budget=50000
    fi

    # Extra emulator flags for this test (e.g. --strict-mem).
    extra_flags=""
    if [ -f "$stem.flags" ]; then
        extra_flags=$(tr '\n' ' ' < "$stem.flags")
    fi

    if ! "$emulator" $extra_flags -e "load $stem.bin 0x20000000; ${uart_cmd}uart output $stem.out; run $budget; exit" >"$stem.run.log" 2>&1; then
        echo "RUN FAIL"
        fail=$((fail + 1))
        failed_tests="$failed_tests $name(run)"
        rerun_with_trace "$src" "$budget" "$extra_flags"
        return
    fi

    if [ ! -f "$stem.out" ]; then
        echo "NO OUTPUT (budget=${budget})"
        fail=$((fail + 1))
        failed_tests="$failed_tests $name(output)"
        rerun_with_trace "$src" "$budget" "$extra_flags"
        return
    fi

    # Expect exactly "PASS\n" (0x50 0x41 0x53 0x53 0x0A).
    if [ "$(cat "$stem.out")" = "PASS" ] && [ "$(wc -c < "$stem.out")" -eq 5 ]; then
        echo "PASS"
        pass=$((pass + 1))
    else
        printf "BAD OUTPUT (%s)\n" "$(od -An -tx1 "$stem.out" | tr -d ' \n')"
        fail=$((fail + 1))
        failed_tests="$failed_tests $name(bad)"
        rerun_with_trace "$src" "$budget" "$extra_flags"
    fi
}

rerun_with_trace() {
    src=$1
    budget=${2:-50000}
    extra_flags=${3:-}
    stem=${src%.S}
    name=$(echo "$src" | sed "s|^$script_dir/||")
    echo "  -> re-running $name with instruction trace (last 30 lines, budget=${budget}):"
    uart_cmd=""
    if [ -f "$stem.uart" ]; then
        uart_cmd="uart input $stem.uart; "
    fi
    "$emulator" $extra_flags --log 1 -e "load $stem.bin 0x20000000; ${uart_cmd}uart output $stem.out; run $budget; exit" >"$stem.trace" 2>&1 || true
    tail -n 30 "$stem.trace" && echo "  -> full trace: $stem.trace"
}

echo "=== ISA test suite ==="

# Generate combinatorial workloads first so they are included in the run.
if [ -x "$script_dir/scripts/gen_alu_tests.py" ]; then
    echo "Generating combinatorial ALU tests..."
    "$script_dir/scripts/gen_alu_tests.py" > "$script_dir/generated/alu_comb.S"
fi
if [ -x "$script_dir/scripts/gen_branch_tests.py" ]; then
    echo "Generating combinatorial branch tests..."
    "$script_dir/scripts/gen_branch_tests.py" > "$script_dir/generated/branch_comb.S"
fi

# Collect all .S files.
tests=$(find "$script_dir/src" "$script_dir/generated" -name '*.S' 2>/dev/null | sort)

echo "Running $(echo "$tests" | grep -c '^' 2>/dev/null || echo 0) test workload(s)..."
for t in $tests; do
    run_test "$t"
done

echo "========================================"
echo "Passed: $pass  Failed: $fail"

if [ "$fail" -ne 0 ]; then
    echo "Failed tests:$failed_tests"
    exit 1
fi
exit 0
