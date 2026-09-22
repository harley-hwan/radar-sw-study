# -*- coding: utf-8 -*-
"""발표자료 그림을 그린다.

    python make_figures.py            # 전부
    python make_figures.py 15 17      # 번호만 골라서

결과는 ../figures/ 에 png 로 떨어진다. 궤적·오차 그림은 ../data/ 의 CSV 를 읽는데,
그 CSV 는 run_sim.c / run_convergence.c 를 실제 Core 와 링크해 돌린 결과다.
그림에 찍힌 숫자는 전부 그 실행 결과이며, 손으로 적은 값은 없다.

작도 규칙 (기술보고서 그림 관례를 따른다)
  - 축은 실선, 가려진 축·보조선은 파선
  - 벡터는 굵은 실선 + 화살촉, 각은 호(arc)로 표시하고 기호를 붙인다
  - 수식은 mathtext(STIX) 로, 좌표계는 C^{to}_{from} 표기
  - 색은 뜻이 있을 때만 쓴다 (객체 구분 / 강조), 나머지는 회색조
"""
import csv
import math
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib import font_manager
from matplotlib.patches import (Arc, Circle, Ellipse, FancyArrowPatch, FancyBboxPatch,
                                PathPatch, Polygon, Rectangle, Wedge)
from matplotlib.path import Path
from matplotlib.ticker import FuncFormatter, NullFormatter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "figures"))
DATA = os.path.normpath(os.path.join(HERE, "..", "data"))
os.makedirs(OUT, exist_ok=True)

# ── 색 (회사 템플릿 팔레트) ─────────────────────────────────────────────
INK = "#1B2740"          # 본문 · 축
INK2 = "#38455E"         # 보조 본문
GREY = "#7C8AA3"         # 설명글
RULE = "#C9D2E0"         # 가는 선 · 표 괘선
FAINT = "#E6EBF3"        # 격자 · 채움
PANEL = "#F7F9FC"        # 패널 바탕
WHITE = "#FFFFFF"

BLUE = "#2563EB"         # 표적 1 (대함) · 강조
RED = "#DC2626"          # 표적 2 (대공)
GREEN = "#059669"        # 확인 · 정답
AMBER = "#D97706"        # 주의 · Pitch
VIOLET = "#7C3AED"       # Roll
PF = "#374151"           # 플랫폼

for cand in ("NanumGothic", "NanumBarunGothic", "NanumSquare"):
    if any(f.name == cand for f in font_manager.fontManager.ttflist):
        KO = cand
        break
else:
    KO = "DejaVu Sans"
MONO = "NanumGothicCoding" if any(f.name == "NanumGothicCoding"
                                  for f in font_manager.fontManager.ttflist) else "DejaVu Sans Mono"

plt.rcParams.update({
    "font.family": KO,
    "axes.unicode_minus": False,
    "mathtext.fontset": "stix",
    "mathtext.default": "it",
    "figure.facecolor": WHITE,
    "savefig.facecolor": WHITE,
    "axes.edgecolor": "#9AA7BD",
    "axes.linewidth": 0.9,
    "axes.labelcolor": INK,
    "text.color": INK,
    "xtick.color": INK2,
    "ytick.color": INK2,
    "xtick.labelsize": 8.5,
    "ytick.labelsize": 8.5,
    "grid.color": FAINT,
    "grid.linewidth": 0.7,
    "legend.frameon": True,
    "legend.framealpha": 0.96,
    "legend.edgecolor": RULE,
})

DPI = 230


# ═══════════════════════════════════════════════════════════════════════
# 공통 도구
# ═══════════════════════════════════════════════════════════════════════
def save(fig, name):
    fig.savefig(os.path.join(OUT, name), dpi=DPI, bbox_inches="tight", pad_inches=0.05)
    plt.close(fig)
    print("  %s" % name)


# 가로·세로를 모두 0~100 으로 두면 배치는 쉽지만 x 단위가 y 단위보다 짧아진다.
# 원이 타원으로 찌그러지므로, 길이를 y 단위로 적고 x 로 옮길 때 이 비를 나눈다.
_AR = [1.0]


def canvas(w, h):
    """가로·세로 0~100 좌표. 배치를 눈으로 셀 수 있게 한다."""
    fig, ax = plt.subplots(figsize=(w, h))
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100)
    ax.axis("off")
    ax.set_position([0, 0, 1, 1])
    _AR[0] = float(w) / float(h)
    return fig, ax


def xr(v):
    """y 단위 길이 v 를 같은 물리 길이의 x 단위로."""
    return v / _AR[0]


def circle(ax, cx, cy, r, **kw):
    """화면에서 진짜 동그란 원 (r 은 y 단위)."""
    kw.setdefault("zorder", 3)
    ax.add_patch(Ellipse((cx, cy), 2.0 * xr(r), 2.0 * r, **kw))


def arcp(ax, cx, cy, r, t1, t2, color=INK, lw=1.4, z=6, ls="-"):
    """각도가 화면 각도와 일치하는 호."""
    ax.add_patch(Arc((cx, cy), 2.0 * xr(r), 2.0 * r, theta1=t1, theta2=t2,
                     color=color, lw=lw, zorder=z, linestyle=ls))


def polar(cx, cy, r, deg):
    a = math.radians(deg)
    return cx + xr(r) * math.cos(a), cy + r * math.sin(a)


def polyx(ax, cx, cy, pts, s=1.0, rot=0.0, **kw):
    """(x, y) 목록을 y 단위로 보고 비율을 맞춰 찍는다. rot 는 화면 기준 회전."""
    c, sn = math.cos(rot), math.sin(rot)
    out = []
    for px, py in pts:
        qx, qy = px * s, py * s
        rx, ry = qx * c - qy * sn, qx * sn + qy * c
        out.append((cx + xr(rx), cy + ry))
    kw.setdefault("zorder", 5)
    ax.add_patch(Polygon(np.array(out), closed=True, **kw))


def arrow(ax, p0, p1, color=INK, lw=1.5, ms=11, ls="-", zorder=4, rad=0.0, style="-|>"):
    ax.add_patch(FancyArrowPatch(p0, p1, arrowstyle=style, mutation_scale=ms, color=color,
                                 linewidth=lw, linestyle=ls, zorder=zorder, shrinkA=0,
                                 shrinkB=0, connectionstyle="arc3,rad=%g" % rad,
                                 joinstyle="miter", capstyle="butt"))


def panel(ax, x, y, w, h, fill=WHITE, edge=RULE, lw=1.0, r=0.0, z=1):
    ax.add_patch(FancyBboxPatch((x, y), w, h,
                                boxstyle="round,pad=0,rounding_size=%g" % max(r, 1e-6),
                                facecolor=fill, edgecolor=edge, linewidth=lw, zorder=z))


def label(ax, x, y, s, size=10, color=INK, bold=False, ha="left", va="center",
          font=None, space=1.45, z=6, bbox=None):
    ax.text(x, y, s, fontsize=size, color=color, ha=ha, va=va, zorder=z,
            fontweight="bold" if bold else "normal", linespacing=space,
            family=font if font else None, bbox=bbox)


def caption(ax, x, y, s, size=9.2, color=GREY, ha="left"):
    label(ax, x, y, s, size=size, color=color, ha=ha, va="center")


def head(ax, x, y, s, size=11.5, color=INK, rule_w=None):
    """작은 절 제목 + 그 아래 가는 밑줄."""
    label(ax, x, y, s, size=size, color=color, bold=True, va="bottom")
    if rule_w:
        ax.plot([x, x + rule_w], [y - 1.6, y - 1.6], color=RULE, lw=1.0, zorder=3)


def tag(ax, x, y, s, color=BLUE, size=8.6):
    """모서리에 붙는 작은 식별 꼬리표."""
    label(ax, x, y, s, size=size, color=color, bold=True, va="center",
          bbox=dict(boxstyle="round,pad=0.30", fc=WHITE, ec=color, lw=0.9))


# ── 행렬 작도 (mathtext 에 bmatrix 가 없어 직접 그린다) ─────────────────
def matrix(ax, cx, cy, rows, cw=9.0, ch=6.2, size=10.5, color=INK, brk=1.3,
           lead=None, trail=None, lead_size=12.5, cell_color=None):
    """가운데(cx, cy)에 대괄호로 묶은 행렬을 그린다. rows 는 문자열 2차원 목록."""
    nr, nc = len(rows), len(rows[0])
    w, h = nc * cw, nr * ch
    x0, y0 = cx - w / 2.0, cy - h / 2.0
    lw = 1.2

    for sx in (x0 - 1.0, x0 + w + 1.0):
        d = brk if sx < cx else -brk
        ax.plot([sx + d, sx, sx, sx + d], [y0 + h + 0.6, y0 + h + 0.6, y0 - 0.6, y0 - 0.6],
                color=color, lw=lw, zorder=5, solid_joinstyle="miter")

    for i, row in enumerate(rows):
        for j, cell in enumerate(row):
            if not cell:
                continue
            cc = cell_color(i, j) if cell_color else color
            ax.text(x0 + (j + 0.5) * cw, y0 + h - (i + 0.5) * ch, cell, fontsize=size,
                    color=cc, ha="center", va="center", zorder=6)

    if lead:
        ax.text(x0 - 2.6, cy, lead, fontsize=lead_size, color=color, ha="right",
                va="center", zorder=6)
    if trail:
        ax.text(x0 + w + 2.6, cy, trail, fontsize=lead_size, color=color, ha="left",
                va="center", zorder=6)
    return (x0 - 2.0, y0 - 1.0, w + 4.0, h + 2.0)


# ── 3D 축 삼각대 (등각 투영) ────────────────────────────────────────────
def iso(p, s=1.0, ax_deg=30.0, az_deg=30.0):
    """(x, y, z) → 화면 (u, v). 오른손 좌표계를 등각으로 눕힌다."""
    a, b = math.radians(ax_deg), math.radians(az_deg)
    x, y, z = p
    u = (x * math.cos(a) - y * math.cos(b)) * s
    v = (-x * math.sin(a) - y * math.sin(b) + z) * s
    return u, v


def triad(ax, origin, axes, s=1.0, color=INK, lw=1.5, hidden=(), size=10,
          ax_deg=30.0, az_deg=30.0, off=2.2):
    """축 삼각대. axes = [((x,y,z), '라벨'), ...], hidden 에 든 라벨은 파선."""
    ox, oy = origin
    for vec, name in axes:
        u, v = iso(vec, s, ax_deg, az_deg)
        ls = (0, (5, 3)) if name in hidden else "-"
        arrow(ax, (ox, oy), (ox + xr(u), oy + v), color=color, lw=lw, ms=10, ls=ls)
        n = math.hypot(u, v) or 1.0
        ax.text(ox + xr(u + off * u / n), oy + v + off * v / n, name, fontsize=size,
                color=color, ha="center", va="center", zorder=7)


# ── 구면 정사영 (기술보고서 그림의 지구 도해와 같은 방식) ──────────────
class Ortho(object):
    """시선 방향 c 에서 본 단위구의 정사영. 화면 좌표는 y 단위."""

    def __init__(self, lat_deg=18.0, lon_deg=26.0):
        fc, lc = math.radians(lat_deg), math.radians(lon_deg)
        self.c = np.array([math.cos(fc) * math.cos(lc),
                           math.cos(fc) * math.sin(lc), math.sin(fc)])
        r = np.cross([0.0, 0.0, 1.0], self.c)
        self.r = r / np.linalg.norm(r)
        self.u = np.cross(self.c, self.r)

    def xy(self, v):
        v = np.asarray(v, dtype=float)
        return float(v @ self.r), float(v @ self.u)

    def front(self, v):
        return float(np.asarray(v, dtype=float) @ self.c) > 0.0

    @staticmethod
    def sph(lat_deg, lon_deg, rad=1.0):
        la, lo = math.radians(lat_deg), math.radians(lon_deg)
        return np.array([rad * math.cos(la) * math.cos(lo),
                         rad * math.cos(la) * math.sin(lo), rad * math.sin(la)])


def sphere_curve(ax, view, pts, cx, cy, R, color=RULE, lw=0.8, front_ls="-",
                 back_ls=(0, (3, 3)), z=3, back=True):
    """구면 위 점열을 앞/뒤로 나눠 그린다."""
    seg, seg_front = [], None
    for v in pts:
        f = view.front(v)
        if seg_front is None:
            seg_front = f
        if f != seg_front:
            _flush(ax, view, seg, cx, cy, R, color, lw,
                   front_ls if seg_front else back_ls, z, back or seg_front)
            seg, seg_front = [seg[-1]] if seg else [], f
        seg.append(v)
    _flush(ax, view, seg, cx, cy, R, color, lw,
           front_ls if seg_front else back_ls, z, back or seg_front)


def _flush(ax, view, seg, cx, cy, R, color, lw, ls, z, draw):
    if len(seg) < 2 or not draw:
        return
    xy = [view.xy(v) for v in seg]
    ax.plot([cx + xr(R * a) for a, _ in xy], [cy + R * b for _, b in xy],
            color=color, lw=lw, ls=ls, zorder=z)


def arc_in_plane(ax, o, e1, e2, r, t0, t1, color=INK, lw=1.5, z=6, n=80):
    """화면 벡터 e1, e2 가 펼치는 평면 안의 원호 (정사영 그대로)."""
    ts = np.linspace(math.radians(t0), math.radians(t1), n)
    xs = [o[0] + xr(r * (math.cos(t) * e1[0] + math.sin(t) * e2[0])) for t in ts]
    ys = [o[1] + r * (math.cos(t) * e1[1] + math.sin(t) * e2[1]) for t in ts]
    ax.plot(xs, ys, color=color, lw=lw, zorder=z)
    return xs, ys


def read_csv(name):
    with open(os.path.join(DATA, name), newline="", encoding="utf-8") as fp:
        return list(csv.DictReader(fp))


def col(rows, key):
    return np.array([float(r[key]) for r in rows])


A_WGS, E_WGS = 6378137.0, 0.08181919


def lla2ecef(lat_deg, lon_deg, alt):
    la, lo = np.radians(lat_deg), np.radians(lon_deg)
    N = A_WGS / np.sqrt(1.0 - E_WGS ** 2 * np.sin(la) ** 2)
    return np.column_stack([(N + alt) * np.cos(la) * np.cos(lo),
                            (N + alt) * np.cos(la) * np.sin(lo),
                            (N * (1.0 - E_WGS ** 2) + alt) * np.sin(la)])


def to_en(lat, lon, lat0, lon0):
    """플랫폼 기준 동·북 [m]. 국지 평면 근사 (그림용)."""
    return (lon - lon0) * 111320.0 * math.cos(math.radians(lat0)), (lat - lat0) * 111132.0


# ═══════════════════════════════════════════════════════════════════════
# 01. 표적 모의가 들어가는 자리
# ═══════════════════════════════════════════════════════════════════════
def fig01():
    fig, ax = canvas(11.2, 4.6)

    head(ax, 2, 92, "레이다 데이터처리 체인에서 모의가 대신하는 구간", 12)

    blocks = [("안테나 · 수신기", 2.0), ("신호처리", 21.0), ("데이터처리", 40.0),
              ("추적 필터", 59.0), ("전시 · 통제", 78.0)]
    for i, (name, x) in enumerate(blocks):
        on = i >= 2
        panel(ax, x, 76.0, 18.0, 11.0, fill=PANEL if on else FAINT,
              edge=INK if on else RULE, lw=1.4 if on else 1.0)
        label(ax, x + 9, 81.5, name, size=10.2, color=INK if on else GREY,
              bold=on, ha="center")
        if i < 4:
            arrow(ax, (x + 18.3, 81.5), (x + 20.7, 81.5), color=INK2, lw=1.3, ms=10)

    for x in (11.0, 30.0):
        ax.plot([x, x], [76.0, 70.0], color=BLUE, lw=1.2, ls=(0, (4, 3)), zorder=3)
    ax.plot([11.0, 30.0], [70.0, 70.0], color=BLUE, lw=1.2, ls=(0, (4, 3)), zorder=3)
    arrow(ax, (20.5, 70.0), (20.5, 75.6), color=BLUE, lw=1.6, ms=11)
    panel(ax, 2.0, 61.0, 37.0, 8.0, fill="#EEF4FF", edge=BLUE, lw=1.4)
    label(ax, 20.5, 65.0, "TargetSim — 표적 궤적 모의 (이번 과제)", size=10.2,
          color=BLUE, bold=True, ha="center")
    caption(ax, 41.5, 65.0, "실장비 없이 같은 규격의 입력을 공급한다 — 시각별 표적 위 · 경 · 고도")

    # 왼쪽: 비교표
    head(ax, 2.0, 51.0, "모의를 쓰는 이유", 11.2, rule_w=45)
    cols = ["", "실제 해상 시험", "표적 모의"]
    body = [("반복성", "같은 상황 재현 불가", "몇 번이든 동일"),
            ("참값(truth)", "기준 장비가 또 필요, 그 값도 오차", "입력값이 곧 참값"),
            ("조건 제어", "날씨 · 공역 · 표적 가용성에 종속", "G · 침로 · 고도 임의"),
            ("소요", "함정 · 항공기 · 인력 · 기간", "60 s 궤적이 5 ms")]
    xs = [2.0, 13.0, 33.5]
    ax.plot([2, 47], [42.5, 42.5], color=INK, lw=1.1, zorder=3)
    for x, c in zip(xs, cols):
        label(ax, x, 44.6, c, size=9.4, color=INK, bold=True)
    for i, r in enumerate(body):
        y = 38.0 - i * 8.4
        for j, (x, v) in enumerate(zip(xs, r)):
            label(ax, x, y, v, size=8.8, color=INK if j == 0 else INK2, bold=(j == 0))
        ax.plot([2, 47], [y - 3.9, y - 3.9], color=RULE, lw=0.7, zorder=2)

    # 오른쪽: 요구 조건
    head(ax, 53.0, 51.0, "모의가 만족해야 할 조건", 11.2, rule_w=45)
    conds = [("규격 일치", "데이터처리 입력과 같은 형식",
              "시각마다 위 · 경 · 고도 — 본 과제는 0.1 s 간격 601 표본"),
             ("물리적 타당성", "불변량이 보존될 것",
              "등속 수평 비행이면 속력과 고도가 변하지 않아야 한다"),
             ("수치적 재현성", "정해진 차수로 수렴할 것",
              "갱신 간격 Δt 를 절반으로 줄이면 오차가 1/4 로 (2차)")]
    for i, (k, w, d) in enumerate(conds):
        y = 37.0 - i * 11.5
        ax.add_patch(Rectangle((53.0, y - 3.0), 0.8, 8.6, facecolor=BLUE,
                               edgecolor="none", zorder=4))
        label(ax, 55.2, y + 3.4, k, size=9.8, color=INK, bold=True)
        label(ax, 68.0, y + 3.4, w, size=9.2, color=BLUE)
        label(ax, 55.2, y - 0.6, d, size=8.8, color=INK2)
    save(fig, "fig01_why.png")


