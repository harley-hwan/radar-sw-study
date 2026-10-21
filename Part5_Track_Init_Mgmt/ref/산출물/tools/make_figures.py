# 과제 5 발표 그림 (시뮬레이션 데이터 data/*.csv 사용)
# python make_figures.py <data dir> <out dir>
import sys, os, math
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
from matplotlib.patches import FancyBboxPatch, Circle, Ellipse, Rectangle, FancyArrowPatch, Polygon, Wedge

DATA = sys.argv[1] if len(sys.argv) > 1 else 'data'
OUT = sys.argv[2] if len(sys.argv) > 2 else 'fig'
os.makedirs(OUT, exist_ok=True)

for name in ['Malgun Gothic', 'Noto Sans CJK KR', 'Noto Sans KR']:
    if any(f.name == name for f in font_manager.fontManager.ttflist):
        plt.rcParams['font.family'] = name
        break
plt.rcParams['axes.unicode_minus'] = False

NAVY = '#1B2740'; SUB = '#4A5A7A'; MUTED = '#8794AC'; LIGHT = '#AEB9CC'; GRID = '#E6EAF0'
BLUE = '#2563EB'; RED = '#DC2626'; ORANGE = '#D97706'; GREEN = '#059669'; PURPLE = '#7C3AED'; GRAY = '#9CA3AF'
F_BLUE = '#EBF2FE'; F_RED = '#FDEEEE'; F_ORANGE = '#FEF5E7'; F_GREEN = '#E8F6F0'; F_GRAY = '#F7F9FC'; F_PURPLE = '#F3E8FF'
TGT = {1: BLUE, 2: RED}

tr = pd.read_csv(os.path.join(DATA, 'truth.csv'))
pl = pd.read_csv(os.path.join(DATA, 'plots.csv'))
tk = pd.read_csv(os.path.join(DATA, 'tracks.csv'))
sl = pd.read_csv(os.path.join(DATA, 'scanlog.csv'))


def save(fig, name, dpi=200):
    fig.savefig(os.path.join(OUT, name), dpi=dpi, facecolor=fig.get_facecolor())
    plt.close(fig)
    print('saved', name)


def clean(ax):
    for s in ax.spines.values():
        s.set_color(LIGHT)
    ax.tick_params(colors=SUB, labelsize=9)


def track_square(ax, x, y, state, size=9, lw=1.6, z=6):
    col = {1: ORANGE, 2: GREEN, 3: PURPLE, 4: GRAY}[state]
    face = {1: 'none', 2: GREEN, 3: 'none', 4: 'none'}[state]
    ls = '--' if state == 3 else '-'
    ax.plot([x], [y], marker='s', ms=size, mfc=face, mec=col, mew=lw, ls='none', zorder=z)
    if state == 4:
        ax.plot([x], [y], marker='x', ms=size * 0.7, mec=col, mew=1.2, ls='none', zorder=z + 1)


# ---------------------------------------------------------------- cover / end background
def cover(name, emphasis=True):
    W, H = 2933, 1650
    fig = plt.figure(figsize=(W / 200, H / 200), dpi=200)
    fig.patch.set_facecolor('#1B2740')
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_facecolor('#1B2740')
    ax.set_xlim(-7.6, 25.3)
    ax.set_ylim(-3.33, 15.17)
    ax.set_aspect('equal')
    ax.axis('off')
    for gx in np.arange(-10, 23, 0.66):
        ax.axvline(gx, color='#26355A', lw=0.6, zorder=0)
    for gy in np.arange(-3, 16, 0.66):
        ax.axhline(gy, color='#26355A', lw=0.6, zorder=0)
    for r in (5, 10, 15):
        ax.add_patch(Circle((0, 0), r, fill=False, ec='#3A4C78', lw=1.0, zorder=1))
    for a in (30, 150):
        ax.plot([0, 16 * math.cos(math.radians(a))], [0, 16 * math.sin(math.radians(a))], color='#3A4C78', lw=1.0, ls=(0, (4, 4)), zorder=1)
    c = pl[pl.source == 0]
    ax.scatter(c.east / 1000, c.north / 1000, s=1.6, c='#8C9BBA', alpha=0.45, lw=0, zorder=2)
    for k in (1, 2):
        t = tr[tr.obj == k]
        ax.plot(t.east / 1000, t.north / 1000, color=TGT[k], lw=2.2, alpha=0.95, zorder=3)
        p = pl[pl.source == k]
        ax.scatter(p.east / 1000, p.north / 1000, s=2.2, c='#C9D3E6', alpha=0.55, lw=0, zorder=2)
    if emphasis:
        made = tk[tk.scan == tk.init_scan]
        for _, r in made.iloc[::9].iterrows():
            ax.plot(r.east / 1000, r.north / 1000, marker='s', ms=5.5, mfc='none', mec='#F59E0B', mew=1.1, zorder=4)
    ax.plot([0], [0], marker='^', ms=12, color='#CBD5E1', zorder=5)
    save(fig, name, dpi=200)


