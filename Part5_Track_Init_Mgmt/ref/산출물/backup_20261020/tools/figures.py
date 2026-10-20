# -*- coding: utf-8 -*-
"""과제 5 발표 그림. python figures.py <data_dir> <out_dir>

data_dir 의 csv 는 dump_fig_data.c 가 만든 것 (응용 시나리오 (원본 크기), SNR 20).
그림 안 글자는 슬라이드에 놓였을 때 8 pt 안팎이 되게 실제 크기 (inch) 로 그린다.
"""
import csv
import math
import os
import sys
from collections import defaultdict

import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Ellipse, FancyArrowPatch, FancyBboxPatch, Rectangle, Polygon
from PIL import Image, ImageDraw

plt.rcParams['font.family'] = 'Malgun Gothic'
plt.rcParams['axes.unicode_minus'] = False
plt.rcParams['font.size'] = 8

NAVY = '#1B2740'
SUB = '#4A5A7A'
MUTE = '#8794AC'
LINE = '#D2D9E5'
BOXF = '#F7F9FC'
BLUE = '#2563EB'
ORANGE = '#D97706'
GREEN = '#059669'
PURPLE = '#7C3AED'
RED = '#DC2626'
TENT = '#EA580C'
CONF = '#16A34A'
MEM = '#9333EA'
DEL = '#6B7280'
PLOT = '#1F2937'
CLUT = '#9CA3AF'
OBJ = ['#374151', '#2563EB', '#DC2626', '#059669', '#D97706', '#7C3AED']

DPI = 220
SIG_R = 4.740
SIG_AZ = math.radians(0.4781)
SIG_EL = math.radians(0.4939)
T = 0.1
VMAX = 800.0

DATA, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)


def save(fig, name):
    fig.savefig(os.path.join(OUT, name), dpi=DPI, facecolor='white')
    plt.close(fig)
    print('saved', name)


def read(name):
    return list(csv.DictReader(open(os.path.join(DATA, name), encoding='utf-8')))


def clean(ax):
    ax.set_xticks([])
    ax.set_yticks([])
    for s in ax.spines.values():
        s.set_visible(False)


def track_box(ax, x, y, half, state, lw=1.6, z=5):
    col = {1: TENT, 2: CONF, 3: MEM, 4: DEL}[state]
    if state == 2:
        ax.add_patch(Rectangle((x - half, y - half), 2 * half, 2 * half, facecolor=col, edgecolor=col, lw=lw, zorder=z))
    else:
        ax.add_patch(Rectangle((x - half, y - half), 2 * half, 2 * half, facecolor='white', edgecolor=col, lw=lw,
                               ls='--' if state == 3 else '-', zorder=z))
        if state == 4:
            ax.plot([x - half, x + half], [y - half, y + half], color=col, lw=1.0, zorder=z + 1)
            ax.plot([x - half, x + half], [y + half, y - half], color=col, lw=1.0, zorder=z + 1)


def arrow(ax, p0, p1, col=NAVY, lw=1.2, style='-|>', ms=9, ls='-', rad=0.0, z=3):
    a = FancyArrowPatch(p0, p1, arrowstyle=style, mutation_scale=ms, color=col, lw=lw, linestyle=ls,
                        connectionstyle='arc3,rad=%g' % rad, zorder=z, shrinkA=0, shrinkB=0)
    ax.add_patch(a)


# 1. 표지 바탕: 과제 4 표지와 같은 틀 (격자, 거리 원, 시야 부채꼴) 위에 이번 시나리오의 클러터, 플롯, 참값 궤적, 트랙 심볼을 그림.
#    2 배 크기로 그린 뒤 줄여 선을 부드럽게 함
def fig_cover():
    W0, H0, K = 2933, 1650, 2                         # 표지 그림 크기 (과제 4 표지와 같음), 그리는 배율
    S, ox, oy = 65.0 * K, 860 * K, 1350 * K           # 1 km = 65 px, 레이다 자리. 서쪽 10 km 표적과 15 km 부채꼴이 제목 왼쪽에 들어가게
    im = Image.new('RGB', (W0 * K, H0 * K), (27, 39, 64))
    d = ImageDraw.Draw(im, 'RGBA')
    step = 91.67 * K
    for i in range(int(W0 * K / step) + 1):
        d.line((round(i * step), 0, round(i * step), H0 * K), fill=(35, 49, 80), width=2 * K)
    for i in range(int(H0 * K / step) + 1):
        d.line((0, round(i * step), W0 * K, round(i * step)), fill=(35, 49, 80), width=2 * K)
    for km in (5, 10, 15):
        r = km * S
        d.ellipse((ox - r, oy - r, ox + r, oy + r), outline=(51, 67, 106), width=2 * K)
    for sgn in (-1, 1):                               # 시야 ±60°, 15 km 까지 점선
        a = math.radians(60 * sgn)
        for k in range(int(15 * S / (24 * K))):
            t0, t1 = k * 24 * K, k * 24 * K + 14 * K
            d.line((ox + t0 * math.sin(a), oy - t0 * math.cos(a), ox + t1 * math.sin(a), oy - t1 * math.cos(a)), fill=(72, 84, 113), width=2 * K)

    def xy(north, east):
        return ox + east / 1000 * S, oy - north / 1000 * S

    for r in read('plots.csv'):
        x, y = xy(float(r['north']), float(r['east']))
        col = (98, 112, 144, 200) if r['source'] == '0' else (196, 205, 222, 150)
        d.ellipse((x - 1.6 * K, y - 1.6 * K, x + 1.6 * K, y + 1.6 * K), fill=col)
    path = defaultdict(list)
    for r in read('truth.csv'):
        path[int(r['target'])].append(xy(float(r['north']), float(r['east'])))
    for k, p in path.items():
        rgb = tuple(int(OBJ[k][i:i + 2], 16) for i in (1, 3, 5))
        d.line(p, fill=rgb + (255,), width=3 * K, joint='curve')
    seen = set()
    for r in read('tracks.csv'):
        s = int(r['scan'])
        if int(r['init_scan']) != s or s % 9 != 0:
            continue
        x, y = xy(float(r['north']), float(r['east']))
        lx, ly = x + float(r['ve']) * 1.0 / 1000 * S, y - float(r['vn']) * 1.0 / 1000 * S
        d.line((x, y, lx, ly), fill=(234, 88, 12, 200), width=3 * K)
        h = 9 * K
        d.rectangle((x - h, y - h, x + h, y + h), outline=(234, 88, 12, 255), width=3 * K)
        seen.add(s)
    d.polygon([(ox, oy - 14 * K), (ox - 12 * K, oy + 8 * K), (ox + 12 * K, oy + 8 * K)], fill=(174, 185, 204))
    im.resize((W0, H0), Image.LANCZOS).save(os.path.join(OUT, 'cover_bg.png'))
    print('saved cover_bg.png', len(seen))


