# -*- coding: utf-8 -*-
"""결과 그림: 시뮬레이션 출력 (tracks/plots/truth csv) 으로 2D · 3D 화면을 그림. python result_figs.py <data> <out> [scan]"""
import csv, math, os, sys
from collections import defaultdict
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Polygon
from matplotlib.lines import Line2D

plt.rcParams['font.family'] = os.environ.get('FIG_FONT', 'Noto Sans CJK KR')
plt.rcParams['axes.unicode_minus'] = False
NAVY, SUB, MUTE, LINE = '#1B2740', '#4A5A7A', '#8794AC', '#D2D9E5'
TENT, CONF, MEM, DEL = '#EA580C', '#16A34A', '#9333EA', '#6B7280'
PLOT, CLUT = '#1F2937', '#9CA3AF'
OBJ = {1: '#2563EB', 2: '#DC2626'}
DATA, OUT = sys.argv[1], sys.argv[2]
SCAN = int(sys.argv[3]) if len(sys.argv) > 3 else 546
os.makedirs(OUT, exist_ok=True)
read = lambda n: list(csv.DictReader(open(os.path.join(DATA, n), encoding='utf-8')))
truth, plots, tracks = read('truth.csv'), read('plots.csv'), read('tracks.csv')
path = defaultdict(list)
for r in truth:
    path[int(r['target'])].append((int(r['scan']), float(r['north']) / 1000, float(r['east']) / 1000, -float(r['down']) / 1000))
cur_plots = [r for r in plots if int(r['scan']) == SCAN]
old_plots = [r for r in plots if SCAN - 30 <= int(r['scan']) < SCAN]
cur_tracks = [r for r in tracks if int(r['scan']) == SCAN]

def track_sym(ax, x, y, state, half, lw=1.5, z=6):
    col = {1: TENT, 2: CONF, 3: MEM, 4: DEL}[state]
    ax.add_patch(Rectangle((x - half, y - half), 2 * half, 2 * half, facecolor=col if state == 2 else 'white', edgecolor=col, lw=lw,
                           ls='--' if state == 3 else '-', zorder=z))
    if state == 4:
        ax.plot([x - half, x + half], [y - half, y + half], color=col, lw=1.0, zorder=z + 1)
        ax.plot([x - half, x + half], [y + half, y - half], color=col, lw=1.0, zorder=z + 1)

def draw2d(ax, xlim, ylim, half, label_tracks=True, fov=True, title=None):
    ax.set_facecolor('white')
    if fov:
        for sgn in (-1, 1):
            a = math.radians(60 * sgn)
            ax.plot([0, 15 * math.sin(a)], [0, 15 * math.cos(a)], color=LINE, lw=0.8, ls='--', zorder=1)
        for km in (5, 10, 15):
            t = np.linspace(-math.pi / 3, math.pi / 3, 80)
            ax.plot(km * np.sin(t), km * np.cos(t), color=LINE, lw=0.6, zorder=1)
        ax.add_patch(Polygon([(0, 0.25), (-0.2, -0.15), (0.2, -0.15)], closed=True, facecolor=MUTE, edgecolor='none', zorder=3))
        ax.text(0.3, -0.1, '레이다', color=SUB, fontsize=7, va='top')
    for r in old_plots:
        ax.plot(float(r['east']) / 1000, float(r['north']) / 1000, 'o', ms=2.2, color=CLUT if r['source'] == '0' else PLOT, alpha=0.35, mec='none', zorder=2)
    for k, p in path.items():
        xs = [v[2] for v in p]; ys = [v[1] for v in p]
        ax.plot(xs, ys, color=OBJ[k], lw=1.4, alpha=0.9, zorder=3)
        s0 = p[0]; sc = p[min(SCAN, len(p) - 1)]
        ax.plot(s0[2], s0[1], 'o', ms=5, mfc='white', mec=OBJ[k], mew=1.2, zorder=4)
        ax.plot(sc[2], sc[1], 'D', ms=6, mfc=OBJ[k], mec='white', mew=0.8, zorder=5)
        ax.annotate('표적 %d' % k, (sc[2], sc[1]), xytext=(6, 6), textcoords='offset points', color=OBJ[k], fontsize=8, fontweight='bold')
    for r in cur_plots:
        x, y = float(r['east']) / 1000, float(r['north']) / 1000
        ax.plot(x, y, 'o', ms=6, mfc=CLUT if r['source'] == '0' else PLOT, mec='white', mew=1.2, zorder=5)
    for r in cur_tracks:
        x, y = float(r['east']) / 1000, float(r['north']) / 1000
        st = int(r['state'])
        track_sym(ax, x, y, st, half)
        if st != 4:
            ax.plot([x, x + float(r['ve']) / 1000], [y, y + float(r['vn']) / 1000], color=TENT, lw=1.2, zorder=6)
        if label_tracks:
            ax.annotate('트랙 ' + r['track_id'] + {1: ' (임시)', 4: ' (삭제)'}.get(st, ''), (x, y), xytext=(6, -12), textcoords='offset points', color=TENT if st != 4 else DEL, fontsize=7.5)
    ax.set_xlim(*xlim); ax.set_ylim(*ylim)
    ax.set_aspect('equal')
    ax.tick_params(labelsize=7, colors=SUB)
    for s in ax.spines.values(): s.set_color(LINE)
    ax.grid(color='#EEF0F4', lw=0.7)
    if title: ax.set_title(title, fontsize=9, color=NAVY, loc='left', fontweight='bold')