# ═══════════════════════════════════════════════════════════════════════
# 02. 소프트웨어 구성과 인터페이스
# ═══════════════════════════════════════════════════════════════════════
def fig02():
    fig, ax = canvas(11.2, 4.7)

    # 계층
    layers = [("표현 계층", "TargetSimUI  (C++ / MFC)", 72.0, 22.0,
               [("CTargetSimUIDlg", "배치 · 실행 · 재생"),
                ("CScenario", "입력 보관 · 범위 검사 · 파일 I/O"),
                ("CTrajectoryPlot / CGridCtrl", "궤적 · 고도 작도, 표 편집")]),
              ("연산 계층", "TargetSimCore  (C, DLL)", 41.0, 25.0,
               [("TargetSim.c", "상태 갱신 — 이번 과제에서 새로 작성"),
                ("CoordinateTransform.c", "좌표 변환 — 제공 코드 원본"),
                ("matrixCalcLib.c", "행렬 연산 — 제공 코드 원본")])]

    for name, title, y, h, items in layers:
        panel(ax, 8.0, y, 57.0, h, fill=PANEL, edge=RULE)
        label(ax, 10.0, y + h - 4.2, title, size=11.5, color=INK, bold=True)
        ax.plot([10.0, 63.0], [y + h - 7.2, y + h - 7.2], color=RULE, lw=0.9, zorder=3)
        for i, (n, d) in enumerate(items):
            yy = y + h - 11.5 - i * 5.0
            label(ax, 11.0, yy, n, size=9.4, color=INK, font=MONO, bold=True)
            label(ax, 31.0, yy, d, size=9.2, color=INK2)
        tag(ax, 3.4, y + h - 4.2, name, color=INK2, size=8.2)

    # 인터페이스
    panel(ax, 69.0, 41.0, 29.0, 53.0, fill=WHITE, edge=INK, lw=1.3)
    label(ax, 83.5, 89.5, "인터페이스", size=11, color=INK, bold=True, ha="center")
    ax.plot([71, 96], [86.5, 86.5], color=RULE, lw=0.9, zorder=3)
    iface = [("입력", "ST_SimConfig", "10.3 KB", BLUE,
              "시뮬레이션 시간 · 간격\n플랫폼 1 + 표적 N"),
             ("출력", "ST_SimSample", "1.2 KB / 표본", GREEN,
              "스텝 번호 · 시각\n객체별 ECEF · LLA · 자세")]
    for i, (k, n, sz, c, d) in enumerate(iface):
        y = 64.0 - i * 22.0
        ax.add_patch(Rectangle((71.0, y), 0.8, 18.0, facecolor=c, edgecolor="none",
                               zorder=4))
        label(ax, 73.2, y + 16.0, k, size=9.0, color=c, bold=True)
        label(ax, 73.2, y + 12.2, n, size=10.0, color=INK, font=MONO, bold=True)
        label(ax, 73.2, y + 8.8, sz, size=8.6, color=GREY, font=MONO)
        label(ax, 73.2, y + 3.6, d, size=8.8, color=INK2, space=1.55)

    arrow(ax, (65.4, 73.0), (68.6, 73.0), color=BLUE, lw=1.6, ms=11)
    arrow(ax, (68.6, 51.0), (65.4, 51.0), color=GREEN, lw=1.6, ms=11)

    # 호출 순서
    panel(ax, 3.0, 4.0, 95.0, 32.0, fill=WHITE, edge=RULE)
    label(ax, 5.0, 31.5, "호출 순서 — 공개 함수는 여섯 개뿐이다", size=10.8, color=INK,
          bold=True)
    steps = [("f_Tgt_ValidateConfig", "설정 전체 검사", "표적 수 · 시간 · 속력 · 기동 구간"),
             ("f_Tgt_InitSim", "표본 0 생성", "LLA → ECEF, 자세 → ECEF 속도"),
             ("f_Tgt_StepSim", "한 스텝 전진", "nStepNum 회 반복 (601 표본)"),
             ("f_Tgt_EcefToNed 등", "작도용 변환", "화면 · CSV 출력 시에만")]
    for i, (fn, what, detail) in enumerate(steps):
        x = 5.0 + i * 23.5
        panel(ax, x, 9.5, 21.0, 15.0, fill=PANEL, edge=RULE)
        ax.add_patch(Circle((x + 2.4, 21.6), 1.5, facecolor=INK, edgecolor="none", zorder=5))
        label(ax, x + 2.4, 21.6, str(i + 1), size=8.0, color=WHITE, bold=True, ha="center")
        label(ax, x + 5.0, 21.6, fn, size=8.6, color=INK, font=MONO, bold=True)
        label(ax, x + 1.6, 16.8, what, size=9.6, color=INK, bold=True)
        label(ax, x + 1.6, 12.6, detail, size=8.6, color=INK2, space=1.5)
        if i < 3:
            arrow(ax, (x + 21.3, 17.0), (x + 23.2, 17.0), color=RULE, lw=1.3, ms=9)
    caption(ax, 5.0, 6.0, "UI 는 Core 의 내부를 보지 않고, Core 는 화면을 모른다. "
                          "그래서 리눅스에서 gcc 로 Core 만 따로 빌드해 이 발표의 숫자를 냈다.")
    save(fig, "fig02_system.png")


# ═══════════════════════════════════════════════════════════════════════
# 03. 상태 벡터 정의
# ═══════════════════════════════════════════════════════════════════════
def fig03():
    fig, ax = canvas(11.2, 4.6)

    head(ax, 3, 90, "표적 하나의 상태 벡터", 12, rule_w=40)
    ax.text(3, 79, r"$\mathbf{x}(t)=\left[\;\mathbf{p}^{e}(t)^{T}\;\;"
                   r"\mathbf{v}^{e}(t)^{T}\;\;\boldsymbol{\Theta}(t)^{T}\;\right]^{T}"
                   r"\in\mathbb{R}^{9}$",
            fontsize=15.5, color=INK, va="center", zorder=6)

    rows = [(r"$\mathbf{p}^{e}=[x\;\;y\;\;z]^{T}$", "ECEF 위치", "m", "3", BLUE),
            (r"$\mathbf{v}^{e}=[v_x\;v_y\;v_z]^{T}$", "ECEF 속도", "m/s", "3", GREEN),
            (r"$\boldsymbol{\Theta}=[\phi\;\;\theta\;\;\psi]^{T}$",
             "자세각 (Roll, Pitch, Yaw)", "rad", "3", AMBER)]
    ax.plot([3, 52], [71.5, 71.5], color=INK, lw=1.1, zorder=3)
    for i, (sym, name, unit, n, c) in enumerate(rows):
        y = 65.0 - i * 8.0
        ax.add_patch(Rectangle((3.0, y - 2.6), 0.8, 5.6, facecolor=c, edgecolor="none",
                               zorder=4))
        ax.text(5.5, y, sym, fontsize=11.5, color=INK, va="center", zorder=6)
        label(ax, 23.0, y, name, size=9.6, color=INK2)
        label(ax, 45.5, y, unit, size=9.0, color=GREY, font=MONO)
        label(ax, 51.0, y, n, size=9.0, color=GREY, font=MONO, ha="right")

    caption(ax, 3, 38.0, "위치와 속도는 ECEF 에서 갱신하고, LLA 는 출력·다음 스텝용으로 함께 보관한다.")

    # 초기 조건 매핑
    head(ax, 3, 30, "명세가 주는 입력 → 초기 상태", 11.5, rule_w=95)
    give = [("위도 · 경도 · 고도", r"$(\varphi_0,\lambda_0,h_0)$", r"$\mathbf{p}^{e}_0=f_{lla\rightarrow ecef}$", BLUE),
            ("자세각", r"$(\phi_0,\theta_0,\psi_0)$", r"$\boldsymbol{\Theta}_0$ (그대로)", AMBER),
            ("동체 속력 1개", r"$V$", r"$\mathbf{v}^{e}_0=\mathbf{C}^{e}_{n}\mathbf{C}^{n}_{b}[V\;0\;0]^{T}$", GREEN)]
    for i, (k, sym, out, c) in enumerate(give):
        x = 3.0 + i * 32.5
        panel(ax, x, 6.0, 30.0, 17.0, fill=PANEL, edge=RULE)
        ax.add_patch(Rectangle((x, 6.0), 30.0, 0.8, facecolor=c, edgecolor="none", zorder=4))
        label(ax, x + 1.8, 19.6, k, size=9.8, color=INK, bold=True)
        ax.text(x + 1.8, 15.2, sym, fontsize=12, color=INK2, va="center", zorder=6)
        arrow(ax, (x + 1.8, 12.6), (x + 5.4, 12.6), color=c, lw=1.2, ms=9)
        ax.text(x + 6.6, 12.6, out, fontsize=10.5, color=INK, va="center", zorder=6)
        if i == 2:
            label(ax, x + 1.8, 8.6, "→ 세 번째 줄이 이 과제의 추가 고민 항목", size=8.4,
                  color=c)
    save(fig, "fig03_state.png")


# ═══════════════════════════════════════════════════════════════════════
# 04. 좌표계 정의 — ECEF / LLA / NED / Body
# ═══════════════════════════════════════════════════════════════════════
def fig04():
    fig, ax = canvas(11.2, 4.8)
    V = Ortho(lat_deg=20.0, lon_deg=24.0)
    cx, cy, R = 23.5, 53.0, 21.0
    LAT_P, LON_P = 36.0, 52.0

    def S(v):
        a, b = V.xy(v)
        return cx + xr(R * a), cy + R * b

    circle(ax, cx, cy, R, facecolor="#F6F8FC", edgecolor=INK2, lw=1.2, zorder=2)

    # 위도선 · 경도선
    for la in (-60, -30, 0, 30, 60):
        pts = [Ortho.sph(la, lo) for lo in np.linspace(0, 360, 180)]
        sphere_curve(ax, V, pts, cx, cy, R, color=INK2 if la == 0 else RULE,
                     lw=1.0 if la == 0 else 0.7, z=3)
    for lo in range(0, 360, 30):
        pts = [Ortho.sph(la, lo) for la in np.linspace(-90, 90, 90)]
        sphere_curve(ax, V, pts, cx, cy, R, color=RULE, lw=0.7, z=3)

    # ECEF 축
    for vec, name, ext in (([1, 0, 0], r"$X_e$", 1.42), ([0, 1, 0], r"$Y_e$", 1.42),
                           ([0, 0, 1], r"$Z_e$", 1.40)):
        a, b = V.xy(vec)
        tipx, tipy = cx + xr(R * ext * a), cy + R * ext * b
        if not V.front(vec):
            ax.plot([cx, tipx], [cy, tipy], color=INK, lw=1.3, ls=(0, (5, 3)), zorder=5)
            arrow(ax, (cx + xr(R * 1.0 * a), cy + R * 1.0 * b), (tipx, tipy),
                  color=INK, lw=1.4, ms=10)
        else:
            arrow(ax, (cx, cy), (tipx, tipy), color=INK, lw=1.5, ms=11, zorder=5)
        n = math.hypot(a, b) or 1.0
        ax.text(cx + xr(R * (ext + 0.16) * a), cy + R * (ext + 0.16) * b, name,
                fontsize=11.5, color=INK, ha="center", va="center", zorder=8)
    ax.plot([cx], [cy], "o", ms=4.2, color=INK, zorder=7)
    ax.text(cx - xr(4.6), cy + 3.2, r"$O_e$", fontsize=10.5, color=INK, ha="center",
            va="center", zorder=8)

    # 점 P, 반경선, 경도호 λ, 위도호 φ
    P = Ortho.sph(LAT_P, LON_P)
    F = Ortho.sph(0.0, LON_P)
    ax.plot(*zip(S([0, 0, 0]), S(P)), color=RED, lw=1.4, zorder=6)
    ax.plot(*zip(S([0, 0, 0]), S(F)), color=GREEN, lw=1.0, ls=(0, (3, 2)), zorder=5)
    px, py = S(P)
    ax.plot([px], [py], "o", ms=6.5, color=RED, zorder=8)
    ax.text(px - xr(3.6), py + 2.6, "P", fontsize=12, color=RED, fontweight="bold",
            ha="center", va="center", zorder=8)

    lam = [Ortho.sph(0.0, lo) * 0.46 for lo in np.linspace(0, LON_P, 60)]
    ax.plot([S(v)[0] for v in lam], [S(v)[1] for v in lam], color=GREEN, lw=1.6, zorder=7)
    mid = Ortho.sph(0.0, LON_P * 0.5) * 0.60
    ax.text(*S(mid), r"$\lambda$", fontsize=13, color=GREEN, ha="center", va="center",
            zorder=8)

    phi = [Ortho.sph(la, LON_P) * 0.72 for la in np.linspace(0, LAT_P, 60)]
    ax.plot([S(v)[0] for v in phi], [S(v)[1] for v in phi], color=BLUE, lw=1.6, zorder=7)
    midp = Ortho.sph(LAT_P * 0.5, LON_P) * 0.86
    ax.text(*S(midp), r"$\varphi$", fontsize=13, color=BLUE, ha="center", va="center",
            zorder=8)

    hx, hy = S(P * 1.34)
    ax.plot([px, hx], [py, hy], color=RED, lw=1.2, ls=(0, (2, 2)), zorder=6)
    arrow(ax, (px, py), (hx, hy), color=RED, lw=1.2, ms=9, ls=(0, (2, 2)))
    ax.text(hx + xr(3.2), hy + 2.2, r"$h$", fontsize=12, color=RED, ha="center",
            va="center", zorder=8)

    label(ax, 2.0, 94.0, "ECEF — 지구중심 지구고정 좌표계", size=11.5, color=INK, bold=True)
    caption(ax, 2.0, 89.0, r"원점 지구중심, $Z_e$ 자전축, $X_e$ 본초자오선. 지구와 함께 돈다.")
    caption(ax, 2.0, 25.0, r"같은 점 P 를 각도로 적은 것이 LLA $(\varphi,\lambda,h)$ — 입·출력 전용")

    # ── 오른쪽: 국지 접평면, NED · Body ──
    ox, oy = 70.0, 55.0
    eN = (0.42, 0.86)
    eE = (0.95, -0.27)
    eD = (0.0, -1.0)
    L = 17.0
    a, b = L * 1.30, L * 1.35
    quad = [(ox + xr(a * eN[0] + b * eE[0]), oy + a * eN[1] + b * eE[1]),
            (ox + xr(-a * eN[0] + b * eE[0]), oy - a * eN[1] + b * eE[1]),
            (ox + xr(-a * eN[0] - b * eE[0]), oy - a * eN[1] - b * eE[1]),
            (ox + xr(a * eN[0] - b * eE[0]), oy + a * eN[1] - b * eE[1])]
    ax.add_patch(Polygon(np.array(quad), closed=True, facecolor="#F6F8FC",
                         edgecolor=RULE, lw=1.0, zorder=2))
    # 접평면 위의 가는 격자
    for t in (-0.6, -0.2, 0.2, 0.6):
        ax.plot([ox + xr(a * eN[0] * t + b * eE[0]), ox + xr(a * eN[0] * t - b * eE[0])],
                [oy + a * eN[1] * t + b * eE[1], oy + a * eN[1] * t - b * eE[1]],
                color="#E9EEF6", lw=0.7, zorder=2)
        ax.plot([ox + xr(a * eN[0] + b * eE[0] * t), ox + xr(-a * eN[0] + b * eE[0] * t)],
                [oy + a * eN[1] + b * eE[1] * t, oy - a * eN[1] + b * eE[1] * t],
                color="#E9EEF6", lw=0.7, zorder=2)

    for e, name, ln, dx in ((eN, r"$N$", L, 0.0), (eE, r"$E$", L, 0.0),
                            (eD, r"$D$", L * 0.72, -3.2)):
        arrow(ax, (ox, oy), (ox + xr(ln * e[0]), oy + ln * e[1]), color=INK, lw=1.6, ms=11)
        ax.text(ox + xr((ln + 3.2) * e[0] + dx), oy + (ln + 3.2) * e[1], name, fontsize=12,
                color=INK, ha="center", va="center", zorder=8)
    ax.plot([ox], [oy], "o", ms=4.2, color=INK, zorder=7)

    psi = 52.0
    c, sn = math.cos(math.radians(psi)), math.sin(math.radians(psi))
    xb = (c * eN[0] + sn * eE[0], c * eN[1] + sn * eE[1])
    yb = (-sn * eN[0] + c * eE[0], -sn * eN[1] + c * eE[1])
    for e, name, ln in ((xb, r"$x_b$", L * 0.86), (yb, r"$y_b$", L * 0.62)):
        arrow(ax, (ox, oy), (ox + xr(ln * e[0]), oy + ln * e[1]),
              color=RED, lw=2.0, ms=11, zorder=6)
        ax.text(ox + xr((ln + 3.4) * e[0]), oy + (ln + 3.4) * e[1], name,
                fontsize=12, color=RED, ha="center", va="center", zorder=8)
    arc_in_plane(ax, (ox, oy), eN, eE, 9.0, 0.0, psi, color=AMBER, lw=1.7)
    mm = math.radians(psi * 0.52)
    ax.text(ox + xr(11.8 * (math.cos(mm) * eN[0] + math.sin(mm) * eE[0])),
            oy + 11.8 * (math.cos(mm) * eN[1] + math.sin(mm) * eE[1]),
            r"$\psi$", fontsize=13.5, color=AMBER, ha="center", va="center", zorder=8)

    label(ax, 49.0, 94.0, "NED · Body — 국지 좌표계", size=11.5, color=INK, bold=True)
    caption(ax, 49.0, 89.0, r"원점은 표적 자신. $N$ 북 · $E$ 동 · $D$ 지심방향, $x_b$ 는 기수 방향.")
    caption(ax, 49.0, 30.0, "국지 접평면 (수평면) 을 위에서 내려다본 그림")
    caption(ax, 49.0, 25.0, r"수평 비행이면 $\phi=\theta=0$ 이라 $x_b$ 와 $N$ 사이 각이 곧 $\psi$ 다")

    # 변환 체인
    panel(ax, 2.0, 2.0, 96.0, 15.0, fill=PANEL, edge=RULE)
    chain = [("LLA", r"$(\varphi,\lambda,h)$", 6.0), ("ECEF", r"$\mathbf{p}^{e}$", 30.0),
             ("NED", r"$\mathbf{p}^{n}$", 54.0), ("Body", r"$\mathbf{p}^{b}$", 78.0)]
    for i, (n, sym, x) in enumerate(chain):
        label(ax, x + 4.0, 12.2, n, size=10.4, color=INK, bold=True, ha="center")
        ax.text(x + 4.0, 6.6, sym, fontsize=11.5, color=INK2, ha="center", va="center",
                zorder=6)
        if i < 3:
            arrow(ax, (x + 11.0, 9.4), (x + 22.0, 9.4), color=INK2, lw=1.2, ms=10)
            dcm = [r"$f_{\rm lla\rightarrow ecef}$", r"$\mathbf{C}^{n}_{e}$",
                   r"$\mathbf{C}^{b}_{n}$"][i]
            ax.text(x + 16.5, 12.4, dcm, fontsize=10.5, color=BLUE, ha="center",
                    va="center", zorder=6)
    save(fig, "fig04_frames.png")


# ═══════════════════════════════════════════════════════════════════════
# 05. 오일러각 정의와 회전 순서
# ═══════════════════════════════════════════════════════════════════════
PLANE_TOP = [(0, 10.0), (1.5, 2.6), (8.5, -0.4), (8.5, -2.4), (1.5, -4.4),
             (3.4, -8.6), (3.4, -10.2), (0, -8.4), (-3.4, -10.2), (-3.4, -8.6),
             (-1.5, -4.4), (-8.5, -2.4), (-8.5, -0.4), (-1.5, 2.6)]
PLANE_FRONT = [(-11, 0.6), (-2.2, 1.4), (-1.6, 3.6), (1.6, 3.6), (2.2, 1.4),
               (11, 0.6), (11, -0.6), (2.0, -1.2), (1.2, -3.2), (-1.2, -3.2),
               (-2.0, -1.2), (-11, -0.6)]
PLANE_SIDE = [(11, 0.0), (6.0, 2.0), (-2.0, 2.4), (-7.0, 2.2), (-9.0, 6.4),
              (-10.6, 6.4), (-10.2, 2.0), (-11.2, 1.2), (-11.2, -1.2),
              (-6.0, -1.8), (-1.0, -3.8), (1.0, -3.8), (2.0, -1.8), (7.0, -1.6)]


def fig05():
    fig, ax = canvas(11.2, 4.4)

    views = [
        ("Roll", r"$\phi$", r"$x_b$ 축 (기수 방향) 둘레", VIOLET, 17.0, PLANE_FRONT,
         "정면에서 본 모습 — 날개가 기운다.\n기수가 향하는 쪽은 그대로다.", 0.34,
         "out", r"$x_b$ 가 화면 밖으로 나온다"),
        ("Pitch", r"$\theta$", r"$y_b$ 축 (오른쪽 날개) 둘레", AMBER, 50.0, PLANE_SIDE,
         "옆에서 본 모습 — 기수가 들린다.\n고도가 바뀌는 회전이다.", 0.26,
         "in", r"$y_b$ 가 화면 안으로 들어간다"),
        ("Yaw", r"$\psi$", r"$z_b$ 축 (아래 방향) 둘레", BLUE, 83.0, PLANE_TOP,
         "위에서 본 모습 — 기수가 좌우로 돈다.\n수평 선회가 이 축이다.", 0.44,
         "in", r"$z_b$ 가 화면 안으로 들어간다"),
    ]
    for name, sym, axis_txt, c, cx, shape, note, rot, mark, mark_txt in views:
        panel(ax, cx - 15.5, 30.0, 31.0, 63.0, fill=WHITE, edge=RULE)
        label(ax, cx, 88.0, "%s  %s" % (name, sym), size=13.5, color=c, bold=True,
              ha="center")
        caption(ax, cx, 82.5, axis_txt, size=9.2, ha="center")

        # 회전 전 기준선은 파선 한 줄로, 회전 후 자세만 채워 그린다
        ref = 13.0
        if shape is PLANE_TOP:
            ax.plot([cx, cx], [60.0 - ref * 0.82, 60.0 + ref], color="#9FAABE", lw=1.1,
                    ls=(0, (5, 3)), zorder=3)
        else:
            ax.plot([cx - xr(ref), cx + xr(ref)], [60.0, 60.0], color="#9FAABE", lw=1.1,
                    ls=(0, (5, 3)), zorder=3)
        polyx(ax, cx, 60.0, shape, s=1.12, rot=rot, facecolor=c, edgecolor="none", zorder=5)

        arcp(ax, cx, 60.0, 14.8, 30.0, 74.0, color=c, lw=1.7)
        arrow(ax, polar(cx, 60.0, 14.8, 66.0), polar(cx, 60.0, 14.8, 75.0),
              color=c, lw=1.7, ms=12)
        ax.text(*polar(cx, 60.0, 19.0, 52.0), sym, fontsize=13.5, color=c, ha="center",
                va="center", zorder=7)

        circle(ax, cx, 60.0, 1.7, facecolor=WHITE, edgecolor=c, lw=1.5, zorder=8)
        if mark == "out":
            circle(ax, cx, 60.0, 0.55, facecolor=c, edgecolor="none", zorder=9)
        else:
            d = 1.20
            ax.plot([cx - xr(d), cx + xr(d)], [60.0 - d, 60.0 + d], color=c, lw=1.3, zorder=9)
            ax.plot([cx - xr(d), cx + xr(d)], [60.0 + d, 60.0 - d], color=c, lw=1.3, zorder=9)
        caption(ax, cx, 47.5, mark_txt, size=8.8, ha="center")
        ax.plot([cx - 11.0, cx + 11.0], [43.5, 43.5], color=RULE, lw=0.7, zorder=3)
        caption(ax, cx, 37.8, note, size=9.2, ha="center")

    panel(ax, 2.0, 3.0, 96.0, 22.0, fill=PANEL, edge=RULE)
    label(ax, 4.0, 20.5, "동체 → NED 회전행렬은 세 회전의 곱이다", size=10.8, color=INK,
          bold=True)
    ax.text(4.0, 12.0, r"$\mathbf{C}^{n}_{b}(\phi,\theta,\psi)"
                       r"=\mathbf{R}_{z}(\psi)\,\mathbf{R}_{y}(\theta)\,\mathbf{R}_{x}(\phi)$",
            fontsize=15.5, color=INK, va="center", zorder=6)
    ax.plot([45.0, 45.0], [5.5, 18.5], color=RULE, lw=0.9, zorder=3)
    label(ax, 48.0, 18.0, "순서가 바뀌면 다른 자세가 된다 — Yaw → Pitch → Roll 로 고정",
          size=9.4, color=INK2)
    label(ax, 48.0, 13.0, "제공받은 f_Trans_Body_To_Ned 는 인자를 (Roll, Yaw, Pitch) 로 받는다",
          size=9.4, color=RED)
    label(ax, 48.0, 8.0, "구조체 필드 순서(Roll, Pitch, Yaw)와 달라 호출부마다 주석을 남겼다",
          size=9.4, color=INK2)
    save(fig, "fig05_euler.png")




