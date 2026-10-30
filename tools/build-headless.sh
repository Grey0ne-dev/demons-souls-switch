#!/usr/bin/env bash
# Build the local PC reference target. This does not cross-compile for Switch.
set -euo pipefail
task_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
task_source="$task_root/upstream/DemonsSoulsRecomp"
task_cmake="$task_root/.tools/cmake-4.4.4-linux-x86_64/bin/cmake"
task_ninja="$task_root/.tools/bin/ninja"
task_jobs="${DES_BUILD_JOBS:-2}"
if [[ ! "$task_jobs" =~ ^[1-9][0-9]*$ ]]; then
    echo "DES_BUILD_JOBS must be a positive integer" >&2
    exit 1
fi
for task_input in "$task_source/work/recomp/hle_nid_table.cpp" \
                  "$task_source/ps3recomp/work_spu/_gen/spu_workloads.cpp" \
                  "$task_source/work/libsre/lifted/ppu_recomp.c" \
                  "$task_source/port/gen/libsre_glue.c"; do
    if [[ ! -s "$task_input" ]]; then
        echo "Missing generated input: $task_input" >&2
        exit 1
    fi
done
"$task_cmake" -S "$task_source/ps3recomp" -B "$task_source/ps3recomp/build-headless" -G Ninja \
    -DCMAKE_MAKE_PROGRAM="$task_ninja" \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Release \
    -DPS3RECOMP_RSX_D3D12_VKD3D=OFF -DPS3RECOMP_RSX_VULKAN=OFF \
    -DPS3RECOMP_FFMPEG_RUNTIME=OFF
"$task_cmake" --build "$task_source/ps3recomp/build-headless" \
    --target ps3recomp_runtime --parallel "$task_jobs"
"$task_cmake" -S "$task_source/port" -B "$task_source/port/build-headless" -G Ninja \
    -DCMAKE_MAKE_PROGRAM="$task_ninja" \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Release \
    -DPS3RECOMP_BUILD_DIR="$task_source/ps3recomp/build-headless" \
    -DDES_VKD3D=OFF -DDES_UI=OFF -DDES_DEV=OFF
"$task_cmake" --build "$task_source/port/build-headless" \
    --target des_boot --parallel "$task_jobs"
