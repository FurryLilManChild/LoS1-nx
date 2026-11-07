#!/usr/bin/env bash
# Package the locally built executable. Game data stays in assets/game_root.
set -euo pipefail
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$REPO/out/mac"
APP="${LOS_APP_PATH:-$REPO/../Lords of Shadow.app}"
BIN="$APP/Contents/MacOS"
for file in los librexruntime.dylib librexgpu-xenos.dylib vulkan/lib/libvulkan.1.dylib vulkan/lib/libMoltenVK.dylib; do
    [[ -f "$BUILD/$file" ]] || { echo "Missing build output: $BUILD/$file" >&2; exit 1; }
done
mkdir -p "$BIN" "$APP/Contents/Resources"
cp -p "$BUILD/los" "$BUILD/librexruntime.dylib" "$BUILD/librexgpu-xenos.dylib" "$BIN/"
ditto "$BUILD/vulkan" "$BIN/vulkan"
python3 - "$REPO" "$APP" <<'PY'
import json
import plistlib
import sys
from pathlib import Path

repo, app = map(Path, sys.argv[1:])
# ReXApp resolves paths before reading los.toml, so its normal game_root
# discovery needs these links for a Finder launch without command-line args.
for name in ('game_root', 'game_root_disc2'):
    link = app / 'Contents/MacOS' / name
    target = repo / 'assets' / name
    if link.is_symlink():
        if link.readlink() == target:
            continue
        link.unlink()
    elif link.exists():
        raise SystemExit(f'Refusing to replace game data at {link}')
    link.symlink_to(target, target_is_directory=True)
info = {
    'CFBundleName': 'Lords of Shadow',
    'CFBundleDisplayName': 'Lords of Shadow',
    'CFBundleIdentifier': 'local.lordsofshadow.recompiled',
    'CFBundleExecutable': 'los',
    'CFBundlePackageType': 'APPL',
    'CFBundleShortVersionString': '0.1.0',
    'CFBundleVersion': '1',
    'LSMinimumSystemVersion': '15.0',
    'NSHighResolutionCapable': True,
    'NSSupportsAutomaticGraphicsSwitching': True,
}
with (app / 'Contents/Info.plist').open('wb') as f:
    plistlib.dump(info, f)
config = app / 'Contents/MacOS/los.toml'
if not config.exists():
    config.write_text(
        (repo / 'app/los_mac.toml').read_text()
        + '\n# Local game assets, kept outside the application bundle.\n'
        + 'game_data_root = ' + json.dumps(str(repo / 'assets/game_root')) + '\n'
    )
PY
plutil -lint "$APP/Contents/Info.plist"
echo "Packaged: $APP"