# ═══════════════════════════════════════════════════════════════════════
# 06. 기동 파라미터 정의
# ═══════════════════════════════════════════════════════════════════════
def fig06():
    fig, ax = canvas(11.2, 4.5)

    head(ax, 2, 92, "기동 1건 = 값 네 개", 12, rule_w=44)
    fields = [("enTurnType", "EN_TurnType", "0 없음 / 1 Roll / 2 Yaw / 3 Pitch", VIOLET),
              ("gravityValue", "FLOAT64  [G]", "크기는 세기, 부호는 회전 방향", AMBER),
              ("startTime", "FLOAT64  [s]", "이 시각부터 (포함)", GREEN),
              ("endTime", "FLOAT64  [s]", "이 시각까지 (제외)", GREEN)]
    ax.plot([2, 46], [85.0, 85.0], color=INK, lw=1.1, zorder=3)
    for i, (n, t, d, c) in enumerate(fields):
        y = 79.0 - i * 9.6
        ax.add_patch(Rectangle((2.0, y - 2.8), 0.7, 6.4, facecolor=c, edgecolor="none",
                               zorder=4))
        label(ax, 4.0, y + 1.9, n, size=9.8, color=INK, font=MONO, bold=True)
        label(ax, 24.0, y + 1.9, t, size=8.8, color=GREY, font=MONO)
        label(ax, 4.0, y - 1.6, d, size=8.8, color=INK2)
        ax.plot([2, 46], [y - 4.6, y - 4.6], color=RULE, lw=0.7, zorder=2)

    panel(ax, 2.0, 22.0, 44.0, 16.0, fill=PANEL, edge=RULE)
    label(ax, 4.0, 34.0, "구간 규칙", size=10.2, color=INK, bold=True)
    ax.text(4.0, 29.0, r"$t\in[\,t_{start},\,t_{end})$", fontsize=12.5, color=INK,
            va="center", zorder=6)
    label(ax, 22.0, 29.0, "끝과 다음 시작이 같아도 겹침이 아니다", size=8.8, color=INK2)
    label(ax, 4.0, 24.8, "같은 표적에서 두 구간이 실제로 겹치면 "
                         "TGT_ERR_MANEUVER_OVERLAP", size=8.6, color=RED)

    caption(ax, 2.0, 16.0, "표적 하나에 최대 30줄. 걸린 구간에서만 자세각이 돌고, 나머지 시간은 직진이다.")
    caption(ax, 2.0, 10.0, "기동 지시는 보통 \"위도 32.1 에서 좌 90도\" 처럼 좌표로 온다. 구조체에는 시각 칸만")
    caption(ax, 2.0, 5.5, "있으므로 Core 를 실제로 돌려 그 좌표에 닿는 시각을 찾아 start / end 로 옮겼다.")

    # 오른쪽: 기동표가 실제 응답으로 나타난 모습
    rows = read_csv("demo.csv")
    t = col(rows, "time")
    yaw = col(rows, "t2_yaw") % 360.0
    sub = fig.add_axes([0.505, 0.155, 0.465, 0.62])
    sub.plot(t, yaw, color=RED, lw=1.8, zorder=5)
    mnv = [(2.8, 6.7, "-8.21"), (13.3, 17.2, "+8.21"), (18.6, 22.5, "+8.21"),
           (36.2, 40.1, "-8.21"), (41.5, 45.4, "-8.21"), (53.5, 60.0, "+8.21")]
    for a, b, g in mnv:
        sub.axvspan(a, b, color="#FDE7E7", zorder=1)
        sub.annotate(g, xy=((a + b) / 2.0, 340.0), ha="center", va="center", fontsize=7.4,
                     color=RED, zorder=6)
    sub.set_xlim(0, 60)
    sub.set_ylim(0, 375)
    sub.set_yticks([0, 90, 180, 270, 360])
    sub.set_xlabel("시각 [s]", fontsize=9)
    sub.set_ylabel("기수 방위 " + r"$\psi$" + " [°]", fontsize=9)
    sub.grid(True, lw=0.6)
    sub.set_title("기동표 6줄이 실제 응답으로 나타난 모습  (대공 표적)",
                  fontsize=10, color=INK, pad=7)
    sub.tick_params(labelsize=8)
    for sp in sub.spines.values():
        sp.set_color("#9AA7BD")
    sub.annotate("직진 구간 — 기울기 0", xy=(28.0, 270.0), xytext=(25.5, 205.0),
                 fontsize=8.2, color=INK2,
                 arrowprops=dict(arrowstyle="->", color=INK2, lw=0.9))
    sub.annotate("기울기 = " + r"$\omega=ng/V$", xy=(15.2, 135.0), xytext=(4.0, 250.0),
                 fontsize=8.2, color=INK2,
                 arrowprops=dict(arrowstyle="->", color=INK2, lw=0.9))
    label(ax, 52.0, 6.0, "색칠한 구간이 기동표에 적은 [start, end) 이고, 그 안에서만 "
                         "기울기가 생긴다.", size=9.0, color=GREY)
    save(fig, "fig06_maneuver.png")


# ═══════════════════════════════════════════════════════════════════════
# 07. G 값 → 각속도 → 선회 반경
# ═══════════════════════════════════════════════════════════════════════
def fig07():
    fig, ax = canvas(11.2, 4.7)

    # ── 왼쪽: 기하 ──
    head(ax, 2, 93, "수평 선회의 기하", 11.5, rule_w=40)
    cx, cy, R = 22.0, 52.0, 22.0
    arcp(ax, cx, cy, R, 8.0, 172.0, color=RULE, lw=1.3, ls=(0, (5, 4)))
    ax.plot([cx], [cy], "o", ms=4.5, color=INK2, zorder=6)
    caption(ax, cx, cy - 4.2, "선회 중심", size=8.8, ha="center")

    a0 = 68.0
    px, py = polar(cx, cy, R, a0)
    ax.plot([cx, px], [cy, py], color=INK2, lw=1.1, ls=(0, (3, 2)), zorder=5)
    ax.text(*polar(cx, cy, R * 0.60, a0 + 10.0), r"$R$", fontsize=13, color=INK2,
            ha="center", va="center", zorder=7)
    ax.plot([px], [py], "o", ms=7.0, color=RED, zorder=7)

    tang = math.radians(a0 + 90.0)
    tipx, tipy = px + xr(14.0 * math.cos(tang)), py + 14.0 * math.sin(tang)
    arrow(ax, (px, py), (tipx, tipy), color=RED, lw=2.1, ms=12)
    ax.text(tipx - xr(3.0), tipy + 3.2, r"$V$", fontsize=14, color=RED, ha="center",
            va="center", zorder=7)
    cen = math.radians(a0 + 180.0)
    ax.text(px + xr(6.0 * math.cos(cen)) + xr(11.0), py + 6.0 * math.sin(cen),
            r"$a_c=n\,g$", fontsize=12.5, color=AMBER, ha="left", va="center", zorder=7)
    arrow(ax, (px, py), (px + xr(11.0 * math.cos(cen)), py + 11.0 * math.sin(cen)),
          color=AMBER, lw=2.1, ms=12)

    a1 = a0 - 32.0
    qx, qy = polar(cx, cy, R, a1)
    ax.plot([cx, qx], [cy, qy], color=RULE, lw=1.0, ls=(0, (3, 2)), zorder=4)
    arcp(ax, cx, cy, R * 0.34, a1, a0, color=BLUE, lw=1.6)
    ax.text(*polar(cx, cy, R * 0.56, (a0 + a1) / 2.0 - 4.0), r"$\Delta\psi$",
            fontsize=12, color=BLUE, ha="center", va="center", zorder=7)
    caption(ax, 2, 18.0, r"등속 원운동이므로 속도 $V$ 는 접선 방향,")
    caption(ax, 2, 13.0, r"가속도 $a_c$ 는 언제나 중심을 향한다.")
    caption(ax, 2, 6.0, "이 그림 한 장에서 오른쪽 세 줄이 모두 나온다.")

    # ── 오른쪽 위: 유도 ──
    head(ax, 47, 93, "G 값에서 각속도와 선회 반경이 나온다", 11.5, rule_w=51)
    lines = [(r"$a_c=\dfrac{V^{2}}{R}=V\,\omega$", "원운동의 구심가속도", 12.5),
             (r"$a_c=n\,g,\qquad g=9.80665\ {\rm m/s^{2}}$",
              "기동 세기를 중력가속도의 n 배로 적는다", 12.5),
             (r"$\omega=\dfrac{n\,g}{V},\qquad R=\dfrac{V^{2}}{n\,g}$",
              "G 값 하나가 회전 속도와 반경을 함께 정한다", 14.0)]
    for i, (eq, note, sz) in enumerate(lines):
        y = 85.0 - i * 12.5
        ax.text(48.5, y, eq, fontsize=sz, color=INK, va="center", zorder=6)
        caption(ax, 48.5, y - 6.6, note, size=8.8)
    label(ax, 47.0, 47.0, "코드에서는 한 줄이다  —", size=9.4, color=INK, bold=True)
    label(ax, 66.5, 47.0, "omega = (gravityValue * G_FORCE) / headingSpeed;",
          size=8.6, color=BLUE, font=MONO)

    # ── 오른쪽 아래: 수치표 ──
    ax.plot([47, 98], [41.0, 41.0], color=RULE, lw=0.9, zorder=3)
    label(ax, 47.0, 36.0, "본 과제 두 표적에 G 를 걸면", size=10.4, color=INK, bold=True)
    xs = [47.0, 58.0, 78.0]
    for x, hh in zip(xs, ["n [G]", "대함  V = 30 m/s", "대공  V = 200 m/s"]):
        label(ax, x, 29.0, hh, size=9.0, color=INK, bold=True)
    ax.plot([47, 98], [26.0, 26.0], color=INK, lw=1.0, zorder=3)
    for i, n in enumerate((1.0, 2.0, 4.0, 8.0)):
        y = 21.0 - i * 5.0
        label(ax, xs[0], y, "%g" % n, size=9.0, color=INK, font=MONO, bold=True)
        for j, V in enumerate((30.0, 200.0)):
            w = math.degrees(n * 9.80665 / V)
            Rr = V * V / (n * 9.80665)
            label(ax, xs[1 + j], y, "%5.1f °/s      R = %s m" %
                  (w, ("%.0f" % Rr) if Rr >= 100 else ("%.1f" % Rr)),
                  size=8.8, color=INK2, font=MONO)
        if i < 3:
            ax.plot([47, 98], [y - 2.5, y - 2.5], color=RULE, lw=0.6, zorder=2)
    caption(ax, 47.0, 1.5, "같은 G 라도 빠른 표적은 덜 꺾인다 — 분모에 속력이 있기 때문이다.")
    save(fig, "fig07_gforce.png")


# ═══════════════════════════════════════════════════════════════════════
# 08. 회전축 세 가지의 응답 (실측)
# ═══════════════════════════════════════════════════════════════════════
def fig08():
    rows = read_csv("turn.csv")
    t = col(rows, "time")
    fig = plt.figure(figsize=(11.2, 4.3))
    gs = fig.add_gridspec(1, 3, width_ratios=[1.0, 1.0, 0.92], wspace=0.30,
                          left=0.055, right=0.985, top=0.86, bottom=0.145)
    names = [(1, "Yaw", BLUE), (2, "Pitch", AMBER), (3, "Roll", VIOLET)]

    ax = fig.add_subplot(gs[0, 0])
    for i, nm, c in names:
        e, n = to_en(col(rows, "t%d_lat" % i), col(rows, "t%d_lon" % i), 32.0, 126.0)
        ax.plot(e / 1000.0, n / 1000.0, color=c, lw=2.0, label=nm, zorder=5)
    ax.plot([0], [0], "^", ms=9, color=PF, zorder=6)
    ax.set_xlabel("동쪽 [km]", fontsize=9.5)
    ax.set_ylabel("북쪽 [km]", fontsize=9.5)
    ax.set_title("지상 궤적 (위에서 본 모습)", fontsize=10.2, color=INK, pad=7)
    ax.grid(True)
    ax.legend(fontsize=8.4, loc="upper left")
    ax.set_aspect("equal", adjustable="datalim")
    ax.annotate("Pitch · Roll 은 겹쳐 있다", xy=(0.0, 5.0), xytext=(-7.6, 3.2),
                fontsize=8.2, color=INK2,
                arrowprops=dict(arrowstyle="->", color=INK2, lw=0.9))

    ax = fig.add_subplot(gs[0, 1])
    for i, nm, c in names:
        ax.plot(t, col(rows, "t%d_alt" % i) / 1000.0, color=c, lw=2.0, zorder=5)
    ax.axvspan(10, 20, color="#EDF1F8", zorder=1)
    ax.text(15, 7.9, "기동 구간\n2 G · 10 s", ha="center", fontsize=8.2, color=INK2,
            linespacing=1.5)
    ax.set_xlabel("시각 [s]", fontsize=9.5)
    ax.set_ylabel("고도 [km]", fontsize=9.5)
    ax.set_title("고도 변화", fontsize=10.2, color=INK, pad=7)
    ax.grid(True)
    ax.annotate("+7.55 km", xy=(60, 7.85), xytext=(40, 6.2), fontsize=8.6, color=AMBER,
                fontweight="bold",
                arrowprops=dict(arrowstyle="->", color=AMBER, lw=1.0))
    ax.annotate("0.000 m", xy=(45, 0.30), xytext=(30, 1.7), fontsize=8.6, color=BLUE,
                arrowprops=dict(arrowstyle="->", color=BLUE, lw=1.0))

    ax = fig.add_subplot(gs[0, 2])
    ax.axis("off")
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100)
    ax.text(0, 97, "같은 표적 · 같은 2 G · 같은 10 s, 축만 바꿨다", fontsize=9.6,
            color=INK, va="top")
    facts = [("Yaw", BLUE, r"$\psi$ 가 118° 돌고 궤적이 휜다.",
              "고도는 300.000 m 그대로."),
             ("Pitch", AMBER, r"$\theta$ 가 들려 7.55 km 상승.",
              "지상 궤적은 거의 제자리."),
             ("Roll", VIOLET, r"$\phi$ 만 변하고 궤적은 불변.",
              "회전축이 곧 속도 방향이라 그렇다.")]
    for k, (nm, c, l1, l2) in enumerate(facts):
        y = 80 - k * 25
        ax.add_patch(Rectangle((0, y - 11), 1.9, 17, facecolor=c, edgecolor="none"))
        ax.text(5, y + 3.5, nm, fontsize=10.8, fontweight="bold", color=c)
        ax.text(5, y - 2.0, l1, fontsize=9.0, color=INK2)
        ax.text(5, y - 8.0, l2, fontsize=9.0, color=GREY)
    ax.text(0, 4, "Roll 이 궤적을 못 바꾸는 것은 구현 오류가 아니라\n"
                  "물리적으로 옳은 결과다.", fontsize=8.8, color=INK2, linespacing=1.6)

    fig.savefig(os.path.join(OUT, "fig08_turntype.png"), dpi=DPI, bbox_inches="tight",
                pad_inches=0.06)
    plt.close(fig)
    print("  fig08_turntype.png")


# ═══════════════════════════════════════════════════════════════════════
# 09. 구조체 관계도
# ═══════════════════════════════════════════════════════════════════════
def _uml(ax, x, y, w, title, fields, accent=BLUE, fh=4.0, note=None):
    h = 6.2 + len(fields) * fh + (3.6 if note else 0.0)
    panel(ax, x, y - h, w, h, fill=WHITE, edge=RULE, lw=1.1)
    ax.add_patch(Rectangle((x, y - 6.0), w, 6.0, facecolor=accent, edgecolor="none",
                           zorder=3))
    label(ax, x + 1.6, y - 3.0, title, size=9.4, color=WHITE, font=MONO, bold=True)
    for i, (n, t, c) in enumerate(fields):
        yy = y - 8.8 - i * fh
        label(ax, x + 1.6, yy, n, size=8.4, color=INK, font=MONO)
        label(ax, x + w * 0.46, yy, t, size=8.0, color=GREY, font=MONO)
        if c:
            label(ax, x + w - 1.6, yy, c, size=8.0, color=accent, ha="right", font=MONO)
    if note:
        label(ax, x + 1.6, y - h + 1.9, note, size=7.8, color=GREY)
    return h


def fig09():
    fig, ax = canvas(11.2, 5.3)

    label(ax, 24.0, 97.3, "입력 — 시나리오가 채운다", size=11, color=BLUE, bold=True,
          ha="center")
    label(ax, 74.0, 97.3, "출력 — Core 가 채운다", size=11, color=GREEN, bold=True,
          ha="center")
    ax.plot([2, 47], [94.5, 94.5], color=BLUE, lw=1.0, zorder=3)
    ax.plot([51, 98], [94.5, 94.5], color=GREEN, lw=1.0, zorder=3)

    h1 = _uml(ax, 2.0, 92.0, 45.0, "ST_SimConfig",
              [("durationTime", "FLOAT64 [s]", ""),
               ("stepTime", "FLOAT64 [s]", ""),
               ("st_Platform", "ST_PlatformInit", "1"),
               ("nTargetNum", "INT32", "1..10"),
               ("st_Target[10]", "ST_TargetInit", "10")])
    h2 = _uml(ax, 8.0, 92.0 - h1 - 1.5, 39.0, "ST_TargetInit",
              [("st_InitLla", "STRUCT_Coord_Lla", "rad, m"),
               ("st_InitAtt", "STRUCT_Coord_Attitude", "rad"),
               ("headingSpeed", "FLOAT64 [m/s]", "> 0"),
               ("nManeuverNum", "INT32", "0..30"),
               ("st_Maneuver[30]", "ST_TargetManeuver", "30")])
    _uml(ax, 14.0, 92.0 - h1 - h2 - 3.0, 33.0, "ST_TargetManeuver",
         [("enTurnType", "EN_TurnType", "0..3"),
          ("gravityValue", "FLOAT64 [G]", "±"),
          ("startTime / endTime", "FLOAT64 [s]", "[a, b)")], accent=VIOLET)

    g1 = _uml(ax, 51.0, 92.0, 47.0, "ST_SimState",
              [("nStepNum", "INT32", "600"),
               ("st_Sample", "ST_SimSample", "최근 1"),
               ("st_Config", "ST_SimConfig", "설정 사본")], accent=GREEN,
              note="설정을 복사해 두어 호출 중 원본이 바뀌어도 진행이 흔들리지 않는다")
    g2 = _uml(ax, 57.0, 92.0 - g1 - 1.5, 41.0, "ST_SimSample",
              [("simTime", "FLOAT64 [s]", ""),
               ("nStepIndex", "INT32", "0..600"),
               ("st_Platform", "ST_TargetState", "1"),
               ("st_Target[10]", "ST_TargetState", "10")], accent=GREEN)
    _uml(ax, 63.0, 92.0 - g1 - g2 - 3.0, 35.0, "ST_TargetState",
         [("st_PosEcef", "STRUCT_Coord_Rect [m]", "갱신"),
          ("st_VelEcef", "STRUCT_Coord_Rect [m/s]", "갱신"),
          ("st_Lla", "STRUCT_Coord_Lla", "출력용"),
          ("st_Att", "STRUCT_Coord_Attitude", "갱신")], accent=GREEN)

    arrow(ax, (47.6, 78.0), (50.4, 78.0), color=INK, lw=1.7, ms=12)
    ax.text(49.0, 58.0, "f_Tgt_InitSim  /  f_Tgt_StepSim", fontsize=8.0,
            color=INK, family=MONO, fontweight="bold", rotation=90,
            ha="center", va="center", zorder=7)

    panel(ax, 2.0, 3.0, 96.0, 12.5, fill=PANEL, edge=RULE)
    label(ax, 4.0, 12.6, "설계 결정 세 가지", size=10.0, color=INK, bold=True)
    for i, (k, v) in enumerate([
            ("입력 / 출력 분리", "설정은 한 번만 읽고, 스텝마다 바뀌는 것은 상태뿐이다"),
            ("고정 배열", "10 × 30 을 배열로 박아 실행 중 메모리 할당이 실패할 자리를 없앴다"),
            ("제공 구조체 재사용", "STRUCT_Coord_Lla / _Attitude 는 좌표변환 헤더의 것을 그대로 쓴다")]):
        y = 9.2 - i * 2.4
        label(ax, 5.5, y, "·", size=9, color=BLUE, bold=True)
        label(ax, 7.5, y, k, size=8.8, color=INK, bold=True)
        label(ax, 26.0, y, v, size=8.8, color=INK2)
    save(fig, "fig09_struct.png")


