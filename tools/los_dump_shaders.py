#!/usr/bin/env python3
"""Dumps every Xbox 360 shader container Castlevania: Lords of Shadow has, for Renut's translator.

(From mof-nx's tools/mof_dump_shaders.py, itself from sa2-nx's tools/sa2_dump_shaders.py, GPL-3.0.) Lords of Shadow
keeps its shaders in its .packed archives (Data00.packed on each disc: bmp/vsh/*.x360.vsb and *.x360.psb, each an
"[XBOX]" wrapper around one container). The archive format, read from the files on 2026-10-26:

  "BFPK", u32 version (1: entries compressed, 0: stored), u32 count, then per entry
  u32 name length, the name, u32 size, u32 offset (all little-endian).
  A compressed entry is a u32 (about the unpacked size) and a zlib stream; a few entries of a version-1 archive are
  stored as they are (no zlib header).

Nothing here decrypts anything: the archives are plain zlib. The executable's loaded image is read too when it is
given (--image), as mof-nx does.

Output: <out>/{vs,ps}_<xxh3_64 of the container>.bin, each a complete container (virtual part, then physical part: the
layout XenosRecomp reads), and <out>/manifest.csv. Same naming as reNut's tools/dump_shaders.py (masterspike52), whose
container check this copies.

  python tools/los_dump_shaders.py --assets assets/game_root --assets assets/game_root_disc2 --out out/renut_shader_dump
"""
import argparse
import csv
import os
import re
import struct
import sys
import zlib

try:
    import xxhash
except ImportError:
    sys.exit("error: the 'xxhash' module is required (pip install xxhash)")

SHADER_MAGIC = re.compile(rb"\x10\x2a\x11[\x00\x01]")
# Entries that hold no shaders (sounds, textures): not unpacked, which keeps the run short.
SKIP_EXTENSIONS = {"xma", "ogg", "dds", "dds1", "ddr", "bik", "png"}


def find_shader_headers(data, aligned=True):
    """reNut's check: a container's sizes, tables and its physical part's (offset, size) agree.
    los1-nx: the game's .vsb / .psb files (a u32 version, a u32 length and an "[XBOX ...]" name, a u32 size, then the
    container) put most containers at offsets that are not a multiple of 4, so in them any offset is tried."""
    found = []
    for m in SHADER_MAGIC.finditer(data):
        o = m.start()
        if (aligned and o % 4) or o + 0x24 > len(data):
            continue
        virt, phys = struct.unpack_from(">II", data, o + 4)
        ct, dt, so = struct.unpack_from(">III", data, o + 0x10)
        if not (0x24 <= virt < 0x10000 and 0 < phys < 0x100000 and o + virt + phys <= len(data)):
            continue
        if ct >= virt or dt >= virt or not (0x24 <= so < virt - 8):
            continue
        poff, psize = struct.unpack_from(">II", data, o + so)
        if poff + psize != phys:
            continue
        found.append(o)
    return found


def packed_entries(path):
    """Yields (name, bytes) for each entry of a BFPK archive, unpacked."""
    with open(path, "rb") as f:
        magic, version, count = struct.unpack("<4sII", f.read(12))
        if magic != b"BFPK":
            print("warning: %s is not a BFPK archive" % path)
            return
        entries = []
        for _ in range(count):
            (length,) = struct.unpack("<I", f.read(4))
            name = f.read(length).decode("latin-1")
            size, offset = struct.unpack("<II", f.read(8))
            entries.append((name, size, offset))
        for name, size, offset in entries:
            if name.rsplit(".", 1)[-1].lower() in SKIP_EXTENSIONS:
                continue
            f.seek(offset)
            data = f.read(size)
            if version == 1 and len(data) > 6 and data[4] == 0x78:
                try:
                    data = zlib.decompress(data[4:])
                except zlib.error:
                    pass
            yield name, data


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--image", default=os.path.join(root, "out", "default_image.bin"),
                    help="the loaded default.xex image, if there is one")
    ap.add_argument("--image-base", type=lambda s: int(s, 0), default=0x82000000)
    ap.add_argument("--assets", action="append", default=None,
                    help="a folder with the game's .packed files (repeat for each disc)")
    ap.add_argument("--out", default=os.path.join(root, "out", "renut_shader_dump"))
    args = ap.parse_args()
    if not args.assets:
        args.assets = [os.path.join(root, "assets", "game_root"), os.path.join(root, "assets", "game_root_disc2")]

    # (label, bytes, base, aligned) for each piece searched.
    def sources():
        if os.path.isfile(args.image):
            yield os.path.basename(args.image), open(args.image, "rb").read(), args.image_base, True
        for folder in args.assets:
            if not os.path.isdir(folder):
                continue
            for name in sorted(os.listdir(folder)):
                if name.lower().endswith(".packed"):
                    path = os.path.join(folder, name)
                    for entry, data in packed_entries(path):
                        yield "%s/%s:%s" % (os.path.basename(folder), name, entry), data, 0, False

    os.makedirs(args.out, exist_ok=True)
    seen = set()
    manifest = []
    searched = 0
    for label, data, base, aligned in sources():
        searched += 1
        for h in find_shader_headers(data, aligned):
            flags, virt, phys = struct.unpack_from(">III", data, h)
            blob = data[h:h + virt + phys]
            kind = "vs" if flags & 1 else "ps"
            name = "%s_%s" % (kind, xxhash.xxh3_64_hexdigest(blob))
            manifest.append((name, kind, virt, phys, label, "0x%08x" % (base + h)))
            if name in seen:
                continue
            seen.add(name)
            with open(os.path.join(args.out, name + ".bin"), "wb") as fh:
                fh.write(blob)
    if not searched:
        sys.exit("no inputs: put the game's files in assets\\game_root first")
    manifest.sort()
    with open(os.path.join(args.out, "manifest.csv"), "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["name", "type", "virtual_size", "physical_size", "source", "asset"])
        w.writerows(manifest)
    ps = sum(1 for n in seen if n.startswith("ps_"))
    print("%d files searched, %d containers, %d unique shaders (%d ps, %d vs) -> %s"
          % (searched, len(manifest), len(seen), ps, len(seen) - ps, args.out))


if __name__ == "__main__":
    main()
