"""Fetches the third-party code of Renut's renderer into app/thirdparty, from sa2-nx at a pinned commit.

Renut's renderer (app/src/renut_engine, tracked here) builds on four projects that sa2-nx carries with its own and
banjo-nx's changes: plume (the graphics layer, with banjo-nx's deko3d back end), reblue-XenosRecomp (the shader
translator, with DXC), uam (the deko3d shader compiler) and Dear ImGui. They are taken as they are, so they are not
stored in this repository: this script gets exactly sa2-nx's copies. Licences: see each folder and
docs/third-party.md.

Usage:
    python tools/fetch_renut_deps.py                 sparse-clone sa2-nx from GitHub (needs git)
    python tools/fetch_renut_deps.py --source DIR    copy from a local sa2-nx checkout instead
"""
import argparse
import os
import shutil
import stat
import subprocess
import sys
import tempfile

UPSTREAM = 'https://github.com/ChanseyIsTheBest/sa2-nx.git'
COMMIT = 'adf3a4eee524dcca5cd7c1b42454ddb8a09124a1'  # sa2-nx 1.0.3.1, 2026-10-20
PARTS = ['plume', 'reblue-XenosRecomp', 'uam', 'imgui']

# DXC, the shader compiler Renut's translator uses: upstream XenosRecomp takes it as a submodule (renderbag/dxc-bin),
# and sa2-nx's copy of the folder leaves its binaries out. Only this machine's files are fetched.
DXC_UPSTREAM = 'https://github.com/renderbag/dxc-bin.git'
DXC_COMMIT = '12395e36295f950361ca842d8cc461492412facc'
DXC_FILES = {
    'win32': ['bin/x64/dxc.exe', 'bin/x64/dxcompiler.dll', 'bin/x64/dxil.dll', 'lib/x64/dxcompiler.lib'],
    'linux': ['bin/x64/dxc-linux', 'lib/x64/libdxcompiler.so', 'lib/x64/libdxil.so'],
}


def run(*args, cwd=None):
    print('>', ' '.join(args))
    subprocess.run(args, cwd=cwd, check=True)


def remove_readonly(func, path, _):
    os.chmod(path, stat.S_IWRITE)
    func(path)


def resolve_symlink_stubs(target):
    """Git for Windows without symlink support writes a symbolic link as a small text file holding its target path.
    Replaces each such stub that points to an existing file with that file (same rule as fetch_thirdparty.py)."""
    fixed = 0
    for dirpath, dirnames, filenames in os.walk(target):
        dirnames[:] = [d for d in dirnames if d != '.git']
        for name in filenames:
            path = os.path.join(dirpath, name)
            if os.path.islink(path) or not 0 < os.path.getsize(path) <= 300:
                continue
            with open(path, 'rb') as f:
                data = f.read().strip()
            if b'\n' in data or b' ' in data or b'/' not in data:
                continue
            linked = os.path.normpath(os.path.join(dirpath, data.decode('ascii', 'replace')))
            if os.path.isfile(linked) and os.path.realpath(linked) != os.path.realpath(path):
                shutil.copyfile(linked, path)
                fixed += 1
    return fixed


def copy_parts(source_app_thirdparty, target):
    for part in PARTS:
        src = os.path.join(source_app_thirdparty, part)
        if not os.path.isdir(src):
            sys.exit(f'{src} not found')
        dst = os.path.join(target, part)
        if os.path.isdir(dst):
            shutil.rmtree(dst, onerror=remove_readonly)
        shutil.copytree(src, dst, symlinks=False, ignore=shutil.ignore_patterns('.git'))
        print(f'{part}: copied')


def fetch_dxc(target):
    files = DXC_FILES.get('win32' if sys.platform.startswith('win') else sys.platform)
    if not files:
        print(f'no DXC files listed for {sys.platform}: skipped')
        return
    dst = os.path.join(target, 'reblue-XenosRecomp', 'thirdparty', 'dxc-bin')
    if all(os.path.isfile(os.path.join(dst, f)) for f in files):
        print('dxc-bin: already there')
        return
    work = tempfile.mkdtemp(prefix='dxc-bin-')
    try:
        run('git', 'init', '-q', work)
        run('git', 'remote', 'add', 'origin', DXC_UPSTREAM, cwd=work)
        run('git', 'sparse-checkout', 'set', '--no-cone', *['/' + f for f in files], cwd=work)
        run('git', 'fetch', '-q', '--depth', '1', '--filter=blob:none', 'origin', DXC_COMMIT, cwd=work)
        run('git', 'checkout', '-q', 'FETCH_HEAD', cwd=work)
        for f in files:
            os.makedirs(os.path.dirname(os.path.join(dst, f)), exist_ok=True)
            shutil.copy2(os.path.join(work, f), os.path.join(dst, f))
            if not sys.platform.startswith('win'):
                os.chmod(os.path.join(dst, f), 0o755)
        print('dxc-bin:', ', '.join(files))
    finally:
        shutil.rmtree(work, onerror=remove_readonly)


def apply_patches(root, target):
    """LoS1-nx's own changes to this code: patches/renut-deps/<part>-*.patch, each relative to its part's folder."""
    folder = os.path.join(root, 'patches', 'renut-deps')
    if not os.path.isdir(folder):
        return
    for name in sorted(os.listdir(folder)):
        if not name.endswith('.patch'):
            continue
        part = next((p for p in PARTS if name.startswith(p + '-')), None)
        if not part:
            sys.exit(f'{name}: the name must start with one of {PARTS}')
        cwd, patch = os.path.join(target, part), os.path.join(folder, name)
        if subprocess.run(['git', 'apply', '--check', patch], cwd=cwd, capture_output=True).returncode == 0:
            run('git', 'apply', patch, cwd=cwd)
        elif subprocess.run(['git', 'apply', '-R', '--check', patch], cwd=cwd, capture_output=True).returncode == 0:
            print(f'{name}: already applied')
        else:
            sys.exit(f'{name} does not apply to {cwd}')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--source', help='local sa2-nx checkout to copy from')
    a = ap.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    target = os.path.join(root, 'app', 'thirdparty')
    os.makedirs(target, exist_ok=True)

    if a.source:
        copy_parts(os.path.join(os.path.abspath(a.source), 'app', 'thirdparty'), target)
    else:
        work = tempfile.mkdtemp(prefix='sa2-nx-')
        try:
            run('git', 'init', '-q', work)
            run('git', 'remote', 'add', 'origin', UPSTREAM, cwd=work)
            run('git', 'sparse-checkout', 'set', '--no-cone', *[f'/app/thirdparty/{p}/' for p in PARTS], cwd=work)
            run('git', 'fetch', '-q', '--depth', '1', '--filter=blob:none', 'origin', COMMIT, cwd=work)
            run('git', 'checkout', '-q', 'FETCH_HEAD', cwd=work)
            copy_parts(os.path.join(work, 'app', 'thirdparty'), target)
        finally:
            shutil.rmtree(work, onerror=remove_readonly)
    fetch_dxc(target)
    apply_patches(root, target)
    stubs = resolve_symlink_stubs(target)
    if stubs:
        print(f'{stubs} symbolic link stubs replaced by the files they point to')
    with open(os.path.join(target, 'FETCHED_FROM.txt'), 'w') as f:
        f.write(f'{UPSTREAM} {COMMIT}\n{DXC_UPSTREAM} {DXC_COMMIT}\n')
    print('done:', ', '.join(PARTS))


if __name__ == '__main__':
    main()
