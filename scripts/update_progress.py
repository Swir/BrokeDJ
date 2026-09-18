#!/usr/bin/env python3
"""Generate BrokeDJ SVG-only roadmap progress assets from docs/progress.json."""
# SPDX-License-Identifier: AGPL-3.0-only
import argparse
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets' / 'readme'
LEGACY = (
    re.compile(r'[█▓▒░]{4,}'),
    re.compile(r'\[(?:[#=\-]{6,})\]'),
)

def read_state():
    data = json.loads((ROOT / 'docs/progress.json').read_text(encoding='utf-8'))
    milestones = data['milestones']
    if not milestones or any(type(item.get('complete')) is not bool for item in milestones):
        raise ValueError('Every milestone requires an explicit boolean completion value')
    if len({item['id'] for item in milestones}) != len(milestones):
        raise ValueError('Milestone IDs must be unique')
    done = sum(item['complete'] for item in milestones)
    total = len(milestones)
    return done, total, done / total

def card(done, total, fraction):
    percent = fraction * 100.0
    fill = 1100.0 * fraction
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="180" viewBox="0 0 1200 180" role="img" aria-labelledby="title desc">\n<title id="title">BrokeDJ engineering roadmap: {done}/{total} milestones, {percent:.1f}%</title>\n<desc id="desc">Equal-weight roadmap milestone count from docs/progress.json. This is not sound-quality, release-readiness, or live-performance readiness.</desc>\n<defs><linearGradient id="bg" x1="0" y1="0" x2="1" y2="1"><stop stop-color="#02050A"/><stop offset="1" stop-color="#07111C"/></linearGradient><linearGradient id="fill" x1="0" y1="0" x2="1" y2="0"><stop stop-color="#0088FF"/><stop offset="1" stop-color="#62E5FF"/></linearGradient></defs>\n<rect x="1" y="1" width="1198" height="178" rx="22" fill="url(#bg)" stroke="#62E5FF" stroke-opacity=".24"/>\n<text x="50" y="39" fill="#62E5FF" font-family="Segoe UI,Arial,sans-serif" font-size="14" font-weight="700" letter-spacing="3">BROKEDJ · ENGINEERING ROADMAP</text>\n<text x="50" y="76" fill="#F4FAFF" font-family="Segoe UI,Arial,sans-serif" font-size="27" font-weight="800">M0–M9 verified milestone scope</text>\n<text x="1150" y="76" text-anchor="end" fill="#F4FAFF" font-family="Segoe UI,Arial,sans-serif" font-size="30" font-weight="800">{percent:.1f}%</text>\n<rect x="50" y="96" width="1100" height="16" rx="8" fill="#0B1928" stroke="#62E5FF" stroke-opacity=".16"/>\n<rect x="50" y="96" width="{fill:.1f}" height="16" rx="8" fill="url(#fill)"/>\n<text x="50" y="142" fill="#8DA8B8" font-family="Segoe UI,Arial,sans-serif" font-size="13">{done}/{total} milestones complete · PRE-ALPHA · source: docs/progress.json</text>\n<text x="1150" y="142" text-anchor="end" fill="#8DA8B8" font-family="Segoe UI,Arial,sans-serif" font-size="13">Not a live-readiness score</text>\n</svg>\n'''

def mini(done, total, fraction):
    percent = fraction * 100.0
    fill = 700.0 * fraction
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="900" height="72" viewBox="0 0 900 72" role="img" aria-labelledby="title desc">\n<title id="title">BrokeDJ roadmap {done}/{total} milestones, {percent:.1f}%</title>\n<desc id="desc">Equal-weight milestone count from docs/progress.json; not release readiness.</desc>\n<defs><linearGradient id="fill" x1="0" y1="0" x2="1" y2="0"><stop stop-color="#0088FF"/><stop offset="1" stop-color="#62E5FF"/></linearGradient></defs>\n<rect x="1" y="1" width="898" height="70" rx="16" fill="#02050A" stroke="#62E5FF" stroke-opacity=".24"/>\n<text x="24" y="29" fill="#F4FAFF" font-family="Segoe UI,Arial,sans-serif" font-size="16" font-weight="700">BROKEDJ</text>\n<text x="24" y="51" fill="#8DA8B8" font-family="Segoe UI,Arial,sans-serif" font-size="11">{done}/{total} milestones · {percent:.1f}%</text>\n<rect x="170" y="26" width="700" height="16" rx="8" fill="#0B1928"/>\n<rect x="170" y="26" width="{fill:.1f}" height="16" rx="8" fill="url(#fill)"/>\n</svg>\n'''

def template():
    return '''<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="180" viewBox="0 0 1200 180" role="img" aria-labelledby="title desc">\n<title id="title">BrokeDJ progress template — not project data</title>\n<desc id="desc">Reusable local template only. Generate live progress from docs/progress.json.</desc>\n<rect x="1" y="1" width="1198" height="178" rx="22" fill="#02050A" stroke="#62E5FF" stroke-opacity=".24"/>\n<text x="50" y="48" fill="#62E5FF" font-family="Segoe UI,Arial,sans-serif" font-size="18" font-weight="700">TEMPLATE · NOT PROJECT DATA</text>\n<text x="50" y="86" fill="#F4FAFF" font-family="Segoe UI,Arial,sans-serif" font-size="28" font-weight="800">Generate from docs/progress.json</text>\n<rect x="50" y="112" width="1100" height="16" rx="8" fill="#07111C" stroke="#62E5FF" stroke-opacity=".20"/>\n</svg>\n'''

def check_no_legacy_meter():
    for rel in ('README.md', 'ROADMAP.md'):
        text = (ROOT / rel).read_text(encoding='utf-8')
        for pattern in LEGACY:
            if pattern.search(text):
                raise ValueError(f'Legacy text progress meter found in {rel}')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    done, total, fraction = read_state()
    expected = {
        OUT / 'progress-card.svg': card(done, total, fraction),
        OUT / 'progress-mini.svg': mini(done, total, fraction),
        OUT / 'progress-template.svg': template(),
    }
    if args.check:
        check_no_legacy_meter()
        stale = [str(path.relative_to(ROOT)) for path, content in expected.items()
                 if not path.exists() or path.read_text(encoding='utf-8') != content]
        if stale:
            print('Stale progress assets: ' + ', '.join(stale), file=sys.stderr)
            return 1
    else:
        OUT.mkdir(parents=True, exist_ok=True)
        for path, content in expected.items():
            path.write_text(content, encoding='utf-8')
    print(f'Roadmap: {done}/{total} milestones ({fraction * 100.0:.1f}%), not production readiness')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
