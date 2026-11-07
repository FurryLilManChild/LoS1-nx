#!/usr/bin/env bash
set -euo pipefail
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
APP="${LOS_APP_PATH:-$REPO/../Lords of Shadow.app}"
[[ -f "$REPO/assets/game_root/default.xex" ]] || { echo 'Disc 1 game data is missing.' >&2; exit 1; }
[[ -x "$APP/Contents/MacOS/los" ]] || "$REPO/tools/macos/package.sh"
# LaunchServices provides the same Cocoa application context as a Finder launch.
# Direct launches from the build task produced a black window on this Mac.
exec /usr/bin/open "$APP" --args "$@"
