# -*- coding: utf-8 -*-
"""과제 5 발표 그림. python figures.py <data_dir> <cover_src.png> <out_dir>

data_dir 의 csv 는 dump_fig_data.c 가 만든 것 (기본 시나리오 + 표적 3개, SNR 20). cover_src 는 과제 4 발표자료 표지 바탕 그림.
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

DATA, COVER_SRC, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
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


# 1. 표지 바탕: 과제 4 표지 그림 위에 트랙 심볼 (임시 트랙 사각형 + 속도 선) 을 얹음
def fig_cover():
    im = Image.open(COVER_SRC).convert('RGBA')
    over = Image.new('RGBA', im.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    S, ox, oy = 79.2, 669, 1350                      # 1 km = 79.2 px, 레이다 자리
    trk = read('tracks.csv')
    seen = set()
    for r in trk:
        s = int(r['scan'])
        if int(r['init_scan']) != s or s % 9 != 0:
            continue
        x = ox + float(r['east']) / 1000 * S
        y = oy - float(r['north']) / 1000 * S
        lx = x + float(r['ve']) * 1.0 / 1000 * S
        ly = y - float(r['vn']) * 1.0 / 1000 * S
        d.line((x, y, lx, ly), fill=(234, 88, 12, 200), width=3)
        h = 9
        d.rectangle((x - h, y - h, x + h, y + h), outline=(234, 88, 12, 255), width=3)
        seen.add(s)
    Image.alpha_composite(im, over).convert('RGB').save(os.path.join(OUT, 'cover_bg.png'))
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


# 3. 9 x 9 오차 공분산 (예시 값)
def fig_pmatrix():
    lines = [l.strip() for l in open(os.path.join(DATA, 'p_example.csv'), encoding='utf-8')]
    P = np.array([[float(v) for v in lines[1 + i].split(',')] for i in range(9)])
    names = ['x', "x'", "x''", 'y', "y'", "y''", 'z', "z'", "z''"]
    kind = np.zeros((9, 9), dtype=int)
    for i in range(9):
        for j in range(9):
            a, b = i % 3, j % 3
            if a == 2 or b == 2:
                kind[i, j] = 0
            elif a == 0 and b == 0:
                kind[i, j] = 1
            elif a == 1 and b == 1:
                kind[i, j] = 3
            else:
                kind[i, j] = 2
    cols = {0: '#EEF0F4', 1: '#DCE7FD', 2: '#DDF3EA', 3: '#FDECD7'}
    fig, ax = plt.subplots(figsize=(4.55, 4.05))
    for i in range(9):
        for j in range(9):
            ax.add_patch(Rectangle((j, 8 - i), 1, 1, fc=cols[kind[i, j]], ec='white', lw=1.2))
            v = P[i, j]
            if kind[i, j] == 0:
                txt, c = '0', MUTE
            elif abs(v) >= 9999.5:
                e = int(math.floor(math.log10(abs(v))))
                txt, c = '%.1fe%d' % (v / 10 ** e, e), NAVY
            else:
                txt, c = '%.0f' % v, NAVY
            ax.text(j + 0.5, 8 - i + 0.5, txt, ha='center', va='center', fontsize=6.3, color=c)
    for k in (3, 6):
        ax.plot([k, k], [0, 9], color=SUB, lw=1.0)
        ax.plot([0, 9], [9 - k, 9 - k], color=SUB, lw=1.0)
    ax.set_xlim(0, 9)
    ax.set_ylim(0, 9)
    ax.set_xticks(np.arange(9) + 0.5)
    ax.set_xticklabels(names, fontsize=8, color=NAVY)
    ax.set_yticks(np.arange(9) + 0.5)
    ax.set_yticklabels(names[::-1], fontsize=8, color=NAVY)
    ax.xaxis.tick_top()
    ax.tick_params(length=0)
    for s in ax.spines.values():
        s.set_visible(False)
    ax.set_aspect('equal')
    fig.tight_layout(pad=0.2)
    save(fig, 'fig_pmatrix.png')


# 4. 속도 게이트: 짝짓기 예와 거리별 잡음
def fig_gate():
    fig = plt.figure(figsize=(11.9, 2.95))
    ax = fig.add_axes([0.0, 0.02, 0.40, 0.90])
    ax.set_aspect('equal')
    clean(ax)
    ax.set_xlim(-140, 330)
    ax.set_ylim(-120, 135)
    ax.set_title('짝짓기: 앞 스캔 플롯 (빈 원) 과 이번 스캔 플롯 (채운 원)', color=NAVY, fontsize=9, fontweight='bold', loc='left')
    prev = {'A': (-40, 10), 'B': (180, 40)}
    cur = {'1': (12, 40), '2': (85, -40), '3': (230, 70), '4': (260, -60)}
    for k, p in prev.items():
        ax.add_patch(Circle(p, 80, fill=False, ls='--', ec=MUTE, lw=0.9))
        ax.plot(*p, 'o', mfc='white', mec=PLOT, ms=6, mew=1.3, zorder=6)
        ax.text(p[0] - 8, p[1] + 8, k, ha='right', color=NAVY, fontsize=9, fontweight='bold')
    for k, p in cur.items():
        ax.plot(*p, 'o', color=PLOT, ms=6, zorder=6)
        ax.text(p[0] + 7, p[1] + 7, k, color=NAVY, fontsize=9, fontweight='bold')
    pairs = [('A', '1', True), ('A', '2', False), ('B', '3', True), ('B', '4', False)]
    for a, b, ok in pairs:
        p0, p1 = np.array(prev[a]), np.array(cur[b])
        dist = np.linalg.norm(p1 - p0)
        col = GREEN if ok else RED
        ax.plot([p0[0], p1[0]], [p0[1], p1[1]], color=col, lw=1.5 if ok else 1.2, ls='-' if ok else ':', zorder=4)
        u = (p1 - p0) / dist
        nrm = np.array([-u[1], u[0]])
        pos = p0 + 0.55 * (p1 - p0) + (16 if ok else -16) * nrm
        ax.text(pos[0], pos[1], '%.0f m/s %s' % (dist / T, '통과' if ok else '탈락'), color=col, fontsize=7.5, ha='center', va='center',
                bbox=dict(fc='white', ec='none', pad=0.6))
    ax.text(-135, -112, '점선 원 = 반경 V_max · T = 80 m.  속력이 작은 쌍부터 하나씩 고르고, 쓴 플롯은 다시 쓰지 않는다',
            color=SUB, fontsize=7.5)
    # 거리별 잡음 (두 플롯 차 Δz 의 2σ 타원) 과 게이트
    rng = [(1000, '1 km'), (5000, '5 km'), (14000, '14 km')]
    gate = defaultdict(lambda: [0, 0])
    for r in read('gate.csv'):
        b = int(float(r['range']) // 1000)
        gate[b][0] += int(r['pass'])
        gate[b][1] += 1
    for n, (r, name) in enumerate(rng):
        ax2 = fig.add_axes([0.43 + n * 0.19, 0.04, 0.18, 0.82])
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
        b = r // 1000 if r < 14000 else 14
        rate = gate[b][0] / gate[b][1] if gate[b][1] else float('nan')
        ax2.set_title('%s:  횡방향 2σ ±%.0f m' % (name, sx), color=NAVY, fontsize=8.5, fontweight='bold')
        ax2.text(0, -335, '게이트 통과율 %.0f %%' % (100 * rate), ha='center', color=RED if rate < 0.5 else GREEN, fontsize=8.5,
                 fontweight='bold')
        if n == 0:
            ax2.text(-350, 300, '파란 타원: 두 플롯 차의 2σ\n빨간 원: 게이트 80 m', color=SUB, fontsize=7.2, va='top')
    save(fig, 'fig_gate.png')


# 5. M/N 로직
def fig_mn():
    fig, ax = plt.subplots(figsize=(6.2, 2.6))
    fig.subplots_adjust(left=0.005, right=0.995, top=0.99, bottom=0.01)
    clean(ax)
    ax.set_aspect('equal')
    ax.set_xlim(-0.6, 13.0)
    ax.set_ylim(-1.2, 4.6)
    hits = [1, 1, 0, 1, 1]
    ax.text(-0.5, 4.25, '임시 트랙 (예: M = 3, N = 5)', color=NAVY, fontsize=9, fontweight='bold')
    for i, h in enumerate(hits):
        ax.add_patch(Circle((i, 3.1), 0.36, fc=GREEN if h else 'white', ec=GREEN if h else RED, lw=1.3))
        ax.text(i, 3.1, '성공' if h else '실패', ha='center', va='center', fontsize=6.5, color='white' if h else RED)
        ax.text(i, 2.5, '스캔 %d' % (i + 1), ha='center', va='top', fontsize=6.5, color=MUTE)
    ax.add_patch(FancyBboxPatch((-0.5, 2.62), 5.0, 0.95, boxstyle='round,pad=0.02,rounding_size=0.15', fill=False, ec=BLUE, lw=1.2,
                                ls='--'))
    ax.text(5.0, 3.35, 'N = 5 번 중 M = 3 번 성공 -> 확정', color=BLUE, fontsize=8, va='center', fontweight='bold')
    ax.text(5.0, 2.8, 'M 번 연속 실패 -> 삭제', color=RED, fontsize=8, va='center', fontweight='bold')
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
    ax.text(9.95, 0.15, '문턱: 확정 5, 임시 삭제 0, 메모리 삭제 5 (과제 값)', ha='right', color=SUB, fontsize=7.5)
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


# 7. 스코어 변화 예 두 가지
def fig_score():
    cases = [([1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0], '연관 결과를 넣은 예'), ([0, 0, 0], '과제 조건: Assigned_Flag 늘 0')]
    fig, axs = plt.subplots(1, 2, figsize=(11.9, 2.75), gridspec_kw={'width_ratios': [2.3, 1]})
    bg = {1: '#FFF1E6', 2: '#E8F7EE', 3: '#F5EBFF', 4: '#EEF0F4'}
    for ax, (flags, title) in zip(axs, cases):
        seq = score_seq(flags)
        n = len(seq)
        for i, (f, ts, ms, st) in enumerate(seq):
            ax.add_patch(Rectangle((i - 0.5, -1.2), 1, 8.2, fc=bg[st], ec='white', lw=1.0, zorder=0))
            ax.text(i, 6.45, '-' if f is None else str(f), ha='center', color=NAVY, fontsize=8, fontweight='bold')
            ax.text(i, -0.95, {1: '임시', 2: '확정', 3: '메모리', 4: '삭제'}[st], ha='center', fontsize=7,
                    color={1: TENT, 2: CONF, 3: MEM, 4: DEL}[st])
        xs = np.arange(n)
        ax.step(xs, [s[1] for s in seq], where='mid', color=TENT, lw=1.8, label='TrackScore')
        ax.plot(xs, [s[1] for s in seq], 'o', color=TENT, ms=3.5)
        ax.step(xs, [s[2] for s in seq], where='mid', color=MEM, lw=1.6, ls='--', label='MemoryScore')
        ax.plot(xs, [s[2] for s in seq], 's', color=MEM, ms=3)
        ax.axhline(5, color=CONF, lw=0.8, ls=':')
        ax.axhline(0, color=DEL, lw=0.8, ls=':')
        ax.set_xlim(-0.5, n - 0.5)
        ax.set_ylim(-1.2, 7.0)
        ax.set_xticks(xs)
        ax.set_xticklabels([str(i) for i in xs], fontsize=7.5, color=SUB)
        ax.set_yticks([0, 1, 2, 3, 4, 5])
        ax.tick_params(labelsize=7.5, colors=SUB, length=0)
        for s in ax.spines.values():
            s.set_color(LINE)
        ax.set_title(title, color=NAVY, fontsize=9, fontweight='bold', loc='left', pad=14)
        ax.text(-0.45, 7.25, 'Assigned_Flag', color=SUB, fontsize=7)
        ax.set_xlabel('개시 후 스캔', fontsize=7.5, color=SUB)
    axs[0].legend(loc='lower right', fontsize=7.5, frameon=True, framealpha=0.95, edgecolor=LINE, bbox_to_anchor=(1.0, 1.0), ncol=2)
    fig.tight_layout(pad=0.4, w_pad=2.0)
    save(fig, 'fig_score.png')


# 8. 쓰레드 · UDP 구조
def fig_arch():
    fig, ax = plt.subplots(figsize=(11.9, 2.7))
    fig.subplots_adjust(left=0.005, right=0.995, top=0.99, bottom=0.01)
    clean(ax)
    ax.set_xlim(0, 24)
    ax.set_ylim(-0.9, 5.2)

    def blk(x, y, w, h, title, lines, ec=NAVY, fc=BOXF, tc=NAVY):
        ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle='round,pad=0.02,rounding_size=0.15', fc=fc, ec=ec, lw=1.4))
        ax.text(x + w / 2, y + h - 0.3, title, ha='center', va='top', color=tc, fontsize=9, fontweight='bold')
        ax.text(x + w / 2, y + h - 1.05, '\n'.join(lines), ha='center', va='top', color=SUB, fontsize=7.5, linespacing=1.5)

    ax.add_patch(FancyBboxPatch((0.2, -0.75), 17.4, 5.75, boxstyle='round,pad=0.02,rounding_size=0.2', fc='white', ec=LINE, lw=1.0, ls='--'))
    ax.text(0.45, 4.8, 'TargetSimCore (DLL)', color=MUTE, fontsize=8, va='top')
    blk(0.6, 0.4, 3.6, 3.9, '측정 모의 (과제 4)', ['표적 플롯 + 클러터', '스캔 600 개', '안테나 구좌표'])
    blk(5.1, 0.4, 4.2, 3.9, '송신 쓰레드', ['신호처리 역할', '스캔마다 플롯을 섞고', '플롯 ID 10000 부터', 'STRUCT_PlotMsg 1 개'], ec=BLUE, tc=BLUE)
    blk(12.6, 0.4, 4.6, 3.9, '수신 쓰레드', ['데이터 처리 역할', '안테나 -> 동체 -> VC-NED', '트랙 관리 (Flag 0)', '2 Point 트랙 개시'], ec=ORANGE,
        tc=ORANGE)
    blk(19.0, 0.4, 4.6, 3.9, 'UI (MFC)', ['50 ms 타이머로', '처리된 스캔을 가져와', '2D, 3D 그림, 트랙 표', '슬라이더로 스캔 고르기'])
    arrow(ax, (4.25, 2.35), (5.05, 2.35), lw=1.4)
    ax.add_patch(FancyBboxPatch((9.75, 1.2), 2.4, 2.3, boxstyle='round,pad=0.02,rounding_size=0.15', fc='#EEF4FF', ec=BLUE, lw=1.2))
    ax.text(10.95, 3.25, 'UDP', ha='center', va='top', color=BLUE, fontsize=9, fontweight='bold')
    ax.text(10.95, 2.6, '127.0.0.1:50505\n데이터그램 1 개\n= 스캔 1 개', ha='center', va='top', color=SUB, fontsize=7, linespacing=1.4)
    arrow(ax, (9.35, 2.35), (9.7, 2.35), col=BLUE, lw=1.4)
    arrow(ax, (12.2, 2.35), (12.55, 2.35), col=BLUE, lw=1.4)
    arrow(ax, (17.25, 2.35), (18.95, 2.35), col=ORANGE, lw=1.4)
    ax.text(18.1, 2.55, '결과 복사', ha='center', color=ORANGE, fontsize=7.5)
    ax.text(18.1, 1.95, '(잠금 안에서)', ha='center', color=MUTE, fontsize=7)
    arrow(ax, (12.6, -0.35), (9.3, -0.35), col=MUTE, lw=1.0, ls='--', ms=7)
    ax.text(10.95, -0.2, '처리가 밀리면 송신이 기다림', ha='center', va='bottom', color=MUTE, fontsize=7)
    save(fig, 'fig_arch.png')


# 9. 거리별 게이트 통과율: 이론 (몬테카를로) 과 시뮬레이션
def fig_rate():
    rng_axis = np.linspace(300, 15000, 60)
    rs = np.random.default_rng(1)
    theo = []
    for r in rng_axis:
        n = 40000
        d = np.stack([rs.normal(0, math.sqrt(2) * SIG_R, n), rs.normal(0, math.sqrt(2) * r * SIG_AZ, n),
                      rs.normal(0, math.sqrt(2) * r * SIG_EL, n)], axis=1)
        d[:, 1] += 100.0 * T
        theo.append(np.mean(np.linalg.norm(d, axis=1) < VMAX * T))
    gate = read('gate.csv')
    b = defaultdict(lambda: [0, 0])
    for r in gate:
        k = int(float(r['range']) // 1000)
        b[k][0] += int(r['pass'])
        b[k][1] += 1
    fig, ax = plt.subplots(figsize=(6.1, 3.0))
    ax.plot(rng_axis / 1000, theo, color=BLUE, lw=1.8, label='이론 (잡음만 고려, 표적 100 m/s)')
    xs = [k + 0.5 for k in sorted(b) if b[k][1] >= 20]
    ys = [b[k][0] / b[k][1] for k in sorted(b) if b[k][1] >= 20]
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


# 11. 측정 공분산 모양 (표적 5 예시, 북-동 평면)
def fig_rcov():
    lines = [l.strip() for l in open(os.path.join(DATA, 'p_example.csv'), encoding='utf-8')]
    i = lines.index('# R1')
    R = np.array([[float(v) for v in lines[i + 1 + k].split(',')] for k in range(3)])
    tail = lines[i + 4].split()
    pn, pe = float(tail[6]), float(tail[7])
    fig, ax = plt.subplots(figsize=(4.7, 3.55))
    ax.set_aspect('equal')
    ax.plot([0, pe / 1000], [0, pn / 1000], color=MUTE, lw=0.9, ls='--')
    ax.plot(0, 0, '^', color=NAVY, ms=7)
    ax.text(0.0, -0.45, '레이다', color=NAVY, fontsize=7.5, ha='center', va='top')
    C = R[:2, :2][[1, 0]][:, [1, 0]]                # (동, 북) 순서
    w, v = np.linalg.eigh(C)
    k = 20.0
    ang = math.degrees(math.atan2(v[1, 1], v[0, 1]))
    ax.add_patch(Ellipse((pe / 1000, pn / 1000), 2 * 2 * k * math.sqrt(w[1]) / 1000, 2 * 2 * k * math.sqrt(w[0]) / 1000, angle=ang,
                         fc='#DCE7FD', ec=BLUE, lw=1.2))
    ax.plot(pe / 1000, pn / 1000, 'o', color=PLOT, ms=4)
    ax.text(pe / 1000 + 0.35, pn / 1000 + 1.55, '표적 5 플롯  (%.1f km)' % (math.hypot(pn, pe) / 1000), color=NAVY, fontsize=7.5)
    ax.text(-5.9, -1.1, '2σ 타원, 20 배로 키워 그림\n가로 방향 σ = r·σ_az = 약 %.0f m\n거리 방향 σ = σ_r = 4.7 m\n아래 방향 σ = r·σ_el = 약 %.0f m'
            % (math.sqrt(w[1]), math.sqrt(R[2, 2])), color=SUB, fontsize=7.5, va='top')
    ax.set_xlim(-6.2, 1.2)
    ax.set_ylim(-3.6, 6.6)
    ax.set_xlabel('동 [km]', fontsize=7.5, color=SUB)
    ax.set_ylabel('북 [km]', fontsize=7.5, color=SUB)
    ax.tick_params(labelsize=7, colors=SUB)
    for s in ax.spines.values():
        s.set_color(LINE)
    ax.grid(color='#EEF0F4', lw=0.8)
    fig.tight_layout(pad=0.3)
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
    fig_rate()