# ---------------------------------------------------------------- 1.1 plot vs track
def fig_plot_track():
    rng = np.random.default_rng(7)
    fig, axs = plt.subplots(1, 2, figsize=(8.1, 2.95))
    fig.patch.set_facecolor('white')
    # synthetic: two targets over 6 scans, clutter random
    scans = np.arange(6)
    t1 = np.c_[1.0 + 0.9 * scans, 1.2 + 0.35 * scans]
    t2 = np.c_[6.4 - 0.15 * scans, 4.6 - 0.62 * scans]
    clutter = [rng.uniform([0, 0], [7.5, 5.2], size=(3, 2)) for _ in scans]
    noise1 = rng.normal(0, 0.12, size=t1.shape); noise2 = rng.normal(0, 0.12, size=t2.shape)
    for ax, title in zip(axs, ['레이다가 주는 것: 스캔마다 점 (플롯)', '우리가 만들고 싶은 것: 같은 물체끼리 묶은 트랙']):
        ax.set_xlim(-0.3, 7.8); ax.set_ylim(-0.3, 5.5); ax.set_aspect('equal')
        ax.set_xticks([]); ax.set_yticks([])
        for s in ax.spines.values(): s.set_color(LIGHT)
        ax.set_title(title, fontsize=10.5, color=NAVY, loc='left', pad=6, fontweight='bold')
    shades = np.linspace(0.25, 1.0, len(scans))
    for i in scans:
        for ax in axs:
            ax.scatter(*(t1[i] + noise1[i]), s=26, color=NAVY, alpha=shades[i], zorder=3)
            ax.scatter(*(t2[i] + noise2[i]), s=26, color=NAVY, alpha=shades[i], zorder=3)
            ax.scatter(clutter[i][:, 0], clutter[i][:, 1], s=22, color=GRAY, alpha=shades[i] * 0.9, zorder=2)
    axs[0].text(0.02, 0.03, '진한 점일수록 최근 스캔  ·  표적인지 클러터인지 꼬리표가 없음', transform=axs[0].transAxes, fontsize=8.2, color=SUB)
    ax = axs[1]
    for pts, col, tid in ((t1 + noise1, BLUE, 'Track 101'), (t2 + noise2, RED, 'Track 102')):
        ax.plot(pts[:, 0], pts[:, 1], color=col, lw=1.6, zorder=2)
        x, y = pts[-1]
        track_square(ax, x, y, 2, size=13)
        v = pts[-1] - pts[-2]
        ax.annotate('', xy=(x + 1.6 * v[0], y + 1.6 * v[1]), xytext=(x, y), arrowprops=dict(arrowstyle='->', color=col, lw=1.6), zorder=5)
        ax.text(x - 1.9 if col == BLUE else x + 0.3, y + 0.35 if col == BLUE else y - 0.1, tid, fontsize=10, color=col, fontweight='bold')
    ax.text(0.02, 0.03, '트랙 = ID + 상태 (위치 · 속도) + 믿을 만한 정도 (TrackState)', transform=ax.transAxes, fontsize=8.2, color=SUB)
    fig.tight_layout(w_pad=1.5)
    save(fig, 'fig_plot_track.png')


