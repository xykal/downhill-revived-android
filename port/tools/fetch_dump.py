"""Fetch the player's SCUS_971.77 (or the ISO holding it) and gate it for the build.

usage: python port/tools/fetch_dump.py <https URL> <output ELF path>

Downloads the URL with curl, following a Drive-style virus-scan confirm page when one comes
back (parse the form, re-request — the ELF a player uploads may be their whole .iso). Accepts
either the bare ELF or an ISO9660/UDF disc image and extracts the root executable with
pycdlib. The output must be the USA dump BUILDING.md and config/splat.yaml require:

    sha1(SCUS_971.77) = 4e25a222ad8a475b5d26c2e928e90b5f2b7075eb

Anything else (another region's disc — SLES/SLUS — a corrupt rip, an HTML page) fails the
step; the game's decomp match, VU1 AOT tables and data fixups are all for that binary.
BYOD: the URL is the player's own dump behind DHD_ELF_URL (a repository secret), never a
committed or fetched game copy.
"""
import hashlib
import html
import os
import re
import subprocess
import sys
import tempfile

EXPECTED_SHA1 = '4e25a222ad8a475b5d26c2e928e90b5f2b7075eb'
ELF_MAGIC = b'\x7fELF'
ISO_MAGIC = b'CD001'  # at 32769


def curl(url, out, tries=4):
    """url -> out (follows redirects); returns False when curl cannot deliver the body."""
    for i in range(tries):
        r = subprocess.run(['curl', '-fsSL', '--retry', '3', '--max-time', '1800',
                            '-o', out, url], capture_output=True, text=True)
        if r.returncode == 0 and os.path.getsize(out) > 0:
            return True
        sys.stderr.write('curl attempt %d failed: %s\n' % (i + 1, r.stderr.strip()[-300:]))
    return False


def looks_like_html(path):
    with open(path, 'rb') as f:
        head = f.read(256).lstrip().lower()
    return head.startswith(b'<!doctype') or head.startswith(b'<html')


def drive_confirm(page, url):
    """The confirm form of a Drive virus-scan page: its hidden inputs, as a new URL."""
    text = open(page, encoding='utf-8', errors='replace').read()
    inputs = {}
    for tag in re.findall(r'<input\b[^>]*>', text):
        n, v = re.search(r'name="([^"]+)"', tag), re.search(r'value="([^"]*)"', tag)
        if n and v:
            inputs[n.group(1)] = html.unescape(v.group(1))
    if 'id' not in inputs:
        raise SystemExit('fetch_dump: HTML but no download form at %s' % url)
    from urllib.parse import urlencode
    return 'https://drive.usercontent.google.com/download?' + urlencode(inputs)


def extract_from_iso(iso_path, elf_path):
    try:
        import pycdlib
    except ImportError:
        raise SystemExit('fetch_dump: pip install pycdlib (to read the player\'s .iso)')
    iso = pycdlib.PyCdlib()
    iso.open(iso_path)
    try:
        files = None
        for path, _dirs, names in iso.walk(iso_path='/'):
            if path == '/':
                files = names
                break
        main = [n for n in (files or []) if n.upper().startswith(('SCUS_', 'SCUS-'))]
        if len(main) != 1:
            raise SystemExit('fetch_dump: no single SCUS_* executable in the ISO root (%s) — '
                             'the dump must be the USA disc (SCUS_971.77)' % (files,))
        iso.get_file_from_iso_fp(open(elf_path, 'wb'), iso_path='/' + main[0])
    finally:
        iso.close()
    print('fetch_dump: extracted /%s from the disc image' % main[0])


def main(argv):
    if len(argv) != 3:
        raise SystemExit(__doc__.split('\n')[0] + '\nusage: fetch_dump.py <url> <out ELF>')
    url, out = argv[1], argv[2]
    os.makedirs(os.path.dirname(os.path.abspath(out)) or '.', exist_ok=True)
    # temp beside the output (the image can exceed a small /tmp; keeps one filesystem)
    tmp = out + '.download.tmp'
    if not curl(url, tmp):
        raise SystemExit('fetch_dump: download failed from %s' % url)
    if looks_like_html(tmp):
        print('fetch_dump: confirm page, re-requesting')
        if not curl(drive_confirm(tmp, url), tmp) or looks_like_html(tmp):
            raise SystemExit('fetch_dump: still HTML after the confirm request')
    with open(tmp, 'rb') as f:
        head = f.read(4)
        if head == ELF_MAGIC:
            os.replace(tmp, out)
        else:
            f.seek(32769)  # ISO9660's Primary Volume Volume Descriptor
            is_iso = f.read(5) == ISO_MAGIC
        if head != ELF_MAGIC:
            if not is_iso:
                raise SystemExit('fetch_dump: neither ELF nor ISO9660 (starts %r)' % head[:16])
            extract_from_iso(tmp, out)
            os.unlink(tmp)
    data = open(out, 'rb').read()
    sha1 = hashlib.sha1(data).hexdigest()
    if sha1 != EXPECTED_SHA1:
        raise SystemExit('fetch_dump: sha1 %s != %s — wrong file or region (need the USA '
                         'SCUS_971.77; see docs/BUILDING.md §2)' % (sha1, EXPECTED_SHA1))
    print('fetch_dump: OK sha1 %s (%d bytes) -> %s' % (sha1, len(data), out))


if __name__ == '__main__':
    main(sys.argv)
