#!/usr/bin/env python3
# sa2-nx: copied from banjo-nx on 2026-10-19 (its codegen round), adapted to sa2-nx's tree.
"""banjo-nx (package 27, codegen round): the guest's vsel as one NEON bit select.

The generator writes VMX vsel (vD = (vA & ~vC) | (vB & vC)) through simde's SSE2 forms:
    simde_mm_or_si128(simde_mm_andnot_si128(C, A), simde_mm_and_si128(C, B))
which GCC fuses into one BSL only some of the time (2 of 6 in sub_829A6DE8, a culling routine of 1.5 % of the console
profile): the others are BIC, AND and ORR. REX_VSEL(C, A, B) is vbslq_u8(C, B, A) on AArch64 (each bit from B where C
is set, from A where it is clear: the same bits) and the simde expression elsewhere. 2,717 sites in the generated code.
Exact: a bitwise operation, the same result bit for bit. REX_VSEL_OFF (CMake BANJO_GEN_VSEL_NEON=OFF) keeps simde's.

usage: vsel_neon.py --gen <app/generated> [--restore]
"""
import glob
import os
import re
import sys

PCH_MACRO = '''
// banjo-nx (package 27): tools/vsel_neon.py. VMX vsel, (A & ~C) | (B & C), as one NEON bit select on AArch64.
#ifndef REX_VSEL
#if defined(__aarch64__) && !defined(REX_VSEL_OFF)
#define REX_VSEL(c, a, b)                                                                      \\
  simde__m128i_from_neon_u8(vbslq_u8(simde__m128i_to_neon_u8(c), simde__m128i_to_neon_u8(b), \\
                                     simde__m128i_to_neon_u8(a)))
#else
#define REX_VSEL(c, a, b) simde_mm_or_si128(simde_mm_andnot_si128((c), (a)), simde_mm_and_si128((c), (b)))
#endif
#endif
'''
HEAD = 'simde_mm_or_si128(simde_mm_andnot_si128('


def split_args(s, start):
    """The comma-separated arguments of the call whose '(' is at s[start - 1], and the index after its ')'."""
    depth, args, cur = 0, [], start
    i = start
    while i < len(s):
        c = s[i]
        if c == '(':
            depth += 1
        elif c == ')':
            if depth == 0:
                args.append(s[cur:i].strip())
                return args, i + 1
            depth -= 1
        elif c == ',' and depth == 0:
            args.append(s[cur:i].strip())
            cur = i + 1
        i += 1
    return None, None


def rewrite_line(line):
    out, pos, n = [], 0, 0
    while True:
        k = line.find(HEAD, pos)
        if k < 0:
            out.append(line[pos:])
            break
        inner, after_andnot = split_args(line, k + len(HEAD))
        ok = False
        if inner and len(inner) == 2 and line.startswith(', simde_mm_and_si128(', after_andnot):
            second, after_and = split_args(line, after_andnot + len(', simde_mm_and_si128('))
            if second and len(second) == 2 and second[0] == inner[0] and after_and < len(line) and \
                    line[after_and] == ')':
                out.append(line[pos:k])
                out.append(f'REX_VSEL({inner[0]}, {inner[1]}, {second[1]})')
                pos = after_and + 1
                n += 1
                ok = True
        if not ok:
            out.append(line[pos:k + len(HEAD)])
            pos = k + len(HEAD)
    return ''.join(out), n


def main():
    args = sys.argv[1:]
    gen = args[args.index('--gen') + 1]
    if '--restore' in args:
        for path in glob.glob(os.path.join(gen, '*_recomp.*.cpp')):
            t = open(path, encoding='utf-8').read()
            if 'REX_VSEL(' in t:
                def back(m):
                    a, _ = split_args(m.string, m.end())
                    return m.group(0)
                # Restoring needs the arguments: rebuild each REX_VSEL(C, A, B) as the simde expression.
                out, pos = [], 0
                while True:
                    k = t.find('REX_VSEL(', pos)
                    if k < 0:
                        out.append(t[pos:])
                        break
                    a, end = split_args(t, k + len('REX_VSEL('))
                    out.append(t[pos:k])
                    out.append(f'{HEAD}{a[0]}, {a[1]}), simde_mm_and_si128({a[0]}, {a[2]}))')
                    pos = end
                open(path, 'w', encoding='utf-8', newline='\n').write(''.join(out))
        return
    pch = next(iter(sorted(glob.glob(os.path.join(gen, '*_pch.h')))), '')  # LoS1-nx: los_pch.h
    if os.path.isfile(pch):
        p = open(pch, encoding='utf-8').read()
        if 'REX_VSEL' not in p:
            open(pch, 'w', encoding='utf-8', newline='\n').write(p + PCH_MACRO)
    total = 0
    for path in sorted(glob.glob(os.path.join(gen, '*_recomp.*.cpp'))):
        lines = open(path, encoding='utf-8').read().split('\n')
        changed = 0
        for i, l in enumerate(lines):
            if HEAD in l:
                new, n = rewrite_line(l)
                if n:
                    lines[i] = new
                    changed += n
        if changed:
            open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines))
            total += changed
    print(f'vsel_neon: {total} vsel as one NEON bit select')


if __name__ == '__main__':
    main()
