#!/usr/bin/env python3
"""
Scan the repository for directories containing HTML files, create/update index.html
for each directory, and fix broken relative links by searching for matching filenames
in the repo. Also updates the top-level index.htm.

Usage: python3 tools/update_indices.py
"""
import os
import re
from pathlib import Path
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parent.parent

HTML_EXTS = {'.html', '.htm'}

def is_local_link(href: str) -> bool:
    if not href:
        return False
    href = href.strip()
    if href.startswith('#'):
        return False
    p = urlparse(href)
    if p.scheme and p.scheme not in ('',):
        return False
    if href.startswith('//'):
        return False
    return True

def find_all_html_files():
    files = []
    for p in ROOT.rglob('*'):
        if p.is_file() and p.suffix.lower() in HTML_EXTS:
            files.append(p)
    return files

def build_name_index(files):
    by_name = {}
    for f in files:
        by_name.setdefault(f.name, []).append(f)
    return by_name

LINK_RE = re.compile(r'(href|src)=("|\')(?P<url>.*?)(\2)', re.IGNORECASE)

def scan_and_fix_links(all_files, name_index):
    broken = []
    multiple = []
    fixed_count = 0
    for src in all_files:
        text = src.read_text(encoding='utf-8', errors='ignore')
        changed = False
        def repl(m):
            nonlocal changed, fixed_count
            attr = m.group(1)
            url = m.group('url')
            if not is_local_link(url):
                return m.group(0)
            # ignore absolute filesystem-like starting with / (treat as repo-root relative)
            # We'll try resolve relative to src.parent
            candidate = (src.parent / url).resolve()
            if candidate.exists():
                return m.group(0)
            # remove query and fragment
            url_clean = url.split('?',1)[0].split('#',1)[0]
            base = os.path.basename(url_clean)
            if not base:
                broken.append((src, url))
                return m.group(0)
            matches = name_index.get(base, [])
            if len(matches) == 1:
                target = matches[0]
                rel = os.path.relpath(target, start=src.parent)
                replaced = f'{attr}={m.group(2)}{rel}{m.group(2)}'
                changed = True
                fixed_count += 1
                return replaced
            elif len(matches) > 1:
                multiple.append((src, url, matches))
                return m.group(0)
            else:
                broken.append((src, url))
                return m.group(0)

        new_text = LINK_RE.sub(repl, text)
        if changed:
            src.write_text(new_text, encoding='utf-8')
    return fixed_count, broken, multiple

def write_index_for_dir(d: Path):
    items = []
    for p in sorted(d.iterdir(), key=lambda x: (x.is_file(), x.name.lower())):
        if p.name.lower().startswith('.'):
            continue
        if p.is_dir():
            # link to directory (will be resolved by web server to index.html)
            items.append((p.name+'/', p.name+'/', 'dir'))
        elif p.suffix.lower() in HTML_EXTS and p.name.lower() not in ('index.html','index.htm'):
            items.append((p.name, p.name, 'file'))
    if not items:
        return False
    lines = [
        '<!doctype html>',
        '<html>',
        '<head><meta charset="utf-8"><title>Index of %s</title></head>' % (d.name or '/'),
        '<body>',
        '<h1>Index of %s</h1>' % (d.name or '/'),
        '<ul>'
    ]
    for text, href, kind in items:
        lines.append(f'<li><a href="{href}">{text}</a></li>')
    lines += ['</ul>', '</body>', '</html>']
    index_path = d / 'index.html'
    index_path.write_text('\n'.join(lines), encoding='utf-8')
    return True

def main():
    all_html = find_all_html_files()
    name_index = build_name_index(all_html)

    # Create/update index.html in each directory
    dirs = set(p.parent for p in all_html)
    created = 0
    updated = 0
    for d in sorted(dirs):
        p = d / 'index.html'
        # always regenerate index.html to reflect current contents
        ok = write_index_for_dir(d)
        if ok:
            if p.exists():
                updated += 1
            else:
                created += 1

    # Now scan and fix links
    fixed_count, broken, multiple = scan_and_fix_links(all_html, name_index)

    # Also update top-level index.htm if present
    top = ROOT / 'index.htm'
    top_updated = False
    if top.exists():
        # treat it like other files and try to fix links inside
        fixed2, broken2, multiple2 = scan_and_fix_links([top], name_index)
        fixed_count += fixed2
        broken += broken2
        multiple += multiple2
        top_updated = fixed2 > 0

    print('Summary:')
    print('  directories processed:', len(dirs))
    print('  index.html created/updated: created=%d updated=%d' % (created, updated))
    print('  links fixed:', fixed_count)
    print('  unresolved broken links:', len(broken))
    if broken:
        for src, url in broken[:20]:
            print('    MISSING in %s -> %s' % (src.relative_to(ROOT), url))
    if multiple:
        print('  ambiguous links (multiple candidates):', len(multiple))
        for src, url, matches in multiple[:20]:
            print('    AMBIG %s -> %s candidates:' % (src.relative_to(ROOT), url))
            for m in matches[:5]:
                print('      - %s' % m.relative_to(ROOT))

if __name__ == '__main__':
    main()
