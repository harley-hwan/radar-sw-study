# 과제 6 발표 그림 (시뮬레이션 결과 data/ 의 CSV 사용. 개념 그림 몇 장은 식으로 그림)
# python make_figures.py <data 폴더> <그림 폴더>
# 색: 과제 값 = 주황, 조정 값 = 파랑 (모든 비교 그림에서 같음), 참값 = 남색 선, 플롯 = 회색 점
import sys, os, math
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
from matplotlib.patches import Circle, Ellipse, FancyArrowPatch, Rectangle
from matplotlib.lines import Line2D
import mpmath

DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join('..', 'data')
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join('..', 'figures')
os.makedirs(OUT, exist_ok=True)

for name in ['Malgun Gothic', 'Noto Sans CJK KR', 'Noto Sans KR']:
    if any(f.name == name for f in font_manager.fontManager.ttflist):
        plt.rcParams['font.family'] = name
        break

NAVY = '#1B2740'; SUB = '#4A5A7A'; MUTED = '#8794AC'; LIGHT = '#AEB9CC'; GRID = '#E6EAF0'; BORDER = '#D2D9E5'
BLUE = '#2563EB'; RED = '#DC2626'; ORANGE = '#D97706'; GREEN = '#059669'; PURPLE = '#7C3AED'; GRAY = '#9CA3AF'
F_BLUE = '#EBF2FE'; F_RED = '#FDEEEE'; F_ORANGE = '#FEF5E7'; F_GRAY = '#F7F9FC'
TURN_BAND = '#E9ECF1'                               # TargetSimUI 추적 성능 보기의 선회 띠와 같은 색
DIVERGE_BAND = '#FEE2E2'                            # 발산 판정 띠
GIVEN = ORANGE; TUNED = BLUE; TRUTH_C = NAVY; PLOT_C = GRAY
GATE = 11.34                                        # TrackMgr.h TRK_GATE_CHI2
DT = 0.1

plt.rcParams.update({
    'axes.unicode_minus': False, 'mathtext.fontset': 'dejavusans', 'font.size': 9.5,
    'axes.edgecolor': LIGHT, 'axes.linewidth': 0.8, 'axes.labelcolor': SUB, 'axes.labelsize': 9.5,
    'axes.titlesize': 10.5, 'axes.titlecolor': NAVY, 'axes.titleweight': 'bold', 'axes.titlelocation': 'left',
    'xtick.color': SUB, 'ytick.color': SUB, 'xtick.labelsize': 8.5, 'ytick.labelsize': 8.5,
    'axes.grid': True, 'grid.color': GRID, 'grid.linewidth': 0.8, 'axes.axisbelow': True,
    'legend.frameon': False, 'legend.fontsize': 8.8, 'savefig.facecolor': 'white',
})


def run_csv(run, name):
    return pd.read_csv(os.path.join(DATA, run, name + '.csv'))


SCENARIOS = ('ship', 'air')
SC_NAME = {'ship': '대함', 'air': '대공'}
TRUTH = {sc: run_csv(sc + '_given', 'truth') for sc in SCENARIOS}
PLOTS = {sc: run_csv(sc + '_given', 'plots') for sc in SCENARIOS}      # 플롯은 필터 파라미터와 상관없음 (과제 값 · 조정 값 실행이 같은 플롯)
TRACKS = {run: run_csv(run, 'tracks') for run in ('ship_given', 'ship_tuned', 'air_given', 'air_tuned')}
RUNS = pd.read_csv(os.path.join(DATA, 'runs.csv')).set_index('run')
SWEEP = pd.read_csv(os.path.join(DATA, 'sweep.csv'))
TUNED_Q = {sc: float(RUNS.loc[sc + '_tuned', 'q']) for sc in SCENARIOS}
TUNED_SIG = {sc: float(RUNS.loc[sc + '_tuned', 'sigma_az_deg']) for sc in SCENARIOS}
GIVEN_SIG = float(RUNS.loc['ship_given', 'sigma_az_deg'])
GIVEN_Q = float(RUNS.loc['ship_given', 'q'])


def save(fig, name, dpi=200):
    fig.savefig(os.path.join(OUT, name), dpi=dpi, facecolor=fig.get_facecolor())
    plt.close(fig)
    print('saved', name)


def clean(ax, grid=True):
    for s in ax.spines.values():
        s.set_color(LIGHT)
    ax.grid(grid)


def turn_spans(sc):
    # truth.csv 의 turn 열 (표본 시각이 선회 구간 안이면 1) -> [(시작, 끝)]
    tr = TRUTH[sc]
    on = tr.turn.values != 0
    t = tr.time.values
    spans, i = [], 0
    while i < len(on):
        if on[i]:
            j = i
            while j + 1 < len(on) and on[j + 1]:
                j += 1
            spans.append((t[i], t[j] + DT))
            i = j + 1
        else:
            i += 1
    return spans


def turn_bands(ax, sc, t0=None, t1=None):
    for a, b in turn_spans(sc):
        if (t1 is None or a < t1) and (t0 is None or b > t0):
            ax.axvspan(a, b, color=TURN_BAND, lw=0, zorder=0)


def diverge_spans(tk):
    # 게이트 밖이 5 번 이상 이어진 구간 (TargetSimUI 의 빨간 띠와 같은 규칙)
    on = tk.gate_run.values >= 5
    t = tk.time.values
    spans, i = [], 0
    while i < len(on):
        if on[i]:
            j = i
            while j + 1 < len(on) and on[j + 1]:
                j += 1
            spans.append((t[i] - 0.5 * DT, t[j] + 0.5 * DT))
            i = j + 1
        else:
            i += 1
    return spans


def truth_acc(sc):
    # 참값 가속도 크기: 0.1 s 위치의 2 차 차분 [m/s^2]
    p = TRUTH[sc][['north', 'east', 'down']].values
    a = np.full(len(p), np.nan)
    a[1:-1] = np.linalg.norm(p[2:] - 2.0 * p[1:-1] + p[:-2], axis=1) / (DT * DT)
    return TRUTH[sc].time.values, a


def est_acc(run):
    tk = TRACKS[run]
    return tk.time.values, np.sqrt(tk.an ** 2 + tk.ae ** 2 + tk.ad ** 2).values


def sph(n, e, d):
    # VC-NED -> 거리, 방위각 [deg], 고각 [deg] (플랫폼이 북쪽을 보고 정지해 있어 안테나 각과 같음)
    h = np.hypot(n, e)
    return np.sqrt(n * n + e * e + d * d), np.degrees(np.arctan2(e, n)), np.degrees(np.arctan2(-d, h))