# ═══════════════════════════════════════════════════════════════════════
# 10. 운동학 모델 — 지배 방정식
# ═══════════════════════════════════════════════════════════════════════
def fig10():
    fig, ax = canvas(11.2, 4.2)

    head(ax, 2, 93, "표적 운동을 지배하는 세 줄", 12, rule_w=58)
    eqs = [(r"$\dot{\mathbf{p}}^{e}=\mathbf{v}^{e}$",
            "위치의 시간 변화율이 곧 속도다", BLUE),
           (r"$\mathbf{v}^{e}=\mathbf{C}^{e}_{n}(\varphi,\lambda)\,"
            r"\mathbf{C}^{n}_{b}(\phi,\theta,\psi)\,[\,V\ \ 0\ \ 0\,]^{T}$",
            "속도는 기수 방향 크기 V 를 두 번 돌려 만든다", GREEN),
           (r"$\dot{\boldsymbol{\Theta}}=\boldsymbol{\omega}(t),\qquad"
            r"\omega_i=n_i\,g\,/\,V$",
            "자세각은 기동표가 지시하는 각속도로 돈다 (기동이 걸린 구간에서만 0 이 아니다)",
            AMBER)]
    for i, (eq, note, c) in enumerate(eqs):
        y = 84.0 - i * 16.5
        ax.add_patch(Rectangle((2.0, y - 4.6), 0.8, 10.0, facecolor=c, edgecolor="none",
                               zorder=4))
        ax.text(4.5, y + 1.6, eq, fontsize=14, color=INK, va="center", zorder=6)
        caption(ax, 4.5, y - 3.2, note, size=9.0)

    ax.plot([2, 98], [31.0, 31.0], color=RULE, lw=0.9, zorder=3)

    label(ax, 2.0, 26.0, "이 모델이 전제하는 것", size=10.4, color=INK, bold=True)
    asm = [("운동학 모델", "힘이 아니라 속도 · 각속도를 직접 준다.",
            "중력 · 추력 · 항력을 풀지 않으므로 탄도 표적에는 맞지 않는다."),
           ("속력 일정", "회전만 하므로 |v| 는 변하지 않는다.",
            "가속 · 감속이 필요하면 V 를 시간 함수로 바꿔야 한다."),
           ("오일러각 적분", "자세각을 직접 적분한다.",
            r"$\theta\to\pm 90°$ 에서 짐벌락 — 사원수로 바꾸면 풀린다.")]
    for i, (k, v1, v2) in enumerate(asm):
        x = 2.0 + i * 32.6
        panel(ax, x, 2.0, 30.6, 19.0, fill=PANEL, edge=RULE)
        label(ax, x + 1.8, 17.2, k, size=9.8, color=INK, bold=True)
        ax.plot([x + 1.8, x + 28.8], [14.8, 14.8], color=RULE, lw=0.8, zorder=3)
        label(ax, x + 1.8, 11.2, v1, size=8.8, color=INK2)
        ax.text(x + 1.8, 6.0, v2, fontsize=8.6, color=GREY, va="center", zorder=6)
    save(fig, "fig10_model.png")


# ═══════════════════════════════════════════════════════════════════════
# 11. 왜 ECEF 에서 적분하는가
# ═══════════════════════════════════════════════════════════════════════
def fig11():
    fig, ax = canvas(11.2, 4.5)

    # 왼쪽: LLA 직접 적분의 문제
    panel(ax, 2.0, 30.0, 46.0, 58.0, fill="#FDF7F7", edge="#F0D8D8")
    label(ax, 25.0, 82.0, "LLA 에서 직접 적분하면", size=11.5, color="#B03B3B", bold=True,
          ha="center")
    ax.text(5.0, 72.0, r"$\dot{\varphi}=\dfrac{v_N}{M(\varphi)+h},\qquad"
                       r"\dot{\lambda}=\dfrac{v_E}{(N(\varphi)+h)\cos\varphi}$",
            fontsize=12.5, color="#8A3232", va="center", zorder=6)
    caption(ax, 5.0, 64.5, r"분모의 곡률반경 $M,N$ 이 위도마다 달라 매 스텝 다시 구해야 한다",
            size=8.8)

    cxx, cyy = 25.0, 45.0
    th = np.linspace(math.radians(133), math.radians(47), 160)
    ax.plot(cxx + xr(62.0 * np.cos(th)), cyy - 49.0 + 62.0 * np.sin(th),
            color="#C98C8C", lw=1.4, zorder=4)
    caption(ax, 5.0, 36.5, "지면(곡면)", size=8.4)
    ax.plot([cxx - xr(17.0), cxx + xr(17.0)], [cyy + 6.0, cyy + 6.0],
            color="#B03B3B", lw=1.8, ls=(0, (5, 3)), zorder=5)
    arrow(ax, (cxx - xr(17.0), cyy + 6.0), (cxx + xr(17.0), cyy + 6.0),
          color="#B03B3B", lw=1.8, ms=11)
    ax.plot([cxx + xr(17.0), cxx + xr(17.0)], [cyy + 6.0, cyy + 1.0], color="#B03B3B",
            lw=1.0, ls=(0, (2, 2)), zorder=5)
    ax.text(cxx + xr(19.0), cyy + 3.5, "오차", fontsize=9, color="#B03B3B",
            ha="left", va="center", zorder=6)
    caption(ax, 5.0, 31.5, r"곧게 날려도 스텝마다 고도가 $d^{2}/2R$ 씩 올라간다", size=9.0)

    # 오른쪽: ECEF 적분
    panel(ax, 52.0, 30.0, 46.0, 58.0, fill="#F4FAF7", edge="#CDE7DA")
    label(ax, 75.0, 82.0, "ECEF 에서 적분하면", size=11.5, color="#0B6B4B", bold=True,
          ha="center")
    ax.text(55.0, 71.0, r"$\mathbf{p}^{e}_{k+1}=\mathbf{p}^{e}_{k}"
                        r"+\mathbf{v}^{e}\,\Delta t$",
            fontsize=15, color="#0B6B4B", va="center", zorder=6)
    caption(ax, 55.0, 63.0, "미터 단위 직교좌표라 그냥 더하면 된다", size=9.0)
    caption(ax, 55.0, 58.0, "지구가 둥근 것은 좌표계가 이미 품고 있다", size=9.0)

    panel(ax, 55.0, 35.0, 40.0, 17.0, fill=WHITE, edge="#CDE7DA")
    label(ax, 57.0, 47.5, "대신 치러야 하는 값", size=9.6, color=INK, bold=True)
    label(ax, 57.0, 43.0, "· 사람이 못 읽는다 → 출력 때 LLA 로 되돌린다", size=8.8, color=INK2)
    label(ax, 57.0, 39.0, "· 속도를 만들 때는 그 지점의 위·경도가 필요하다", size=8.8, color=INK2)

    # 아래: 수치 근거
    ax.plot([2, 98], [25.0, 25.0], color=RULE, lw=0.9, zorder=3)
    label(ax, 2.0, 20.5, "본 과제 조건에서 실제로 얼마인가", size=10.4, color=INK, bold=True)
    d = 200.0 * 0.1
    R_e = 6378137.0
    drift = d * d / (2.0 * R_e)
    items = [(r"한 스텝 이동거리 $d=V\Delta t$", r"$200\times 0.1=20$ m"),
             (r"스텝당 고도 편차 $d^{2}/2R$", r"$%.2f\ \mu$m" % (drift * 1e6)),
             ("600 스텝 누적 (보정 없이)", r"$%.2f$ mm" % (drift * 600.0 * 1e3)),
             ("중점법으로 보정한 실측", r"고도 $300.000000$ m 고정")]
    for i, (k, v) in enumerate(items):
        x = 2.0 + i * 24.5
        ax.text(x, 13.0, k, fontsize=8.8, color=INK2, va="center", zorder=6)
        ax.text(x, 7.5, v, fontsize=11.5, color=GREEN if i == 3 else INK, va="center",
                zorder=6)
        if i < 3:
            ax.plot([x + 22.5, x + 22.5], [4.0, 16.0], color=RULE, lw=0.7, zorder=2)
    caption(ax, 2.0, 1.8, "값 자체는 작지만, 이런 항이 남아 있으면 갱신 간격을 줄여도 오차가 "
                          "선형으로만 줄어든다 — 2차 수렴이 깨진다.")
    save(fig, "fig11_whyecef.png")


# ═══════════════════════════════════════════════════════════════════════
# 12. 동체 속도 → ECEF 속도
# ═══════════════════════════════════════════════════════════════════════
def fig12():
    fig, ax = canvas(11.2, 4.4)

    head(ax, 2, 93, "동체 속도를 ECEF 속도로 — 회전 두 번", 12, rule_w=96)

    steps = [("동체 좌표  " + r"$b$", r"$[\,V\ \ 0\ \ 0\,]^{T}$",
              "기수 방향으로 V.\n옆도 아래도 0 이다.", RED, 2.0),
             ("NED 좌표  " + r"$n$", r"$[\,v_N\ \ v_E\ \ v_D\,]^{T}$",
              "자세각 " + r"$(\phi,\theta,\psi)$" + " 로\n한 번 돌린다.", AMBER, 35.5),
             ("ECEF 좌표  " + r"$e$", r"$[\,v_x\ \ v_y\ \ v_z\,]^{T}$",
              "그 지점의 " + r"$(\varphi,\lambda)$" + " 로\n한 번 더 돌린다.", GREEN, 69.0)]
    for name, val, desc, c, x in steps:
        panel(ax, x, 40.0, 29.0, 42.0, fill=WHITE, edge=c, lw=1.6)
        ax.add_patch(Rectangle((x, 76.0), 29.0, 6.0, facecolor=c, edgecolor="none",
                               zorder=3))
        label(ax, x + 14.5, 79.0, name, size=10.6, color=WHITE, bold=True, ha="center")
        ax.text(x + 14.5, 65.0, val, fontsize=13.5, color=INK, ha="center", va="center",
                zorder=6)
        ax.text(x + 14.5, 51.0, desc, fontsize=9.2, color=INK2, ha="center", va="center",
                zorder=6, linespacing=1.6)

    for x0, dcm in ((31.5, r"$\mathbf{C}^{n}_{b}$"), (65.0, r"$\mathbf{C}^{e}_{n}$")):
        arrow(ax, (x0, 61.0), (x0 + 3.0, 61.0), color=INK, lw=2.0, ms=13)
        ax.text(x0 + 1.5, 68.0, dcm, fontsize=13, color=INK, ha="center", va="center",
                zorder=7)

    ax.text(2.0, 32.0, r"$\mathbf{v}^{e}=\mathbf{C}^{e}_{n}(\varphi,\lambda)\;"
                       r"\mathbf{C}^{n}_{b}(\phi,\theta,\psi)\;[\,V\ \ 0\ \ 0\,]^{T}$",
            fontsize=15, color=INK, va="center", zorder=6)
    ax.plot([48.0, 48.0], [22.0, 38.0], color=RULE, lw=0.9, zorder=3)
    label(ax, 51.0, 35.0, "두 번 다 회전행렬이다 — 직교행렬이므로 크기를 바꾸지 않는다",
          size=9.4, color=INK2)
    ax.text(51.0, 29.0, r"$\|\mathbf{v}^{e}\|=\|\mathbf{C}^{e}_{n}\mathbf{C}^{n}_{b}"
                        r"[\,V\,0\,0]^{T}\|=V$", fontsize=11.5, color=GREEN, va="center",
            zorder=6)
    label(ax, 51.0, 23.5, "그래서 속력 보존은 증명된 성질이고, 시험은 구현 확인용이다",
          size=9.0, color=GREY)

    panel(ax, 2.0, 3.0, 96.0, 15.0, fill=PANEL, edge=RULE)
    label(ax, 4.0, 14.0, "구현에서 주의한 두 가지", size=10.0, color=INK, bold=True)
    label(ax, 4.0, 9.4, "① 평행이동 제거", size=9.2, color=RED, bold=True)
    label(ax, 17.0, 9.4, "f_Trans_Ned_To_Ecef 는 회전에 더해 기준점만큼 평행이동까지 한다. "
                         "속도는 위치가 아니라 방향이므로 기준점 자리에 (0, 0, 0) 을 넣어 회전만 남겼다.",
          size=8.8, color=INK2)
    label(ax, 4.0, 5.4, "② 인자 순서", size=9.2, color=RED, bold=True)
    label(ax, 17.0, 5.4, "f_Trans_Body_To_Ned 는 (Roll, Yaw, Pitch) 순서로 받는다. "
                         "자세각 구조체 필드 순서(Roll, Pitch, Yaw)와 달라 호출부에 주석을 남겼다.",
          size=8.8, color=INK2)
    save(fig, "fig12_velocity.png")


# ═══════════════════════════════════════════════════════════════════════
# 13. 회전행렬 원소
# ═══════════════════════════════════════════════════════════════════════
def fig13():
    fig, ax = canvas(11.2, 4.5)

    label(ax, 2.0, 94.0, "① 동체 → NED", size=11.5, color=AMBER, bold=True)
    ax.text(20.0, 94.0, r"$\mathbf{C}^{n}_{b}(\phi,\theta,\psi)$", fontsize=13,
            color=INK, va="center", zorder=6)
    ax.plot([2, 55], [90.0, 90.0], color=RULE, lw=0.9, zorder=3)
    matrix(ax, 28.0, 66.0,
           [[r"$c\psi\,c\theta$", r"$s\phi\,s\theta\,c\psi-c\phi\,s\psi$",
             r"$c\phi\,s\theta\,c\psi+s\phi\,s\psi$"],
            [r"$c\theta\,s\psi$", r"$s\phi\,s\theta\,s\psi+c\phi\,c\psi$",
             r"$c\phi\,s\theta\,s\psi-s\phi\,c\psi$"],
            [r"$-s\theta$", r"$s\phi\,c\theta$", r"$c\phi\,c\theta$"]],
           cw=17.0, ch=8.0, size=10.0)
    caption(ax, 2.0, 46.0, r"$\phi$ Roll   $\theta$ Pitch   $\psi$ Yaw    "
                           r"($c=\cos,\ s=\sin$)")
    caption(ax, 2.0, 41.0, "세 축 회전 " + r"$\mathbf{R}_z(\psi)\mathbf{R}_y(\theta)"
                           r"\mathbf{R}_x(\phi)$" + " 를 곱해 펼친 것이다.")

    ax.plot([58.0, 58.0], [38.0, 92.0], color=RULE, lw=0.9, zorder=3)

    label(ax, 62.0, 94.0, "② NED → ECEF", size=11.5, color=GREEN, bold=True)
    ax.text(82.0, 94.0, r"$\mathbf{C}^{e}_{n}(L,\lambda)$", fontsize=13, color=INK,
            va="center", zorder=6)
    ax.plot([62, 98], [90.0, 90.0], color=RULE, lw=0.9, zorder=3)
    matrix(ax, 79.0, 66.0,
           [[r"$-s L\,c\lambda$", r"$-s\lambda$", r"$-c L\,c\lambda$"],
            [r"$-s L\,s\lambda$", r"$c\lambda$", r"$-c L\,s\lambda$"],
            [r"$c L$", r"$0$", r"$-s L$"]],
           cw=12.0, ch=8.0, size=10.5)
    caption(ax, 62.0, 46.0, r"$L$ 위도   $\lambda$ 경도")
    caption(ax, 62.0, 41.0, "표적이 움직이면 이 행렬도 매 스텝 다시 만들어야 한다.")

    panel(ax, 2.0, 3.0, 96.0, 30.0, fill=PANEL, edge=RULE)
    label(ax, 4.0, 28.5, "두 행렬을 쓰는 방법 — 위치는 평행이동까지, 속도는 회전만",
          size=10.4, color=INK, bold=True)
    ax.plot([4.0, 96.0], [25.5, 25.5], color=RULE, lw=0.8, zorder=3)
    ax.text(5.0, 18.5, r"$\mathbf{p}^{e}=\mathbf{C}^{e}_{n}\,\mathbf{p}^{n}"
                       r"+\mathbf{p}^{e}_{ref}$", fontsize=13.5, color=INK, va="center",
            zorder=6)
    caption(ax, 5.0, 12.0, "위치를 옮길 때는 기준점을 더한다", size=9.0)
    ax.text(5.0, 7.0, "f_Trans_Ned_To_Ecef(..., Pf_X, Pf_Y, Pf_Z, ...)", fontsize=8.6,
            color=GREY, family=MONO, va="center", zorder=6)

    ax.plot([50.0, 50.0], [5.0, 23.0], color=RULE, lw=0.8, zorder=3)
    ax.text(53.0, 18.5, r"$\mathbf{v}^{e}=\mathbf{C}^{e}_{n}\,\mathbf{v}^{n}$",
            fontsize=13.5, color=GREEN, va="center", zorder=6)
    caption(ax, 53.0, 12.0, "속도는 방향뿐이라 기준점을 0 으로 준다", size=9.0)
    ax.text(53.0, 7.0, "f_Trans_Ned_To_Ecef(..., 0.0, 0.0, 0.0, ...)", fontsize=8.6,
            color=RED, family=MONO, va="center", zorder=6)
    save(fig, "fig13_dcm.png")


# ═══════════════════════════════════════════════════════════════════════
# 14. 오일러법과 중점법
# ═══════════════════════════════════════════════════════════════════════
def fig14():
    fig, ax = canvas(11.2, 4.5)

    def draw(base, title, color, mid):
        def cv(x):
            return base + 15.0 * np.sin((x - 5.0) / 40.0 * 1.35)

        xs = np.linspace(5.0, 45.0, 200)
        ax.plot(xs, cv(xs), color="#9FAABE", lw=1.7, ls=(0, (5, 4)), zorder=3)
        n = 4
        xk = np.linspace(5.0, 45.0, n + 1)
        px, py = 5.0, cv(5.0)
        for i in range(n):
            h = xk[i + 1] - xk[i]
            if mid:
                s0 = (cv(xk[i] + 0.4) - cv(xk[i])) / 0.4
                ax.plot([px, px + h / 2.0], [py, py + s0 * h / 2.0], color=GREY,
                        lw=1.0, ls=(0, (2, 2)), zorder=4)
                ax.plot([px + h / 2.0], [py + s0 * h / 2.0], "o", ms=3.2, color=GREY,
                        zorder=5)
                sl = (cv(xk[i] + h / 2.0 + 0.4) - cv(xk[i] + h / 2.0)) / 0.4
            else:
                sl = (cv(xk[i] + 0.4) - cv(xk[i])) / 0.4
            ax.plot([px, px + h], [py, py + sl * h], color=color, lw=1.9, zorder=5)
            ax.plot([px], [py], "o", ms=4.4, color=color, zorder=6)
            px, py = px + h, py + sl * h
        ax.plot([px], [py], "o", ms=4.4, color=color, zorder=6)
        label(ax, 2.0, base + 28.0, title, size=10.4, color=color, bold=True)
        return px, py, cv(45.0)

    px, py, ty = draw(60.0, "오일러법 — 출발할 때의 기울기로 한 스텝 간다", RED, False)
    ax.annotate("", xy=(px, py), xytext=(px, ty),
                arrowprops=dict(arrowstyle="<->", color=INK2, lw=1.0))
    ax.text(px + 1.6, (py + ty) / 2.0, "벌어짐", fontsize=8.8, color=INK2, va="center",
            zorder=6)
    caption(ax, 30.0, 82.0, "실제 궤적", size=8.8)

    draw(20.0, "중점법 — 반 스텝 가서 기울기를 다시 재고 한 스텝 간다", GREEN, True)
    caption(ax, 2.0, 8.0, "굽은 길을 직선 토막으로 잇는 것은 같다. 다른 것은 어느 지점의")
    caption(ax, 2.0, 3.5, "기울기를 쓰느냐다 — 중점의 기울기가 구간 평균에 훨씬 가깝다.")

    # 오른쪽 수식
    ax.plot([50.0, 50.0], [2.0, 96.0], color=RULE, lw=0.9, zorder=3)
    head(ax, 54.0, 92.0, "두 방법의 식과 오차 차수", 11.2, rule_w=44)
    ax.text(54.0, 80.0, r"$\mathbf{x}_{k+1}=\mathbf{x}_k+\Delta t\,"
                        r"f(t_k,\mathbf{x}_k)$", fontsize=12.5, color=RED, va="center",
            zorder=6)
    caption(ax, 54.0, 73.5, r"국소 $O(\Delta t^{2})$ · 전역 $O(\Delta t)$ — 1차", size=9.0)

    ax.text(54.0, 62.0, r"$\mathbf{k}_1=f(t_k,\mathbf{x}_k)$", fontsize=12, color=GREEN,
            va="center", zorder=6)
    ax.text(54.0, 53.0, r"$\mathbf{x}_{k+1}=\mathbf{x}_k+\Delta t\,"
                        r"f\left(t_k+\frac{\Delta t}{2},\ "
                        r"\mathbf{x}_k+\frac{\Delta t}{2}\mathbf{k}_1\right)$",
            fontsize=12.5, color=GREEN, va="center", zorder=6)
    caption(ax, 54.0, 45.5, r"국소 $O(\Delta t^{3})$ · 전역 $O(\Delta t^{2})$ — 2차", size=9.0)

    panel(ax, 54.0, 12.0, 44.0, 27.0, fill=PANEL, edge=RULE)
    label(ax, 56.0, 34.5, "이 과제에서의 반 스텝", size=9.8, color=INK, bold=True)
    for i, (k, v) in enumerate([
            ("자세각", "각속도가 상수라 반 스텝 값을 바로 더한다"),
            ("위치", "스텝 시작 속도로 반 스텝 예측한다"),
            ("속도", "그 자리 · 그 자세에서 다시 만든다")]):
        y = 29.0 - i * 6.0
        label(ax, 57.0, y, k, size=9.0, color=GREEN, bold=True)
        label(ax, 68.0, y, v, size=8.8, color=INK2)
    caption(ax, 54.0, 6.0, "계산량은 좌표변환 2회 → 4회로 늘지만, 0.1 s 에서")
    caption(ax, 54.0, 1.8, "오차가 1.36 m → 1.36 cm 로 100 배 줄었다.")
    save(fig, "fig14_midpoint.png")