def legend(ax):
    h = [Line2D([], [], color=OBJ[1], lw=1.4, label='참값 궤적 (표적 색), 마름모: 지금 위치'),
         Line2D([], [], marker='o', ls='', mfc=PLOT, mec='white', ms=6, label='플롯 (큰 점: 이번 스캔, 옅은 점: 지난 30 스캔)'),
         Line2D([], [], marker='o', ls='', mfc=CLUT, mec='white', ms=6, label='클러터'),
         Line2D([], [], marker='s', ls='', mfc='white', mec=TENT, mew=1.5, ms=7, label='임시 트랙 (선: 1 초 동안 가는 거리)'),
         Line2D([], [], marker='x', ls='', color=DEL, ms=7, mew=1.2, label='삭제 트랙'),
         Line2D([], [], marker='s', ls='', mfc=CONF, mec=CONF, ms=7, label='확정 트랙 (이번 과제에서는 생기지 않음)')]
    ax.legend(handles=h, fontsize=7, frameon=True, framealpha=0.95, edgecolor=LINE, loc='lower right')

# 1. 2D 전체 + 표적 2 확대 + 3D 를 한 장에 (슬라이드 폭 11.93 in 에 맞춤)
from mpl_toolkits.mplot3d import Axes3D  # noqa
fig = plt.figure(figsize=(11.9, 3.1))
ax1 = fig.add_axes([0.035, 0.13, 0.27, 0.78])
draw2d(ax1, (-9, 9), (-1, 15.5), 0.18, label_tracks=False, title='2D 전체 (동 · 북, km)   스캔 %d, %.1f s' % (SCAN, SCAN * 0.1))
p2 = path[2][SCAN]
ax2 = fig.add_axes([0.345, 0.13, 0.225, 0.78])
draw2d(ax2, (p2[2] - 1.0, p2[2] + 1.0), (p2[1] - 1.0, p2[1] + 1.0), 0.04, fov=False, title='표적 2 주변 확대 (±1 km)')
for a in (ax1, ax2):
    a.set_xlabel('동 [km]', fontsize=7, color=SUB, labelpad=1); a.set_ylabel('북 [km]', fontsize=7, color=SUB, labelpad=1)
ax = fig.add_axes([0.575, -0.06, 0.43, 1.08], projection='3d')
ZK = 3.0
for k, p in path.items():
    xs = [v[2] for v in p]; ys = [v[1] for v in p]; zs = [v[3] * ZK for v in p]
    ax.plot(xs, ys, zs, color=OBJ[k], lw=1.4)
    ax.plot(xs, ys, [0] * len(xs), color=OBJ[k], lw=0.8, alpha=0.25)
    sc = p[SCAN]
    ax.plot([sc[2]], [sc[1]], [sc[3] * ZK], 'D', ms=6, mfc=OBJ[k], mec='white')
    ax.plot([sc[2], sc[2]], [sc[1], sc[1]], [0, sc[3] * ZK], color=OBJ[k], lw=0.6, ls=':')
    ax.text(sc[2] + 0.3, sc[1], sc[3] * ZK + 0.3, '표적 %d' % k, color=OBJ[k], fontsize=8, fontweight='bold')