def scale_bar(ax, length, label, x=0.96, y=0.06, left=False):
    # 축 비율이 같은 평면 그림의 축척 막대. x 는 막대 오른쪽 끝 (left=True 면 왼쪽 끝) 의 축 비율 위치
    x0, x1 = ax.get_xlim()
    y0, y1 = ax.get_ylim()
    xa = x0 + x * (x1 - x0)
    xb = xa + length if left else xa - length
    yb = y0 + y * (y1 - y0)
    ax.plot([xa, xb], [yb, yb], color=SUB, lw=1.6, solid_capstyle='butt', zorder=9)
    ax.text(0.5 * (xa + xb), yb + 0.025 * (y1 - y0), label, ha='center', va='bottom', fontsize=8.5, color=SUB, zorder=9)


# ---------------------------------------------------------------- 표지 / 끝 배경
def cover(name, emphasis=True):
    W, H = 2933, 1650
    bg = '#1B2740'
    fig = plt.figure(figsize=(W / 200, H / 200), dpi=200)
    fig.patch.set_facecolor(bg)
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_facecolor(bg)
    ax.set_xlim(-7.6, 25.3)
    ax.set_ylim(-3.33, 15.17)
    ax.set_aspect('equal')
    ax.axis('off')
    for gx in np.arange(-10, 26, 0.66):
        ax.axvline(gx, color='#26355A', lw=0.6, zorder=0)
    for gy in np.arange(-4, 16, 0.66):
        ax.axhline(gy, color='#26355A', lw=0.6, zorder=0)
    for r in (5, 10, 15):
        ax.add_patch(Circle((0, 0), r, fill=False, ec='#3A4C78', lw=1.0, zorder=1))
    for a in (30, 150):
        ax.plot([0, 16 * math.cos(math.radians(a))], [0, 16 * math.sin(math.radians(a))], color='#3A4C78', lw=1.0, ls=(0, (4, 4)), zorder=1)
    for sc, col in (('ship', '#60A5FA'), ('air', '#F87171')):
        p = PLOTS[sc]
        ax.scatter(p.east / 1000, p.north / 1000, s=2.0, c='#C9D3E6', alpha=0.45, lw=0, zorder=2)
        t = TRUTH[sc]
        ax.plot(t.east / 1000, t.north / 1000, color=col, lw=2.0, alpha=0.95, zorder=3)
        if emphasis:
            tk = TRACKS[sc + '_tuned']
            step = 40 if sc == 'air' else 150                     # 기호 간격 약 0.8 km / 0.45 km
            for _, r in tk.iloc[::step].iterrows():
                ax.plot(r.east / 1000, r.north / 1000, marker='s', ms=5.0, mfc='none', mec='#F59E0B', mew=1.1, zorder=5)
    ax.plot([0], [0], marker='^', ms=12, color='#CBD5E1', zorder=6)
    save(fig, name)


# ---------------------------------------------------------------- 1.1 추적 필터가 하는 일 (대공 조정 값, 두 번째 선회)
def fig_role():
    t0, t1 = 22.0, 34.0
    tr = TRUTH['air']; pl = PLOTS['air']; tk = TRACKS['air_tuned']
    tw = tr[(tr.time >= t0) & (tr.time <= t1)]
    pw = pl[(pl.time >= t0) & (pl.time <= t1)]
    kw = tk[(tk.time >= t0) & (tk.time <= t1)]
    pad = 130.0
    xlim = (pw.east.min() - pad, pw.east.max() + pad)
    ylim = (pw.north.min() - pad, pw.north.max() + pad)
    ph = 2.45                                                                   # 칸 높이 [in], 폭은 자료 비율대로
    pwid = ph * (xlim[1] - xlim[0]) / (ylim[1] - ylim[0])
    gap, side, top, bottom = 0.3, 0.04, 0.34, 0.5
    W, H = 2 * pwid + gap + 2 * side, ph + top + bottom
    fig = plt.figure(figsize=(W, H))
    axs = [fig.add_axes([(side + k * (pwid + gap)) / W, bottom / H, pwid / W, ph / H]) for k in range(2)]
    for ax, title in zip(axs, ['플롯 (레이다가 주는 것)', '트랙 (추적 필터 결과)']):
        ax.set_xlim(*xlim); ax.set_ylim(*ylim); ax.set_aspect('equal')
        ax.set_xticks([]); ax.set_yticks([])
        clean(ax, grid=False)
        ax.set_title(title, fontsize=10, pad=5)
        ax.plot(tw.east, tw.north, color=TRUTH_C, lw=1.0, alpha=0.55, zorder=2)
        scale_bar(ax, 500, '500 m', x=0.06, y=0.06, left=True)
    axs[0].scatter(pw.east, pw.north, s=8, color=SUB, lw=0, zorder=3)
    axs[1].scatter(pw.east, pw.north, s=6, color=PLOT_C, alpha=0.55, lw=0, zorder=1)
    axs[1].plot(kw.east, kw.north, color=TUNED, lw=1.5, zorder=4)
    axs[1].plot(kw.east.iloc[::5], kw.north.iloc[::5], ls='none', marker='s', ms=3.6, mfc='white', mec=TUNED, mew=1.0, zorder=5)
    handles = [Line2D([], [], color=TRUTH_C, lw=1.0, alpha=0.55, label='참값'),
               Line2D([], [], ls='none', marker='o', ms=4, color=SUB, label='플롯'),
               Line2D([], [], color=TUNED, lw=1.5, marker='s', ms=3.6, mfc='white', mec=TUNED, label='트랙')]
    fig.legend(handles=handles, loc='lower center', ncol=3, fontsize=9, bbox_to_anchor=(0.5, 0.0), columnspacing=1.4)
    save(fig, 'fig_role.png')
    return W, H


# ---------------------------------------------------------------- 1.2 칼만 필터 한 주기 (개념 그림, 식으로 계산한 타원)
def ellipse(ax, m, P, k=1.6, **kw):
    w, v = np.linalg.eigh(P)
    ang = math.degrees(math.atan2(v[1, 1], v[0, 1]))
    ax.add_patch(Ellipse(m, 2 * k * math.sqrt(w[1]), 2 * k * math.sqrt(w[0]), angle=ang, **kw))