# 2. 1-Point, 2-Point 개시 비교
def fig_1p2p():
    fig, axs = plt.subplots(1, 2, figsize=(7.3, 2.75))
    for ax in axs:
        ax.set_aspect('equal')
        ax.set_xlim(-150, 190)
        ax.set_ylim(-125, 125)
        clean(ax)
    # 1-Point
    ax = axs[0]
    ax.set_title('1-Point: 플롯 하나로 개시', color=NAVY, fontsize=9, fontweight='bold', loc='left')
    ax.add_patch(Circle((0, 0), 80, fill=False, ls='--', ec=MUTE, lw=1.0))
    ax.add_patch(Circle((0, 0), 80, fc='#FEF3E2', ec='none', alpha=0.6))
    ax.plot(0, 0, 'o', color=PLOT, ms=5, zorder=6)
    track_box(ax, 0, 0, 9, 1)
    ax.text(0, -112, 'z_k  (위치 = 플롯)', ha='center', va='bottom', color=NAVY, fontsize=8)
    for ang in (20, 110, 200, 290):
        a = math.radians(ang)
        arrow(ax, (12 * math.cos(a), 12 * math.sin(a)), (74 * math.cos(a), 74 * math.sin(a)), col=ORANGE, lw=1.0, ls='--', ms=7)
    ax.text(88, 62, '속도를 모름\n어느 쪽으로도 갈 수 있다\n(속도 0, 오차는 크게)', color=SUB, fontsize=8, va='center')
    ax.text(0, 98, '다음 스캔 위치 후보', ha='center', color=MUTE, fontsize=7.5)
    # 2-Point
    ax = axs[1]
    ax.set_title('2-Point: 연속 두 스캔의 플롯으로 개시', color=NAVY, fontsize=9, fontweight='bold', loc='left')
    p0 = np.array([-70.0, -30.0])
    p1 = np.array([0.0, 0.0])
    ax.add_patch(Circle(p0, 80, fill=False, ls='--', ec=MUTE, lw=1.0))
    ax.text(p0[0] - 4, p0[1] - 86, '속도 게이트: 반경 V_max · T', ha='center', va='top', color=MUTE, fontsize=7.5)
    ax.plot(*p0, 'o', mfc='white', mec=PLOT, ms=5, mew=1.2, zorder=6)
    ax.text(p0[0] - 6, p0[1] - 14, 'z_k-1', ha='right', va='top', color=NAVY, fontsize=8)
    arrow(ax, p0 + 6 * (p1 - p0) / np.linalg.norm(p1 - p0), p1 - 9 * (p1 - p0) / np.linalg.norm(p1 - p0), col=BLUE, lw=1.4, ms=8)
    ax.plot(*p1, 'o', color=PLOT, ms=5, zorder=6)
    track_box(ax, 0, 0, 9, 1)
    ax.text(4, -20, 'z_k', ha='left', va='top', color=NAVY, fontsize=8)
    lead = p1 + (p1 - p0)
    arrow(ax, p1 + 12 * (p1 - p0) / np.linalg.norm(p1 - p0), lead, col=TENT, lw=1.4, ms=8)
    ax.add_patch(Ellipse(lead, 46, 30, angle=23, fill=False, ec=TENT, lw=1.0, ls=':'))
    ax.text(-44, 4, '(z_k - z_k-1) / T', color=BLUE, fontsize=8, ha='center', rotation=23, bbox=dict(fc='white', ec='none', pad=0.5))
    ax.text(lead[0] + 8, lead[1] + 18, '다음 예상 위치', color=TENT, fontsize=8)
    ax.text(100, -60, '위치 = z_k\n속도 = 두 플롯의 차 / T\n가속도 = 0', color=SUB, fontsize=8, va='center')
    fig.tight_layout(pad=0.4, w_pad=1.2)
    save(fig, 'fig_1p2p.png')


# 3. 9 x 9 오차 공분산: 일반적인 숫자 예 (세 축 모두 σ = 10 m, 축끼리 상관 없음, T = 0.1 s)
P_COL = {0: '#F3F4F7', 1: '#DCE7FD', 2: '#DDF3EA', 3: '#FDECD7'}