# ═══════════════════════════════════════════════════════════════════════
# 15. 한 스텝 알고리즘
# ═══════════════════════════════════════════════════════════════════════
def fig15():
    fig, ax = canvas(11.2, 4.6)

    head(ax, 2, 93, "f_Tgt_StepSim — 한 스텝에 벌어지는 일", 12, rule_w=52)
    code = [("for  각 객체 (플랫폼 1 + 표적 N)", "k", 0),
            ("", "", 0),
            ("① 현재 시각에 걸린 기동을 찾는다", "c", 1),
            ("rate = f_Tgt_GetAttRate(target, curTime)", "", 1),
            ("", "", 0),
            ("② 자세각을 반 스텝 돌린다", "c", 1),
            ("attMid = att + rate * (dt / 2)", "", 1),
            ("", "", 0),
            ("③ 위치를 반 스텝 예측한다", "c", 1),
            ("velStart = GetVelEcef(att, lla)", "", 1),
            ("posMid   = pos + velStart * (dt / 2)", "", 1),
            ("llaMid   = Ecef_To_Lla(posMid)", "", 1),
            ("", "", 0),
            ("④ 그 자리 · 그 자세의 속도로 한 스텝 간다", "c", 1),
            ("velMid = GetVelEcef(attMid, llaMid)", "", 1),
            ("posNew = pos + velMid * dt", "", 1),
            ("", "", 0),
            ("⑤ 출력용 LLA 와 다음 속도를 만든다", "c", 1),
            ("llaNew = Ecef_To_Lla(posNew)", "", 1),
            ("velNew = GetVelEcef(attNew, llaNew)", "", 1)]
    panel(ax, 2.0, 6.0, 50.0, 82.0, fill=PANEL, edge=RULE)
    for i, (txt, kind, ind) in enumerate(code):
        y = 83.0 - i * 3.9
        if not txt:
            continue
        c = GREY if kind == "c" else (BLUE if kind == "k" else INK)
        label(ax, 4.5 + ind * 2.6, y, txt, size=8.8, color=c, font=MONO,
              bold=(kind == "k"))

    # 오른쪽: 비용과 결과
    head(ax, 57.0, 93.0, "한 스텝의 비용", 11.2, rule_w=41)
    cost = [("좌표변환 호출", "4 회", "GetVelEcef 2 + Ecef_To_Lla 2"),
            ("행렬 곱셈", "8 회", "제공 라이브러리 Matrix_Product2"),
            ("삼각함수", "약 20 회", "DCM 원소 계산"),
            ("분기 · 나눗셈", "기동 조회 1 회", "속력으로 나누는 곳은 여기뿐")]
    for i, (k, v, d) in enumerate(cost):
        y = 84.0 - i * 9.0
        label(ax, 57.0, y, k, size=9.2, color=INK, bold=True)
        label(ax, 78.0, y, v, size=9.2, color=BLUE, font=MONO, bold=True)
        caption(ax, 57.0, y - 4.0, d, size=8.4)
        ax.plot([57, 98], [y - 6.4, y - 6.4], color=RULE, lw=0.7, zorder=2)

    panel(ax, 57.0, 22.0, 41.0, 24.0, fill="#F1F8F4", edge="#C6E4D2")
    label(ax, 59.0, 42.0, "실측 성능", size=9.8, color="#0B6B4B", bold=True)
    for i, (k, v) in enumerate([("601 표본 × 객체 3", "4.8 ms"),
                                ("한 스텝 · 한 객체", "약 2.7 µs".replace("µ", "u")),
                                ("6001 표본 × 객체 11", "0.5 s 미만")]):
        y = 37.0 - i * 5.0
        label(ax, 60.0, y, k, size=8.8, color=INK2)
        label(ax, 92.0, y, v, size=8.8, color=INK, font=MONO, bold=True, ha="right")
    caption(ax, 59.0, 24.0, "동적 할당 없음 · 스텝 간 상태 의존만 있음", size=8.4)

    panel(ax, 57.0, 6.0, 41.0, 13.0, fill=WHITE, edge=RED, lw=1.3)
    label(ax, 59.0, 15.5, "④ 가 이 과제의 핵심이다", size=9.6, color=RED, bold=True)
    label(ax, 59.0, 10.5, "③ 에서 위치까지 반 스텝 옮기지 않으면\n"
                          "수평 비행 고도가 스텝마다 올라간다.", size=8.8, color=INK2,
          space=1.55)
    save(fig, "fig15_algorithm.png")