def fig_kf_cycle():
    m0 = np.array([0.0, 0.0]); P0 = np.array([[0.05, 0.015], [0.015, 0.035]])
    m1 = m0 + np.array([2.3, 0.9]); P1 = np.array([[0.42, 0.12], [0.12, 0.26]])        # F P F^T + Q: 커짐
    z = m1 + np.array([0.42, -0.5])
    c, s_ = math.cos(math.radians(65)), math.sin(math.radians(65))
    Rot = np.array([[c, -s_], [s_, c]])
    R = Rot @ np.diag([0.5, 0.03]) @ Rot.T                                             # 플롯 오차: 거리는 정확, 옆으로 긴 타원
    K = P1 @ np.linalg.inv(P1 + R)
    m2 = m1 + K @ (z - m1)
    P2 = (np.eye(2) - K) @ P1
    fig, ax = plt.subplots(figsize=(6.3, 3.95))
    ax.set_xlim(-0.95, 4.75); ax.set_ylim(-1.3, 2.25); ax.set_aspect('equal'); ax.axis('off')
    ellipse(ax, m0, P0, fc=F_BLUE, ec=BLUE, lw=1.4, zorder=2)
    ellipse(ax, m1, P1, fc='none', ec=BLUE, lw=1.4, ls=(0, (4, 3)), zorder=2)
    ellipse(ax, z, R, fc=F_GRAY, ec=GRAY, lw=1.2, zorder=1)
    ellipse(ax, m2, P2, fc=F_BLUE, ec=BLUE, lw=1.6, zorder=3)
    ax.plot(*m0, 'o', color=BLUE, ms=6, zorder=4)
    ax.plot(*m1, 'o', mfc='white', mec=BLUE, mew=1.5, ms=6.5, zorder=4)
    ax.plot(*z, 'o', color=SUB, ms=6, zorder=4)
    ax.plot(*m2, 'o', color=BLUE, ms=6.5, zorder=5)
    ax.add_patch(FancyArrowPatch(m0, m1, arrowstyle='-|>', mutation_scale=13, color=NAVY, lw=1.3, shrinkA=10, shrinkB=7, zorder=4))
    ax.add_patch(FancyArrowPatch(m1, m2, arrowstyle='-|>', mutation_scale=11, color=NAVY, lw=1.3, shrinkA=5, shrinkB=5, zorder=6))
    ax.plot([z[0], m2[0]], [z[1], m2[1]], color=MUTED, lw=0.9, ls=(0, (2, 2)), zorder=3)
    ax.text(m0[0], m0[1] - 0.4, '스캔 k−1 의 추정\n' + r'$\hat{X}_{k-1|k-1},\ P_{k-1|k-1}$', ha='center', va='top', fontsize=9, color=NAVY)
    ax.text(1.2, -0.12, '① 예측', ha='center', va='top', fontsize=10, color=NAVY, fontweight='bold')
    ax.text(1.2, -0.42, r'$X = F\,X,\ \ P = F P F^{T} + Q$', ha='center', va='top', fontsize=8.8, color=SUB)
    ax.text(0.15, 2.05, '예측 ' + r'$\hat{X}_{k|k-1}$' + ': 오차 공분산이 커짐 (+Q)', ha='left', va='center', fontsize=9, color=NAVY)
    ax.plot([1.45, 1.75], [1.92, 1.6], color=MUTED, lw=0.8)
    ax.text(3.33, -0.05, '플롯 ' + r'$z_k$' + ' 와 R\n(측정 잡음)', ha='left', va='center', fontsize=9, color=NAVY)
    ax.text(3.58, 1.62, '② 갱신 ' + r'$\hat{X}_{k|k}$', ha='left', va='center', fontsize=10, color=NAVY, fontweight='bold')
    ax.text(3.58, 1.22, 'K 만큼 플롯 쪽으로\n오차 공분산은 줄어듦', ha='left', va='center', fontsize=8.8, color=SUB)
    ax.plot([3.55, m2[0] + 0.07], [1.5, m2[1] + 0.07], color=MUTED, lw=0.8)
    ax.text(4.75, -1.28, '타원 = 오차 공분산 (P, R) 의 크기와 방향', ha='right', va='bottom', fontsize=8.3, color=MUTED)
    fig.subplots_adjust(left=0.0, right=1.0, top=1.0, bottom=0.0)
    save(fig, 'fig_kf_cycle.png')


# ---------------------------------------------------------------- 1.3 측정 행렬 H
def fig_hmatrix():
    labels = ["x", "x'", "x''", "y", "y'", "y''", "z", "z'", "z''"]
    fig, ax = plt.subplots(figsize=(6.0, 1.85))
    ax.set_xlim(-1.35, 9.05); ax.set_ylim(-3.15, 1.0); ax.set_aspect('equal'); ax.axis('off')
    for c, lab in enumerate(labels):
        ax.text(c + 0.5, 0.35, lab, ha='center', va='center', fontsize=10, color=NAVY if c % 3 == 0 else MUTED, fontweight='bold' if c % 3 == 0 else 'normal')
    for r, lab in enumerate(['x', 'y', 'z']):
        ax.text(-0.25, -r - 0.5, lab, ha='right', va='center', fontsize=10, color=NAVY, fontweight='bold')
        for c in range(9):
            one = (c == 3 * r)
            ax.add_patch(Rectangle((c + 0.04, -r - 0.96), 0.92, 0.92, fc=F_BLUE if one else 'white', ec=BLUE if one else BORDER, lw=1.2 if one else 0.8))
            ax.text(c + 0.5, -r - 0.5, '1' if one else '0', ha='center', va='center', fontsize=10.5, color=BLUE if one else LIGHT, fontweight='bold' if one else 'normal')
    for c in (3, 6):
        ax.plot([c, c], [-3.05, 0.65], color=SUB, lw=0.9)
    ax.text(-1.3, 0.35, '상태 →', ha='left', va='center', fontsize=8.5, color=MUTED)
    ax.text(-1.3, -1.5, '측정\n↓', ha='left', va='center', fontsize=8.5, color=MUTED)
    fig.subplots_adjust(left=0.0, right=1.0, top=1.0, bottom=0.0)
    save(fig, 'fig_hmatrix.png')