def fig_pmatrix():
    R, TT = 100.0, 0.1
    val = {1: R, 2: R / TT, 3: 2 * R / TT ** 2}
    names = ['x', "x'", "x''", 'y', "y'", "y''", 'z', "z'", "z''"]
    fig, ax = plt.subplots(figsize=(5.45, 3.95))
    fig.subplots_adjust(left=0.005, right=0.995, top=0.995, bottom=0.005)
    ax.set_aspect('equal')
    clean(ax)
    ax.set_xlim(-2.1, 11.85)
    ax.set_ylim(-0.12, 10.0)
    for i in range(9):
        for j in range(9):
            a, b = i % 3, j % 3
            same = (i // 3) == (j // 3)
            if not same or a == 2 or b == 2:
                k = 0
            elif a == 0 and b == 0:
                k = 1
            elif a == 1 and b == 1:
                k = 3
            else:
                k = 2
            ax.add_patch(Rectangle((j, 8 - i), 1, 1, fc=P_COL[k], ec='white', lw=1.4))
            txt = '0' if k == 0 else '{:,.0f}'.format(val[k])
            ax.text(j + 0.5, 8 - i + 0.5, txt, ha='center', va='center', fontsize=8 if k else 7, color=NAVY if k else '#B3BCCB',
                    fontweight='bold' if k else 'normal')
    for k in range(3):
        ax.add_patch(Rectangle((3 * k, 6 - 3 * k), 3, 3, fill=False, ec=NAVY, lw=1.6))
    for n, t in enumerate(names):
        ax.text(n + 0.5, 9.12, t, ha='center', va='bottom', fontsize=9, color=NAVY)
        ax.text(-0.15, 8 - n + 0.5, t, ha='right', va='center', fontsize=9, color=NAVY)
    for k, t in enumerate(['x 축', 'y 축', 'z 축']):
        ax.text(9.2, 7.5 - 3 * k, t, ha='left', va='center', fontsize=9, color=SUB, fontweight='bold')
    save(fig, 'fig_pmatrix.png')


# 4. 속도 게이트: 짝짓기 예와 거리별 잡음 (처음 판으로 되돌림. 글자 겹침만 고침)
def fig_gate():
    fig = plt.figure(figsize=(11.9, 3.3))
    ax = fig.add_axes([0.0, 0.02, 0.40, 0.88])
    ax.set_aspect('equal')
    clean(ax)
    ax.set_xlim(-140, 330)
    ax.set_ylim(-120, 135)
    ax.set_title('짝짓기: 앞 스캔 플롯 (빈 원) 과 이번 스캔 플롯 (채운 원)', color=NAVY, fontsize=10, fontweight='bold', loc='left')
    prev = {'A': (-40, 10), 'B': (180, 40)}
    cur = {'1': (12, 40), '2': (85, -40), '3': (230, 70), '4': (260, -60)}
    for k, p in prev.items():
        ax.add_patch(Circle(p, 80, fill=False, ls='--', ec=MUTE, lw=0.9))
        ax.plot(*p, 'o', mfc='white', mec=PLOT, ms=6, mew=1.3, zorder=6)
        ax.text(p[0] - 8, p[1] + 8, k, ha='right', color=NAVY, fontsize=9.5, fontweight='bold')
    for k, p in cur.items():
        ax.plot(*p, 'o', color=PLOT, ms=6, zorder=6)
        ax.text(p[0] + 7, p[1] + 7, k, color=NAVY, fontsize=9.5, fontweight='bold')
    # 글자 자리는 선 · 점과 겹치지 않게 쌍마다 정함
    pairs = [('A', '1', True, (-38, 56)), ('A', '2', False, (8, -58)), ('B', '3', True, (182, 90)), ('B', '4', False, (268, 4))]
    for a, b, ok, pos in pairs:
        p0, p1 = np.array(prev[a]), np.array(cur[b])
        dist = np.linalg.norm(p1 - p0)
        col = GREEN if ok else RED
        ax.plot([p0[0], p1[0]], [p0[1], p1[1]], color=col, lw=1.5 if ok else 1.2, ls='-' if ok else ':', zorder=4)
        ax.text(pos[0], pos[1], '%.0f m/s %s' % (dist / T, '통과' if ok else '탈락'), color=col, fontsize=8.5, ha='center', va='center',
                bbox=dict(fc='white', ec='none', pad=0.6), zorder=7)
    ax.text(-135, -92, '점선 원 = 반경 V_max · T = 80 m\n속력이 작은 쌍부터 하나씩 고르고, 쓴 플롯은 다시 쓰지 않는다',
            color=SUB, fontsize=8.3, va='top', linespacing=1.4)
    # 거리별 잡음 (같은 표적 두 플롯 차의 2σ 타원) 과 게이트
    fig.text(0.43, 0.955, '거리별: 같은 표적 두 플롯의 차이 (파란 타원) 와 게이트 (빨간 원)', color=NAVY, fontsize=10, fontweight='bold',
             ha='left', va='center')
    # 이번 시나리오의 표적 거리 (4.8 ~ 14.2 km) 안에서 세 거리. 통과율은 그 거리 ±0.5 km 안의 같은 표적 쌍
    rng = [(5000, '5 km'), (9000, '9 km'), (14000, '14 km')]
    pairs = read('gate.csv')
    for n, (r, name) in enumerate(rng):
        ax2 = fig.add_axes([0.43 + n * 0.19, 0.03, 0.18, 0.79])
        ax2.set_aspect('equal')
        clean(ax2)
        ax2.set_xlim(-360, 360)
        ax2.set_ylim(-360, 360)
        sx = 2 * math.sqrt(2) * r * SIG_AZ
        sy = 2 * math.sqrt(2) * SIG_R
        ax2.add_patch(Ellipse((0, 0), 2 * sx, 2 * max(sy, 4), fc='#DCE7FD', ec=BLUE, lw=1.0))
        ax2.add_patch(Circle((0, 0), 80, fill=False, ec=RED, lw=1.4, ls='--'))
        ax2.plot([-360, 360], [0, 0], color=LINE, lw=0.6, zorder=0)
        ax2.plot([0, 0], [-360, 360], color=LINE, lw=0.6, zorder=0)
        near = [int(p['pass']) for p in pairs if abs(float(p['range']) - r) < 500]
        rate = np.mean(near) if near else float('nan')
        ax2.set_title('%s:  가로 방향 2σ ±%.0f m' % (name, sx), color=NAVY, fontsize=9.5, fontweight='bold')
        ax2.text(0, -335, '게이트 통과율 %.0f %%' % (100 * rate), ha='center', color=RED if rate < 0.5 else GREEN, fontsize=9.5,
                 fontweight='bold')
        if n == 0:
            ax2.text(-350, 300, '파란 타원: 두 플롯 차의 2σ\n빨간 원: 게이트 80 m', color=SUB, fontsize=8, va='top',
                     bbox=dict(fc='white', ec='none', pad=1.0), zorder=5)
    save(fig, 'fig_gate.png')


# 5. M/N 로직
def fig_mn():
    fig, ax = plt.subplots(figsize=(6.2, 2.6))
    fig.subplots_adjust(left=0.005, right=0.995, top=0.99, bottom=0.01)
    clean(ax)
    ax.set_aspect('equal')
    ax.set_xlim(-0.6, 13.0)
    ax.set_ylim(-1.2, 4.6)
    # 세 번째 성공이 나오는 스캔 4 에서 확정. 스캔 5 는 볼 필요가 없어 빈 칸
    hits = [1, 1, 0, 1, None]
    ax.text(-0.5, 4.25, '임시 트랙 (예: M = 3, N = 5)', color=NAVY, fontsize=9, fontweight='bold')
    for i, h in enumerate(hits):
        if h is None:
            ax.add_patch(Circle((i, 3.1), 0.36, fc='white', ec=LINE, lw=1.0, ls='--'))
        else:
            ax.add_patch(Circle((i, 3.1), 0.36, fc=GREEN if h else 'white', ec=GREEN if h else RED, lw=1.3))
            ax.text(i, 3.1, '성공' if h else '실패', ha='center', va='center', fontsize=6.5, color='white' if h else RED)
        if i == 3:
            ax.text(i, 2.5, '스캔 4 확정', ha='center', va='top', fontsize=6.5, color=CONF, fontweight='bold')
        else:
            ax.text(i, 2.5, '스캔 %d' % (i + 1), ha='center', va='top', fontsize=6.5, color=MUTE)
    ax.add_patch(FancyBboxPatch((-0.5, 2.62), 5.0, 0.95, boxstyle='round,pad=0.02,rounding_size=0.15', fill=False, ec=BLUE, lw=1.2,
                                ls='--'))
    ax.text(5.0, 3.42, 'N = 5 번 안에 M = 3 번 성공 -> 확정', color=BLUE, fontsize=8, va='center', fontweight='bold')
    ax.text(5.0, 3.05, '(이 예는 스캔 4 에서 세 번째 성공)', color=SUB, fontsize=7, va='center')
    ax.text(5.0, 2.68, 'M 번 연속 실패 -> 삭제', color=RED, fontsize=8, va='center', fontweight='bold')
    ax.text(-0.5, 1.45, '확정 트랙', color=NAVY, fontsize=9, fontweight='bold')
    seq = [1, 1, 0, 0, 0, 0]
    for i, h in enumerate(seq):
        ax.add_patch(Circle((i, 0.35), 0.36, fc=GREEN if h else 'white', ec=GREEN if h else MEM, lw=1.3, ls='-' if h else '--'))
        ax.text(i, 0.35, '성공' if h else '실패', ha='center', va='center', fontsize=6.5, color='white' if h else MEM)
    ax.annotate('', xy=(5.4, -0.55), xytext=(1.6, -0.55), arrowprops=dict(arrowstyle='<->', color=MEM, lw=1.0))
    ax.text(3.5, -0.95, '메모리 상태로 지낸 시간', color=MEM, fontsize=7, ha='center', va='center')
    ax.text(6.2, 0.6, '연관 실패 -> 메모리 트랙', color=MEM, fontsize=8, va='center', fontweight='bold')
    ax.text(6.2, 0.05, '정한 시간보다 길게 이어지면 -> 삭제', color=MEM, fontsize=8, va='center', fontweight='bold')
    save(fig, 'fig_mn.png')


# 6. 트랙 스코어 상태 전이
def fig_state():
    fig, ax = plt.subplots(figsize=(7.0, 3.35))
    fig.subplots_adjust(left=0.005, right=0.995, top=0.99, bottom=0.01)
    clean(ax)
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 4.9)
    box = {'1': (1.5, 3.4, '1  임시', TENT, '#FFF1E6'), '2': (4.75, 3.4, '2  확정', CONF, '#E8F7EE'), '3': (8.75, 3.4, '3  메모리', MEM, '#F5EBFF'),
           '4': (5.3, 0.8, '4  삭제', DEL, '#F1F2F4')}
    W, H = 1.7, 0.72
    for k, (x, y, name, ec, fc) in box.items():
        ax.add_patch(FancyBboxPatch((x - W / 2, y - H / 2), W, H, boxstyle='round,pad=0.02,rounding_size=0.12', fc=fc, ec=ec, lw=1.6))
        ax.text(x, y, name, ha='center', va='center', color=ec, fontsize=10, fontweight='bold')
    # 개시 (왼쪽 아래에서 들어옴)
    arrow(ax, (0.05, 2.25), (0.75, 3.0), col=NAVY)
    ax.text(0.05, 2.1, '2 Point 개시\nTrackScore 2', color=NAVY, fontsize=7.5, va='top')
    # 1 -> 2
    arrow(ax, (2.4, 3.4), (3.85, 3.4), col=CONF, lw=1.5)
    ax.text(3.12, 3.52, 'TrackScore ≥ 5', ha='center', color=CONF, fontsize=8, fontweight='bold')
    # 1 -> 4
    arrow(ax, (1.8, 3.02), (4.4, 0.95), col=DEL, lw=1.4)
    ax.text(2.5, 1.75, 'TrackScore ≤ 0', color=DEL, fontsize=8, fontweight='bold', rotation=-33, ha='center')
    # 2 -> 3 (위), 3 -> 2 (아래)
    arrow(ax, (5.65, 3.55), (7.85, 3.55), col=MEM, lw=1.5)
    ax.text(6.75, 4.08, '연관 실패', ha='center', color=MEM, fontsize=8, fontweight='bold')
    ax.text(6.75, 3.72, 'MemoryScore +1', ha='center', color=MEM, fontsize=7.5)
    arrow(ax, (7.85, 3.22), (5.65, 3.22), col=CONF, lw=1.5)
    ax.text(6.75, 3.05, '연관 성공: 바로 확정', ha='center', va='top', color=CONF, fontsize=8, fontweight='bold')
    ax.text(6.75, 2.7, 'MemoryScore = 0', ha='center', va='top', color=CONF, fontsize=7.5)
    # 3 -> 4
    arrow(ax, (8.55, 3.02), (6.2, 0.95), col=DEL, lw=1.4)
    ax.text(7.85, 1.62, 'MemoryScore ≥ 5', color=DEL, fontsize=8, fontweight='bold', rotation=44, ha='center')
    # 자기 고리
    arrow(ax, (1.15, 3.78), (1.85, 3.78), col=TENT, lw=1.1, rad=-1.4, ms=7)
    ax.text(1.5, 4.62, '연관 성공 +1, 실패 -1', ha='center', color=TENT, fontsize=7.5)
    arrow(ax, (8.4, 3.78), (9.1, 3.78), col=MEM, lw=1.1, rad=-1.4, ms=7)
    ax.text(8.75, 4.62, '실패: MemoryScore +1', ha='center', color=MEM, fontsize=7.5)
    save(fig, 'fig_state.png')


