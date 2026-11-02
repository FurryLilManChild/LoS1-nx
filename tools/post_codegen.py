#!/usr/bin/env python3
"""LoS1-nx's post-codegen steps, run after every `rexglue codegen los_manifest.toml` (tools/setup_linux_codegen.sh
runs it). Each step rewrites the generated C++ so the compiler can make faster code; the guest-visible result is the
same. All of them come from sa2-nx (ChanseyIsTheBest/sa2-nx, GPL-3.0; its tools/post_codegen.py, with their measurements
in its docs/cpu-rounds.md), run in its order. Each is idempotent and writes only the files it changes.

2026-10-31 (round 1):
  pch_no_volatile.py   guest loads and stores without `volatile` (masseffect-nx +15-20 % fps; LoS1 18.4 -> 20.6 fps)
  loop_barriers.py     a compiler barrier at the loop headers that need one. REQUIRED with pch_no_volatile: without it
                       GCC turns guest polling loops into endless loops. Check every new ELF with find_self_loops.py.
2026-11-01 (round 2, the rest of sa2-nx's automatic steps that need no game-specific lists):
  pch_cold_traps.py    the trap helper (before every division) out of line and cold
  direct_calls.py      calls to functions without a hook become direct calls (GCC can inline them); a function any
                       app/SDK source or toml names counts as hooked
  guest_cas.py         guest compare-and-swap without the legacy builtin's trailing barrier on AArch64
  flush_mode_flow.py   float flush-mode checks the control flow proves compile to nothing on the Switch (the PC build
                       still tests them: RexFlushFlowMismatch)
  vec_byteswap.py      16-byte vector byte reversal without a table lookup on AArch64
  pch_neon_shuffles.py constant word shuffles as one permutation on AArch64
  ro_constants.py      loads from the image's read-only data as constants (the PC build still loads and compares:
                       RexRoMismatch, app/src/cpu/los_codegen_checks.cpp). Needs out/default_image.bin, the
                       generator's image dump (REXGLUE_DUMP_IMAGE=out/default_image.bin rexglue codegen ...), and
                       keeps the XEX's import records as loads (--xex)
  float_loads.py       single-precision loads straight into a float register
  loop_locals.py       the ctx registers of call-free loops as locals for the length of the loop
  leaf_locals.py       the ctx registers of leaf functions with loops as locals
  vperm_one_source.py  a vperm of one vector as a one-register table lookup
  vsel_neon.py         vsel as one NEON bit select
  inline_leaves.py     inline copies of the tiny leaf functions each file calls (last)
Left out (they need game-specific lists or runtime support LoS1 does not have yet): indirect_calls (a PC census of the
indirect call sites), frame_traffic (marks only), the banjo-nx round (leaf_dead_stores, dead_ctx_defs, single_floats,
fp_sign_ops, lr_preserve, flush_hoist, ro_constants_cfg, inline_memcalls, vec_unaligned_stores), store_cards.

  python tools/post_codegen.py [app/generated/default] [--round2]

Round 2 is OFF by default (2026-11-01): Mark's console run of the build with it (los_cleanup2.elf, same spot and
settings) gave 21.8-21.9 fps against 22.0-22.2 without it: about 1 % slower, so the default is round 1 only. Which of
its steps cost the time was not measured (the inline copies are the likeliest: more code for the A57's caches).
"""
import os
import subprocess
import sys

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(here)
args = [a for a in sys.argv[1:] if not a.startswith('--')]
folder = os.path.abspath(args[0] if args else os.path.join(root, 'app', 'generated', 'default'))
pch = next(os.path.join(folder, f) for f in sorted(os.listdir(folder)) if f.endswith('_pch.h'))
image = os.path.join(root, 'out', 'default_image.bin')
xex = os.path.join(root, 'assets', 'game_root', 'default.xex')
os.environ.setdefault('REXSDK_DIR', os.path.join(root, 'sdk'))


def run(name, *a):
    print(f'post_codegen: {name}', flush=True)
    subprocess.run([sys.executable, os.path.join(here, name), *a], check=True, cwd=root)


run('pch_no_volatile.py', pch)
run('loop_barriers.py', folder)
if '--round2' in sys.argv:
    if not os.path.isfile(image):
        sys.exit('post_codegen: out/default_image.bin is missing: run the code generator with '
                 'REXGLUE_DUMP_IMAGE=out/default_image.bin once')
    run('pch_cold_traps.py', pch)
    run('direct_calls.py', '--redo', '--gen', folder)
    run('guest_cas.py', '--gen', folder)
    run('flush_mode_flow.py', '--redo', '--gen', folder, '--image', image)
    run('vec_byteswap.py', '--gen', folder)
    run('pch_neon_shuffles.py', pch)
    run('ro_constants.py', '--gen', folder, '--image', image, *(['--xex', xex] if os.path.isfile(xex) else []))
    run('float_loads.py', '--gen', folder)
    run('loop_locals.py', '--gen', folder)
    run('leaf_locals.py', '--gen', folder)
    run('vperm_one_source.py', '--gen', folder)
    run('vsel_neon.py', '--gen', folder)
    run('inline_leaves.py', '--gen', folder)
print('post_codegen: done')