# ---------------------------------------------------------------- 1.2 1-point vs 2-point
def fig_1p2p():
    fig, axs = plt.subplots(1, 2, figsize=(10.4, 3.9))
    for ax in axs:
        ax.set_xlim(-0.6, 11.0); ax.set_ylim(-0.9, 5.9); ax.set_aspect('equal'); ax.axis('off')
    # 1-point
    ax = axs[0]
    ax.set_title('1-Point: 플롯 1개로 개시', fontsize=12.5, color=NAVY, loc='left', fontweight='bold')
    p = np.array([2.3, 2.4])
    ax.add_patch(Circle(p, 2.6, fc=F_ORANGE, ec=ORANGE, lw=1.4, ls='--', zorder=1))
    for a in np.linspace(0, 2 * np.pi, 12, endpoint=False):
        ax.annotate('', xy=(p[0] + 2.1 * np.cos(a), p[1] + 2.1 * np.sin(a)), xytext=p, arrowprops=dict(arrowstyle='->', color='#E8B26A', lw=1.0), zorder=2)
    ax.scatter(*p, s=70, color=NAVY, zorder=4)
    ax.text(p[0] - 0.2, p[1] - 0.75, 'z$_k$', fontsize=12, color=NAVY)
    ax.text(5.9, 4.6, '위치 = z$_k$', fontsize=11.5, color=NAVY)
    ax.text(5.9, 3.8, '속도 = 0 (모름)', fontsize=11.5, color=ORANGE, fontweight='bold')
    ax.text(5.9, 2.9, '→ 속도 오차가 표적 최대 속도만큼 큼', fontsize=10.5, color=SUB)
    ax.text(5.9, 2.2, '→ 다음 스캔에 넓은 범위를 뒤져야 함', fontsize=10.5, color=SUB)
    ax.text(5.9, 1.2, '장점: 플롯 하나면 바로 개시 (빠름)', fontsize=10.5, color=GREEN)
    # 2-point
    ax = axs[1]
    ax.set_title('2-Point: 연속 두 스캔 플롯으로 개시', fontsize=12.5, color=NAVY, loc='left', fontweight='bold')
    p0 = np.array([0.4, 0.6]); p1 = np.array([2.0, 1.9])
    ax.scatter(*p0, s=70, color=NAVY, alpha=0.4, zorder=4)
    ax.scatter(*p1, s=70, color=NAVY, zorder=4)
    ax.text(p0[0] - 0.3, p0[1] - 0.8, 'z$_{k-1}$', fontsize=12, color=SUB)
    ax.text(p1[0] - 0.1, p1[1] - 0.85, 'z$_k$', fontsize=12, color=NAVY)
    ax.annotate('', xy=p1, xytext=p0, arrowprops=dict(arrowstyle='->', color=BLUE, lw=2.0), zorder=3)
    nxt = p1 + (p1 - p0)
    ax.add_patch(Ellipse(nxt, 1.6, 1.1, angle=36, fc=F_BLUE, ec=BLUE, lw=1.3, ls='--', zorder=1))
    ax.text(nxt[0] - 1.2, nxt[1] + 0.75, '다음 스캔 예상 위치', fontsize=9.5, color=BLUE)
    ax.text(5.9, 4.6, '위치 = z$_k$', fontsize=11.5, color=NAVY)
    ax.text(5.9, 3.8, '속도 = (z$_k$ − z$_{k-1}$) / T', fontsize=11.5, color=BLUE, fontweight='bold')
    ax.text(5.9, 2.9, '→ 1-Point 보다 속도 오차가 작음', fontsize=10.5, color=SUB)
    ax.text(5.9, 2.2, '→ 대신 두 번째 스캔까지 기다림', fontsize=10.5, color=SUB)
    ax.text(5.9, 1.2, '이번 과제에서 구현한 방식', fontsize=10.5, color=GREEN, fontweight='bold')
    fig.tight_layout(w_pad=3)
    save(fig, 'fig_1p2p.png')