def score_seq(flags):
    st, ts, ms = 1, 2, 0
    out = [(None, ts, ms, st)]
    for f in flags:
        if st == 1:
            ts += 1 if f else -1
            st = 2 if ts >= 5 else (4 if ts <= 0 else 1)
        elif st == 2:
            if not f:
                st, ms = 3, ms + 1
        elif st == 3:
            if f:
                st, ms = 2, 0
            else:
                ms += 1
                st = 4 if ms >= 5 else 3
        out.append((f, ts, ms, st))
        if st == 4:
            break
    return out


# 7. 스코어 변화 예 두 가지. 그래프 아래 '계산' 줄에 스캔마다 어떤 규칙으로 값이 바뀌었는지 적음
def score_calc(seq):
    out = []
    for i, (f, ts, ms, st) in enumerate(seq):
        if i == 0:
            out.append(('개시\nTS = %d' % ts, st))
            continue
        pst = seq[i - 1][3]
        if pst == 1:
            if st == 2:
                t = 'TS + 1\n= %d\n-> 확정' % ts
            elif st == 4:
                t = 'TS - 1\n= %d\n-> 삭제' % ts
            else:
                t = 'TS %s 1\n= %d' % ('+' if f else '-', ts)
        elif pst == 2:
            t = '확정\n유지' if f else '-> 메모리\nMS = %d' % ms
        elif f:
            t = '-> 확정\nMS = 0'
        elif st == 4:
            t = 'MS + 1\n= %d\n-> 삭제' % ms
        else:
            t = 'MS + 1\n= %d' % ms
        out.append((t, st))
    return out