# ---------------------------------------------------------------- 1.4 Singer: 가속도 자기 상관
def fig_singer():
    t = np.linspace(0, 60, 601)
    fig, ax = plt.subplots(figsize=(5.5, 2.75))
    clean(ax)
    for tau, col, lw in ((1, GRAY, 1.4), (60, GRAY, 1.4), (10, GIVEN, 2.2)):
        ax.plot(t, np.exp(-t / tau), color=col, lw=lw, zorder=3)
    ax.text(2.6, 0.1, 'τ = 1 s', fontsize=9, color=NAVY, va='bottom')
    ax.text(45, math.exp(-45 / 60) + 0.035, 'τ = 60 s', fontsize=9, color=NAVY, va='bottom', ha='center')
    ax.text(17.5, math.exp(-17.5 / 10) + 0.07, 'τ = 10 s (과제 값)', fontsize=9, color=NAVY, va='bottom')
    ax.plot([10], [math.exp(-1)], 'o', color=GIVEN, ms=6, zorder=4)
    ax.annotate('τ 초 뒤 37 % (e$^{-1}$) 만 남음', (10, math.exp(-1)), xytext=(13.5, 0.5), fontsize=8.8, color=NAVY,
                arrowprops=dict(arrowstyle='-', color=MUTED, lw=0.8))
    ax.set_xlim(0, 60); ax.set_ylim(0, 1.05)
    ax.set_xlabel('시간 차 t [s]')
    ax.set_ylabel(r'가속도 상관  $e^{-t/\tau}$')
    fig.subplots_adjust(left=0.12, right=0.98, top=0.96, bottom=0.17)
    save(fig, 'fig_singer.png')


# ---------------------------------------------------------------- 1.5 Q 의 q11: 닫힌 식과 급수의 정확도 (60 자리 기준값 대비)
Q11_COEF = (1.0 / 10.0, -1.0 / 18.0, 5.0 / 252.0, -1.0 / 180.0, 17.0 / 12960.0)   # TrackMgr.c f_Trk_SingerQ s_Coef[0]


def q11_closed(x):
    e1 = math.exp(-x); e2 = math.exp(-2.0 * x)
    return 1.0 - e2 + (2.0 * x) + ((2.0 * x * x * x) / 3.0) - (2.0 * x * x) - (4.0 * x * e1)


def q11_series(x):
    p = math.pow(x, 5.0); s = 0.0
    for c in Q11_COEF:
        s = s + c * p
        p = p * x
    return s


def q11_ref(x):
    X = mpmath.mpf(x)
    return 1 - mpmath.exp(-2 * X) + 2 * X + 2 * X ** 3 / 3 - 2 * X ** 2 - 4 * X * mpmath.exp(-X)


def fig_qprec():
    mpmath.mp.dps = 60
    xs = np.logspace(-6, 0, 241)
    err_c, err_s = [], []
    for x in xs:
        ref = q11_ref(float(x))
        err_c.append(max(1e-17, float(abs((mpmath.mpf(q11_closed(float(x))) - ref) / ref))))
        err_s.append(max(1e-17, float(abs((mpmath.mpf(q11_series(float(x))) - ref) / ref))))
    neg = [x for x in np.logspace(-6, -1, 4001) if q11_closed(float(x)) <= 0.0]
    fig, ax = plt.subplots(figsize=(5.6, 3.05))
    clean(ax)
    ax.axvspan(1e-6, 0.05, color=F_BLUE, lw=0, zorder=0)
    ax.plot(xs, err_c, color=SUB, lw=1.6, zorder=3)
    ax.plot(xs, err_s, color=TUNED, lw=2.0, zorder=3)
    ax.axvline(0.05, color=NAVY, lw=1.0, ls=(0, (4, 3)), zorder=2)
    ax.text(0.058, 1e-3, '0.05 에서\n식을 바꿈', fontsize=8.5, color=NAVY, va='center')
    x0 = 0.01
    ec = float(abs((mpmath.mpf(q11_closed(x0)) - q11_ref(x0)) / q11_ref(x0)))
    es = max(1e-17, float(abs((mpmath.mpf(q11_series(x0)) - q11_ref(x0)) / q11_ref(x0))))
    ax.plot([x0, x0], [ec, es], 'o', color=NAVY, ms=5.5, zorder=4)
    ax.annotate(f'과제 (T 0.1 s, τ 10 s → T/τ 0.01)\n닫힌 식 {ec:.0e}, 급수 {es:.0e}', (x0, ec), xytext=(1.6e-5, 1e-9), fontsize=8.5, color=NAVY,
                arrowprops=dict(arrowstyle='-', color=MUTED, lw=0.8))
    if neg:
        ax.text(1.4e-6, 3e-2, f'T/τ < {max(neg):.0e} (τ > {0.1 / max(neg):.0f} s) 이면\nq11 이 음수로도 나옴 → Q 가 공분산이 아님', fontsize=8.5, color=NAVY, va='center')
    ax.text(1.4e-6, 1e-15, '급수: 작은 T/τ 에서도 정확', fontsize=8.5, color=NAVY, va='bottom')
    ax.set_xscale('log'); ax.set_yscale('log')
    ax.set_xlim(1e-6, 1); ax.set_ylim(1e-17, 1e4)
    ax.set_yticks([1e-16, 1e-12, 1e-8, 1e-4, 1])
    ax.set_xlabel('T / τ   (T 0.1 s 에서 τ 0.1 ~ 100,000 s = 화면 입력 범위)')
    ax.set_ylabel('q11 상대 오차 (60 자리 기준)')
    handles = [Line2D([], [], color=SUB, lw=1.6, label='닫힌 식 (자료 식)'), Line2D([], [], color=TUNED, lw=2.0, label='급수 5 항'),
               Rectangle((0, 0), 1, 1, fc=F_BLUE, ec='none', label='급수를 쓰는 범위')]
    ax.legend(handles=handles, loc='lower center', ncol=3, fontsize=8.5, bbox_to_anchor=(0.5, 1.0), columnspacing=1.2)
    fig.subplots_adjust(left=0.15, right=0.98, top=0.9, bottom=0.17)
    save(fig, 'fig_qprec.png')


