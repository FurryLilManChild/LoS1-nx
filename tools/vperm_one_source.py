#!/usr/bin/env python3
# sa2-nx: copied from banjo-nx on 2026-10-19 (its codegen round), adapted to sa2-nx's tree.
"""A vperm of one vector as a one-register table lookup (from sasrt-nx's tool of 2026-10-14, idea 8 of sa2-nx's
list; banjo-nx's copy).
The generator writes every vperm as rex::ppc::simde_mm_perm_epi8_(A, B, C): on AArch64 a two-register TBL, whose
table is a pair of consecutive registers. When A and B are the same register the result only ever takes bytes of A
(bit 4 of the control picks between two equal halves), so
    rex::ppc::simde_mm_perm_epi8_(L, L, C)   ->   REX_VPERM1(L, C)
where L is the same load of the same register text; REX_VPERM1 is rex::ppc::simde_mm_perm1_epi8_
(sdk/include/rex/ppc/intrinsics.h: one one-register TBL with the index (C & 0x0F) ^ 0x0F; x86: one pshufb). Exact for
every control byte; sdk/tests/vmx_helpers_neon_test.cpp compares the helper with the two-source form.
In Banjo (2026-10-14): 282 of the 2,145 vperm in 77 generated files.
REX_VPERM1_OFF (compile time) gives the two-source call back.
  python tools/vperm_one_source.py --gen app/generated [--check]
"""
import argparse
import os
import re
import sys

LOAD = r'simde_mm_load_si128\(\(simde__m128i\*\)(?:ctx\.|l_|ll_)?v\d+\.u8\)'
PATTERN = re.compile(r'rex::ppc::simde_mm_perm_epi8_\((' + LOAD + r'), \1, ')
MARKER = '// rexglue ports: a vperm of one vector as a one-register lookup (tools/vperm_one_source.py)'
END = '// (end of the macros of tools/vperm_one_source.py)\n'
MACROS = MARKER + '''
#ifndef REX_VPERM1
#if defined(REX_VPERM1_OFF)
#define REX_VPERM1(a, c) rex::ppc::simde_mm_perm_epi8_((a), (a), (c))
#else
#define REX_VPERM1(a, c) rex::ppc::simde_mm_perm1_epi8_((a), (c))
#endif  // REX_VPERM1_OFF
#endif  // REX_VPERM1
''' + END


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--gen', required=True, help='generated folder (*_recomp.N.cpp and *_pch.h)')
    ap.add_argument('--check', action='store_true', help='count only, write nothing')
    a = ap.parse_args()
    files = sorted(f for f in os.listdir(a.gen) if re.match(r'.*_recomp\.\d+\.cpp$', f))
    pchs = [f for f in os.listdir(a.gen) if f.endswith('_pch.h')]
    if not files or len(pchs) != 1:
        sys.exit('ERROR (post-codegen patch): vperm_one_source: no *_recomp.N.cpp or no single *_pch.h in %s' % a.gen)
    rewritten = already = two_source = changed = 0
    for name in files:
        path = os.path.join(a.gen, name)
        text = open(path, encoding='utf-8').read()
        already += text.count('REX_VPERM1(')
        new, count = PATTERN.subn(r'REX_VPERM1(\1, ', text)
        two_source += new.count('rex::ppc::simde_mm_perm_epi8_(')
        if count:
            rewritten += count
            changed += 1
            if not a.check:
                open(path, 'w', encoding='utf-8', newline='\n').write(new)
    pch = os.path.join(a.gen, pchs[0])
    text = open(pch, encoding='utf-8').read()
    if MACROS not in text:
        anchor = '#define REX_LOAD_U8(x)'
        if MARKER in text:
            start = text.index(MARKER)
            end = text.index(END, start) + len(END)
            text = text[:start] + MACROS + text[end:]
        elif anchor in text:
            text = text.replace(anchor, MACROS + anchor, 1)
        else:
            sys.exit('ERROR (post-codegen patch): vperm_one_source: no REX_LOAD_U8 in %s: the generated pch changed' % pch)
        if not a.check:
            open(pch, 'w', encoding='utf-8', newline='\n').write(text)
    print('vperm_one_source%s: %d vperm of one vector written as one-register lookups in %d files (%d already in that '
          'form); %d two-source vperm left' % (' (check only)' if a.check else '', rewritten, changed, already,
                                                two_source))


if __name__ == '__main__':
    main()