def fig_score():
    cases = [([1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0], '연관 결과를 넣은 예'), ([0, 0, 0], '과제 조건: Assigned_Flag 늘 0')]
    fig, axs = plt.subplots(1, 2, figsize=(11.9, 4.15), gridspec_kw={'width_ratios': [2.3, 1]})
    bg = {1: '#FFF1E6', 2: '#E8F7EE', 3: '#F5EBFF', 4: '#EEF0F4'}
    col = {1: TENT, 2: CONF, 3: MEM, 4: DEL}
    for k, (ax, (flags, title)) in enumerate(zip(axs, cases)):
        seq = score_seq(flags)
        calc = score_calc(seq)
        n = len(seq)
        for i, (f, ts, ms, st) in enumerate(seq):
            ax.add_patch(Rectangle((i - 0.5, -1.2), 1, 8.2, fc=bg[st], ec='white', lw=1.0, zorder=0))
            ax.text(i, 6.45, '-' if f is None else str(f), ha='center', color=NAVY, fontsize=10, fontweight='bold')
            ax.text(i, -0.95, {1: '임시', 2: '확정', 3: '메모리', 4: '삭제'}[st], ha='center', fontsize=9, color=col[st])
            ax.add_patch(Rectangle((i - 0.5, -4.05), 1, 2.7, fc='white', ec=LINE, lw=0.8, zorder=0))
            t, cst = calc[i]
            ax.text(i, -2.7, t, ha='center', va='center', fontsize=8, color=col[cst], linespacing=1.35)
        xs = np.arange(n)
        ax.step(xs, [q[1] for q in seq], where='mid', color=TENT, lw=2.2, label='TrackScore (TS)')
        ax.plot(xs, [q[1] for q in seq], 'o', color=TENT, ms=4.5)
        ax.step(xs, [q[2] for q in seq], where='mid', color=MEM, lw=2.0, ls='--', label='MemoryScore (MS)')
        ax.plot(xs, [q[2] for q in seq], 's', color=MEM, ms=4)
        ax.axhline(5, xmin=0, xmax=1, color=CONF, lw=0.8, ls=':')
        ax.plot([-0.5, n - 0.5], [0, 0], color=DEL, lw=0.8, ls=':')
        ax.set_xlim(-0.5, n - 0.5)
        ax.set_ylim(-4.1, 7.0)
        ax.set_xticks(xs)
        ax.set_xticklabels([str(i) for i in xs], fontsize=9, color=SUB)
        ax.set_yticks([0, 1, 2, 3, 4, 5])
        ax.tick_params(labelsize=9, colors=SUB, length=0)
        for sp in ax.spines.values():
            sp.set_color(LINE)
        ax.set_title(title, color=NAVY, fontsize=11, fontweight='bold', loc='left', pad=10)
        ax.set_xlabel('개시 후 스캔', fontsize=9, color=SUB)
        if k == 0:
            ax.text(-0.75, 6.62, 'Assigned_Flag', ha='right', va='center', fontsize=8.5, color=SUB)
            ax.text(-0.75, -0.8, '상태', ha='right', va='center', fontsize=8.5, color=SUB)
            ax.text(-0.75, -2.7, '계산', ha='right', va='center', fontsize=8.5, color=SUB)
    axs[0].legend(loc='lower right', fontsize=9, frameon=True, framealpha=0.95, edgecolor=LINE, bbox_to_anchor=(1.0, 1.0), ncol=2)
    fig.tight_layout(pad=0.4, w_pad=2.0)
    save(fig, 'fig_score.png')