# ---------------------------------------------------------------- 1.6 NIS 분포와 게이트, 게이트 밖이 이어질 때 (대공 과제 값)
def fig_chi2():
    x = np.linspace(1e-4, 16, 1200)
    f = np.sqrt(x) * np.exp(-x / 2) / math.sqrt(2 * math.pi)                         # 자유도 3 카이제곱 밀도
    fig, ax = plt.subplots(figsize=(5.3, 2.55))
    clean(ax)
    ax.fill_between(x, f, where=x >= GATE, color=RED, alpha=0.35, lw=0)
    ax.plot(x, f, color=NAVY, lw=1.8, zorder=3)
    ax.axvline(3, color=SUB, lw=1.0)
    ax.axvline(GATE, color=RED, lw=1.4, ls=(0, (4, 3)))
    ax.text(3.25, 0.215, '평균 3\n(R · Q 가 맞을 때)', fontsize=8.6, color=NAVY, va='top')
    ax.text(GATE - 0.25, 0.215, '게이트 11.34', fontsize=8.6, color=NAVY, ha='right', va='top')
    ax.annotate('게이트 밖 1 %', (12.4, 0.0035), xytext=(12.7, 0.075), fontsize=8.6, color=NAVY, arrowprops=dict(arrowstyle='-', color=MUTED, lw=0.8))
    ax.set_xlim(0, 16); ax.set_ylim(0, 0.255)
    ax.set_xlabel('NIS')
    ax.set_ylabel('확률 밀도')
    fig.subplots_adjust(left=0.13, right=0.98, top=0.96, bottom=0.18)
    save(fig, 'fig_chi2.png')


def fig_runs():
    tk = TRACKS['air_given']
    w = tk[(tk.time >= 8.75) & (tk.time <= 10.35) & (tk.nis >= 0)]
    first = tk[tk.diverge_num >= 1].iloc[0]
    fig, ax = plt.subplots(figsize=(5.7, 2.55))
    clean(ax)
    for a, b in diverge_spans(tk):
        ax.axvspan(a, b, color=DIVERGE_BAND, lw=0, zorder=0)
    ax.axhline(GATE, color=RED, lw=1.4, ls=(0, (4, 3)), zorder=2)
    out = w.nis > GATE
    ax.scatter(w.time[~out], w.nis[~out], s=26, color=GRAY, zorder=3, lw=0)
    ax.scatter(w.time[out], w.nis[out], s=30, color=NAVY, zorder=3, lw=0)
    for _, r in w[out].iterrows():
        ax.text(r.time, r.nis + 1.6, str(int(r.gate_run)), ha='center', va='bottom', fontsize=8.5, color=NAVY, fontweight='bold' if r.gate_run == 5 else 'normal')
    ax.plot([first.time], [first.nis], 'o', ms=13, mfc='none', mec=RED, mew=1.4, zorder=4)
    ax.annotate(f'5 번째 연속 → 발산 판정 1 회 ({first.time:.1f} s)', (first.time, first.nis - 1.8), xytext=(10.37, 4.0), ha='right', fontsize=8.8,
                color=NAVY, arrowprops=dict(arrowstyle='-', color=MUTED, lw=0.8))
    ax.text(8.72, GATE + 0.8, '게이트 11.34', fontsize=8.5, color=NAVY, va='bottom')
    ax.set_xlim(8.7, 10.4); ax.set_ylim(0, 44)
    ax.set_xlabel('시간 [s]   (숫자 = 게이트 밖 연속 횟수, 안으로 들어오면 0)')
    ax.set_ylabel('NIS')
    fig.subplots_adjust(left=0.1, right=0.98, top=0.96, bottom=0.18)
    save(fig, 'fig_runs.png')


# ---------------------------------------------------------------- 3.1 시나리오 (VC-NED 평면, 레이다 원점)
def fig_scenarios():
    fig = plt.figure(figsize=(5.2, 4.3))
    ax = fig.add_axes([0.1, 0.1, 0.88, 0.88])
    clean(ax)
    for r in (5, 10, 15):
        ax.add_patch(Circle((0, 0), r, fill=False, ec=BORDER, lw=0.9, zorder=1))
        ax.text(r * math.cos(math.radians(-60)) + 0.25, r * math.sin(math.radians(-60)), f'{r} km', fontsize=7.8, color=MUTED, zorder=1)
    ax.plot([0], [0], marker='^', ms=9, color=NAVY, zorder=5)
    ax.text(-0.4, -0.15, '레이다 (정지, 북쪽을 봄)', fontsize=8.5, color=NAVY, va='top', ha='right')
    for sc in SCENARIOS:
        tr = TRUTH[sc]
        e, n = tr.east.values / 1000, tr.north.values / 1000
        ax.plot(e, n, color=TRUTH_C, lw=1.5, zorder=3)
        for a, b in turn_spans(sc):
            m = (tr.time.values >= a - 1e-9) & (tr.time.values <= b + 1e-9)
            ax.plot(e[m], n[m], color=PURPLE, lw=3.2, alpha=0.9, solid_capstyle='round', zorder=4)
        ax.plot(e[0], n[0], 'o', mfc='white', mec=TRUTH_C, mew=1.4, ms=6.5, zorder=6)
        k = len(e) - 12
        ax.annotate('', (e[-1], n[-1]), xytext=(e[k], n[k]), arrowprops=dict(arrowstyle='-|>', color=TRUTH_C, lw=1.4, mutation_scale=11), zorder=6)
    ax.text(3.1, 14.25, '대함  (고도 0 m)\n서쪽 30 m/s, ±1 G 선회 4 번', fontsize=8.6, color=NAVY, va='bottom')
    ax.text(2.75, 4.75, '대공  (고도 2,000 m)\n남쪽 200 m/s, ±4 G 선회 6 번', fontsize=8.6, color=NAVY, va='center')
    ax.set_xlim(-5.2, 9.2); ax.set_ylim(-1.2, 15.6); ax.set_aspect('equal')
    ax.set_xlabel('동 [km]'); ax.set_ylabel('북 [km]')
    handles = [Line2D([], [], color=TRUTH_C, lw=1.5, label='참값 궤적'), Line2D([], [], color=PURPLE, lw=3.2, label='선회 구간'),
               Line2D([], [], ls='none', marker='o', mfc='white', mec=TRUTH_C, mew=1.4, ms=6.5, label='시작')]
    ax.legend(handles=handles, loc='lower right', fontsize=8.5)
    # 대함 확대 (옆으로 긴 지그재그)
    ins = fig.add_axes([0.6, 0.39, 0.37, 0.2])
    tr = TRUTH['ship']
    e, n = tr.east.values, tr.north.values - 13800.0
    ins.plot(e, n, color=TRUTH_C, lw=1.3)
    for a, b in turn_spans('ship'):
        m = (tr.time.values >= a - 1e-9) & (tr.time.values <= b + 1e-9)
        ins.plot(e[m], n[m], color=PURPLE, lw=3.0, alpha=0.9, solid_capstyle='round')
    ins.plot(e[0], n[0], 'o', mfc='white', mec=TRUTH_C, mew=1.2, ms=5)
    ins.set_aspect('equal')
    ins.set_xlim(e.min() - 250, e.max() + 250); ins.set_ylim(n.min() - 160, n.max() + 160)
    ins.set_xticks([]); ins.set_yticks([])
    for s in ins.spines.values():
        s.set_color(SUB)
    ins.set_title('대함 확대', fontsize=8.5, pad=3)
    scale_bar(ins, 1000, '1 km', x=0.97, y=0.08)
    save(fig, 'fig_scenarios.png')