for r in old_plots:
    z = -float(r['down']) / 1000 * ZK
    if z > 8: continue
    ax.plot([float(r['east']) / 1000], [float(r['north']) / 1000], [z], 'o', ms=2, color=CLUT if r['source'] == '0' else PLOT, alpha=0.35, mec='none')
for r in cur_plots:
    z = -float(r['down']) / 1000 * ZK
    if z > 8: continue
    ax.plot([float(r['east']) / 1000], [float(r['north']) / 1000], [z], 'o', ms=5, mfc=CLUT if r['source'] == '0' else PLOT, mec='white', mew=1)
for r in cur_tracks:
    x, y, z = float(r['east']) / 1000, float(r['north']) / 1000, -float(r['down']) / 1000 * ZK
    st = int(r['state'])
    ax.plot([x], [y], [z], marker='s', ms=8, mfc='white', mec=TENT if st != 4 else DEL, mew=1.5, ls='')
    if st == 4:
        ax.plot([x], [y], [z], marker='x', ms=6, color=DEL, mew=1.2, ls='')
    else:
        ax.plot([x, x + float(r['ve']) / 1000], [y, y + float(r['vn']) / 1000], [z, z - float(r['vd']) / 1000 * ZK], color=TENT, lw=1.2)
    ax.text(x + 0.2, y, z + 0.25, r['track_id'], color=TENT if st != 4 else DEL, fontsize=7)
ax.set_xlim(-9, 9); ax.set_ylim(0, 15.5); ax.set_zlim(0, 8)
ax.set_xlabel('동 [km]', fontsize=7, color=SUB, labelpad=-3); ax.set_ylabel('북 [km]', fontsize=7, color=SUB, labelpad=-3); ax.set_zlabel('고도 x3 [km]', fontsize=7, color=SUB, labelpad=-5)
ax.tick_params(labelsize=6, colors=SUB, pad=-3)
ax.view_init(elev=28, azim=-55)
ax.xaxis.pane.set_facecolor('#F7F9FC'); ax.yaxis.pane.set_facecolor('#F7F9FC'); ax.zaxis.pane.set_facecolor('white')
ax.text2D(0.08, 0.90, '3D (동 · 북 · 고도, 고도 3 배, 옅은 선: 바닥 그림자)', transform=ax.transAxes, fontsize=9, color=NAVY, fontweight='bold')
fig.savefig(os.path.join(OUT, 'res_view.png'), dpi=220, facecolor='white'); plt.close(fig)

# 3. 트랙 수명 띠: 스캔 540~552 에 살아 있는 트랙을 ID 별로 (상태 색)
fig, ax = plt.subplots(figsize=(5.8, 2.1))
s0, s1 = 538, 552
rows = defaultdict(dict)
for r in tracks:
    s = int(r['scan'])
    if s0 <= s <= s1:
        rows[int(r['track_id'])][s] = (int(r['state']), int(r['score']))
ids = sorted(rows)
for i, tid in enumerate(ids):
    for s, (st, sc) in rows[tid].items():
        col = {1: TENT, 4: DEL}.get(st, CONF)
        ax.add_patch(Rectangle((s - 0.45, i - 0.36), 0.9, 0.72, facecolor=col, alpha=0.18 if st == 4 else 0.35, edgecolor=col, lw=1.0))
        ax.text(s, i, str(sc), ha='center', va='center', fontsize=7.5, color=NAVY)
    ax.text(s0 - 0.7, i, 'ID %d' % tid, ha='right', va='center', fontsize=7.5, color=NAVY)
ax.set_xlim(s0 - 0.6, s1 + 0.6); ax.set_ylim(-0.7, len(ids) - 0.3)
ax.set_yticks([]); ax.set_xticks(range(s0, s1 + 1)); ax.tick_params(labelsize=7, colors=SUB)
for sp in ax.spines.values(): sp.set_visible(False)
ax.set_xlabel('스캔 번호  (칸 안 숫자: TrackScore,  주황: 임시,  회색: 삭제)', fontsize=7.5, color=SUB)
fig.tight_layout(pad=0.3)
fig.savefig(os.path.join(OUT, 'res_life.png'), dpi=220, facecolor='white'); plt.close(fig)
print('done', SCAN, [(r['track_id'], r['state']) for r in cur_tracks], 'ids', ids)
