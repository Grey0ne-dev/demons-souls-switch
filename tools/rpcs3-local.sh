#!/usr/bin/env bash
set -euo pipefail
task_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
export XDG_CONFIG_HOME="$task_root/.tools/state/config"
export XDG_CACHE_HOME="$task_root/.tools/state/cache"
export XDG_DATA_HOME="$task_root/.tools/state/data"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$XDG_DATA_HOME"
exec "$task_root/.tools/rpcs3/AppDir/AppRun" "$@"