# ---------------------------------------------------------------- 3.5 발산 원인: 선회 가속도와 필터가 추정한 가속도, 선회에서 늦게 따라감
def fig_accel(name, runs, size=(6.3, 2.65)):
    t, a = truth_acc('air')
    fig, ax = plt.subplots(figsize=size)
    clean(ax)
    turn_bands(ax, 'air')
    ax.plot(t, a, color=TRUTH_C, lw=1.4, zorder=3)
    for run in runs:
        te, ae = est_acc(run)
        ax.plot(te, ae, color=GIVEN if run.endswith('given') else TUNED, lw=1.3, zorder=4)
    ax.set_xlim(0, 116); ax.set_ylim(0, 48)
    ax.set_xlabel('시간 [s]   (회색 띠 = 선회)')
    ax.set_ylabel('가속도 크기 [m/s²]')
    handles = [Line2D([], [], color=TRUTH_C, lw=1.4, label='참값 (선회 4 G = 39 m/s²)')]
    for run in runs:
        q = RUNS.loc[run, 'q']
        handles.append(Line2D([], [], color=GIVEN if run.endswith('given') else TUNED, lw=1.3,
                              label=f'트랙 추정 ({"과제 값" if run.endswith("given") else "조정 값"} q {q:g})'))
    ax.legend(handles=handles, loc='upper center', ncol=len(handles), fontsize=8.5, bbox_to_anchor=(0.5, 1.13), handlelength=1.6, columnspacing=1.2)
    fig.subplots_adjust(left=0.09, right=0.99, top=0.88, bottom=0.17)
    save(fig, name)


def fig_lag(name, runs, size=(4.6, 3.75)):
    t0, t1 = 3.0, 17.5
    tr = TRUTH['air']; pl = PLOTS['air']
    tw = tr[(tr.time >= t0) & (tr.time <= t1)]
    pw = pl[(pl.time >= t0) & (pl.time <= t1)]
    fig, ax = plt.subplots(figsize=size)
    clean(ax, grid=False)
    ax.scatter(pw.east, pw.north, s=6, color=PLOT_C, alpha=0.65, lw=0, zorder=1)
    ax.plot(tw.east, tw.north, color=TRUTH_C, lw=1.5, zorder=3)
    for run in runs:
        tk = TRACKS[run]
        kw = tk[(tk.time >= t0) & (tk.time <= t1)]
        col = GIVEN if run.endswith('given') else TUNED
        ax.plot(kw.east, kw.north, color=col, lw=1.4, zorder=4)
        ax.plot(kw.east.iloc[::5], kw.north.iloc[::5], ls='none', marker='s', ms=3.4, mfc='white', mec=col, mew=1.0, zorder=5)
        if run.endswith('given'):
            k = kw.err.idxmax()
            r = kw.loc[k]
            trow = tr.loc[(tr.time - r.time).abs().idxmin()]
            ax.plot([r.east, trow.east], [r.north, trow.north], color=RED, lw=1.0, zorder=6)
            ax.annotate(f'참값과 {r.err:.0f} m ({r.time:.1f} s)', (0.5 * (r.east + trow.east), 0.5 * (r.north + trow.north)),
                        xytext=(r.east + 230, r.north + 520), fontsize=8.6, color=NAVY, arrowprops=dict(arrowstyle='-', color=MUTED, lw=0.8), zorder=7)
    ax.set_aspect('equal')
    ax.set_xticks([]); ax.set_yticks([])
    xm, ym = pw.east.mean(), pw.north.mean()
    span = max(pw.east.max() - pw.east.min(), pw.north.max() - pw.north.min()) * 0.62
    ax.set_xlim(xm - span * size[0] / size[1], xm + span * size[0] / size[1] * 0.98)
    ax.set_ylim(ym - span, ym + span)
    scale_bar(ax, 500, '500 m', x=0.95, y=0.05)
    ax.annotate('', (tw.east.iloc[-1], tw.north.iloc[-1]), xytext=(tw.east.iloc[-8], tw.north.iloc[-8]), arrowprops=dict(arrowstyle='-|>', color=TRUTH_C, lw=1.2, mutation_scale=10))
    handles = [Line2D([], [], color=TRUTH_C, lw=1.5, label='참값'), Line2D([], [], ls='none', marker='o', ms=4, color=PLOT_C, label='플롯')]
    for run in runs:
        col = GIVEN if run.endswith('given') else TUNED
        handles.append(Line2D([], [], color=col, lw=1.4, marker='s', ms=3.4, mfc='white', mec=col, label='트랙 (' + ('과제 값' if run.endswith('given') else '조정 값') + ')'))
    ax.legend(handles=handles, loc='upper left', fontsize=8.5)
    ax.text(0.99, 0.99, f'대공 첫 선회 ({t0:.0f} ~ {t1:.1f} s), 평면', transform=ax.transAxes, ha='right', va='top', fontsize=8, color=MUTED)
    fig.subplots_adjust(left=0.01, right=0.99, top=0.99, bottom=0.01)
    save(fig, name)