# ═══════════════════════════════════════════════════════════════════════
# 16. 방어적 구현 — 상태의 원자성
# ═══════════════════════════════════════════════════════════════════════
def fig16():
    fig, ax = canvas(11.2, 4.2)

    head(ax, 2, 92, "계산이 깨져도 상태는 지킨다", 12, rule_w=96)

    stages = [("① 사본 생성", "st_Next = st_Sim->st_Sample", BLUE,
               "지금 상태는 건드리지 않는다"),
              ("② 사본에서 전부 계산", "플랫폼 → 표적 1 → … → 표적 N", AMBER,
               "중간에 실패하면 거기서 멈춘다"),
              ("③ 유한성 검사", "isfinite(x) && isfinite(y) && isfinite(z)", VIOLET,
               "제공 변환 함수는 실패를 알려주지 않는다"),
              ("④ 한 번에 반영", "st_Sim->st_Sample = st_Next", GREEN,
               "모두 성공했을 때만 여기 온다")]
    for i, (k, code_txt, c, note) in enumerate(stages):
        x = 2.0 + i * 24.5
        panel(ax, x, 48.0, 22.5, 34.0, fill=WHITE, edge=c, lw=1.5)
        ax.add_patch(Rectangle((x, 76.0), 22.5, 6.0, facecolor=c, edgecolor="none",
                               zorder=3))
        label(ax, x + 11.25, 79.0, k, size=9.8, color=WHITE, bold=True, ha="center")
        label(ax, x + 1.6, 66.0, code_txt, size=8.0, color=INK, font=MONO)
        label(ax, x + 1.6, 55.0, note, size=8.6, color=INK2)
        if i < 3:
            arrow(ax, (x + 22.8, 65.0), (x + 24.2, 65.0), color=RULE, lw=1.4, ms=10)

    panel(ax, 2.0, 27.0, 96.0, 16.0, fill="#FDF7F7", edge="#F0D8D8")
    label(ax, 4.0, 38.5, "그래서 생기지 않는 상태", size=9.8, color="#B03B3B", bold=True)
    for i, t in enumerate(["표적 1 은 새 시각인데 표적 2 는 옛 시각인 표본",
                           "위치는 갱신됐는데 속도는 갱신 전인 객체",
                           "NaN 이 섞인 채 다음 스텝으로 넘어가 전체가 오염되는 것"]):
        label(ax, 5.5, 33.5 - i * 4.0, "·", size=9, color="#B03B3B", bold=True)
        label(ax, 7.5, 33.5 - i * 4.0, t, size=8.8, color="#8A3232")

    head(ax, 2.0, 21.0, "오류 코드", 10.2, rule_w=96)
    errs = [("TGT_ERR_TARGET_NUM", "표적 수 범위 밖"),
            ("TGT_ERR_MANEUVER_NUM", "기동 수 범위 밖"),
            ("TGT_ERR_TIME", "시간 · 간격 · 기동 구간 이상"),
            ("TGT_ERR_SPEED", "속력 0 이하 또는 비유한"),
            ("TGT_ERR_MANEUVER_OVERLAP", "기동 구간 겹침"),
            ("TGT_ERR_COORD", "변환 결과가 비유한"),
            ("TGT_ERR_SIM_STATE", "호출 사이 설정이 바뀜"),
            ("TGT_ERR_SIM_END", "마지막 스텝까지 진행 완료")]
    for i, (e, d) in enumerate(errs):
        x = 2.0 + (i % 4) * 24.5
        y = 12.0 - (i // 4) * 6.0
        label(ax, x, y, e, size=8.0, color=RED if i < 7 else GREEN, font=MONO)
        caption(ax, x, y - 3.0, d, size=7.8)
    save(fig, "fig16_guard.png")


# ═══════════════════════════════════════════════════════════════════════
# 17. 명세 시나리오 궤적
# ═══════════════════════════════════════════════════════════════════════
def fig17():
    rows = read_csv("spec.csv")
    t = col(rows, "time")
    e1, n1 = to_en(col(rows, "t1_lat"), col(rows, "t1_lon"), 32.0, 126.0)
    e2, n2 = to_en(col(rows, "t2_lat"), col(rows, "t2_lon"), 32.0, 126.0)

    fig = plt.figure(figsize=(11.2, 4.4))
    gs = fig.add_gridspec(2, 2, width_ratios=[1.12, 1.0], height_ratios=[1, 1],
                          wspace=0.24, hspace=0.62, left=0.065, right=0.985,
                          top=0.90, bottom=0.12)

    ax = fig.add_subplot(gs[:, 0])
    ax.plot(e2 / 1000.0, n2 / 1000.0, color=RED, lw=2.2, label="표적 2  대공 200 m/s",
            zorder=5)
    ax.plot(e1 / 1000.0, n1 / 1000.0, color=BLUE, lw=2.2, label="표적 1  대함 30 m/s",
            zorder=5)
    for e, n, c in ((e1, n1, BLUE), (e2, n2, RED)):
        ax.plot(e[::100] / 1000.0, n[::100] / 1000.0, "o", ms=3.4, color=c, zorder=6)
        ax.plot(e[0] / 1000.0, n[0] / 1000.0, "o", ms=8, mfc=WHITE, mec=c, mew=2,
                zorder=7)
        ax.plot(e[-1] / 1000.0, n[-1] / 1000.0, "s", ms=6.5, color=c, zorder=7)
    ax.plot([0], [0], "^", ms=11, color=PF, zorder=7,
            label="플랫폼  32.0 °, 126.0 °  정지", ls="none")
    ax.set_xlim(-6.2, 6.2)
    ax.set_ylim(-1.4, 15.4)
    ax.set_xlabel("플랫폼 기준 동쪽 [km]", fontsize=9.5)
    ax.set_ylabel("플랫폼 기준 북쪽 [km]", fontsize=9.5)
    ax.set_title("지상 궤적 (점 = 10 s 간격)", fontsize=10.4, color=INK, pad=8)
    ax.grid(True)
    ax.set_aspect("equal", adjustable="box")
    ax.legend(fontsize=7.8, loc="lower left", handlelength=1.4)
    ax.annotate("12.000 km 남진", xy=(0.0, 7.0), xytext=(1.2, 8.6), fontsize=8.6,
                color=RED, arrowprops=dict(arrowstyle="->", color=RED, lw=0.9))

    sub = ax.inset_axes([0.05, 0.62, 0.44, 0.24])
    sub.plot(e1 / 1000.0, n1 / 1000.0, color=BLUE, lw=2.0)
    sub.plot(e1[0] / 1000.0, n1[0] / 1000.0, "o", ms=6, mfc=WHITE, mec=BLUE, mew=1.8)
    sub.plot(e1[-1] / 1000.0, n1[-1] / 1000.0, "s", ms=5.5, color=BLUE)
    sub.plot(e1[100::100] / 1000.0, n1[100::100] / 1000.0, "o", ms=3, color=BLUE)
    sub.set_xlim(e1.min() / 1000.0 - 0.25, e1.max() / 1000.0 + 0.25)
    sub.set_ylim(n1.mean() / 1000.0 - 0.45, n1.mean() / 1000.0 + 0.45)
    sub.set_xticks([]); sub.set_yticks([])
    sub.set_facecolor("#FBFCFE")
    for sp in sub.spines.values():
        sp.set_color(BLUE); sp.set_linewidth(1.1)
    sub.set_title("표적 1 확대 — 서쪽 1.800 km", fontsize=7.6, color=BLUE, pad=3)

    ax = fig.add_subplot(gs[0, 1])
    ax.plot(t, col(rows, "t2_alt"), color=RED, lw=2.0)
    ax.plot(t, col(rows, "t1_alt"), color=BLUE, lw=2.0)
    ax.set_ylim(-60, 420)
    ax.set_ylabel("고도 [m]", fontsize=9)
    ax.set_xlabel("시각 [s]", fontsize=9)
    ax.grid(True)
    ax.set_title("고도 — 60 s 내내 평평하다", fontsize=9.8, color=INK, pad=6)
    ax.text(30, 330, "300.000000 m", fontsize=8.2, color=RED, ha="center")
    ax.text(30, 60, "0.000000 m", fontsize=8.2, color=BLUE, ha="center")

    ax = fig.add_subplot(gs[1, 1])
    ax.plot(t, col(rows, "t2_spd"), color=RED, lw=2.0)
    ax.plot(t, col(rows, "t1_spd"), color=BLUE, lw=2.0)
    ax.set_ylim(-20, 260)
    ax.set_ylabel("속력 [m/s]", fontsize=9)
    ax.set_xlabel("시각 [s]", fontsize=9)
    ax.grid(True)
    ax.set_title(r"$\|\mathbf{v}^{e}\|$ — 넣은 값 그대로", fontsize=9.8, color=INK, pad=6)
    ax.text(30, 215, "200.000000000000 m/s", fontsize=8.2, color=RED, ha="center")
    ax.text(30, 55, "30.000000000000 m/s", fontsize=8.2, color=BLUE, ha="center")

    fig.savefig(os.path.join(OUT, "fig17_result_map.png"), dpi=DPI, bbox_inches="tight",
                pad_inches=0.06)
    plt.close(fig)
    print("  fig17_result_map.png")


# ═══════════════════════════════════════════════════════════════════════
# 18. 불변량 잔차
# ═══════════════════════════════════════════════════════════════════════
def fig18():
    rows = read_csv("spec.csv")
    t = col(rows, "time")
    dh2 = (col(rows, "t2_alt") - 300.0) * 1e6           # 대공 고도 잔차 [um]
    dh1 = (col(rows, "t1_alt") - 0.0) * 1e6
    dlat = np.abs(col(rows, "t1_lat") - 32.125) * 111132.0 * 1e6
    dlon = np.abs(col(rows, "t2_lon") - 126.0) * 94266.0 * 1e6
    dv2 = np.abs(col(rows, "t2_spd") - 200.0)
    dv1 = np.abs(col(rows, "t1_spd") - 30.0)

    fig = plt.figure(figsize=(11.2, 4.3))
    gs = fig.add_gridspec(1, 2, width_ratios=[1.0, 1.05], wspace=0.22, left=0.075,
                          right=0.985, top=0.88, bottom=0.14)

    ax = fig.add_subplot(gs[0, 0])
    ax.plot(t, dh2, color=RED, lw=2.2, label="대공 고도 (300 m 기준)")
    ax.plot(t, dh1, color=BLUE, lw=1.4, ls=(0, (5, 3)), label="대함 고도 (0 m 기준)")
    ax.plot(t, dlat, color=GREEN, lw=1.6, label="대함 위도 편차 (거리 환산)")
    ax.set_xlabel("시각 [s]", fontsize=9.5)
    ax.set_ylabel(r"잔차 [$\mu$m]", fontsize=9.5)
    ax.set_ylim(-4, 40)
    ax.grid(True)
    ax.legend(fontsize=8.0, loc="center right")
    ax.set_title("등속 수평 비행에서 남는 잔차", fontsize=10.4, color=INK, pad=8)
    ax.annotate("첫 스텝에서 30.8 " + r"$\mu$m,",
                xy=(2.5, dh2[25]), xytext=(6.0, 37.0), fontsize=8.4, color=INK2,
                arrowprops=dict(arrowstyle="->", color=INK2, lw=0.9))
    ax.text(6.0, 33.6, "그 뒤 60 s 내내 상수", fontsize=8.4, color=INK2)
    ax.text(30.0, 3.0, "시간에 따라 자라지 않는다 = 적분이 만든 오차가 아니다",
            fontsize=8.4, color=GREEN, ha="center")

    ax = fig.add_subplot(gs[0, 1])
    ax.axis("off"); ax.set_xlim(0, 100); ax.set_ylim(0, 100)
    ax.text(0, 97, "불변량별 최대 잔차 (601 표본)", fontsize=10.4, color=INK, va="top",
            fontweight="bold")
    ax.plot([0, 100], [88, 88], color=INK, lw=1.1)
    tab = [("대공 고도", "%.1f " % dh2.max() + r"$\mu$m", "제공 코드의 ECEF→LLA 5회 반복 근사", RED),
           ("대함 고도", "%.1f " % dh1.max() + r"$\mu$m", "같은 원인 — 두 표적이 같은 값", BLUE),
           ("대함 위도", "%.2f " % dlat.max() + r"$\mu$m", "정서진 — 위도가 변하면 안 된다", GREEN),
           ("대공 경도", "%.2f " % dlon.max() + r"$\mu$m", "정남진 — 경도가 변하면 안 된다", GREEN),
           ("대공 속력", "%.0f m/s (완전 일치)" % dv2.max(), "회전행렬이 크기를 보존한다", AMBER),
           ("대함 속력", "%.0f m/s (완전 일치)" % dv1.max(), "601 표본 모두 비트까지 같다", AMBER)]
    for i, (k, v, d, c) in enumerate(tab):
        y = 81 - i * 12.8
        ax.add_patch(Rectangle((0, y - 5.0), 1.6, 11.0, facecolor=c, edgecolor="none"))
        ax.text(4, y + 2.6, k, fontsize=9.4, color=INK, fontweight="bold")
        ax.text(30, y + 2.6, v, fontsize=10.4, color=c, fontweight="bold",
                family=MONO if "e" in v else None)
        ax.text(4, y - 3.0, d, fontsize=8.6, color=GREY)
        if i < 5:
            ax.plot([0, 100], [y - 6.8, y - 6.8], color=RULE, lw=0.7)
    ax.text(0, 5.5, "잔차의 출처는 적분기가 아니라 제공받은 좌표변환의 반복 근사다.",
            fontsize=8.8, color=INK2)
    ax.text(0, 0.5, "Bowring 폐형식으로 바꾸면 사라지지만, 과제 조건이 \"제공 코드 사용\" 이라 그대로 두었다.",
            fontsize=8.8, color=GREY)

    fig.savefig(os.path.join(OUT, "fig18_invariant.png"), dpi=DPI, bbox_inches="tight",
                pad_inches=0.06)
    plt.close(fig)
    print("  fig18_invariant.png")


# ═══════════════════════════════════════════════════════════════════════
# 19. 검증 결과표
# ═══════════════════════════════════════════════════════════════════════
def fig19():
    rows = read_csv("spec.csv")
    path = []
    for k in (1, 2):
        pe = lla2ecef(col(rows, "t%d_lat" % k), col(rows, "t%d_lon" % k),
                      col(rows, "t%d_alt" % k))
        path.append(float(np.sum(np.linalg.norm(np.diff(pe, axis=0), axis=1))))
    dh = (col(rows, "t2_alt") - 300.0).max() * 1e6
    dv = np.abs(col(rows, "t2_spd") - 200.0).max()

    fig, ax = canvas(11.2, 4.4)
    head(ax, 2, 94, "세 가지 방법으로 검증했다", 12, rule_w=96)

    groups = [("① 해석해와 대조", BLUE,
               [("대함 이동거리", r"$30\times 60=1{,}800$ m", "%.5f m" % path[0],
                 "%+.2e m" % (path[0] - 1800.0)),
                ("대공 이동거리", r"$200\times 60=12{,}000$ m", "%.5f m" % path[1],
                 "%+.2e m" % (path[1] - 12000.0))]),
              ("② 불변량 보존", GREEN,
               [("대공 고도", "300 m 유지", "300.000000 ~ 300.000031 m",
                 r"$%.1f\ \mu$m" % dh),
                ("대공 속력", "200 m/s 유지", "200.000000000000 m/s", "%.1e" % dv),
                ("대함 위도", "32.125 ° 유지 (정서진)", "32.125000000000 °",
                 r"$0.1\ \mu$m"),
                ("대공 경도", "126.0 ° 유지 (정남진)", "126.000000000000 °", "0")]),
              ("③ 수렴 차수", AMBER,
               [(r"$\Delta t$ 를 1/2 로", "오차 1/4 (2차)", "1.357 → 0.339 → 0.085 m",
                 "기울기 2.00"),
                (r"$\Delta t=0.1$ s", "— (설계값 확인)", "60 s 뒤 위치오차 1.36 cm",
                 "분해능 대비 무시")])]

    xs = [4.5, 27.0, 51.0, 81.0]
    y = 88.0
    for gname, gc, items in groups:
        ax.add_patch(Rectangle((2.0, y - 4.4), 96.0, 6.0, facecolor=gc, edgecolor="none",
                               zorder=3))
        label(ax, 4.5, y - 1.4, gname, size=9.6, color=WHITE, bold=True)
        for hx, hh in zip(xs[1:], ["이론값 · 기대값", "시뮬레이션 결과", "잔차"]):
            label(ax, hx, y - 1.4, hh, size=8.4, color=WHITE, bold=True)
        y -= 8.4
        for k, (nm, exp, got, res) in enumerate(items):
            if k % 2 == 0:
                ax.add_patch(Rectangle((2.0, y - 2.9), 96.0, 6.2, facecolor="#F8FAFD",
                                       edgecolor="none", zorder=2))
            label(ax, xs[0], y, nm, size=8.6, color=INK, bold=True)
            ax.text(xs[1], y, exp, fontsize=8.4, color=INK2, va="center", zorder=6)
            label(ax, xs[2], y, got, size=8.4, color=INK2, font=MONO)
            ax.text(xs[3], y, res, fontsize=8.6, color=GREEN, va="center", zorder=6,
                    fontweight="bold")
            y -= 6.2
        y -= 2.4

    panel(ax, 2.0, 1.5, 96.0, 9.5, fill=PANEL, edge=RULE)
    label(ax, 4.0, 7.8, "이동거리는 601개 표본 사이 거리를 전부 더해 잰 것이다 — 직선거리가 아니라 실제로 지나간 길이.",
          size=8.8, color=INK2)
    label(ax, 4.0, 4.0, "12 km 를 날아 잔차가 0.05 mm 면 적분기의 오차가 아니라 double 형의 표현 한계에 가깝다.",
          size=8.8, color=INK2)
    save(fig, "fig19_verify.png")


# ═══════════════════════════════════════════════════════════════════════
# 20. 갱신 간격에 대한 수렴
# ═══════════════════════════════════════════════════════════════════════
def fig20():
    dts, errs = [], []
    with open(os.path.join(DATA, "conv.txt"), encoding="utf-8") as fp:
        section = 0
        for line in fp:
            line = line.strip()
            if line.startswith("#"):
                section += 1
                continue
            if not line or line.startswith("dt,"):
                continue
            if section == 2:
                a, b, _ = line.split(",")
                dts.append(float(a))
                errs.append(float(b))
    dts, errs = np.array(dts), np.array(errs)
    slope = float(np.polyfit(np.log(dts), np.log(errs), 1)[0])

    fig = plt.figure(figsize=(11.2, 4.3))
    gs = fig.add_gridspec(1, 2, width_ratios=[1.0, 1.0], wspace=0.20, left=0.075,
                          right=0.985, top=0.88, bottom=0.145)

    ax = fig.add_subplot(gs[0, 0])
    ax.loglog(dts, errs, "o-", color=BLUE, lw=2.0, ms=6.5, label="실측 오차", zorder=5)
    ref = errs[0] * (dts / dts[0]) ** 2
    ax.loglog(dts, ref, "--", color=GREY, lw=1.4, label=r"기울기 2 기준선 ($\Delta t^{2}$)",
              zorder=4)
    ax.axvline(0.1, color=AMBER, lw=1.3, ls=(0, (4, 3)), zorder=3)
    ax.text(0.106, errs[0] * 0.5, "과제 조건\n" + r"$\Delta t=0.1$ s", fontsize=8.4,
            color=AMBER, fontweight="bold", linespacing=1.5)
    ax.set_xlabel(r"갱신 간격 $\Delta t$ [s]", fontsize=9.5)
    ax.set_ylabel("60 s 뒤 위치 오차 [m]", fontsize=9.5)
    ax.grid(True, which="both")
    ax.legend(fontsize=8.2, loc="upper left")
    ax.set_title(r"Yaw 2 G 로 20 s 선회 · 기준 $\Delta t=0.001$ s",
                 fontsize=10.2, color=INK, pad=8)
    ax.xaxis.set_major_formatter(FuncFormatter(lambda v, _: "%g" % v))
    ax.xaxis.set_minor_formatter(NullFormatter())
    ax.set_xticks([0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0])
    ax.yaxis.set_major_formatter(FuncFormatter(
        lambda v, _: ("%g" % v) if v >= 0.01 else ("%.0e" % v).replace("e-0", "e-")))
    ax.annotate("회귀 기울기 %.3f" % slope, xy=(0.05, errs[5]), xytext=(0.12, 3e-3),
                fontsize=9.0, color=BLUE, fontweight="bold",
                arrowprops=dict(arrowstyle="->", color=BLUE, lw=1.0))

    ax = fig.add_subplot(gs[0, 1])
    ax.axis("off"); ax.set_xlim(0, 100); ax.set_ylim(0, 100)
    ax.text(0, 97, "읽는 법", fontsize=10.6, color=INK, va="top", fontweight="bold")
    ax.plot([0, 100], [90, 90], color=INK, lw=1.0)
    for i, (hh, d) in enumerate([
            ("두 선이 나란하다", "간격을 절반으로 줄이면 오차가 1/4 로 줄어든다.\n"
                             "중점법이 설계대로 2차 정확도로 돌고 있다는 뜻."),
            (r"$\Delta t=0.1$ s 에서 1.36 cm", "20 s 동안 2 G 로 꺾는 내내 쌓인 값이다.\n"
                                            "레이다 거리 분해능(수 m)에 비하면 없는 값."),
            ("직진이면 더 작다", "기동이 없으면 오차가 10 nm 수준 —\n"
                            "사실상 부동소수점 오차만 남는다."),
            ("차수가 확인의 핵심", "값 하나가 작은 것보다, 간격을 바꿨을 때\n"
                              "정해진 기울기로 줄어드는 것이 구현이 옳다는 증거다.")]):
        y = 80 - i * 22
        ax.add_patch(Rectangle((0, y - 9), 1.7, 16, facecolor=[BLUE, GREEN, AMBER, INK][i],
                               edgecolor="none"))
        ax.text(4.5, y + 4.0, hh, fontsize=9.8, fontweight="bold",
                color=[BLUE, GREEN, AMBER, INK][i])
        ax.text(4.5, y - 3.0, d, fontsize=8.8, color=INK2, linespacing=1.6, va="center")

    fig.savefig(os.path.join(OUT, "fig20_convergence.png"), dpi=DPI, bbox_inches="tight",
                pad_inches=0.06)
    plt.close(fig)
    print("  fig20_convergence.png")


# ═══════════════════════════════════════════════════════════════════════
# 21. 기동 시연
# ═══════════════════════════════════════════════════════════════════════
def fig21():
    rows = read_csv("demo.csv")
    t = col(rows, "time")
    e1, n1 = to_en(col(rows, "t1_lat"), col(rows, "t1_lon"), 32.0, 126.0)
    e2, n2 = to_en(col(rows, "t2_lat"), col(rows, "t2_lon"), 32.0, 126.0)

    fig = plt.figure(figsize=(11.2, 4.4))
    gs = fig.add_gridspec(2, 2, width_ratios=[1.0, 1.32], hspace=0.60, wspace=0.22,
                          left=0.07, right=0.985, top=0.90, bottom=0.145)

    ax = fig.add_subplot(gs[:, 0])
    ax.plot(e2 / 1000.0, n2 / 1000.0, color=RED, lw=2.0, zorder=5)
    ax.plot(e1 / 1000.0, n1 / 1000.0, color=BLUE, lw=2.0, zorder=5)
    for e, n, c in ((e1, n1, BLUE), (e2, n2, RED)):
        ax.plot(e[0] / 1000.0, n[0] / 1000.0, "o", ms=7, mfc=WHITE, mec=c, mew=1.8,
                zorder=7)
        ax.plot(e[-1] / 1000.0, n[-1] / 1000.0, "s", ms=6, color=c, zorder=7)
    ax.plot([0], [0], "^", ms=10, color=PF, zorder=7)
    ax.set_xlabel("동쪽 [km]", fontsize=9.5)
    ax.set_ylabel("북쪽 [km]", fontsize=9.5)
    ax.grid(True)
    ax.set_aspect("equal", adjustable="box")
    ax.set_xlim(-1.6, 3.6)
    ax.set_ylim(-0.8, 14.8)
    ax.set_title("기동 10줄을 얹은 60 s 궤적", fontsize=10.2, color=INK, pad=8)

    for key, c, nm, pos in (("t1_yaw", BLUE, "표적 1  대함", 0),
                            ("t2_yaw", RED, "표적 2  대공", 1)):
        ax = fig.add_subplot(gs[pos, 1])
        ax.plot(t, col(rows, key) % 360.0, color=c, lw=1.9)
        ax.set_ylim(-25, 390)
        ax.set_yticks([0, 90, 180, 270, 360])
        ax.set_ylabel(r"$\psi$ [°]", fontsize=9)
        ax.set_xlabel("시각 [s]", fontsize=9)
        ax.grid(True)
        ax.set_title("%s — 계단의 기울기가 곧 $\\omega$" % nm, fontsize=9.6, color=INK,
                     pad=6)
        ax.tick_params(labelsize=8)

    fig.text(0.07, 0.028,
             "초기값 · 시뮬레이션 시간 · 갱신 간격은 명세 그대로다. 기동표 10줄만 추가했다.  "
             "회전 지점이 좌표로 주어져, Core 를 돌려 그 좌표에 닿는 시각을 찾아 start / end 로 옮겼다.",
             fontsize=8.8, color=GREY)

    fig.savefig(os.path.join(OUT, "fig21_zigzag.png"), dpi=DPI, bbox_inches="tight",
                pad_inches=0.06)
    plt.close(fig)
    print("  fig21_zigzag.png")


# ═══════════════════════════════════════════════════════════════════════
# 22. TargetSim 화면 구성 (IDD_TARGETSIMUI_DIALOG 배치 그대로)
# ═══════════════════════════════════════════════════════════════════════
def fig22():
    fig = plt.figure(figsize=(11, 6.8))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(0, 760)
    ax.set_ylim(470, 0)
    ax.axis("off")

    PAGE, CARD, BORDER, TXT, SUB, OFF = "#F3F4F6", "#FFFFFF", "#DFE2E7", "#1F2937", "#6B7280", "#B0B6BF"
    ACC, ACCS, TRACK = "#2563EB", "#E8F0FE", "#E5E7EB"

    def card(x, y, w, h):
        ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=4",
                                    facecolor=CARD, edgecolor=BORDER, lw=1.0, zorder=2))

    def btn(x, y, w, h, label, primary=False):
        ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=3",
                                    facecolor=ACC if primary else "#FAFAFB",
                                    edgecolor=ACC if primary else BORDER, lw=1.0, zorder=4))
        ax.text(x + w / 2, y + h / 2, label, ha="center", va="center", fontsize=6.6,
                color=WHITE if primary else TXT, zorder=5)

    def field(x, y, w, h, label, align="right"):
        ax.add_patch(Rectangle((x, y), w, h, facecolor=WHITE, edgecolor=BORDER, lw=1.0, zorder=4))
        ax.text(x + w - 4 if align == "right" else x + 4, y + h / 2, label,
                ha=align, va="center", fontsize=6.6, color=TXT, zorder=5)

    ax.add_patch(Rectangle((0, 0), 760, 470, facecolor=PAGE, edgecolor="none", zorder=1))

    # ── 상단 바 ──
    card(8, 8, 744, 26)
    btn(14, 14, 62, 15, "시나리오")
    ax.text(88, 21.5, "시간 [s]", fontsize=6.6, color=SUB, va="center")
    field(122, 14, 44, 15, "60")
    ax.text(190, 21.5, "간격 [s]", fontsize=6.6, color=SUB, va="center")
    field(224, 14, 52, 15, "0.1")
    ax.text(296, 21.5, "명세 시나리오 (과제 3)", fontsize=6.9, color=TXT, va="center",
            fontweight="bold")

    # ── 좌측 : 객체 표 ──
    card(8, 42, 344, 176)
    ax.text(18, 54, "플랫폼 / 표적", fontsize=7.2, color=TXT, va="center", fontweight="bold")
    btn(212, 47, 44, 14, "표적 추가")
    btn(260, 47, 36, 14, "복제")
    btn(300, 47, 36, 14, "삭제")
    hdr = ["", "이름", "위도", "경도", "고도", "속력", "Roll", "Pitch", "Yaw", "기동"]
    wid = [10, 46, 42, 44, 30, 30, 26, 28, 26, 46]
    x = 16
    ax.add_patch(Rectangle((16, 66), 328, 15, facecolor="#F9FAFB", edgecolor=BORDER, lw=0.8, zorder=3))
    for h, w in zip(hdr, wid):
        ax.text(x + w / 2, 73.5, h, ha="center", va="center", fontsize=5.9, color=SUB, zorder=5)
        x += w
    data = [("플랫폼", "32.0", "126.0", "0", "0", "0", "0", "0", "", PF),
            ("표적 1", "32.125", "126.03", "0", "30", "0", "0", "270", "", BLUE),
            ("표적 2", "32.12", "126.0", "300", "200", "0", "0", "180", "", RED)]
    for r, row in enumerate(data):
        y = 81 + r * 15
        if r == 2:
            ax.add_patch(Rectangle((16, y), 328, 15, facecolor=ACCS, edgecolor="none", zorder=3))
        ax.add_patch(Rectangle((16, y), 328, 15, facecolor="none", edgecolor="#ECEEF1",
                               lw=0.7, zorder=4))
        x = 16
        for ci, (v, w) in enumerate(zip(("",) + row[:-1], wid)):
            if ci == 0:
                ax.add_patch(Rectangle((x + 3, y + 5), 5, 5, facecolor=row[-1],
                                       edgecolor="none", zorder=5))
            else:
                ax.text(x + w - 4, y + 7.5, v, ha="right", va="center", fontsize=6.2,
                        color=TXT, zorder=5)
            x += w
    ax.text(18, 208, "표적 최대 10개 · 칸을 두 번 눌러 편집", fontsize=6.0, color=OFF, va="center")

    # ── 좌측 : 기동 표 ──
    card(8, 226, 344, 176)
    ax.text(18, 238, "기동   (표적 2)", fontsize=7.2, color=TXT, va="center", fontweight="bold")
    btn(260, 231, 44, 14, "기동 추가")
    btn(308, 231, 36, 14, "삭제")
    mh = ["#", "축", "G", "시작", "종료", "회전각", "선회 반경"]
    mw = [18, 54, 46, 40, 40, 48, 82]
    x = 16
    ax.add_patch(Rectangle((16, 250), 328, 15, facecolor="#F9FAFB", edgecolor=BORDER, lw=0.8, zorder=3))
    for h, w in zip(mh, mw):
        ax.text(x + w / 2, 257.5, h, ha="center", va="center", fontsize=5.9, color=SUB, zorder=5)
        x += w
    md = [("1", "2 Yaw", "-8.205", "2.8", "6.7", "-89.90 °", "497 m"),
          ("2", "2 Yaw", "8.205", "13.3", "17.2", "89.90 °", "497 m"),
          ("3", "2 Yaw", "8.205", "18.6", "22.5", "89.90 °", "497 m")]
    for r, row in enumerate(md):
        y = 265 + r * 15
        ax.add_patch(Rectangle((16, y), 328, 15, facecolor="none", edgecolor="#ECEEF1",
                               lw=0.7, zorder=4))
        x = 16
        for ci, (v, w) in enumerate(zip(row, mw)):
            if ci == 1:
                ax.add_patch(Rectangle((x + 4, y + 5), 5, 5, facecolor=ACC, edgecolor="none", zorder=5))
                ax.text(x + 13, y + 7.5, v, ha="left", va="center", fontsize=6.2, color=TXT, zorder=5)
            else:
                ax.text(x + w - 5, y + 7.5, v, ha="right", va="center", fontsize=6.2,
                        color=TXT if ci < 5 else SUB, zorder=5)
            x += w
    ax.text(18, 390, "G 부호가 회전 방향 (Yaw+ 우선회) · 구간은 [시작, 종료)", fontsize=6.0,
            color=OFF, va="center")

    # ── 우측 : 궤적 그림 ──
    card(356, 42, 396, 244)
    ax.text(366, 54, "궤적   (플랫폼 기준 동-북 [m])", fontsize=7.0, color=SUB, va="center")
    px0, py0, pw, ph = 362, 62, 384, 158
    ax.add_patch(Rectangle((px0, py0), pw, ph, facecolor="#FCFCFD", edgecolor=BORDER, lw=0.8, zorder=3))
    for gx in np.linspace(px0, px0 + pw, 9)[1:-1]:
        ax.plot([gx, gx], [py0, py0 + ph], color="#ECEEF1", lw=0.6, zorder=4)
    for gy in np.linspace(py0, py0 + ph, 6)[1:-1]:
        ax.plot([px0, px0 + pw], [gy, gy], color="#ECEEF1", lw=0.6, zorder=4)
    rows = read_csv("demo.csv")
    e1, n1 = to_en(col(rows, "t1_lat"), col(rows, "t1_lon"), 32.0, 126.0)
    e2, n2 = to_en(col(rows, "t2_lat"), col(rows, "t2_lon"), 32.0, 126.0)
    allx = np.concatenate([e1, e2, [0.0]])
    ally = np.concatenate([n1, n2, [0.0]])
    sc = min(pw * 0.86 / (allx.max() - allx.min()), ph * 0.86 / (ally.max() - ally.min()))
    mx, my = (allx.max() + allx.min()) / 2, (ally.max() + ally.min()) / 2

    def P(e, n):
        return px0 + pw / 2 + (e - mx) * sc, py0 + ph / 2 - (n - my) * sc

    for e, n, c in ((e1, n1, BLUE), (e2, n2, RED)):
        X, Y = P(e, n)
        ax.plot(X, Y, color=c, lw=1.5, zorder=5)
        ax.plot(X[0], Y[0], "o", ms=4.5, mfc=WHITE, mec=c, mew=1.4, zorder=6)
        ax.plot(X[300], Y[300], "o", ms=5, color=c, zorder=7)
    X, Y = P(np.array([0.0]), np.array([0.0]))
    ax.plot(X, Y, "^", ms=7, color=PF, zorder=6)

    ax.text(366, 232, "고도 [m]", fontsize=6.4, color=SUB, va="center")
    ax.add_patch(Rectangle((px0, 236), pw, 44, facecolor="#FCFCFD", edgecolor=BORDER, lw=0.8, zorder=3))
    tt = col(rows, "time")
    a2 = col(rows, "t2_alt")
    ax.plot(px0 + tt / tt.max() * pw, 280 - a2 / 400.0 * 40, color=RED, lw=1.4, zorder=5)
    ax.plot(px0 + tt / tt.max() * pw, 280 - col(rows, "t1_alt") / 400.0 * 40, color=BLUE,
            lw=1.4, zorder=5)
    ax.plot([px0 + 0.5 * pw, px0 + 0.5 * pw], [236, 280], color=ACC, lw=1.0, zorder=6)

    btn(362, 292, 48, 15, "■ 정지", True)
    field(414, 292, 40, 15, "x1", "left")
    ax.add_patch(Rectangle((460, 298), 196, 4, facecolor=TRACK, edgecolor="none", zorder=4))
    ax.add_patch(Rectangle((460, 298), 98, 4, facecolor=ACC, edgecolor="none", zorder=5))
    ax.add_patch(FancyBboxPatch((554, 292), 9, 16, boxstyle="round,pad=0,rounding_size=2",
                                facecolor=WHITE, edgecolor=ACC, lw=1.2, zorder=6))
    ax.text(664, 300, "t = 30.0 s   (300 / 600)", fontsize=6.6, color=TXT, va="center")

    # ── 우측 : 결과 표 ──
    card(356, 314, 396, 148)
    ax.text(366, 326, "결과 (LLA)", fontsize=7.2, color=TXT, va="center", fontweight="bold")
    btn(672, 319, 80, 14, "CSV 저장...")
    rh = ["스텝", "시각 [s]", "플랫폼 위도", "플랫폼 경도", "표적1 위도", "표적1 경도",
          "표적2 위도", "표적2 경도"]
    rw = [32, 42, 56, 58, 54, 56, 54, 36]
    x = 362
    ax.add_patch(Rectangle((362, 338), 384, 14, facecolor="#F9FAFB", edgecolor=BORDER, lw=0.8, zorder=3))
    for h, w in zip(rh, rw):
        ax.text(x + w / 2, 345, h, ha="center", va="center", fontsize=5.6, color=SUB, zorder=5)
        x += w
    show = [298, 299, 300, 301, 302, 303, 304]
    for r, si in enumerate(show):
        y = 352 + r * 14
        if si == 300:
            ax.add_patch(Rectangle((362, y), 384, 14, facecolor=ACCS, edgecolor="none", zorder=3))
        ax.add_patch(Rectangle((362, y), 384, 14, facecolor="none", edgecolor="#ECEEF1",
                               lw=0.6, zorder=4))
        rr = rows[si]
        vals = [str(si), "%.1f" % float(rr["time"]), "%.6f" % float(rr["pf_lat"]),
                "%.6f" % float(rr["pf_lon"]), "%.6f" % float(rr["t1_lat"]),
                "%.6f" % float(rr["t1_lon"]), "%.6f" % float(rr["t2_lat"]), "..."]
        x = 362
        for v, w in zip(vals, rw):
            ax.text(x + w - 4, y + 7, v, ha="right", va="center", fontsize=5.8, color=TXT, zorder=5)
            x += w

    ax.text(14, 464, "601 표본 × 3 객체 생성 — 4.8 ms", fontsize=6.4, color=GREEN, va="center")

    fig.savefig(os.path.join(OUT, "fig22_ui.png"), dpi=190, bbox_inches="tight", pad_inches=0.05)
    plt.close(fig)
    print("  fig22_ui.png")


