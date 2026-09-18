#!/usr/bin/env python3
"""Render the roadmap's explicit milestone count; --check never changes files."""
# SPDX-License-Identifier: AGPL-3.0-only
import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    data = json.loads((ROOT / 'docs/progress.json').read_text(encoding='utf-8'))
    milestones = data['milestones']
    if not milestones or any(type(item.get('complete')) is not bool for item in milestones):
        raise ValueError('Every milestone requires an explicit boolean completion value')
    if len({item['id'] for item in milestones}) != len(milestones):
        raise ValueError('Milestone IDs must be unique')
    done = sum(item['complete'] for item in milestones)
    total = len(milestones)
    percent = round(100 * done / total)
    width = 760 * done / total
    svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="820" height="138" viewBox="0 0 820 138" role="img" aria-labelledby="title desc">
<title id="title">BrokeDJ roadmap: {done}/{total} milestones ({percent}%)</title>
<desc id="desc">Equal-weight milestone count, not production readiness or time remaining.</desc>
<rect width="820" height="138" rx="18" fill="#0e1a2c"/>
<text x="30" y="37" fill="#8bacd2" font-family="Segoe UI,Arial,sans-serif" font-size="13" letter-spacing="2">ENGINEERING ROADMAP</text>
<text x="790" y="39" fill="#dbeeff" text-anchor="end" font-family="Segoe UI,Arial,sans-serif" font-size="21" font-weight="700">{done}/{total} MILESTONES · {percent}%</text>
<rect x="30" y="60" width="760" height="12" rx="6" fill="#203149"/>
<rect x="30" y="60" width="{width:.1f}" height="12" rx="6" fill="#399bff"/>
<text x="30" y="106" fill="#8bacd2" font-family="Segoe UI,Arial,sans-serif" font-size="13">PRE-ALPHA · Milestone count is not a live-performance readiness score.</text>
</svg>
'''
    target = ROOT / 'assets/progress.svg'
    if args.check:
        if not target.exists() or target.read_text(encoding='utf-8') != svg:
            print('Progress SVG is stale; run python scripts/update_progress.py', file=sys.stderr)
            return 1
    else:
        target.write_text(svg, encoding='utf-8')
    print(f'Roadmap: {done}/{total} milestones ({percent}%), not production readiness')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