# 8. 쓰레드 · UDP 구조
def fig_arch():
    fig, ax = plt.subplots(figsize=(11.9, 2.75))
    fig.subplots_adjust(left=0.005, right=0.995, top=0.99, bottom=0.01)
    clean(ax)
    ax.set_xlim(0, 24)
    ax.set_ylim(-0.9, 5.2)

    def blk(x, y, w, h, title, lines, ec=NAVY, fc=BOXF, tc=NAVY):
        ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle='round,pad=0.02,rounding_size=0.15', fc=fc, ec=ec, lw=1.4))
        ax.text(x + w / 2, y + h - 0.3, title, ha='center', va='top', color=tc, fontsize=10.5, fontweight='bold')
        ax.text(x + w / 2, y + h - 1.05, '\n'.join(lines), ha='center', va='top', color=SUB, fontsize=8.8, linespacing=1.55)

    ax.add_patch(FancyBboxPatch((0.2, -0.75), 17.4, 5.75, boxstyle='round,pad=0.02,rounding_size=0.2', fc='white', ec=LINE, lw=1.0, ls='--'))
    ax.text(0.45, 4.85, 'TargetSimCore (DLL)', color=MUTE, fontsize=8.5, va='top')
    blk(0.6, 0.4, 3.6, 3.9, '측정 모의 (과제 4)', ['표적 플롯 + 클러터', '스캔 1,160 개', '안테나 구좌표'])
    blk(5.1, 0.4, 4.2, 3.9, '송신 쓰레드', ['신호처리 역할', '스캔마다 플롯을 섞어', 'STRUCT_PlotMsg 1 개로 보냄', '(과제 1 의 UDP 함수)'], ec=BLUE, tc=BLUE)
    blk(12.6, 0.4, 4.6, 3.9, '수신 쓰레드', ['데이터 처리 역할', '안테나 -> 동체 -> VC-NED', '트랙 관리 (Assigned_Flag 0)', '2 Point 트랙 개시'], ec=ORANGE,
        tc=ORANGE)
    blk(19.0, 0.4, 4.6, 3.9, 'UI (MFC)', ['처리가 끝난 스캔을 가져와', '2D, 3D 그림과 트랙 표', '슬라이더로 스캔 고르기'])
    arrow(ax, (4.25, 2.35), (5.05, 2.35), lw=1.4)
    ax.add_patch(FancyBboxPatch((9.75, 1.2), 2.4, 2.3, boxstyle='round,pad=0.02,rounding_size=0.15', fc='#EEF4FF', ec=BLUE, lw=1.2))
    ax.text(10.95, 3.3, 'UDP', ha='center', va='top', color=BLUE, fontsize=10.5, fontweight='bold')
    ax.text(10.95, 2.6, '127.0.0.1:50505\n데이터그램 1 개\n= 스캔 1 개', ha='center', va='top', color=SUB, fontsize=8, linespacing=1.45)
    arrow(ax, (9.35, 2.35), (9.7, 2.35), col=BLUE, lw=1.4)
    arrow(ax, (12.2, 2.35), (12.55, 2.35), col=BLUE, lw=1.4)
    arrow(ax, (17.25, 2.35), (18.95, 2.35), col=ORANGE, lw=1.4)
    ax.text(18.1, 2.6, '스캔별 결과', ha='center', color=ORANGE, fontsize=8.5)
    arrow(ax, (12.6, -0.35), (9.3, -0.35), col=MUTE, lw=1.0, ls='--', ms=7)
    ax.text(10.95, -0.2, '처리가 밀리면 송신이 기다림', ha='center', va='bottom', color=MUTE, fontsize=8)
    save(fig, 'fig_arch.png')


# 8-1. 연관 성공 / 실패 (개념)
def fig_assoc():
    fig, axs = plt.subplots(1, 2, figsize=(6.2, 2.55))
    fig.subplots_adjust(left=0.005, right=0.995, top=0.90, bottom=0.01, wspace=0.06)
    titles = ['예상 위치 근처에 이번 스캔 플롯이 있음', '예상 위치 근처에 플롯이 없음']
    for n, ax in enumerate(axs):
        ax.set_aspect('equal')
        ax.set_xlim(0, 10)
        ax.set_ylim(-0.9, 6.0)
        clean(ax)
        ax.add_patch(Rectangle((0.05, -0.85), 9.9, 6.8, fc=BOXF, ec=LINE, lw=0.8, zorder=0))
        ax.set_title(titles[n], color=NAVY, fontsize=8.5, fontweight='bold', loc='left', pad=4)
        # 지난 트랙 자리와 이번 예상 위치
        hist = [(1.2, 1.3), (2.5, 2.0), (3.8, 2.7)]
        ax.plot([p[0] for p in hist], [p[1] for p in hist], color=NAVY, lw=1.0, ls=':', zorder=2)
        for k, p in enumerate(hist):
            ax.add_patch(Rectangle((p[0] - 0.22, p[1] - 0.22), 0.44, 0.44, fc='white', ec=NAVY, lw=1.3 if k == 2 else 0.9,
                                   alpha=1.0 if k == 2 else 0.55, zorder=4))
        pred = (5.3, 3.4)
        arrow(ax, (4.05, 2.84), (pred[0] - 0.12, pred[1] - 0.06), col=NAVY, lw=1.0, ls='--', ms=7)
        ax.add_patch(Circle(pred, 1.15, fill=False, ec=BLUE, lw=1.1, ls='--', zorder=3))
        ax.plot(*pred, 'x', color=BLUE, ms=5, mew=1.2, zorder=5)
        ax.text(pred[0], pred[1] + 1.3, '예상 위치', ha='center', va='bottom', color=BLUE, fontsize=7.5)
        ax.text(3.8, 2.15, '트랙', ha='center', va='top', color=NAVY, fontsize=7.5)
        for c in [(1.3, 4.6), (8.6, 1.1), (8.9, 4.9)]:
            ax.plot(*c, 'o', color=CLUT, ms=3.5, zorder=4)
        if n == 0:
            ax.plot(5.75, 3.05, 'o', color=PLOT, ms=5, zorder=6)
            ax.text(6.6, 2.35, '이번 스캔 플롯', color=PLOT, fontsize=7.5, va='center')
            ax.text(5.0, -0.45, '연관 성공  ->  Assigned_Flag = 1', ha='center', va='center', color=CONF, fontsize=8.5, fontweight='bold')
        else:
            ax.text(6.6, 2.35, '들어온 플롯 없음', color=SUB, fontsize=7.5, va='center')
            ax.text(5.0, -0.45, '연관 실패  ->  Assigned_Flag = 0', ha='center', va='center', color=RED, fontsize=8.5, fontweight='bold')
        ax.text(8.75, 5.35, '클러터', ha='center', va='bottom', color=MUTE, fontsize=6.5)
    save(fig, 'fig_assoc.png')