# ═══════════════════════════════════════════════════════════════════════
# A. 표적 상태와 좌표계  (상태 벡터 + ECEF/LLA/NED/Body + 자세각)
# ═══════════════════════════════════════════════════════════════════════
def figA():
    fig, ax = canvas(11.2, 4.6)

    # ── 위 띠: 좌표계 ──
    V = Ortho(lat_deg=20.0, lon_deg=24.0)
    cx, cy, R = 13.0, 68.0, 14.0
    LAT_P, LON_P = 36.0, 52.0

    def S(v):
        a, b = V.xy(v)
        return cx + xr(R * a), cy + R * b

    circle(ax, cx, cy, R, facecolor="#F6F8FC", edgecolor=INK2, lw=1.1, zorder=2)
    for la in (-60, -30, 0, 30, 60):
        pts = [Ortho.sph(la, lo) for lo in np.linspace(0, 360, 160)]
        sphere_curve(ax, V, pts, cx, cy, R, color=INK2 if la == 0 else RULE,
                     lw=0.9 if la == 0 else 0.6, z=3)
    for lo in range(0, 360, 30):
        pts = [Ortho.sph(la, lo) for la in np.linspace(-90, 90, 80)]
        sphere_curve(ax, V, pts, cx, cy, R, color=RULE, lw=0.6, z=3)
    for vec, name, ext in (([1, 0, 0], r"$X_e$", 1.40), ([0, 1, 0], r"$Y_e$", 1.40),
                           ([0, 0, 1], r"$Z_e$", 1.38)):
        a, b = V.xy(vec)
        tip = (cx + xr(R * ext * a), cy + R * ext * b)
        if not V.front(vec):
            ax.plot([cx, tip[0]], [cy, tip[1]], color=INK, lw=1.1, ls=(0, (5, 3)), zorder=5)
            arrow(ax, (cx + xr(R * a), cy + R * b), tip, color=INK, lw=1.2, ms=9)
        else:
            arrow(ax, (cx, cy), tip, color=INK, lw=1.3, ms=10, zorder=5)
        ax.text(cx + xr(R * (ext + 0.20) * a), cy + R * (ext + 0.20) * b, name,
                fontsize=10, color=INK, ha="center", va="center", zorder=8)
    P = Ortho.sph(LAT_P, LON_P)
    ax.plot(*zip(S([0, 0, 0]), S(P)), color=RED, lw=1.2, zorder=6)
    px, py = S(P)
    ax.plot([px], [py], "o", ms=5.5, color=RED, zorder=8)
    lam = [Ortho.sph(0.0, lo) * 0.46 for lo in np.linspace(0, LON_P, 50)]
    ax.plot([S(v)[0] for v in lam], [S(v)[1] for v in lam], color=GREEN, lw=1.4, zorder=7)
    ax.text(*S(Ortho.sph(0.0, LON_P * 0.5) * 0.66), r"$\lambda$", fontsize=11,
            color=GREEN, ha="center", va="center", zorder=8)
    phi = [Ortho.sph(la, LON_P) * 0.72 for la in np.linspace(0, LAT_P, 50)]
    ax.plot([S(v)[0] for v in phi], [S(v)[1] for v in phi], color=BLUE, lw=1.4, zorder=7)
    ax.text(*S(Ortho.sph(LAT_P * 0.5, LON_P) * 0.90), r"$\varphi$", fontsize=11,
            color=BLUE, ha="center", va="center", zorder=8)
    hx, hy = S(P * 1.32)
    arrow(ax, (px, py), (hx, hy), color=RED, lw=1.1, ms=8, ls=(0, (2, 2)))
    ax.text(hx + xr(2.6), hy + 1.6, r"$h$", fontsize=10.5, color=RED, ha="center",
            va="center", zorder=8)
    label(ax, 2.0, 93.0, "ECEF · LLA", size=10.4, color=INK, bold=True)
    caption(ax, 2.0, 50.0, r"지구중심 직교좌표와 그 각도 표현 $(\varphi,\lambda,h)$", size=8.6)

    # NED / Body
    ox, oy = 41.0, 68.0
    eN, eE, eD = (0.42, 0.86), (0.95, -0.27), (0.0, -1.0)
    L = 11.5
    a_, b_ = L * 1.30, L * 1.35
    quad = [(ox + xr(a_ * eN[0] + b_ * eE[0]), oy + a_ * eN[1] + b_ * eE[1]),
            (ox + xr(-a_ * eN[0] + b_ * eE[0]), oy - a_ * eN[1] + b_ * eE[1]),
            (ox + xr(-a_ * eN[0] - b_ * eE[0]), oy - a_ * eN[1] - b_ * eE[1]),
            (ox + xr(a_ * eN[0] - b_ * eE[0]), oy + a_ * eN[1] - b_ * eE[1])]
    ax.add_patch(Polygon(np.array(quad), closed=True, facecolor="#F6F8FC", edgecolor=RULE,
                         lw=0.9, zorder=2))
    for e, name, ln, dx in ((eN, r"$N$", L, 0.0), (eE, r"$E$", L, 0.0),
                            (eD, r"$D$", L * 0.72, -2.6)):
        arrow(ax, (ox, oy), (ox + xr(ln * e[0]), oy + ln * e[1]), color=INK, lw=1.4, ms=10)
        ax.text(ox + xr((ln + 2.8) * e[0] + dx), oy + (ln + 2.8) * e[1], name, fontsize=10.5,
                color=INK, ha="center", va="center", zorder=8)
    psi = 52.0
    c_, s_ = math.cos(math.radians(psi)), math.sin(math.radians(psi))
    xb = (c_ * eN[0] + s_ * eE[0], c_ * eN[1] + s_ * eE[1])
    arrow(ax, (ox, oy), (ox + xr(L * 0.88 * xb[0]), oy + L * 0.88 * xb[1]), color=RED,
          lw=1.8, ms=10, zorder=6)
    ax.text(ox + xr((L * 0.88 + 2.8) * xb[0]), oy + (L * 0.88 + 2.8) * xb[1], r"$x_b$",
            fontsize=10.5, color=RED, ha="center", va="center", zorder=8)
    arc_in_plane(ax, (ox, oy), eN, eE, 6.2, 0.0, psi, color=AMBER, lw=1.5)
    mm = math.radians(psi * 0.5)
    ax.text(ox + xr(8.6 * (math.cos(mm) * eN[0] + math.sin(mm) * eE[0])),
            oy + 8.6 * (math.cos(mm) * eN[1] + math.sin(mm) * eE[1]), r"$\psi$",
            fontsize=11.5, color=AMBER, ha="center", va="center", zorder=8)
    label(ax, 30.0, 93.0, "NED · Body", size=10.4, color=INK, bold=True)
    caption(ax, 30.0, 50.0, r"표적 발밑의 북 · 동 · 아래와 기수 축 $x_b$", size=8.6)

    # 자세각 3종 미니 + 변환 체인
    label(ax, 57.0, 93.0, "자세각 세 개", size=10.4, color=INK, bold=True)
    for i, (nm, sym, shape, c, rot) in enumerate([
            ("Roll", r"$\phi$", PLANE_FRONT, VIOLET, 0.34),
            ("Pitch", r"$\theta$", PLANE_SIDE, AMBER, 0.26),
            ("Yaw", r"$\psi$", PLANE_TOP, BLUE, 0.44)]):
        x = 58.0 + i * 13.5
        polyx(ax, x + 5.0, 78.0, shape, s=0.62, rot=rot, facecolor=c, edgecolor="none",
              zorder=5)
        label(ax, x + 5.0, 86.5, "%s  %s" % (nm, sym), size=9.4, color=c, bold=True,
              ha="center")
        caption(ax, x + 5.0, 68.0, ["날개가 기운다", "기수가 들린다", "기수가 돈다"][i],
                size=8.2, ha="center")
    ax.text(58.0, 59.0, r"$\mathbf{C}^{n}_{b}=\mathbf{R}_{z}(\psi)\mathbf{R}_{y}(\theta)"
                        r"\mathbf{R}_{x}(\phi)$", fontsize=12, color=INK, va="center",
            zorder=6)
    caption(ax, 58.0, 52.0, "곱하는 순서는 Yaw → Pitch → Roll 로 고정한다", size=8.6)

    ax.plot([2, 98], [46.0, 46.0], color=RULE, lw=0.9, zorder=3)

    # ── 아래 띠: 상태 벡터 ──
    label(ax, 2.0, 41.0, "표적 하나의 상태 벡터", size=10.6, color=INK, bold=True)
    ax.text(2.0, 32.0, r"$\mathbf{x}(t)=\left[\;\mathbf{p}^{e\,T}\;\;"
                       r"\mathbf{v}^{e\,T}\;\;\boldsymbol{\Theta}^{T}\;\right]^{T}"
                       r"\in\mathbb{R}^{9}$", fontsize=14, color=INK, va="center",
            zorder=6)
    rows = [(r"$\mathbf{p}^{e}$", "ECEF 위치", "m", BLUE),
            (r"$\mathbf{v}^{e}$", "ECEF 속도", "m/s", GREEN),
            (r"$\boldsymbol{\Theta}$", "자세각 " + r"$(\phi,\theta,\psi)$", "rad", AMBER)]
    for i, (sym, nm, unit, c) in enumerate(rows):
        y = 22.0 - i * 6.6
        ax.add_patch(Rectangle((2.0, y - 2.4), 0.7, 5.0, facecolor=c, edgecolor="none",
                               zorder=4))
        ax.text(4.2, y, sym, fontsize=11.5, color=INK, va="center", zorder=6)
        label(ax, 9.0, y, nm, size=9.2, color=INK2)
        label(ax, 26.0, y, unit, size=8.8, color=GREY, font=MONO)

    ax.plot([33.0, 33.0], [3.0, 40.0], color=RULE, lw=0.9, zorder=3)
    label(ax, 36.0, 41.0, "명세가 주는 입력이 초기 상태가 되는 길", size=10.6, color=INK,
          bold=True)
    give = [("위도 · 경도 · 고도", r"$(\varphi_0,\lambda_0,h_0)$",
             r"$\mathbf{p}^{e}_{0}$", "좌표변환 1회", BLUE),
            ("자세각", r"$(\phi_0,\theta_0,\psi_0)$", r"$\boldsymbol{\Theta}_{0}$",
             "그대로", AMBER),
            ("동체 속력 1개", r"$V$",
             r"$\mathbf{v}^{e}_{0}=\mathbf{C}^{e}_{n}\mathbf{C}^{n}_{b}[V\,0\,0]^{T}$",
             "회전 2회 — 이 과제의 추가 고민 항목", GREEN)]
    for i, (k, sym, out, note, c) in enumerate(give):
        y = 31.0 - i * 9.4
        ax.add_patch(Rectangle((36.0, y - 3.2), 0.7, 6.6, facecolor=c, edgecolor="none",
                               zorder=4))
        label(ax, 38.2, y + 1.2, k, size=9.2, color=INK, bold=True)
        ax.text(53.0, y + 1.2, sym, fontsize=10.5, color=INK2, va="center", zorder=6)
        arrow(ax, (60.0, y + 1.2), (63.0, y + 1.2), color=c, lw=1.2, ms=9)
        ax.text(64.5, y + 1.2, out, fontsize=10.5, color=INK, va="center", zorder=6)
        caption(ax, 38.2, y - 2.0, note, size=8.2)
    save(fig, "figA_state_frames.png")


# ═══════════════════════════════════════════════════════════════════════
# B. 기동 정의와 G → 각속도 · 선회 반경
# ═══════════════════════════════════════════════════════════════════════
def figB():
    fig, ax = canvas(11.2, 4.6)

    # ── 왼쪽 위: 기동 4필드 ──
    label(ax, 2.0, 93.0, "기동 1건 = 값 네 개", size=11.0, color=INK, bold=True)
    ax.plot([2, 46], [89.0, 89.0], color=INK, lw=1.0, zorder=3)
    fields = [("enTurnType", "0 없음 / 1 Roll / 2 Yaw / 3 Pitch", VIOLET),
              ("gravityValue", "크기는 세기, 부호는 회전 방향  [G]", AMBER),
              ("startTime", "이 시각부터 (포함)  [s]", GREEN),
              ("endTime", "이 시각까지 (제외)  [s]", GREEN)]
    for i, (n, d, c) in enumerate(fields):
        y = 83.0 - i * 6.6
        ax.add_patch(Rectangle((2.0, y - 2.0), 0.7, 4.6, facecolor=c, edgecolor="none",
                               zorder=4))
        label(ax, 4.0, y, n, size=9.2, color=INK, font=MONO, bold=True)
        label(ax, 19.0, y, d, size=8.8, color=INK2)
    ax.text(2.0, 54.0, r"$t\in[\,t_{start},\,t_{end})$", fontsize=11.5, color=INK,
            va="center", zorder=6)
    caption(ax, 18.0, 54.0, "끝과 다음 시작이 같아도 겹침이 아니다", size=8.6)
    label(ax, 2.0, 49.0, "실제로 겹치면 계산 전에 TGT_ERR_MANEUVER_OVERLAP", size=8.6,
          color=RED)

    # ── 오른쪽 위: G 유도 ──
    ax.plot([49.0, 49.0], [46.0, 95.0], color=RULE, lw=0.9, zorder=3)
    label(ax, 52.0, 93.0, "G 값 하나가 각속도와 선회 반경을 정한다", size=11.0, color=INK,
          bold=True)
    ax.plot([52, 98], [89.0, 89.0], color=INK, lw=1.0, zorder=3)
    ax.text(53.0, 81.0, r"$a_c=\dfrac{V^{2}}{R}=V\,\omega=n\,g$", fontsize=13,
            color=INK, va="center", zorder=6)
    caption(ax, 53.0, 73.5, "원운동의 구심가속도를 중력가속도의 n 배로 적는다", size=8.6)
    ax.text(53.0, 65.0, r"$\omega=\dfrac{n\,g}{V},\qquad R=\dfrac{V^{2}}{n\,g}$",
            fontsize=14, color=BLUE, va="center", zorder=6)
    label(ax, 53.0, 56.0, "omega = (gravityValue * G_FORCE) / headingSpeed;", size=8.6,
          color=GREY, font=MONO)

    xs = [53.0, 61.0, 79.0]
    for x, hh in zip(xs, ["n [G]", "대함  V = 30 m/s", "대공  V = 200 m/s"]):
        label(ax, x, 50.0, hh, size=8.8, color=INK, bold=True)
    ax.plot([52, 98], [47.5, 47.5], color=RULE, lw=0.8, zorder=3)
    for i, n in enumerate((1.0, 4.0, 8.0)):
        y = 43.5 - i * 4.6
        label(ax, xs[0], y, "%g" % n, size=8.6, color=INK, font=MONO, bold=True)
        for j, V in enumerate((30.0, 200.0)):
            w = math.degrees(n * 9.80665 / V)
            Rr = V * V / (n * 9.80665)
            label(ax, xs[1 + j], y, "%5.1f °/s      R = %s m" %
                  (w, ("%.0f" % Rr) if Rr >= 100 else ("%.1f" % Rr)),
                  size=8.4, color=INK2, font=MONO)
    caption(ax, 52.0, 27.0, "같은 G 라도 빠른 표적은 덜 꺾인다 — 분모에 속력이 있기 때문이다.",
            size=8.6)

    # ── 아래: 실측 응답 ──
    ax.plot([2, 98], [33.0, 33.0], color=RULE, lw=0.9, zorder=3)
    label(ax, 2.0, 28.0, "기동표 6줄이 실제 응답으로 나타난 모습  (대공 표적)", size=10.0,
          color=INK, bold=True)
    rows = read_csv("demo.csv")
    t = col(rows, "time")
    yaw = col(rows, "t2_yaw") % 360.0
    sub = fig.add_axes([0.045, 0.045, 0.415, 0.20])
    sub.plot(t, yaw, color=RED, lw=1.7, zorder=5)
    for a, b, g in [(2.8, 6.7, "-8.2"), (13.3, 17.2, "+8.2"), (18.6, 22.5, "+8.2"),
                    (36.2, 40.1, "-8.2"), (41.5, 45.4, "-8.2"), (53.5, 60.0, "+8.2")]:
        sub.axvspan(a, b, color="#FDE7E7", zorder=1)
        sub.annotate(g, xy=((a + b) / 2.0, 335.0), ha="center", va="center", fontsize=6.4,
                     color=RED, zorder=6)
    sub.set_xlim(0, 60)
    sub.set_ylim(0, 380)
    sub.set_yticks([0, 180, 360])
    sub.set_xlabel("시각 [s]", fontsize=8)
    sub.set_ylabel(r"$\psi$ [°]", fontsize=8)
    sub.grid(True, lw=0.5)
    sub.tick_params(labelsize=7)
    for sp in sub.spines.values():
        sp.set_color("#9AA7BD")

    label(ax, 49.0, 22.0, "색칠한 구간이 기동표에 적은 [start, end) 이고,", size=9.0,
          color=INK2)
    label(ax, 49.0, 17.5, "그 안에서만 기울기가 생긴다. 기울기가 곧 " + r"$\omega$" + " 다.",
          size=9.0, color=INK2)
    label(ax, 49.0, 11.0, "기동 지시는 \"위도 32.1 에서 좌 90도\" 처럼 좌표로 온다.",
          size=9.0, color=GREY)
    label(ax, 49.0, 6.5, "구조체에는 시각 칸만 있어 Core 를 돌려 그 좌표에 닿는", size=9.0,
          color=GREY)
    label(ax, 49.0, 2.0, "시각을 찾아 start / end 로 옮겼다.", size=9.0, color=GREY)
    save(fig, "figB_maneuver_g.png")


# ═══════════════════════════════════════════════════════════════════════
# C. 운동학 모델과 ECEF 적분
# ═══════════════════════════════════════════════════════════════════════
def figC():
    fig, ax = canvas(11.2, 4.4)

    label(ax, 2.0, 93.0, "표적 운동을 지배하는 세 줄", size=11.2, color=INK, bold=True)
    ax.plot([2, 98], [89.0, 89.0], color=INK, lw=1.0, zorder=3)
    eqs = [(r"$\dot{\mathbf{p}}^{e}=\mathbf{v}^{e}$", "위치의 시간 변화율이 곧 속도", BLUE),
           (r"$\mathbf{v}^{e}=\mathbf{C}^{e}_{n}(\varphi,\lambda)\,"
            r"\mathbf{C}^{n}_{b}(\phi,\theta,\psi)\,[\,V\ \ 0\ \ 0\,]^{T}$",
            "속도는 기수 방향 크기 V 를 두 번 돌려 만든다", GREEN),
           (r"$\dot{\boldsymbol{\Theta}}=\boldsymbol{\omega}(t),\quad"
            r"\omega_i=n_i\,g\,/\,V$",
            "자세각은 기동이 걸린 구간에서만 돈다", AMBER)]
    for i, (eq, note, c) in enumerate(eqs):
        x = 2.0 + i * 32.6
        ax.add_patch(Rectangle((x, 66.0), 0.7, 16.0, facecolor=c, edgecolor="none",
                               zorder=4))
        ax.text(x + 2.2, 77.0, eq, fontsize=12.5 if i == 1 else 13.5, color=INK,
                va="center", zorder=6)
        caption(ax, x + 2.2, 68.5, note, size=8.6)

    ax.plot([2, 98], [61.0, 61.0], color=RULE, lw=0.9, zorder=3)

    # 왼쪽: 모델 가정
    label(ax, 2.0, 56.0, "이 모델이 전제하는 것", size=10.4, color=INK, bold=True)
    asm = [("운동학 모델", "힘이 아니라 속도 · 각속도를 직접 준다.",
            "중력 · 추력 · 항력을 풀지 않아 탄도 표적에는 맞지 않는다."),
           ("속력 일정", "회전만 하므로 |v| 가 변하지 않는다.",
            "가속이 필요하면 V 를 시간 함수로 바꿔야 한다."),
           ("오일러각 적분", "자세각을 직접 적분한다.",
            r"$\theta\to\pm 90°$ 에서 짐벌락 — 사원수로 바꾸면 풀린다.")]
    for i, (k, v1, v2) in enumerate(asm):
        y = 47.0 - i * 15.0
        panel(ax, 2.0, y - 8.0, 44.0, 13.0, fill=PANEL, edge=RULE)
        label(ax, 4.0, y + 1.6, k, size=9.4, color=INK, bold=True)
        label(ax, 17.0, y + 1.6, v1, size=8.8, color=INK2)
        ax.text(4.0, y - 3.4, v2, fontsize=8.4, color=GREY, va="center", zorder=6)

    # 오른쪽: 왜 ECEF
    ax.plot([49.0, 49.0], [3.0, 58.0], color=RULE, lw=0.9, zorder=3)
    label(ax, 52.0, 56.0, "왜 ECEF 에서 적분하는가", size=10.4, color=INK, bold=True)

    panel(ax, 52.0, 30.0, 21.5, 20.0, fill="#FDF7F7", edge="#F0D8D8")
    label(ax, 62.75, 46.5, "LLA 에서 직접", size=9.4, color="#B03B3B", bold=True,
          ha="center")
    ax.text(62.75, 41.0, r"$\dot{\varphi}=\dfrac{v_N}{M(\varphi)+h}$", fontsize=11,
            color="#8A3232", ha="center", va="center", zorder=6)
    caption(ax, 62.75, 33.0, "분모가 위도마다 달라\n곧게 날려도 고도가 뜬다", size=8.2,
            ha="center")

    panel(ax, 76.5, 30.0, 21.5, 20.0, fill="#F4FAF7", edge="#CDE7DA")
    label(ax, 87.25, 46.5, "ECEF 에서", size=9.4, color="#0B6B4B", bold=True, ha="center")
    ax.text(87.25, 41.0, r"$\mathbf{p}^{e}_{k+1}=\mathbf{p}^{e}_{k}+\mathbf{v}^{e}\Delta t$",
            fontsize=11, color="#0B6B4B", ha="center", va="center", zorder=6)
    caption(ax, 87.25, 33.0, "미터 단위 직교좌표라\n그냥 더하면 된다", size=8.2, ha="center")
    arrow(ax, (73.8, 40.0), (76.2, 40.0), color=INK2, lw=1.4, ms=10)

    d = 200.0 * 0.1
    drift = d * d / (2.0 * 6378137.0)
    label(ax, 52.0, 24.0, "본 과제 조건에서 실제로 얼마인가", size=9.4, color=INK, bold=True)
    items = [(r"$d=V\Delta t$", r"$20$ m"),
             (r"$d^{2}/2R$", r"$%.1f\ \mu$m/스텝" % (drift * 1e6)),
             ("600 스텝 누적", r"$%.1f$ mm" % (drift * 600.0 * 1e3)),
             ("중점법 보정 후", "고도 불변")]
    for i, (k, v) in enumerate(items):
        x = 52.0 + i * 11.8
        ax.text(x, 17.0, k, fontsize=8.6, color=INK2, va="center", zorder=6)
        ax.text(x, 11.0, v, fontsize=10.0, color=GREEN if i == 3 else INK, va="center",
                zorder=6)
        if i < 3:
            ax.plot([x + 10.6, x + 10.6], [8.0, 19.0], color=RULE, lw=0.7, zorder=2)
    caption(ax, 52.0, 4.5, "값은 작지만, 이런 항이 남으면 간격을 줄여도 오차가 선형으로만 줄어 2차 수렴이 깨진다.",
            size=8.4)
    save(fig, "figC_model_ecef.png")