# ---------------------------------------------------------------- 1.3 P matrix
def fig_pmatrix():
    labels = ['x', "x'", "x''", 'y', "y'", "y''", 'z', "z'", "z''"]
    fig, ax = plt.subplots(figsize=(5.0, 4.7))
    ax.set_xlim(0, 9); ax.set_ylim(9, 0); ax.set_aspect('equal')
    for i in range(9):
        for j in range(9):
            ki, kj = i % 3, j % 3
            if ki == 2 or kj == 2:
                fc, txt, tc = '#F1F3F6', '0', '#B8C0CC'
            elif ki == 0 and kj == 0:
                fc, txt, tc = F_BLUE, 'R$_k$', BLUE
            elif ki == 1 and kj == 1:
                fc, txt, tc = F_RED, '(R$_k$+R$_{k-1}$)\n/T²', RED
            else:
                fc, txt, tc = F_GREEN, 'R$_k$/T', GREEN
            same = (i // 3) == (j // 3)
            ax.add_patch(Rectangle((j, i), 1, 1, fc=fc, ec='white', lw=1.5))
            ax.text(j + 0.5, i + 0.5, txt, ha='center', va='center', fontsize=7.6 if '\n' in txt else 9, color=tc, alpha=1.0 if same else 0.75)
    for b in (3, 6):
        ax.plot([b, b], [0, 9], color=NAVY, lw=1.2); ax.plot([0, 9], [b, b], color=NAVY, lw=1.2)
    ax.set_xticks(np.arange(9) + 0.5); ax.set_xticklabels(labels, fontsize=10, color=NAVY)
    ax.set_yticks(np.arange(9) + 0.5); ax.set_yticklabels(labels, fontsize=10, color=NAVY)
    ax.xaxis.tick_top(); ax.tick_params(length=0)
    for s in ax.spines.values(): s.set_visible(False)
    fig.tight_layout()
    save(fig, 'fig_pmatrix.png')


# ---------------------------------------------------------------- 1.4 R ellipse
def fig_rcov():
    r = 14148.0
    sr = 10.0; sa = math.radians(0.2); spl = math.radians(0.478)
    fig, axs = plt.subplots(1, 2, figsize=(7.6, 2.85), gridspec_kw=dict(width_ratios=[1.05, 1]))
    ax = axs[0]
    ax.set_aspect('equal'); ax.axis('off')
    ax.set_xlim(-2.0, 12.5); ax.set_ylim(-1.3, 6.1)
    ax.plot([0, 9.0], [0, 4.2], color=NAVY, lw=1.2)
    ax.plot([0], [0], marker='^', ms=11, color=NAVY)
    ax.text(0.0, -0.95, '레이다', fontsize=10, color=NAVY, ha='center')
    c = np.array([9.0, 4.2]); ang = math.degrees(math.atan2(4.2, 9.0))
    ax.add_patch(Ellipse(c, 0.55, 2.6, angle=ang, fc=F_BLUE, ec=BLUE, lw=1.6))
    ax.scatter(*c, s=30, color=NAVY, zorder=5)
    ax.annotate('', xy=(c[0] + 0.55 * math.cos(math.radians(ang)), c[1] + 0.55 * math.sin(math.radians(ang))), xytext=(c[0] - 0.55 * math.cos(math.radians(ang)), c[1] - 0.55 * math.sin(math.radians(ang))),
                arrowprops=dict(arrowstyle='<->', color=SUB, lw=1.0))
    ax.annotate('거리 방향  σr = 10 m', xy=(c[0] + 0.3, c[1] + 0.05), xytext=(c[0] - 0.4, c[1] - 2.2), fontsize=10, color=SUB,
                arrowprops=dict(arrowstyle='-', color=SUB, lw=0.8))
    ax.annotate('방위각 · 고각 방향\nr · σθ = 14 km × 0.2° ≈ 49 m', xy=(c[0] - 0.45, c[1] + 1.05), xytext=(c[0] - 2.3, c[1] + 1.25), fontsize=9.5, color=BLUE, fontweight='bold',
                va='center', ha='right', arrowprops=dict(arrowstyle='-', color=BLUE, lw=0.8))
    ax.text(2.6, 2.2, 'r ≈ 14 km', fontsize=10, color=NAVY, rotation=ang)
    ax.set_title('R 은 레이다 시선을 따라 납작한 타원', fontsize=12.5, color=NAVY, loc='left', fontweight='bold')
    ax = axs[1]
    vals = [sr, r * sa]
    names = ['거리 방향\nσr', '방위각 · 고각 방향\nr · σθ (0.2°)']
    cols = [MUTED, BLUE]
    ax.barh(range(2), vals, color=cols, height=0.5)
    for i, v in enumerate(vals):
        ax.text(v + 1.5, i, f'{v:.0f} m', va='center', fontsize=11, color=NAVY, fontweight='bold')
    ax.set_yticks(range(2)); ax.set_yticklabels(names, fontsize=10, color=NAVY)
    ax.set_ylim(1.6, -0.6); ax.set_xlim(0, 70)
    ax.set_xlabel('위치 표준편차 [m]  (표적 1, 거리 14.1 km)', fontsize=10, color=SUB)
    clean(ax)
    ax.set_title('같은 R 이라도 방향마다 크기가 다름', fontsize=12.5, color=NAVY, loc='left', fontweight='bold')
    fig.tight_layout(w_pad=2)
    save(fig, 'fig_rcov.png')


# ---------------------------------------------------------------- 1.5 velocity gate
def fig_gate():
    fig, ax = plt.subplots(figsize=(5.4, 3.75))
    fig.subplots_adjust(0, 0, 1, 1)
    ax.set_aspect('equal'); ax.axis('off')
    ax.set_xlim(-1.0, 12.6); ax.set_ylim(-0.9, 7.9)
    prev = {'A': (2.0, 4.6), 'B': (7.6, 5.2), 'C': (5.0, 1.4)}
    cur = {'a': (3.1, 5.3), 'b': (10.6, 5.6), 'c': (5.6, 2.3), 'd': (1.0, 1.0)}
    for k, (x, y) in prev.items():
        ax.add_patch(Circle((x, y), 1.6, fc=F_BLUE, ec=BLUE, lw=1.2, ls='--', alpha=0.9))
        ax.scatter(x, y, s=70, facecolors='white', edgecolors=NAVY, lw=1.6, zorder=4)
        ax.text(x - 0.55, y + 0.35, k, fontsize=11, color=NAVY, fontweight='bold')
    for k, (x, y) in cur.items():
        ax.scatter(x, y, s=70, color=NAVY, zorder=4)
        ax.text(x + 0.25, y - 0.55, k, fontsize=11, color=NAVY)
    for (p, c) in (('A', 'a'), ('C', 'c')):
        ax.plot(*zip(prev[p], cur[c]), color=GREEN, lw=2.2, zorder=3)
    ax.plot(*zip(prev['B'], cur['b']), color=RED, lw=1.6, ls='--', zorder=3)
    ax.text(9.6, 6.45, '80 m 밖 → 탈락', fontsize=10, color=RED)
    ax.text(-0.9, 7.45, '점선 원 반경 = V$_{max}$ · T = 800 m/s × 0.1 s = 80 m', fontsize=10.5, color=BLUE)
    ax.text(-0.9, -0.6, '○ 앞 스캔 플롯 (k−1)    ● 이번 스캔 플롯 (k)    ━ 개시    ┅ 탈락', fontsize=9.5, color=SUB)
    save(fig, 'fig_gate.png')


# ---------------------------------------------------------------- 2.1 states
def fig_states():
    fig, ax = plt.subplots(figsize=(8.0, 2.75))
    fig.subplots_adjust(0, 0, 1, 1)
    ax.set_xlim(0, 10.4); ax.set_ylim(0, 3.4); ax.axis('off')
    W, H = 2.1, 1.0
    boxes = {1: (1.0, 2.0, '1  임시 트랙', 'Tentative', ORANGE, F_ORANGE),
             2: (4.15, 2.0, '2  확정 트랙', 'Confirmed', GREEN, F_GREEN),
             3: (7.3, 2.0, '3  메모리 트랙', 'Memory', PURPLE, F_PURPLE),
             4: (4.15, 0.2, '4  삭제 트랙', 'Deleted', GRAY, F_GRAY)}
    for k, (x, y, t, e, ec, fc) in boxes.items():
        ax.add_patch(FancyBboxPatch((x, y), W, H, boxstyle='round,pad=0.02,rounding_size=0.12', fc=fc, ec=ec, lw=1.6))
        ax.text(x + W / 2, y + H * 0.62, t, ha='center', va='center', fontsize=12, color=NAVY, fontweight='bold')
        ax.text(x + W / 2, y + H * 0.27, e, ha='center', va='center', fontsize=9.5, color=SUB)
    def arr(p0, p1, col, rad=0.0):
        ax.add_patch(FancyArrowPatch(p0, p1, arrowstyle='-|>', mutation_scale=14, color=col, lw=1.5, connectionstyle=f'arc3,rad={rad}'))
    arr((0.05, 2.5), (0.98, 2.5), NAVY); ax.text(0.05, 2.75, '개시', fontsize=10, color=NAVY)
    arr((3.12, 2.62), (4.13, 2.62), GREEN); ax.text(3.62, 2.85, '확정 조건', fontsize=9.5, color=GREEN, ha='center')
    arr((6.27, 2.68), (7.28, 2.68), PURPLE); ax.text(6.77, 2.9, '연관 실패', fontsize=9.5, color=PURPLE, ha='center')
    arr((7.28, 2.3), (6.27, 2.3), GREEN); ax.text(6.77, 2.02, '연관 성공', fontsize=9.5, color=GREEN, ha='center')
    arr((2.0, 1.98), (4.13, 0.75), GRAY, rad=0.15); ax.text(2.35, 1.05, '삭제 조건\n(임시)', fontsize=9.5, color=GRAY, ha='center')
    arr((8.35, 1.98), (6.27, 0.75), GRAY, rad=-0.15); ax.text(8.1, 1.05, '삭제 조건\n(메모리)', fontsize=9.5, color=GRAY, ha='center')
    return save(fig, 'fig_states.png')


# ---------------------------------------------------------------- 2.2 M/N
def fig_mn():
    fig, ax = plt.subplots(figsize=(10.4, 2.6))
    ax.set_xlim(-0.2, 10.6); ax.set_ylim(-0.5, 2.6); ax.axis('off')
    hist = [1, 0, 1, 1, 0, 1, 1]
    for i, h in enumerate(hist):
        x = 0.3 + i * 1.05
        ax.add_patch(FancyBboxPatch((x, 0.9), 0.8, 0.8, boxstyle='round,pad=0.02,rounding_size=0.1', fc=F_GREEN if h else F_RED, ec=GREEN if h else RED, lw=1.4))
        ax.text(x + 0.4, 1.3, '성공' if h else '실패', ha='center', va='center', fontsize=10.5, color=GREEN if h else RED, fontweight='bold')
        ax.text(x + 0.4, 0.55, f'스캔 {i + 1}', ha='center', fontsize=9, color=SUB)
    ax.add_patch(Rectangle((0.3 + 2 * 1.05 - 0.1, 0.78), 5 * 1.05 - 0.05, 1.04, fill=False, ec=BLUE, lw=1.8, ls='--'))
    ax.text(0.3 + 2 * 1.05 - 0.1, 2.05, '최근 N = 5 스캔 창', fontsize=10, color=BLUE)
    ax.text(7.9, 1.55, '창 안 성공 4 번 ≥ M = 3', fontsize=11, color=NAVY, fontweight='bold')
    ax.text(7.9, 1.0, '→ 확정 트랙', fontsize=11, color=GREEN, fontweight='bold')
    ax.text(0.3, -0.25, '예: N = 5, M = 3 (값은 설명용). 연속 실패가 M 번이면 임시 트랙 삭제', fontsize=9.5, color=SUB)
    save(fig, 'fig_mn.png')


# ---------------------------------------------------------------- 2.3 score timeline
def fig_score():
    flags = [None, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0]
    st, sc, mem = 1, 2, 0
    S, C, M = [st], [sc], [mem]
    for f in flags[1:]:
        if st == 1:
            sc += 1 if f else -1
            st = 2 if sc >= 5 else (4 if sc <= 0 else 1)
        elif st == 2:
            if not f: st, mem = 3, mem + 1
        elif st == 3:
            if f: st, mem = 2, 0
            else:
                mem += 1
                if mem >= 5: st = 4
        S.append(st); C.append(sc); M.append(mem)
    x = np.arange(len(flags))
    fig, ax = plt.subplots(figsize=(7.75, 2.6))
    colmap = {1: F_ORANGE, 2: F_GREEN, 3: F_PURPLE, 4: '#EEF0F3'}
    namemap = {1: '임시', 2: '확정', 3: '메모리', 4: '삭제'}
    for i in x:
        ax.axvspan(i - 0.5, i + 0.5, color=colmap[S[i]], lw=0)
        ax.text(i, 6.55, namemap[S[i]], ha='center', fontsize=9.5, color=NAVY)
        ax.text(i, -1.35, '-' if flags[i] is None else str(flags[i]), ha='center', fontsize=10.5, color=GREEN if flags[i] == 1 else (RED if flags[i] == 0 else SUB), fontweight='bold')
    ax.step(x, C, where='mid', color=ORANGE, lw=2.2, label='TrackScore')
    ax.step(x, M, where='mid', color=PURPLE, lw=2.2, ls='--', label='MemoryScore')
    ax.axhline(5, color=GREEN, lw=1.0, ls=':')
    ax.text(-0.4, 5.15, '확정 문턱 5 (TrackScore) · 메모리 삭제 문턱 5 (MemoryScore)', fontsize=9, color=GREEN, ha='left')
    ax.axhline(0, color=RED, lw=1.0, ls=':')
    ax.text(11.6, 0.12, '임시 삭제 문턱 0', fontsize=9, color=RED, ha='right')
    ax.set_xlim(-0.5, len(flags) - 0.5); ax.set_ylim(-1.8, 7.0)
    ax.set_xticks(x); ax.set_xticklabels([str(i) for i in x], fontsize=9, color=SUB)
    ax.set_yticks([0, 1, 2, 3, 4, 5]); ax.tick_params(colors=SUB, labelsize=9)
    ax.text(-0.45, -1.35, 'Flag', fontsize=9, color=SUB, ha='right')
    ax.set_xlabel('개시 후 스캔', fontsize=9.5, color=SUB)
    for s in ax.spines.values(): s.set_visible(False)
    ax.legend(loc='upper left', bbox_to_anchor=(0.31, 0.80), fontsize=9.5, frameon=False)
    fig.tight_layout()
    save(fig, 'fig_score.png')


# ---------------------------------------------------------------- 4.2 zoom near targets at scan 701
def fig_zoom(scan=701):
    fig, axs = plt.subplots(1, 2, figsize=(8.2, 3.55))
    T = tk[tk.scan == scan]
    for ax, k in zip(axs, (1, 2)):
        t = tr[(tr.obj == k) & (tr.step <= scan)]
        cur = tr[(tr.obj == k) & (tr.step == scan)].iloc[0]
        ce, cn = cur.east, cur.north
        ax.plot(t.east - ce, t.north - cn, color=TGT[k], lw=1.6, zorder=2)
        hist = pl[(pl.source == k) & (pl.scan <= scan) & (pl.scan > scan - 40)]
        ax.scatter(hist.east - ce, hist.north - cn, s=8, color=NAVY, alpha=0.25, lw=0, zorder=3)
        now = pl[(pl.source == k) & (pl.scan == scan)]
        prev = pl[(pl.source == k) & (pl.scan == scan - 1)]
        ax.scatter(prev.east - ce, prev.north - cn, s=46, facecolors='white', edgecolors=NAVY, lw=1.4, zorder=5)
        ax.scatter(now.east - ce, now.north - cn, s=46, color=NAVY, zorder=5)
        ax.plot([0], [0], marker='o', ms=8, mfc='white', mec=TGT[k], mew=2, zorder=6)
        for _, r in T[T.src_cur == k].iterrows():
            x, y = r.east - ce, r.north - cn
            track_square(ax, x, y, int(r.state), size=12, z=7)
            if r.state == 1:
                ax.annotate('', xy=(x + r.ve * 0.25, y + r.vn * 0.25), xytext=(x, y), arrowprops=dict(arrowstyle='->', color=ORANGE, lw=1.6), zorder=7)
                ax.add_patch(Ellipse((x, y), 2 * r.pse, 2 * r.psn, fc='none', ec=ORANGE, lw=0.9, ls=':', zorder=6))
            ax.text(x + 8, y + (12 if r.state == 1 else -26), str(int(r.track_id)) + (' 임시' if r.state == 1 else ' 삭제'), fontsize=9.5, color=ORANGE if r.state == 1 else GRAY, fontweight='bold')
        ax.set_aspect('equal')
        lim = 170
        ax.set_xlim(-lim, lim); ax.set_ylim(-lim * 0.62, lim * 0.62)
        ax.grid(color=GRID, lw=0.8); clean(ax)
        ax.set_xlabel('동 [m] (지금 참값 기준)', fontsize=9.5, color=SUB); ax.set_ylabel('북 [m]', fontsize=9.5, color=SUB)
        ax.set_title(f'표적 {k}  ({"대함 30 m/s" if k == 1 else "대공 200 m/s"})', fontsize=12, color=TGT[k], loc='left', fontweight='bold')
    from matplotlib.lines import Line2D
    hs = [Line2D([], [], color=NAVY, lw=1.6, label='참값 궤적'), Line2D([], [], marker='o', ls='none', mfc='white', mec=NAVY, label='지금 참값 위치'),
          Line2D([], [], marker='o', ls='none', mfc='white', mec=NAVY, mew=1.4, ms=7, label='앞 스캔 플롯'), Line2D([], [], marker='o', ls='none', color=NAVY, ms=7, label='이번 스캔 플롯'),
          Line2D([], [], marker='s', ls='none', mfc='none', mec=ORANGE, mew=1.6, ms=9, label='임시 트랙 (화살표: 속도 0.25 s)'), Line2D([], [], marker='s', ls='none', mfc='none', mec=GRAY, mew=1.6, ms=9, label='삭제 트랙'),
          Line2D([], [], color=ORANGE, ls=':', label='위치 1σ (mat_EstP)')]
    fig.legend(handles=hs, loc='lower center', ncol=4, fontsize=9, frameon=False, bbox_to_anchor=(0.5, -0.01))
    fig.tight_layout(w_pad=2, rect=(0, 0.1, 1, 1))
    save(fig, 'fig_zoom.png')


# ---------------------------------------------------------------- 4.3 track lifetime
def fig_life():
    fig, axs = plt.subplots(2, 1, figsize=(7.4, 2.85), sharex=True, gridspec_kw=dict(height_ratios=[1.2, 1]))
    made = tk[tk.scan == tk.init_scan].copy()
    made['src'] = np.where(made.src_prev == made.src_cur, made.src_cur, -1)
    ax = axs[0]
    for k, y, col, nm in ((1, 2, BLUE, '표적 1'), (2, 1, RED, '표적 2'), (0, 0, GRAY, '클러터끼리')):
        m = made[made.src == k]
        ax.vlines(m.time, y - 0.35, y + 0.35, color=col, lw=1.0)
        ax.text(116.8, y, f'{nm}  {len(m)}개', va='center', fontsize=10, color=col, fontweight='bold')
    ax.set_yticks([]); ax.set_ylim(-0.6, 2.6)
    for s in ax.spines.values(): s.set_visible(False)
    ax.set_title('트랙이 만들어진 시각 (세로선 하나 = 트랙 하나)', fontsize=11.5, color=NAVY, loc='left', fontweight='bold')
    ax = axs[1]
    ax.step(sl.time, sl.tracks, where='post', color=NAVY, lw=1.0)
    ax.set_ylabel('표의 트랙 수', fontsize=9.5, color=SUB)
    ax.set_xlabel('시간 [s]', fontsize=9.5, color=SUB)
    ax.set_yticks([0, 2, 4]); clean(ax)
    ax.set_xlim(0, 116)
    fig.tight_layout()
    save(fig, 'fig_life.png')


def crop_ui():
    # 4.2 · 4.3 · 4.5 슬라이드용 확대 캡처 (capture_agent 로 찍은 큰 창 캡처를 자름)
    from PIL import Image
    a = os.path.join(OUT, 'ui_big_s701_2d.png'); b = os.path.join(OUT, 'ui_big_s701_3d.png')
    if os.path.exists(a) and os.path.exists(b):
        im = Image.open(a)
        im.crop((690, 140, 1890, 905)).save(os.path.join(OUT, 'ui_crop_2d.png'))
        im.crop((640, 955, 1625, 1062)).save(os.path.join(OUT, 'ui_crop_table.png'))
        Image.open(b).crop((645, 140, 1845, 900)).save(os.path.join(OUT, 'ui_crop_3d.png'))
        print('saved ui_crop_*.png')
    # 4.5 전체 궤적: 마지막 스캔 (1159) 큰 창 캡처
    a = os.path.join(OUT, 'ui_big_s1159_2d.png'); b = os.path.join(OUT, 'ui_big_s1159_3d.png')
    if os.path.exists(a) and os.path.exists(b):
        Image.open(a).crop((690, 140, 1890, 905)).save(os.path.join(OUT, 'ui_crop_full_2d.png'))
        Image.open(b).crop((645, 140, 1845, 900)).save(os.path.join(OUT, 'ui_crop_full_3d.png'))
        print('saved ui_crop_full_*.png')


# ---------------------------------------------------------------- 2.1 track association (Assigned_Flag)
def fig_assoc():
    fig, axs = plt.subplots(1, 2, figsize=(3.6, 1.6))
    fig.patch.set_facecolor(F_GRAY)
    fig.subplots_adjust(0.01, 0.02, 0.99, 0.84, wspace=0.08)
    for ax, flag in zip(axs, (1, 0)):
        ax.set_xlim(-0.5, 3.6); ax.set_ylim(-1.0, 1.7); ax.set_aspect('equal'); ax.axis('off')
        trk = np.array([0.0, 0.0]); pred = np.array([2.0, 0.55])
        track_square(ax, *trk, 1, size=8, lw=1.4)
        ax.annotate('', xy=pred, xytext=trk, arrowprops=dict(arrowstyle='->', color=ORANGE, lw=1.1, ls='--'))
        ax.add_patch(Circle(pred, 0.75, fc=F_BLUE, ec=BLUE, lw=1.0, ls='--', zorder=1))
        ax.plot(*pred, marker='+', ms=8, mew=1.4, color=BLUE, zorder=4)
        if flag:
            pl_ = np.array([2.35, 0.25])
            ax.plot(*zip(pred, pl_), color=GREEN, lw=1.4, zorder=3)
            ax.scatter(*pl_, s=22, color=NAVY, zorder=5)
            ax.text(1.55, -0.75, '게이트 안 플롯 할당', fontsize=7.5, color=GREEN, ha='center')
        else:
            ax.scatter(3.25, 1.45, s=22, color=GRAY, zorder=5)
            ax.text(1.55, -0.75, '게이트 안 플롯 없음', fontsize=7.5, color=RED, ha='center')
        ax.set_title(f'Assigned_Flag = {flag}', fontsize=9, color=GREEN if flag else RED, fontweight='bold', pad=2)
    save(fig, 'fig_assoc.png', dpi=300)


if __name__ == '__main__':
    crop_ui()
    cover('cover_bg.png', True)
    cover('end_bg.png', False)
    fig_plot_track(); fig_1p2p(); fig_pmatrix(); fig_rcov(); fig_gate(); fig_states(); fig_assoc(); fig_mn(); fig_score(); fig_zoom(); fig_life()