# 9. 거리별 게이트 통과율: 이론 (몬테카를로) 과 시뮬레이션
def fig_rate():
    rng_axis = np.linspace(300, 15000, 60)
    rs = np.random.default_rng(1)
    theo = []
    for r in rng_axis:
        n = 40000
        d = np.stack([rs.normal(0, math.sqrt(2) * SIG_R, n), rs.normal(0, math.sqrt(2) * r * SIG_AZ, n),
                      rs.normal(0, math.sqrt(2) * r * SIG_EL, n)], axis=1)
        d[:, 1] += 200.0 * T                           # 표적 2 (대공) 속력. 이 거리들에서는 속력이 통과율을 거의 바꾸지 않음
        theo.append(np.mean(np.linalg.norm(d, axis=1) < VMAX * T))
    # 1 km 구간마다 같은 표적 쌍의 통과율. 점은 구간 안 쌍들의 평균 거리에 찍음 (구간을 고르게 채우지 않으므로)
    b = defaultdict(list)
    for r in read('gate.csv'):
        b[int(float(r['range']) // 1000)].append((float(r['range']), int(r['pass'])))
    fig, ax = plt.subplots(figsize=(6.1, 3.0))
    ax.plot(rng_axis / 1000, theo, color=BLUE, lw=1.8, label='이론 (잡음만 고려, 표적 200 m/s)')
    keys = [k for k in sorted(b) if len(b[k]) >= 20]
    xs = [np.mean([v[0] for v in b[k]]) / 1000 for k in keys]
    ys = [np.mean([v[1] for v in b[k]]) for k in keys]
    ax.plot(xs, ys, 'o', color=TENT, ms=5, label='시뮬레이션 (참 표적 쌍, 1 km 구간)')
    ax.axhline(1.0, color=LINE, lw=0.8)
    ax.set_xlim(0, 15)
    ax.set_ylim(0, 1.05)
    ax.set_xlabel('표적 거리 [km]', color=SUB, fontsize=8)
    ax.set_ylabel('게이트 통과율', color=SUB, fontsize=8)
    ax.tick_params(labelsize=7.5, colors=SUB)
    for s in ax.spines.values():
        s.set_color(LINE)
    ax.grid(color='#EEF0F4', lw=0.8)
    ax.legend(fontsize=7.5, frameon=False, loc='upper right')
    fig.tight_layout(pad=0.3)
    save(fig, 'fig_rate.png')


# 10. 플롯이 트랙이 되기까지 (개념)
def fig_track_concept():
    fig, ax = plt.subplots(figsize=(6.9, 2.55))
    fig.subplots_adjust(left=0.005, right=0.995, top=0.99, bottom=0.01)
    clean(ax)
    ax.set_xlim(-0.3, 10.3)
    ax.set_ylim(-0.5, 3.6)
    rs = np.random.default_rng(7)
    xs = [1.0, 3.4, 5.8, 8.2]
    names = ['스캔 k-1', '스캔 k', '스캔 k+1', '스캔 k+2']
    tgt = [(1.0 + 0.35, 1.35), (3.4 + 0.5, 1.6), (5.8 + 0.62, 1.9), (8.2 + 0.75, 2.15)]
    for i, x in enumerate(xs):
        ax.add_patch(Rectangle((x - 1.0, -0.15), 2.0, 3.2, fc=BOXF, ec=LINE, lw=0.8))
        ax.text(x, 2.85, names[i], ha='center', color=NAVY, fontsize=8, fontweight='bold')
        for _ in range(4):
            cx, cy = x - 0.8 + 1.6 * rs.random(), 0.05 + 2.4 * rs.random()
            if abs(cx - tgt[i][0]) + abs(cy - tgt[i][1]) > 0.5:
                ax.plot(cx, cy, 'o', color=CLUT, ms=3.2)
        ax.plot(*tgt[i], 'o', color=PLOT, ms=4.5, zorder=5)
    ax.plot([tgt[0][0], tgt[1][0]], [tgt[0][1], tgt[1][1]], color=BLUE, lw=1.4, zorder=4)
    track_box(ax, tgt[1][0], tgt[1][1], 0.13, 1, lw=1.4)
    ax.text(2.2, 0.62, '두 플롯으로 개시\n(속도 게이트 통과)', ha='center', color=BLUE, fontsize=7.5, bbox=dict(fc='white', ec='none', pad=1.0),
            zorder=6)
    ax.plot([tgt[1][0], tgt[2][0], tgt[3][0]], [tgt[1][1], tgt[2][1], tgt[3][1]], color=CONF, lw=1.2, ls='--', zorder=4)
    track_box(ax, tgt[2][0], tgt[2][1], 0.13, 1, lw=1.4)
    track_box(ax, tgt[3][0], tgt[3][1], 0.13, 2, lw=1.4)
    ax.text(7.1, 0.35, '연관 결과로 스코어를 올리고 내려\n확정 / 메모리 / 삭제 (트랙 관리)', ha='center', color=CONF, fontsize=7.5,
            bbox=dict(fc='white', ec='none', pad=1.0), zorder=6)
    ax.plot(9.55, 3.3, 'o', color=PLOT, ms=4)
    ax.text(9.68, 3.3, '플롯', va='center', fontsize=7, color=SUB)
    ax.plot(8.45, 3.3, 'o', color=CLUT, ms=3.2)
    ax.text(8.58, 3.3, '클러터', va='center', fontsize=7, color=SUB)
    save(fig, 'fig_track_concept.png')


# 11. 측정 오차를 북 · 동 (VC-NED) 로 옮기면: 빔 기준으로는 기울지 않은 타원이 북 · 동 축에서는 비스듬함 (p_example 의 표적 예)
def fig_rcov():
    lines = [l.strip() for l in open(os.path.join(DATA, 'p_example.csv'), encoding='utf-8')]
    k = lines.index('# R1')
    R = np.array([[float(v) for v in lines[k + 1 + i].split(',')] for i in range(3)])
    tail = lines[k + 4].split()
    pn, pe = float(tail[-3]), float(tail[-2])
    rng = math.hypot(pn, pe)
    los = np.array([pe, pn]) / rng                      # (동, 북) 레이다 -> 표적
    lat = np.array([los[1], -los[0]])                   # 옆 방향 (시선에 수직)
    if lat[0] < 0:
        lat = -lat
    s_lat = rng * SIG_AZ
    ang = math.degrees(math.atan2(lat[1], lat[0]))
    fig = plt.figure(figsize=(5.1, 3.9))
    L = max(68.0, 1.45 * s_lat)                        # 칸 반폭 [m]. 옆 오차 타원이 들어가게
    u = L / 68.0                                       # 글자 · 화살표 자리 비율
    for n, x0 in enumerate([0.02, 0.55]):
        ax = fig.add_axes([x0, 0.30, 0.43, 0.58])
        ax.set_aspect('equal')
        clean(ax)
        ax.set_xlim(-L, L)
        ax.set_ylim(-L, L)
        ax.add_patch(Rectangle((-L, -L), 2 * L, 2 * L, fc=BOXF, ec=LINE, lw=0.8, zorder=0))
        if n == 0:
            ax.set_title('빔 기준으로 보면', fontsize=10.5, color=NAVY, fontweight='bold', pad=4)
            arrow(ax, (-L + 8 * u, 0), (L - 6 * u, 0), col=MUTE, lw=0.8, ms=6)
            arrow(ax, (0, -L + 8 * u), (0, L - 6 * u), col=MUTE, lw=0.8, ms=6)
            ax.text(L - 6 * u, -7 * u, '옆 방향', ha='right', va='top', fontsize=8.5, color=SUB)
            ax.text(4 * u, L - 8 * u, '빔 방향\n(거리)', ha='left', va='top', fontsize=8.5, color=SUB, linespacing=1.15)
            ax.add_patch(Ellipse((0, 0), 2 * s_lat, 2 * SIG_R, fc='#DCE7FD', ec=BLUE, lw=1.2, zorder=3))
            ax.text(0, 9 * u, '옆 ±%.0f m' % s_lat, ha='center', va='bottom', fontsize=9.5, color=BLUE, fontweight='bold')
            ax.text(-L + 6 * u, -16 * u, '거리 ±%.1f m' % SIG_R, ha='left', va='top', fontsize=9.5, color=BLUE, fontweight='bold')
            ax.text(0, -L + 4 * u, '레이다 쪽', ha='center', va='bottom', fontsize=8, color=MUTE)
        else:
            ax.set_title('북 · 동 (VC-NED) 기준으로 보면', fontsize=10.5, color=NAVY, fontweight='bold', pad=4)
            arrow(ax, (-L + 8 * u, 0), (L - 6 * u, 0), col=MUTE, lw=0.8, ms=6)
            arrow(ax, (0, -L + 8 * u), (0, L - 6 * u), col=MUTE, lw=0.8, ms=6)
            ax.text(L - 6 * u, -7 * u, '동', ha='right', va='top', fontsize=9.5, color=SUB)
            ax.text(4 * u, L - 8 * u, '북', ha='left', va='top', fontsize=9.5, color=SUB)
            ax.plot([0, -los[0] * L * 0.95], [0, -los[1] * L * 0.95], color=MUTE, lw=0.9, ls=':', zorder=2)
            ax.text(-los[0] * L * 0.62 + 4 * u, -los[1] * L * 0.62, '레이다 쪽', ha='left', va='center', fontsize=8, color=MUTE)
            ax.add_patch(Ellipse((0, 0), 2 * s_lat, 2 * SIG_R, angle=ang, fc='#DCE7FD', ec=BLUE, lw=1.2, zorder=3))
            ax.text(-L + 6 * u, L - 8 * u, '같은 오차가\n축에 비스듬함', ha='left', va='top', fontsize=9, color=BLUE, fontweight='bold', linespacing=1.2)
    # 두 칸 사이: 축을 바꿈
    fig.text(0.50, 0.62, '->', ha='center', va='center', fontsize=13, color=NAVY, fontweight='bold')
    fig.text(0.50, 0.53, 'J, M', ha='center', va='center', fontsize=8.5, color=SUB)
    # 아래: 같은 오차를 두 축으로 적은 R (북 · 동 2 x 2 만, 단위 m²)
    rows = [(0.235, '[ %4.0f    %4.0f ]\n[ %4.0f    %4.0f ]' % (SIG_R ** 2, 0, 0, s_lat ** 2), '기울지 않아 대각선만 값이 있음'),
            (0.765, '[ %4.0f    %4.0f ]\n[ %4.0f    %4.0f ]' % (R[0, 0], R[0, 1], R[1, 0], R[1, 1]), '북 · 동 오차가 같이 움직여\n대각선 밖 칸도 큼')]
    for x, mat, note in rows:
        fig.text(x, 0.185, mat, ha='center', va='center', fontsize=10, color=NAVY, family='Consolas', linespacing=1.3)
        fig.text(x, 0.065, note, ha='center', va='center', fontsize=9, color=SUB, linespacing=1.2)
    fig.text(0.02, 0.185, 'R', ha='left', va='center', fontsize=10.5, color=NAVY, fontweight='bold')
    tgt = int(lines[0].split()[2])
    compass = ['북', '북북동', '북동', '동북동', '동', '동남동', '남동', '남남동', '남', '남남서', '남서', '서남서', '서', '서북서', '북서', '북북서']
    side = compass[int(round(math.degrees(math.atan2(pe, pn)) / 22.5)) % 16]
    fig.text(0.5, 0.965, '표적 %d (%s쪽 %.1f km) 플롯의 오차 범위  (단위 m², 아래 방향은 생략)' % (tgt, side, rng / 1000), ha='center', va='center', fontsize=8.5,
             color=SUB)
    save(fig, 'fig_rcov.png')


if __name__ == '__main__':
    fig_track_concept()
    fig_rcov()
    fig_cover()
    fig_1p2p()
    fig_pmatrix()
    fig_gate()
    fig_mn()
    fig_state()
    fig_score()
    fig_arch()
    fig_assoc()
    fig_rate()