# ---------------------------------------------------------------- 4.2 실제 플롯 잡음과 R 이 가정한 잡음
def fig_plotnoise():
    dr, daz, del_ = [], [], []
    for sc in SCENARIOS:
        tr = TRUTH[sc].set_index('step')
        pl = PLOTS[sc]
        t = tr.loc[pl.scan.values]
        rp, ap, ep = sph(pl.north.values, pl.east.values, pl.down.values)
        rt, at, et = sph(t.north.values, t.east.values, t.down.values)
        dr.append(rp - rt); daz.append(ap - at); del_.append(ep - et)
    dr, daz, del_ = np.concatenate(dr), np.concatenate(daz), np.concatenate(del_)
    fig, axs = plt.subplots(1, 3, figsize=(7.4, 2.9))
    panels = ((axs[0], dr, 'm', 10.0, 10.0, 40.0, '거리'), (axs[1], daz, '°', GIVEN_SIG, TUNED_SIG['air'], 2.0, '방위각'),
              (axs[2], del_, '°', GIVEN_SIG, TUNED_SIG['air'], 2.0, '고각'))
    for ax, d, unit, sg, st, lim, name in panels:
        clean(ax)
        bins = np.linspace(-lim, lim, 41)
        ax.hist(d, bins=bins, density=True, color='#CBD2DC', lw=0, zorder=2)
        xx = np.linspace(-lim, lim, 400)
        gauss = lambda s: np.exp(-0.5 * (xx / s) ** 2) / (s * math.sqrt(2 * math.pi))
        if sg == st:
            ax.plot(xx, gauss(sg), color=SUB, lw=1.8, zorder=3)
        else:
            ax.plot(xx, gauss(sg), color=GIVEN, lw=1.8, zorder=3)
            ax.plot(xx, gauss(st), color=TUNED, lw=1.8, zorder=3)
        ax.set_title(f'{name}  (실제 σ {np.std(d):.2f}°)' if unit == '°' else f'{name}  (실제 σ {np.std(d):.1f} {unit})', fontsize=9.5)
        ax.set_xlim(-lim, lim)
        ax.set_xticks([-20, 0, 20] if unit == 'm' else [-1, 0, 1])
        ax.set_yticks([])
        ax.set_xlabel(f'플롯 − 참값 [{unit}]')
    handles = [Rectangle((0, 0), 1, 1, fc='#CBD2DC', ec='none', label=f'실제 플롯 (SNR 20, {len(dr):,} 개)'),
               Line2D([], [], color=SUB, lw=1.8, label='R 의 σ 10 m (과제 값 = 조정 값)'),
               Line2D([], [], color=GIVEN, lw=1.8, label=f'R 의 σ {GIVEN_SIG:g}° (과제 값)'),
               Line2D([], [], color=TUNED, lw=1.8, label=f'R 의 σ {TUNED_SIG["air"]:g}° (조정 값)')]
    fig.legend(handles=handles, loc='lower center', ncol=4, fontsize=8.3, bbox_to_anchor=(0.5, -0.01), handlelength=1.5, columnspacing=1.0)
    fig.subplots_adjust(left=0.01, right=0.99, top=0.91, bottom=0.27, wspace=0.08)
    save(fig, 'fig_plotnoise.png')


def sweep_rows(sc, sig):
    s = SWEEP[(SWEEP.scenario == sc) & (SWEEP.sweep == 'q_sigma') & (np.isclose(SWEEP.sigma_az_deg, sig))]
    return s.sort_values('q')


def fig_sweep_nis():
    fig, axs = plt.subplots(1, 2, figsize=(5.6, 2.55), sharey=True)
    for ax, sc in zip(axs, SCENARIOS):
        clean(ax)
        for sig, col in ((GIVEN_SIG, GIVEN), (TUNED_SIG[sc], TUNED)):
            d = sweep_rows(sc, sig)
            ax.plot(d.q, d.mean_nis, color=col, lw=1.8, marker='o', ms=3.8, zorder=3)
        ax.axhline(3, color=SUB, lw=1.0, zorder=2)
        ax.text(75, 3.2, 'R 이 맞으면 3', fontsize=8.3, color=NAVY, va='bottom', ha='right')
        ax.set_xscale('log'); ax.set_yscale('log')
        ax.set_xticks([0.1, 1, 10, 80]); ax.set_xticklabels(['0.1', '1', '10', '80'])
        ax.set_yticks([1, 3, 10, 100]); ax.set_yticklabels(['1', '3', '10', '100'])
        ax.set_ylim(1.5, 600)
        ax.set_title(f'{SC_NAME[sc]} (30 회 평균)', fontsize=9.5)
        ax.set_xlabel('Singer q [m/s²]')
    axs[0].set_ylabel('NIS 평균')
    handles = [Line2D([], [], color=GIVEN, lw=1.8, marker='o', ms=3.8, label=f'R σ {GIVEN_SIG:g}° (과제 값)'),
               Line2D([], [], color=TUNED, lw=1.8, marker='o', ms=3.8, label=f'R σ {TUNED_SIG["air"]:g}° (조정 값)')]
    fig.legend(handles=handles, loc='lower center', ncol=2, fontsize=8.5, bbox_to_anchor=(0.5, -0.01))
    fig.subplots_adjust(left=0.11, right=0.98, top=0.9, bottom=0.31, wspace=0.08)
    save(fig, 'fig_sweep_nis.png')


# ---------------------------------------------------------------- 4.3 q 조정 (몬테카를로 30 회): 위치 오차와 발산이 난 회차 수
def fig_sweep_q():
    from matplotlib.ticker import NullFormatter
    fig, axs = plt.subplots(2, 2, figsize=(7.4, 4.25), sharex=True, gridspec_kw={'height_ratios': [1.45, 1]})
    # 표시 글자 위치 (q, 값): 대함은 두 점이 거의 겹쳐 왼쪽 위 빈 곳에 둠
    label_at = {'ship': {'given': (0.11, 150), 'tuned': (0.11, 85)}, 'air': {'given': (1.3, 260), 'tuned': (1.6, 95)}}
    for c, sc in enumerate(SCENARIOS):
        a1, a2 = axs[0, c], axs[1, c]
        clean(a1); clean(a2)
        for sig, col in ((GIVEN_SIG, GIVEN), (TUNED_SIG[sc], TUNED)):
            d = sweep_rows(sc, sig)
            a1.plot(d.q, d.rms, color=col, lw=1.8, marker='o', ms=3.8, zorder=3)
            a2.plot(d.q, d.diverged_seeds, color=col, lw=1.8, marker='o', ms=3.8, zorder=3)
        g = sweep_rows(sc, GIVEN_SIG); g = g[np.isclose(g.q, GIVEN_Q)].iloc[0]
        t = sweep_rows(sc, TUNED_SIG[sc]); t = t[np.isclose(t.q, TUNED_Q[sc])].iloc[0]
        for ax, yg, yt in ((a1, g.rms, t.rms), (a2, g.diverged_seeds, t.diverged_seeds)):
            ax.plot([g.q], [yg], 'o', ms=11, mfc='none', mec=NAVY, mew=1.2, zorder=4)
            ax.plot([t.q], [yt], '*', ms=13, color=NAVY, zorder=5)
        a1.annotate(f'과제 값 {g.rms:.0f} m, 30 회 중 {int(g.diverged_seeds)} 회 발산', (g.q, g.rms), xytext=label_at[sc]['given'], fontsize=8.4, color=NAVY,
                    arrowprops=dict(arrowstyle='-', color=MUTED, lw=0.8))
        a1.annotate(f'조정 값 {t.rms:.0f} m, 발산 없음', (t.q, t.rms), xytext=label_at[sc]['tuned'], fontsize=8.4, color=NAVY, fontweight='bold',
                    arrowprops=dict(arrowstyle='-', color=MUTED, lw=0.8))
        a1.set_yscale('log')
        a1.set_yticks([30, 50, 100, 200, 400]); a1.set_yticklabels(['30', '50', '100', '200', '400'])
        a1.yaxis.set_minor_formatter(NullFormatter())
        a1.set_ylim(24, 520)
        a1.set_title(f'{SC_NAME[sc]}', fontsize=10)
        a2.set_ylim(-2, 32); a2.set_yticks([0, 10, 20, 30])
        a2.set_xscale('log')
        a2.set_xticks([0.1, 0.25, 0.5, 1, 2, 5, 10, 20, 40, 80]); a2.set_xticklabels(['0.1', '', '0.5', '1', '2', '5', '10', '20', '', '80'])
        a2.xaxis.set_minor_formatter(NullFormatter())
        a2.set_xlabel('Singer q [m/s²]')
    axs[0, 0].set_ylabel('위치 오차 RMS [m]')
    axs[1, 0].set_ylabel('발산 회차 (/30)')
    handles = [Line2D([], [], color=GIVEN, lw=1.8, marker='o', ms=3.8, label=f'R σ {GIVEN_SIG:g}° (과제 값)'),
               Line2D([], [], color=TUNED, lw=1.8, marker='o', ms=3.8, label=f'R σ {TUNED_SIG["air"]:g}° (조정 값)'),
               Line2D([], [], ls='none', marker='o', ms=9, mfc='none', mec=NAVY, mew=1.2, label='과제 값 (q 0.5)'),
               Line2D([], [], ls='none', marker='*', ms=11, color=NAVY, label='조정 값')]
    fig.legend(handles=handles, loc='lower center', ncol=4, fontsize=8.5, bbox_to_anchor=(0.5, -0.005), columnspacing=1.2)
    fig.subplots_adjust(left=0.1, right=0.99, top=0.94, bottom=0.2, hspace=0.12, wspace=0.12)
    save(fig, 'fig_sweep_q.png')


