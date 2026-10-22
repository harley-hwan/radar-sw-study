"""Render harness drawing-call records (JSON) to PNG for a visual check. usage: python render.py <json>... (writes .png next to each)"""
import glob
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

# 한글 글꼴: 맑은 고딕 (Windows) 또는 Noto Sans KR. 없으면 PIL 기본 글꼴 (한글은 네모로 보임)
DIRS = ['C:/Windows/Fonts', os.path.expanduser('~/.local/share/fonts'), '/usr/share/fonts', os.path.expanduser('~/Library/Fonts')]


def find(*names):
    for d in DIRS:
        for n in names:
            hit = sorted(glob.glob(f'{d}/**/{n}', recursive=True))
            if hit:
                return hit[0]
    return None


REG = find('malgun.ttf', '*NotoSansKR*Regular*', '*NotoSansKR*400*')
BOLD = find('malgunbd.ttf', '*NotoSansKR*Bold*', '*NotoSansKR*700*') or REG


def rgba(c):
    h, a = c
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5)) + (a,)


for path in sys.argv[1:]:
    d = json.load(open(path, encoding='utf-8'))
    img = Image.new('RGB', (d['w'], d['h']), 'white')
    over = Image.new('RGBA', img.size, (0, 0, 0, 0))
    g = ImageDraw.Draw(over)
    for op in d['ops']:
        if op['op'] == 'fill':
            g.rectangle([op['x'], op['y'], op['x'] + op['w'] - 1, op['y'] + op['h'] - 1], fill=rgba(op['c']))
        elif op['op'] == 'line':
            g.line([op['x1'], op['y1'], op['x2'], op['y2']], fill=rgba(op['c']), width=1)
        elif op['op'] == 'box':
            g.rectangle([op['x'], op['y'], op['x'] + op['w'], op['y'] + op['h']], outline=rgba(op['c']), width=1)
        else:
            size = max(6, round(op['size'] * 0.92))
            font = ImageFont.truetype(BOLD if op['bold'] else REG, size) if REG else ImageFont.load_default()
            w, h = op['w'], op['size'] * 1.33
            x = op['x'] - (0, 0.5, 1.0)[op['ax']] * w + 0.16 * op['size']
            y = op['y'] - (0, 0.5, 1.0)[op['ay']] * h
            g.text((x, y + 0.1 * op['size']), op['s'], font=font, fill=rgba(op['c']))
    img = Image.alpha_composite(img.convert('RGBA'), over).convert('RGB')
    img.save(path[:-5] + '.png')
    print('rendered', path[:-5] + '.png', img.size)
