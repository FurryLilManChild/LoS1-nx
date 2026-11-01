#!/usr/bin/env python3
"""LoS1-nx's post-codegen steps (2026-10-31), run after every `rexglue codegen los_manifest.toml`.

From sa2-nx (ChanseyIsTheBest/sa2-nx, GPL-3.0; its tools/post_codegen.py), in the order it runs them. Each step is
idempotent and writes only the files it changes.
  1. pch_no_volatile.py: guest loads and stores without `volatile` (masseffect-nx measured +15-20 % fps).
  2. loop_barriers.py: a compiler barrier at the loop headers that need one. REQUIRED with step 1: without it GCC turns
     guest polling loops (a wait for another thread or the GPU) into endless loops on the Switch. Check a new ELF with
     find_self_loops.py.

  python tools/post_codegen.py [app/generated/default]
"""
import os
import subprocess
import sys

here = os.path.dirname(os.path.abspath(__file__))
folder = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, '..', 'app', 'generated', 'default')
pch = next(os.path.join(folder, f) for f in os.listdir(folder) if f.endswith('_pch.h'))
for cmd in (['pch_no_volatile.py', pch], ['loop_barriers.py', folder]):
    subprocess.run([sys.executable, os.path.join(here, cmd[0])] + cmd[1:], check=True)