def fig_sweep_tau():
    fig, axs = plt.subplots(1, 2, figsize=(4.2, 2.1), sharey=True)
    for ax, sc in zip(axs, SCENARIOS):
        clean(ax)
        d = SWEEP[(SWEEP.scenario == sc) & (SWEEP.sweep == 'tau')].sort_values('tau')
        ax.plot(d.tau, d.rms, color=TUNED, lw=1.8, marker='o', ms=3.8, zorder=3)
        k = d[np.isclose(d.tau, 10)].iloc[0]
        ax.plot([10], [k.rms], '*', ms=11, color=NAVY, zorder=4)
        ax.set_xscale('log')
        ax.set_xticks([1, 2, 5, 10, 20, 60]); ax.set_xticklabels(['1', '2', '5', '10', '20', '60'])
        ax.set_ylim(20, 45)
        ax.set_title(f'{SC_NAME[sc]} (q {TUNED_Q[sc]:g})', fontsize=9)
        ax.set_xlabel('τ [s]')
    axs[0].set_ylabel('RMS [m]')
    fig.subplots_adjust(left=0.15, right=0.98, top=0.87, bottom=0.24, wspace=0.1)
    save(fig, 'fig_sweep_tau.png')


# ---------------------------------------------------------------- 4.4 ~ 4.6 과제 값과 조정 값 비교 (위치 오차, NIS)
def fig_compare(sc, size=(7.4, 3.7)):
    fig, axs = plt.subplots(2, 1, figsize=size, sharex=True, gridspec_kw={'height_ratios': [1.2, 1]})
    for ax in axs:
        clean(ax)
        turn_bands(ax, sc)
        ax.axvspan(0, 2.2, color=F_GRAY, lw=0, zorder=0)
    for run, col in ((sc + '_given', GIVEN), (sc + '_tuned', TUNED)):
        tk = TRACKS[run]
        axs[0].plot(tk.time, tk.err, color=col, lw=1.1, zorder=3)
        n = tk[tk.nis >= 0]
        axs[1].scatter(n.time, n.nis, s=2.5, color=col, alpha=0.75, lw=0, zorder=3)
    axs[1].axhline(GATE, color=RED, lw=1.2, ls=(0, (4, 3)), zorder=4)
    axs[1].set_yscale('log')
    axs[1].set_ylim(0.01, 1000 if sc == 'air' else 100)
    axs[0].set_ylim(0, 340 if sc == 'air' else 160)
    axs[0].set_ylabel('위치 오차 [m]')
    axs[1].set_ylabel('NIS')
    axs[1].set_xlabel('시간 [s]   (회색 띠 = 선회, 0 ~ 2.2 s 는 개시 직후라 통계에서 뺌)')
    axs[1].set_xlim(0, 116)
    axs[1].text(115, GATE * 1.25, '게이트 11.34', ha='right', va='bottom', fontsize=8.3, color=NAVY)
    handles = []
    for run, col, name in ((sc + '_given', GIVEN, '과제 값'), (sc + '_tuned', TUNED, '조정 값')):
        r = RUNS.loc[run]
        handles.append(Line2D([], [], color=col, lw=1.6, label=f'{name}: RMS {r.rms:.1f} m, NIS 평균 {r.mean_nis:.2f}, 발산 판정 {int(r.divergence)} 번'))
    axs[0].legend(handles=handles, loc='upper center', ncol=2, fontsize=8.4, bbox_to_anchor=(0.5, 1.2), handlelength=1.5, columnspacing=1.4)
    fig.subplots_adjust(left=0.09, right=0.99, top=0.9, bottom=0.14, hspace=0.08)
    save(fig, f'fig_compare_{sc}.png')


if __name__ == '__main__':
    cover('cover_bg.png', True)
    cover('end_bg.png', False)
    fig_role()
    fig_kf_cycle()
    fig_hmatrix()
    fig_singer()
    fig_qprec()
    fig_chi2()
    fig_runs()
    fig_scenarios()
    fig_accel('fig_accel_given.png', ['air_given'])
    fig_lag('fig_lag_given.png', ['air_given'])
    fig_lag('fig_lag_both.png', ['air_given', 'air_tuned'])
    fig_plotnoise()
    fig_sweep_nis()
    fig_sweep_q()
    fig_sweep_tau()
    fig_compare('air')
    fig_compare('ship')
