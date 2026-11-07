#!/usr/bin/env python3
"""Install a completed Disc 2 ISO or 7z download without replacing existing data."""

import argparse
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools"))
from extract_iso import ExtractError, extract_image, find_partition, read_volume_descriptor, walk


def verify_disc_marker(image):
    # Both discs ship the same default.xex (including its Disc 1 header).
    # The game itself identifies the media by the root Disc.1 / Disc.2 file.
    with image.open("rb") as stream:
        base, _ = find_partition(stream)
        sector, size = read_volume_descriptor(stream, base)
        files = {name.lower() for name, entry in walk(stream, base, sector, size)
                 if not entry["is_dir"]}
    if "disc.2" not in files or "disc.1" in files:
        raise ValueError("Expected a Disc 2 image with the root Disc.2 marker.")


def verify_disc(xex):
    with xex.open("rb") as stream:
        header = stream.read(24)
        if len(header) != 24 or header[:4] != b"XEX2":
            raise ValueError("The disc has no valid XEX2 executable.")
        count = struct.unpack_from(">I", header, 20)[0]
        if count > 4096:
            raise ValueError("Invalid XEX optional-header count.")
        headers = dict(struct.iter_unpack(">II", stream.read(count * 8)))
        if 0x40006 not in headers:
            raise ValueError("Missing XEX execution info.")
        stream.seek(headers[0x40006])
        info = stream.read(24)
        title = struct.unpack_from(">I", info, 12)[0]
        disc, total = info[18:20]
        if title != 0x4B4E0809 or total != 2 or disc not in (1, 2):
            raise ValueError(f"Expected Lords of Shadow; got title {title:08X}, disc {disc}/{total}.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="completed Disc 2 .7z or .iso")
    args = parser.parse_args()
    source = args.input.expanduser().resolve()
    if any(part.endswith((".download", ".crdownload", ".part")) for part in source.parts):
        parser.error("This is an unfinished download. Wait for the final .7z or .iso file.")
    if not source.is_file() or source.suffix.lower() not in (".7z", ".iso"):
        parser.error("Provide an existing .7z or .iso file.")
    destination = REPO / "assets/game_root_disc2"
    if destination.exists():
        parser.error(f"Disc 2 already exists at {destination}; no files were changed.")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".disc2-import-", dir=destination.parent) as temporary:
        work = Path(temporary)
        image = source
        if source.suffix.lower() == ".7z":
            listing = subprocess.check_output(["/usr/bin/bsdtar", "-tf", str(source)], text=True)
            members = [name for name in listing.splitlines() if name.lower().endswith(".iso")]
            if len(members) != 1:
                raise ValueError(f"Expected exactly one ISO in the archive; found {len(members)}.")
            image = work / "disc2.iso"
            print("Decompressing Disc 2 (this can take several minutes)...", flush=True)
            with image.open("wb") as output:
                subprocess.run(["/usr/bin/bsdtar", "-xOf", str(source), members[0]], stdout=output, check=True)
        verify_disc_marker(image)
        staging = work / "game_root_disc2"
        extract_image(str(image), str(staging), xex_only=True)
        xex = next((p for p in staging.iterdir() if p.name.lower() == "default.xex"), None)
        if xex is None:
            raise ValueError("No default.xex at the root of the disc.")
        verify_disc(xex)
        extract_image(str(image), str(staging))
        staging.rename(destination)
    print(f"Installed Disc 2: {destination}\nRestart Lords of Shadow to mount it. No rebuild is needed.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, struct.error, ExtractError, subprocess.CalledProcessError) as error:
        sys.exit(f"Disc 2 import failed: {error}")
