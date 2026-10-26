"""Replaces git symbolic links that were checked out as plain text files with copies of their targets.

On Windows, git writes a symlink as a small text file holding the target path unless symlinks are enabled
(Developer Mode or admin rights). The SDK's libmspack submodule has 15 such links to its C sources
(thirdparty/libmspack/cabextract/mspack/*.c and *.h), and the compiler then reads the path as code
("lzxd.c:1:1: error: expected identifier"). This finds every symlink git knows about in the tree and all its
submodules, and when the checked-out file is a text placeholder whose target is a regular file, copies the
target over it. Links to folders and links that are already real are left alone. Safe to run again.

Usage: python tools/fix_symlinks.py <git checkout>
"""
import os
import shutil
import subprocess
import sys


def git(cwd, *args):
    return subprocess.run(['git', '-C', cwd, *args], capture_output=True, text=True, check=True).stdout


def repos(root):
    yield root
    out = git(root, 'submodule', 'status', '--recursive')
    for line in out.splitlines():
        parts = line.strip().split()
        if len(parts) >= 2:
            yield os.path.join(root, parts[1])


def links(repo):
    for line in git(repo, 'ls-files', '-s').splitlines():
        meta, _, path = line.partition('\t')
        if meta.split()[0] == '120000':
            yield os.path.join(repo, path)


def main():
    root = os.path.abspath(sys.argv[1])
    fixed = skipped = 0
    for repo in repos(root):
        if not os.path.isdir(repo):
            continue
        for link in links(repo):
            if os.path.islink(link) or not os.path.isfile(link) or os.path.getsize(link) > 1024:
                continue  # a real symlink, missing, or already replaced by real content
            with open(link, 'r', encoding='utf-8', errors='replace') as f:
                target_rel = f.read().strip()
            target = os.path.normpath(os.path.join(os.path.dirname(link), target_rel))
            if os.path.isfile(target) and not os.path.samefile(target, link):
                shutil.copyfile(target, link)
                fixed += 1
            else:
                skipped += 1  # a link to a folder (MoltenVK, macOS only) or to nothing
    print(f'symlink placeholders replaced: {fixed}, left alone: {skipped}')


if __name__ == '__main__':
    main()
