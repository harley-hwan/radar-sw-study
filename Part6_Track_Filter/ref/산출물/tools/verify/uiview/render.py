"""Render uiview drawing-call records (JSON) to PNG for a visual check.
usage: python render.py <json>...                  -> .png next to each
       python render.py --out <figures> <json>...  -> <figures>/pv_<name>.png + pv_<name>.json (컨트롤 위치만, 발표자료가 그림 부분을 자를 때 씀)
글꼴 크기 · 글자 폭은 근사. 실제 화면은 Windows 에서 확인."""
import glob
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

DIRS = ['C:/Windows/Fonts', os.path.expanduser('~/.local/share/fonts'), '/usr/share/fonts', os.path.expanduser('~/Library/Fonts')]


def find(*names):
    for d in DIRS:
        for n in names:
            hit = sorted(glob.glob(f'{d}/**/{n}', recursive=True))
            if hit:
                return hit[0]
    return None


REG = find('malgun.ttf', '*NotoSansKR*Regular*', '*NotoSansKR*400*', '*NotoSansCJK*Regular*')
BOLD = find('malgunbd.ttf', '*NotoSansKR*Bold*', '*NotoSansKR*700*', '*NotoSansCJK*Bold*') or REG
FONTS = {}


def font(size, bold):
    key = (size, bold)
    if key not in FONTS:
        path = BOLD if bold else REG
        FONTS[key] = ImageFont.truetype(path, size) if path else ImageFont.load_default()
    return FONTS[key]


def rgba(c):
    h, a = c
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5)) + (a,)


def width(op):
    return max(1, round(op.get('lw', 1)))


def dashed(g, pts, fill, w, on=4.0, off=3.0):
    # GDI+ 처럼 무늬 (펜 굵기의 배수) 를 선 전체에 이어서 적용 (짧은 선분이 많은 꺾은선도 점선으로 보이게)
    on, off = on * max(w, 1), off * max(w, 1)
    phase = 0.0                                                     # 지금 무늬 주기 안의 위치
    for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
        dx, dy = x2 - x1, y2 - y1
        length = (dx * dx + dy * dy) ** 0.5
        pos = 0.0
        while pos < length:
            drawing = phase < on
            step = min(length - pos, (on - phase) if drawing else (on + off - phase))
            if drawing and step > 0:
                a, b = pos / length, (pos + step) / length
                g.line([x1 + dx * a, y1 + dy * a, x1 + dx * b, y1 + dy * b], fill=fill, width=w)
            pos += step
            phase = (phase + step) % (on + off)


def draw(g, op):
    kind = op['op']
    if kind == 'fill':
        g.rectangle([op['x'], op['y'], op['x'] + op['w'] - 1, op['y'] + op['h'] - 1], fill=rgba(op['c']))
    elif kind == 'box':
        g.rectangle([op['x'], op['y'], op['x'] + op['w'], op['y'] + op['h']], outline=rgba(op['c']), width=width(op))
    elif kind == 'rbox':
        g.rounded_rectangle([op['x'], op['y'], op['x'] + op['w'], op['y'] + op['h']], radius=op['r'], fill=rgba(op['fill']), outline=rgba(op['c']))
    elif kind == 'line':
        pts = [(op['x1'], op['y1']), (op['x2'], op['y2'])]
        (dashed if op.get('dash') else lambda gg, p, f, w: gg.line(p, fill=f, width=w))(g, pts, rgba(op['c']), width(op))
    elif kind == 'poly':
        pts = [tuple(p) for p in op['pts']]
        if op.get('dash'):
            dashed(g, pts, rgba(op['c']), width(op))
        else:
            g.line(pts, fill=rgba(op['c']), width=width(op), joint='curve')
    elif kind == 'polygon':
        pts = [tuple(p) for p in op['pts']]
        g.polygon(pts, fill=rgba(op['fill']) if op['fill'] else None, outline=rgba(op['c']) if op['c'] else None)
    elif kind == 'ellipse':
        box = [op['x'], op['y'], op['x'] + op['w'], op['y'] + op['h']]
        g.ellipse(box, fill=rgba(op['fill']) if op['fill'] else None, outline=rgba(op['c']) if op['c'] else None, width=width(op) if op['c'] else 0)
    elif kind == 'text':
        size = max(6, round(op['size'] * 0.92))
        f = font(size, op['bold'])
        w = f.getlength(op['s']) if hasattr(f, 'getlength') else op['w']
        h = op['size'] * 1.33
        x = op['x'] - (0, 0.5, 1.0)[op['ax']] * (w + 0.33 * op['size']) + 0.16 * op['size']
        y = op['y'] - (0, 0.5, 1.0)[op['ay']] * h
        g.text((x, y + 0.1 * op['size']), op['s'], font=f, fill=rgba(op['c']))


args = sys.argv[1:]
out_dir = None
if args[:1] == ['--out']:
    out_dir, args = args[1], args[2:]
    os.makedirs(out_dir, exist_ok=True)

for path in args:
    d = json.load(open(path, encoding='utf-8'))
    img = Image.new('RGBA', (d['w'], d['h']), 'white')
    clip = None
    layer = Image.new('RGBA', img.size, (0, 0, 0, 0))
    g = ImageDraw.Draw(layer)

    def flush():
        global img
        if clip is None:
            img = Image.alpha_composite(img, layer)
        else:
            mask = Image.new('L', img.size, 0)
            ImageDraw.Draw(mask).rectangle([clip[0], clip[1], clip[0] + clip[2] - 1, clip[1] + clip[3] - 1], fill=255)
            cut = Image.new('RGBA', img.size, (0, 0, 0, 0))
            cut.paste(layer, (0, 0), mask)
            img = Image.alpha_composite(img, cut)

    for op in d['ops']:
        if op['op'] in ('clip', 'noclip'):
            flush()
            layer = Image.new('RGBA', img.size, (0, 0, 0, 0))
            g = ImageDraw.Draw(layer)
            clip = (op['x'], op['y'], op['w'], op['h']) if op['op'] == 'clip' else None
        else:
            draw(g, op)
    flush()
    if out_dir is None:
        png = path[:-5] + '.png'
    else:
        name = os.path.basename(path)[:-5]
        stem = os.path.join(out_dir, 'pv_' + (name[3:] if name.startswith('ui_') else name))
        png = stem + '.png'
        with open(stem + '.json', 'w', encoding='utf-8') as f:
            json.dump({'source': 'uiview', 'w': d['w'], 'h': d['h'], 'rects': d.get('rects', {})}, f, ensure_ascii=False, indent=1)
    img.convert('RGB').save(png)
    print('rendered', png, img.size)