# ═══════════════════════════════════════════════════════════════════════
# D. 동체 속도 → ECEF 속도와 회전행렬
# ═══════════════════════════════════════════════════════════════════════
def figD():
    fig, ax = canvas(11.2, 4.5)

    label(ax, 2.0, 94.0, "동체 속도를 ECEF 속도로 — 회전 두 번", size=11.2, color=INK,
          bold=True)
    ax.plot([2, 98], [90.0, 90.0], color=INK, lw=1.0, zorder=3)

    steps = [("동체 " + r"$b$", r"$[\,V\ \ 0\ \ 0\,]^{T}$", "기수 방향으로 V", RED, 2.0),
             ("NED " + r"$n$", r"$[\,v_N\ v_E\ v_D\,]^{T}$", "자세각으로 한 번", AMBER, 25.0),
             ("ECEF " + r"$e$", r"$[\,v_x\ v_y\ v_z\,]^{T}$", "위 · 경도로 한 번 더", GREEN, 48.0)]
    for name, val, desc, c, x in steps:
        panel(ax, x, 68.0, 20.0, 18.0, fill=WHITE, edge=c, lw=1.4)
        ax.add_patch(Rectangle((x, 81.5), 20.0, 4.5, facecolor=c, edgecolor="none",
                               zorder=3))
        label(ax, x + 10.0, 83.7, name, size=9.6, color=WHITE, bold=True, ha="center")
        ax.text(x + 10.0, 76.5, val, fontsize=11.5, color=INK, ha="center", va="center",
                zorder=6)
        caption(ax, x + 10.0, 71.0, desc, size=8.4, ha="center")
    for x0, dcm in ((22.2, r"$\mathbf{C}^{n}_{b}$"), (45.2, r"$\mathbf{C}^{e}_{n}$")):
        arrow(ax, (x0, 76.0), (x0 + 2.6, 76.0), color=INK, lw=1.8, ms=12)
        ax.text(x0 + 1.3, 81.0, dcm, fontsize=11.5, color=INK, ha="center", va="center",
                zorder=7)

    panel(ax, 71.0, 68.0, 27.0, 18.0, fill="#F4FAF7", edge="#CDE7DA")
    ax.text(84.5, 80.5, r"$\|\mathbf{v}^{e}\|=V$", fontsize=14, color="#0B6B4B",
            ha="center", va="center", zorder=6)
    caption(ax, 84.5, 74.0, "두 번 다 직교행렬이라 크기가 변하지 않는다.\n"
                            "601 표본에서 속력이 입력값과 완전히 일치했다.", size=8.4,
            ha="center")

    ax.plot([2, 98], [63.0, 63.0], color=RULE, lw=0.9, zorder=3)

    # 두 행렬
    label(ax, 2.0, 58.0, "① 동체 → NED", size=10.0, color=AMBER, bold=True)
    ax.text(17.0, 58.0, r"$\mathbf{C}^{n}_{b}(\phi,\theta,\psi)$", fontsize=11.5,
            color=INK, va="center", zorder=6)
    matrix(ax, 27.0, 36.0,
           [[r"$c\psi\,c\theta$", r"$s\phi\,s\theta\,c\psi-c\phi\,s\psi$",
             r"$c\phi\,s\theta\,c\psi+s\phi\,s\psi$"],
            [r"$c\theta\,s\psi$", r"$s\phi\,s\theta\,s\psi+c\phi\,c\psi$",
             r"$c\phi\,s\theta\,s\psi-s\phi\,c\psi$"],
            [r"$-s\theta$", r"$s\phi\,c\theta$", r"$c\phi\,c\theta$"]],
           cw=16.0, ch=7.0, size=9.2)
    caption(ax, 2.0, 18.0, r"$\phi$ Roll   $\theta$ Pitch   $\psi$ Yaw   "
                           r"($c=\cos,\ s=\sin$)", size=8.4)

    ax.plot([53.0, 53.0], [6.0, 60.0], color=RULE, lw=0.9, zorder=3)
    label(ax, 56.0, 58.0, "② NED → ECEF", size=10.0, color=GREEN, bold=True)
    ax.text(72.0, 58.0, r"$\mathbf{C}^{e}_{n}(L,\lambda)$", fontsize=11.5, color=INK,
            va="center", zorder=6)
    matrix(ax, 77.0, 36.0,
           [[r"$-s L\,c\lambda$", r"$-s\lambda$", r"$-c L\,c\lambda$"],
            [r"$-s L\,s\lambda$", r"$c\lambda$", r"$-c L\,s\lambda$"],
            [r"$c L$", r"$0$", r"$-s L$"]],
           cw=11.5, ch=7.0, size=9.6)
    caption(ax, 56.0, 18.0, r"$L$ 위도   $\lambda$ 경도 — 표적이 움직이면 매 스텝 다시 만든다",
            size=8.4)

    panel(ax, 2.0, 2.0, 96.0, 12.0, fill=PANEL, edge=RULE)
    label(ax, 4.0, 10.6, "쓰는 방법이 위치와 속도에서 다르다", size=9.6, color=INK,
          bold=True)
    ax.text(4.0, 5.6, r"$\mathbf{p}^{e}=\mathbf{C}^{e}_{n}\mathbf{p}^{n}"
                      r"+\mathbf{p}^{e}_{ref}$", fontsize=11, color=INK, va="center",
            zorder=6)
    caption(ax, 22.0, 5.6, "위치는 기준점을 더한다", size=8.6)
    ax.text(44.0, 5.6, r"$\mathbf{v}^{e}=\mathbf{C}^{e}_{n}\mathbf{v}^{n}$",
            fontsize=11, color=GREEN, va="center", zorder=6)
    label(ax, 57.0, 5.6, "속도는 방향뿐이라 기준점을 0 으로 준다 — "
                         "f_Trans_Ned_To_Ecef(..., 0.0, 0.0, 0.0, ...)", size=8.6,
          color=RED)
    save(fig, "figD_velocity_dcm.png")


# ═══════════════════════════════════════════════════════════════════════
# F. 검증 종합 — 세 갈래 + 수렴 차수
# ═══════════════════════════════════════════════════════════════════════
def figF():
    rows = read_csv("spec.csv")
    path = []
    for k in (1, 2):
        pe = lla2ecef(col(rows, "t%d_lat" % k), col(rows, "t%d_lon" % k),
                      col(rows, "t%d_alt" % k))
        path.append(float(np.sum(np.linalg.norm(np.diff(pe, axis=0), axis=1))))
    dh = (col(rows, "t2_alt") - 300.0).max() * 1e6

    dts, errs = [], []
    with open(os.path.join(DATA, "conv.txt"), encoding="utf-8") as fp:
        section = 0
        for line in fp:
            line = line.strip()
            if line.startswith("#"):
                section += 1
                continue
            if not line or line.startswith("dt,"):
                continue
            if section == 2:
                a, b, _ = line.split(",")
                dts.append(float(a))
                errs.append(float(b))
    dts, errs = np.array(dts), np.array(errs)
    slope = float(np.polyfit(np.log(dts), np.log(errs), 1)[0])

    fig, ax = canvas(11.2, 4.4)
    label(ax, 2.0, 94.0, "세 가지 방법으로 검증했다", size=11.2, color=INK, bold=True)
    ax.plot([2, 58], [90.0, 90.0], color=INK, lw=1.0, zorder=3)

    groups = [("① 해석해와 대조", BLUE, "스케일이 틀린 경우를 잡는다",
               [("대함 이동거리", r"$30\times 60=1800$ m", "%+.1e m" % (path[0] - 1800.0)),
                ("대공 이동거리", r"$200\times 60=12000$ m", "%+.1e m" % (path[1] - 12000.0))]),
              ("② 불변량 보존", GREEN, "방향이 틀린 경우를 잡는다",
               [("고도 (수평 비행)", "300 m 유지", r"$%.1f\ \mu$m" % dh),
                ("속력 (회전만)", "200 m/s 유지", "0  (601 표본 일치)"),
                ("위도 · 경도 (직진)", "정서진 · 정남진", r"$0.1\ \mu$m / 0")]),
              ("③ 수렴 차수", AMBER, "적분 방법이 틀린 경우를 잡는다",
               [(r"$\Delta t$ 를 1/2 로", "오차 1/4 (2차)", "기울기 %.3f" % slope)])]
    y = 85.0
    for gname, gc, why, items in groups:
        ax.add_patch(Rectangle((2.0, y - 4.2), 56.0, 5.6, facecolor=gc, edgecolor="none",
                               zorder=3))
        label(ax, 4.0, y - 1.4, gname, size=9.4, color=WHITE, bold=True)
        label(ax, 25.0, y - 1.4, why, size=8.4, color=WHITE)
        y -= 8.0
        for k, (nm, exp, res) in enumerate(items):
            if k % 2 == 0:
                ax.add_patch(Rectangle((2.0, y - 2.7), 56.0, 5.8, facecolor="#F8FAFD",
                                       edgecolor="none", zorder=2))
            label(ax, 4.0, y, nm, size=8.6, color=INK, bold=True)
            ax.text(23.0, y, exp, fontsize=8.4, color=INK2, va="center", zorder=6)
            ax.text(44.0, y, res, fontsize=8.8, color=GREEN, va="center", zorder=6,
                    fontweight="bold")
            y -= 5.8
        y -= 2.6

    caption(ax, 2.0, 12.0, "이동거리는 601개 표본 사이 거리를 전부 더해 잰 값이다 — 직선거리가 아니라")
    caption(ax, 2.0, 7.5, "실제로 지나간 길이. 12 km 를 날아 잔차가 0.0002 mm 수준이다.")
    caption(ax, 2.0, 2.0, "고도 잔차 30.8 µm 는 제공 코드의 ECEF→LLA 반복 근사에서 오는 상수 편차다.".replace("µ", "u"))

    # 수렴 그래프
    sub = fig.add_axes([0.625, 0.175, 0.345, 0.66])
    sub.loglog(dts, errs, "o-", color=BLUE, lw=1.9, ms=5.5, label="실측 오차", zorder=5)
    sub.loglog(dts, errs[0] * (dts / dts[0]) ** 2, "--", color=GREY, lw=1.3,
               label=r"기울기 2 기준선", zorder=4)
    sub.axvline(0.1, color=AMBER, lw=1.2, ls=(0, (4, 3)), zorder=3)
    sub.text(0.107, errs[0] * 0.45, "과제 조건\n0.1 s", fontsize=7.6, color=AMBER,
             fontweight="bold", linespacing=1.5)
    sub.set_xlabel(r"갱신 간격 $\Delta t$ [s]", fontsize=8.6)
    sub.set_ylabel("60 s 뒤 위치 오차 [m]", fontsize=8.6)
    sub.grid(True, which="both", lw=0.5)
    sub.legend(fontsize=7.6, loc="upper left")
    sub.set_title("Yaw 2 G 로 20 s 선회하는 표적", fontsize=9.4, color=INK, pad=6)
    sub.xaxis.set_major_formatter(FuncFormatter(lambda v, _: "%g" % v))
    sub.xaxis.set_minor_formatter(NullFormatter())
    sub.set_xticks([0.01, 0.05, 0.1, 0.5, 1.0])
    sub.yaxis.set_major_formatter(FuncFormatter(
        lambda v, _: ("%g" % v) if v >= 0.01 else ("%.0e" % v).replace("e-0", "e-")))
    sub.tick_params(labelsize=7.4)
    sub.annotate("0.1 s 에서 1.36 cm", xy=(0.1, 1.357e-2), xytext=(0.13, 1.5e-3),
                 fontsize=7.8, color=BLUE, fontweight="bold",
                 arrowprops=dict(arrowstyle="->", color=BLUE, lw=0.9))
    for sp in sub.spines.values():
        sp.set_color("#9AA7BD")
    caption(ax, 62.0, 2.0, "값 하나가 작은 것보다, 간격을 바꿨을 때 정해진 기울기로 줄어드는 것이 더 강한 증거다.",
            size=8.4)
    save(fig, "figF_verify.png")


# ═══════════════════════════════════════════════════════════════════════
# G. 기동 시연과 운용 화면
# ═══════════════════════════════════════════════════════════════════════
def figG():
    rows = read_csv("demo.csv")
    t = col(rows, "time")
    e1, n1 = to_en(col(rows, "t1_lat"), col(rows, "t1_lon"), 32.0, 126.0)
    e2, n2 = to_en(col(rows, "t2_lat"), col(rows, "t2_lon"), 32.0, 126.0)

    fig, ax = canvas(11.2, 4.5)
    label(ax, 2.0, 95.0, "기동 10줄을 얹은 60 s 궤적", size=10.6, color=INK, bold=True)
    label(ax, 40.0, 95.0, "TargetSim — 값을 바꿔 가며 확인하는 화면", size=10.6, color=INK,
          bold=True)

    m1 = fig.add_axes([0.032, 0.135, 0.145, 0.725])
    m1.plot(e2 / 1000.0, n2 / 1000.0, color=RED, lw=1.7, zorder=5)
    m1.plot(e1 / 1000.0, n1 / 1000.0, color=BLUE, lw=1.7, zorder=5)
    for e, n, c in ((e1, n1, BLUE), (e2, n2, RED)):
        m1.plot(e[0] / 1000.0, n[0] / 1000.0, "o", ms=5.5, mfc=WHITE, mec=c, mew=1.5,
                zorder=7)
        m1.plot(e[-1] / 1000.0, n[-1] / 1000.0, "s", ms=4.5, color=c, zorder=7)
    m1.plot([0], [0], "^", ms=8, color=PF, zorder=7)
    m1.set_xlabel("동쪽 [km]", fontsize=8)
    m1.set_ylabel("북쪽 [km]", fontsize=8)
    m1.grid(True, lw=0.5)
    m1.set_aspect("equal", adjustable="box")
    m1.set_xlim(-1.6, 3.6)
    m1.set_ylim(-0.8, 14.8)
    m1.tick_params(labelsize=7)
    for sp in m1.spines.values():
        sp.set_color("#9AA7BD")

    for k, (key, c, nm) in enumerate((("t1_yaw", BLUE, "표적 1  대함"),
                                      ("t2_yaw", RED, "표적 2  대공"))):
        sub = fig.add_axes([0.207, 0.575 - k * 0.440, 0.145, 0.285])
        sub.plot(t, col(rows, key) % 360.0, color=c, lw=1.5)
        sub.set_ylim(-25, 390)
        sub.set_yticks([0, 180, 360])
        sub.set_ylabel(r"$\psi$ [°]", fontsize=7.6)
        sub.grid(True, lw=0.5)
        sub.tick_params(labelsize=7)
        sub.set_title(nm, fontsize=8.2, color=INK, pad=4)
        if k == 1:
            sub.set_xlabel("시각 [s]", fontsize=8)
        else:
            sub.set_xticklabels([])
        for sp in sub.spines.values():
            sp.set_color("#9AA7BD")

    img = plt.imread(os.path.join(OUT, "fig22_ui.png"))
    ui = fig.add_axes([0.375, 0.135, 0.415, 0.725])
    ui.imshow(img)
    ui.set_xticks([]); ui.set_yticks([])
    for sp in ui.spines.values():
        sp.set_color(RULE)
        sp.set_linewidth(0.9)

    for i, (k, d, c) in enumerate([
            ("고치면 곧바로 다시 돈다",
             "표의 칸을 고치면 0.25 s 뒤 자동으로\n다시 계산한다. 601 표본 × 3 객체가\n4.8 ms 라 기다릴 일이 없다.", BLUE),
            ("그림에서 끌어서 배치",
             "시작점과 기수 손잡이를 마우스로 끌면\n위도 · 경도 · Yaw 가 표에 반영된다.\n회전각과 선회 반경은 G 로부터 계산.", AMBER),
            ("결과는 표 · CSV · 그림",
             "시각별 LLA 를 표로 보고 슬라이더로\n되감고 CSV 로 내보낸다. 오늘 숫자가\n전부 여기서 나온 CSV 다.", GREEN)]):
        y = 76.0 - i * 25.0
        ax.add_patch(Rectangle((81.0, y - 14.0), 0.8, 19.0, facecolor=c, edgecolor="none",
                               zorder=4))
        label(ax, 83.0, y + 2.6, k, size=9.2, color=INK, bold=True)
        label(ax, 83.0, y - 5.0, d, size=8.4, color=INK2, space=1.55)

    save(fig, "figG_demo_ui.png")


# ═══════════════════════════════════════════════════════════════════════
# 발표자료가 쓰는 그림 (순서대로). fig22 는 figG 의 재료라 먼저 그린다.
# ═══════════════════════════════════════════════════════════════════════
FIGS = {
    1: fig02,       # 소프트웨어 구성과 인터페이스
    2: figA,        # 표적 상태와 좌표계
    3: figB,        # 기동 정의와 G → 각속도
    4: fig08,       # 회전축 세 가지 응답 (실측)
    5: fig09,       # 구조체 관계와 설계 결정
    6: figC,        # 운동학 모델과 ECEF 적분
    7: figD,        # 동체 속도 → ECEF 속도와 회전행렬
    8: fig14,       # 오일러법과 중점법
    9: fig17,       # 명세 시나리오 궤적
    10: figF,       # 검증 종합과 수렴 차수
    11: fig22,      # TargetSim 화면 (figG 의 재료)
    12: figG,       # 기동 시연과 운용 화면
}

# 질의응답 예비용. 발표자료에는 넣지 않지만 질문이 깊게 들어올 때 꺼내 쓴다.
EXTRA = {
    101: fig01,     # 모의가 대신하는 구간과 비교표
    102: fig03,     # 상태 벡터 상세
    103: fig04,     # 좌표계 4종 상세
    104: fig05,     # 오일러각 3면도 상세
    105: fig06,     # 기동 파라미터 상세
    106: fig07,     # G 유도 상세
    107: fig10,     # 운동학 모델 상세
    108: fig11,     # 왜 ECEF 상세
    109: fig12,     # 속도 변환 상세
    110: fig13,     # DCM 원소 상세
    111: fig15,     # 한 스텝 알고리즘과 비용
    112: fig16,     # 방어적 구현
    113: fig18,     # 불변량 잔차 시계열
    114: fig19,     # 검증표 상세
    115: fig20,     # 수렴 그래프 상세
    116: fig21,     # 기동 시연 상세
}

if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--all"]
    table_ = dict(FIGS)
    if "--all" in sys.argv[1:]:
        table_.update(EXTRA)
    want = [int(a) for a in args] or sorted(table_)
    print("그림 생성 -> %s" % OUT)
    for n in want:
        fn = table_.get(n) or EXTRA.get(n)
        if fn is None:
            sys.exit("그림 번호 %d 없음 (발표용 1~%d, 예비 101~%d)"
                     % (n, max(FIGS), max(EXTRA)))
        fn()
    print("끝 (%d 장)" % len(want))
