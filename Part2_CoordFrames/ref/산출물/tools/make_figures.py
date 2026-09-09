# -*- coding: utf-8 -*-
"""발표자료 그림 생성기. 7장(회전)과 10장(안테나 극좌표) 그림을 PNG 로 뽑는다.

    python make_figures.py              # 두 장 다
    python make_figures.py polar        # 10장 (fig02_polar.png)
    python make_figures.py cartesian    # 11장 (fig17_cartesian.png)
    python make_figures.py rotate       # 7장 (fig05_same_target.png)

결과는 ../../참고 자료/figures/ 에 저장되고 같은 파일이 pptx 에 들어간다.
글꼴은 맑은 고딕이 있으면 그걸 쓰고, 없으면(리눅스) 나눔바른고딕을 쓴다. build_combo.py 와 같은 규칙이다.
각도가 눈에 보이는 대로여야 하므로 그림 영역은 전부 aspect="equal" 로 두고 데이터 좌표에서 직접 그린다.
"""
import math
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import font_manager as fm
from matplotlib import patheffects as pe
from matplotlib.patches import Arc, Circle, FancyArrowPatch, Polygon, Wedge

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.normpath(os.path.join(HERE, "..", "..", "참고 자료", "figures"))

FONT_CANDIDATES = [
    (r"C:\Windows\Fonts\malgun.ttf", r"C:\Windows\Fonts\malgunbd.ttf"),
    ("/usr/share/fonts/truetype/nanum/NanumBarunGothic.ttf",
     "/usr/share/fonts/truetype/nanum/NanumBarunGothicBold.ttf"),
    ("/usr/share/fonts/truetype/nanum/NanumGothic.ttf",
     "/usr/share/fonts/truetype/nanum/NanumGothicBold.ttf"),
]
for _reg, _bold in FONT_CANDIDATES:
    if os.path.exists(_reg) and os.path.exists(_bold):
        REG, BOLD = fm.FontProperties(fname=_reg), fm.FontProperties(fname=_bold)
        break
else:
    sys.exit("한글 글꼴을 못 찾음 (맑은 고딕 / 나눔바른고딕 / 나눔고딕)")

# 색은 다른 그림들이 쓰는 값 그대로
NAVY = "#18202F"        # 제목 · 본문 글자
GREY = "#5A6478"        # 부제 · 설명 글자
FAINT = "#8A93A8"       # 보조선 · 흐린 글자
BLUE = "#2B57A6"        # 축 · 선체 외곽선
HULL = "#E2EAF7"        # 선체 채움
DECK = "#C7D6EE"        # 갑판 구조물 채움
ORANGE = "#C2521C"      # 표적 · 측정값
GREEN = "#2F855A"       # 진북 · NED 계열
WATER = "#9FBADF"       # 물결

# 어두운 배경 슬라이드(2장)용. 배경 #12223E 위에 얹는다
DK_TEXT = "#FFFFFF"     # 굵은 글자
DK_MUTE = "#8FA6CC"     # 설명 글자 (카드 라벨과 같은 색)
DK_GRID = "#5A79AC"     # 격자 · 거리 링
DK_BLUE = "#9CC0F0"     # 안테나 · 배
DK_ORANGE = "#F0894C"   # 표적 · 측정값
DPI = 170


# ---------------------------------------------------------------- 캔버스 도구
def canvas(px_w, px_h, bg="white"):
    """bg=None 이면 배경을 비운다 (어두운 슬라이드에 얹을 때)"""
    fig = plt.figure(figsize=(px_w / DPI, px_h / DPI), dpi=DPI,
                     facecolor=("none" if bg is None else bg))
    lay = fig.add_axes([0, 0, 1, 1], zorder=10)     # 글자 전용 층 (그림 좌표 0~1)
    lay.set_xlim(0, 1)
    lay.set_ylim(0, 1)
    lay.set_axis_off()
    lay.patch.set_alpha(0)
    return fig, lay


def stage(fig, rect, xlim, ylim):
    """rect = [left, bottom, w, h] (그림 비율). 데이터 좌표는 xlim/ylim, 가로세로 1:1"""
    ax = fig.add_axes(rect)
    ax.set_xlim(*xlim)
    ax.set_ylim(*ylim)
    ax.set_aspect("equal", adjustable="box")
    ax.set_axis_off()
    ax.patch.set_alpha(0)
    return ax


def text(ax, x, y, s, size=11, color=NAVY, bold=False, ha="left", va="center", **kw):
    return ax.text(x, y, s, fontproperties=(BOLD if bold else REG), fontsize=size,
                   color=color, ha=ha, va=va, **kw)


def arrow(ax, p0, p1, color=BLUE, lw=1.9, head=10.0, z=5, ls="-", halo=False):
    """halo=True 면 흰 테두리를 둘러 배 위를 지나가도 화살표가 끊겨 보이지 않게 한다"""
    a = FancyArrowPatch(p0, p1, arrowstyle="-|>", mutation_scale=head, lw=lw,
                        color=color, linestyle=ls, shrinkA=0, shrinkB=0,
                        zorder=z, joinstyle="miter")
    if halo:
        a.set_path_effects([pe.withStroke(linewidth=lw + 2.2, foreground="white")])
    ax.add_patch(a)


def line(ax, p0, p1, color=FAINT, lw=1.0, ls="-", z=2, dashes=None):
    ln, = ax.plot([p0[0], p1[0]], [p0[1], p1[1]], color=color, lw=lw, ls=ls, zorder=z,
                  solid_capstyle="butt")
    if dashes:
        ln.set_dashes(dashes)
    return ln


def poly(ax, pts, fc=HULL, ec=BLUE, lw=1.6, z=3, closed=True):
    ax.add_patch(Polygon(pts, closed=closed, facecolor=fc, edgecolor=ec, lw=lw, zorder=z,
                         joinstyle="round"))


def arcdeg(ax, c, r, a0, a1, color=ORANGE, lw=1.7, z=6):
    ax.add_patch(Arc(c, 2 * r, 2 * r, theta1=a0, theta2=a1, color=color, lw=lw, zorder=z))


def arc_between(ax, o, p1, p2, r, color=ORANGE, lw=1.7, z=6, n=48):
    """o 에서 본 두 방향 사이의 각 표시. 사선(축측) 그림에서는 화면 각이 실제 각과 다르지만
    "여기가 그 각" 을 가리키는 표시로는 이것으로 충분하다. 라벨 자리를 돌려준다"""
    a0 = math.degrees(math.atan2(p1[1] - o[1], p1[0] - o[0]))
    a1 = math.degrees(math.atan2(p2[1] - o[1], p2[0] - o[0]))
    if a1 - a0 > 180.0:
        a1 -= 360.0
    if a0 - a1 > 180.0:
        a0 -= 360.0
    pts = [at(o, r, a0 + (a1 - a0) * i / float(n)) for i in range(n + 1)]
    ax.plot([q[0] for q in pts], [q[1] for q in pts], color=color, lw=lw, zorder=z,
            solid_capstyle="round")
    return at(o, r * 1.45, 0.5 * (a0 + a1))


def right_angle(ax, corner, p1, p2, size, color=FAINT, lw=1.1, z=6):
    """corner 에서 p1, p2 쪽으로 난 두 변이 직각임을 알리는 작은 ㄱ 자 표시"""
    def unit(p):
        dx, dy = p[0] - corner[0], p[1] - corner[1]
        h = math.hypot(dx, dy) or 1.0
        return (dx / h, dy / h)
    u, v = unit(p1), unit(p2)
    a = (corner[0] + u[0] * size, corner[1] + u[1] * size)
    b = (corner[0] + (u[0] + v[0]) * size, corner[1] + (u[1] + v[1]) * size)
    c = (corner[0] + v[0] * size, corner[1] + v[1] * size)
    ax.plot([a[0], b[0], c[0]], [a[1], b[1], c[1]], color=color, lw=lw, zorder=z,
            solid_capstyle="butt")


def at(c, r, deg):
    return (c[0] + r * math.cos(math.radians(deg)), c[1] + r * math.sin(math.radians(deg)))


# ------------------------------------------------------------------ 배 모양
# 배 길이를 1.0 으로 본 비율. 선미가 0.0, 선수가 1.0. 구축함 정도 비율(길이:폭 = 8:1)
HALF_BEAM = 0.062       # 평면도 선체 반폭 (배 길이 기준). HULL_TOP 의 최대 |v| 와 같다
HULL_TOP = [(0.00, -0.049), (0.00, 0.049), (0.13, 0.059), (0.56, 0.062),
            (0.73, 0.058), (0.87, 0.045), (0.955, 0.023), (1.00, 0.000),
            (0.955, -0.023), (0.87, -0.045), (0.73, -0.058), (0.56, -0.062),
            (0.13, -0.059)]
HULL_SIDE = [(0.00, -0.018), (0.00, 0.042), (0.34, 0.042), (0.66, 0.048),
             (0.86, 0.060), (1.00, 0.086), (0.988, 0.020), (0.940, -0.010),
             (0.80, -0.026), (0.45, -0.034), (0.14, -0.033)]


def ship_top(ax, x_stern, y_axis, length, z=3, wake=True, ang=0.0):
    """위에서 내려다본 배. ang = 선수가 향하는 각(0 이면 오른쪽, 90 이면 위쪽).
    (x_stern, y_axis) 는 ang = 0 일 때의 선미 중앙이고, 회전은 그 점을 축으로 한다"""
    ca, sa = math.cos(math.radians(ang)), math.sin(math.radians(ang))

    def P(u, v):
        dx, dy = u * length, v * length
        return (x_stern + dx * ca - dy * sa, y_axis + dx * sa + dy * ca)

    poly(ax, [P(u, v) for u, v in HULL_TOP], fc=HULL, ec=BLUE, lw=1.7, z=z)
    poly(ax, [P(0.22, -0.033), P(0.22, 0.033), P(0.36, 0.033), P(0.36, -0.033)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 격납고
    poly(ax, [P(0.40, -0.026), P(0.40, 0.026), P(0.50, 0.026), P(0.50, -0.026)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 연돌
    poly(ax, [P(0.54, -0.038), P(0.54, 0.038), P(0.72, 0.038), P(0.72, -0.038)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 함교
    poly(ax, [P(0.80, -0.026), P(0.80, 0.026), P(0.88, 0.026), P(0.88, -0.026)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 함포
    line(ax, P(0.18, -0.052), P(0.18, 0.052), color=BLUE, lw=0.9, z=z + 1)  # 비행갑판 경계
    ax.add_patch(Circle(P(0.09, 0.0), 0.034 * length, facecolor="none", edgecolor=BLUE,
                        lw=0.9, zorder=z + 1))                    # 헬기 갑판
    if wake:
        for k in (1, 2):
            d = 0.045 * k
            line(ax, P(-0.01, 0.045), P(-0.13 - d, 0.070 + d), color=WATER, lw=1.1, z=z - 1)
            line(ax, P(-0.01, -0.045), P(-0.13 - d, -0.070 - d), color=WATER, lw=1.1, z=z - 1)
    return P


def ship_side(ax, x0, y_water, length, z=3, flip=False, ang=0.0):
    """옆에서 본 배. flip=False 면 선수가 오른쪽, True 면 왼쪽. 흘수선은 y = y_water.
    ang 은 배 한가운데를 축으로 한 기울기(도). 선수가 들리는 쪽이 +"""
    ca, sa = math.cos(math.radians(ang)), math.sin(math.radians(ang))
    px, py = x0 + 0.5 * length, y_water

    def P(u, v):
        uu = (1.0 - u) if flip else u
        dx, dy = x0 + uu * length - px, v * length
        return (px + dx * ca - dy * sa, py + dx * sa + dy * ca)

    poly(ax, [P(u, v) for u, v in HULL_SIDE], fc=HULL, ec=BLUE, lw=1.7, z=z)
    poly(ax, [P(0.16, 0.041), P(0.16, 0.104), P(0.76, 0.104), P(0.76, 0.052)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 갑판실
    poly(ax, [P(0.545, 0.104), P(0.545, 0.180), P(0.700, 0.180), P(0.755, 0.104)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 함교
    poly(ax, [P(0.400, 0.104), P(0.378, 0.198), P(0.442, 0.198), P(0.478, 0.104)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 연돌
    poly(ax, [P(0.78, 0.050), P(0.78, 0.088), P(0.875, 0.088), P(0.875, 0.050)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 함포
    poly(ax, [P(0.548, 0.180), P(0.556, 0.322), P(0.584, 0.322), P(0.592, 0.180)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 마스트
    ax.plot(*zip(P(0.524, 0.272), P(0.616, 0.272)), color=BLUE, lw=1.1, zorder=z + 2)
    return P


# 선미 쪽에서 본 배. 배 폭을 1.0 으로 본 비율. 흘수선이 0.0, 위가 +, 화면 오른쪽이 우현
HULL_STERN = [(-0.20, -0.34), (-0.36, -0.26), (-0.45, -0.10), (-0.48, 0.10),
              (-0.50, 0.42), (0.50, 0.42), (0.48, 0.10), (0.45, -0.10),
              (0.36, -0.26), (0.20, -0.34)]

# 전시기 지도 위 "우리 배" 표식 (2장). 선수가 +v(위) 쪽이고 크기는 지도 단위다
HULL_MARKER = [(0.0, 3.0), (1.6, -0.4), (1.2, -2.5), (-1.2, -2.5), (-1.6, -0.4)]


def ship_stern(ax, x_center, y_water, beam, z=3, ang=0.0):
    """배 뒤에서 선수 쪽을 본 모습. 배가 관측자를 등지고 있어 화면 오른쪽이 우현이다.
    보어사이트가 우현이면 El 이 놓인 수직면이 곧 이 화면이라 각을 그대로 그릴 수 있다.
    ang 은 흘수선 한가운데를 축으로 한 횡경사(도). 우현(화면 오른쪽)이 내려가는 쪽이 +"""
    ca, sa = math.cos(math.radians(-ang)), math.sin(math.radians(-ang))

    def P(u, v):
        dx, dy = u * beam, v * beam
        return (x_center + dx * ca - dy * sa, y_water + dx * sa + dy * ca)

    poly(ax, [P(u, v) for u, v in HULL_STERN], fc=HULL, ec=BLUE, lw=1.7, z=z)
    poly(ax, [P(-0.17, 0.10), P(-0.17, 0.38), P(0.17, 0.38), P(0.17, 0.10)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 격납고 문
    poly(ax, [P(-0.40, 0.42), P(-0.40, 0.72), P(0.40, 0.72), P(0.40, 0.42)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 갑판실
    poly(ax, [P(-0.27, 0.72), P(-0.27, 1.02), P(0.27, 1.02), P(0.27, 0.72)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 상부 구조물
    poly(ax, [P(-0.12, 1.02), P(-0.12, 1.20), P(0.12, 1.20), P(0.12, 1.02)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 마스트 받침
    poly(ax, [P(-0.038, 1.20), P(-0.028, 1.58), P(0.028, 1.58), P(0.038, 1.20)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 마스트
    line(ax, P(-0.15, 1.40), P(0.15, 1.40), color=BLUE, lw=1.1, z=z + 2)
    return P


# ---------------------------------------------------------------- 사선(축측) 투영
# 수평면을 위에서 비스듬히 내려다본 평행투영. 오른손 좌표계가 눈으로도 오른손으로 보인다.
# 첫째 축은 뒤로 물러나고(북 · 선수), 둘째 축은 오른쪽 앞으로 나오고(동 · 우현),
# 셋째 축은 화면 수직이다(아래 또는 위).
ISO_FWD = (0.500, 0.420)
ISO_RGT = (0.940, -0.342)


def iso(o, f, r, d=0.0):
    """평면 위 (앞으로 f, 오른쪽으로 r) 과 아래로 d 를 화면 좌표로 옮긴다"""
    return (o[0] + f * ISO_FWD[0] + r * ISO_RGT[0],
            o[1] + f * ISO_FWD[1] + r * ISO_RGT[1] - d)


def iso_axis(ax, o, key, length, color, label, num=None, tone=None, size=11.5, z=6):
    """원점에서 뻗는 축 하나. key 는 fwd / rgt / down / up"""
    d = {"fwd": ISO_FWD, "rgt": ISO_RGT, "down": (0.0, -1.0), "up": (0.0, 1.0)}[key]
    tip = (o[0] + d[0] * length, o[1] + d[1] * length)
    arrow(ax, o, tip, color=color, lw=2.0, head=11, z=z)
    q = (o[0] + d[0] * length * 1.20, o[1] + d[1] * length * 1.20)
    text(ax, q[0], q[1], label, size=size, color=color, bold=True, ha="center", va="center")
    if num:                                             # 몇 번째로 세는 축인가
        h = math.hypot(*d)
        c = (o[0] + d[0] * length * 0.60 - d[1] / h * 4.4,
             o[1] + d[1] * length * 0.60 + d[0] / h * 4.4)
        ax.add_patch(Circle(c, 2.6, facecolor="white", edgecolor=tone, lw=1.3, zorder=z + 1))
        text(ax, c[0], c[1] - 0.1, num, size=10.5, color=tone, bold=True, ha="center",
             va="center", zorder=z + 2)
    return tip


def ship_iso(ax, o, length, z=3):
    """사선으로 본 배. 갑판은 앞-오른쪽 평면에 놓이고 선체는 아래로 두껍게 그린다.

    P(f, s, d) 에서 f 는 선미 0 ~ 선수 1, s 는 우현 쪽 거리, d 는 아래로 내려간 거리다.
    (모두 배 길이를 1 로 본 비율)
    """
    hull_d = 0.052 * length

    def P(f, s, d=0.0):
        return iso(o, (f - 0.42) * length, s * length, d * length)

    deck = [P(f, -v) for f, v in HULL_TOP]              # 위에서 본 그림은 +v 가 좌현이다
    keel = [P(f, -v, 0.052) for f, v in HULL_TOP]
    poly(ax, keel, fc="#CFDBEE", ec=BLUE, lw=1.0, z=z)
    for i in range(len(deck)):                          # 뱃전. 갑판에 가려 앞쪽만 남는다
        j = (i + 1) % len(deck)
        poly(ax, [deck[i], deck[j], keel[j], keel[i]], fc="#CFDBEE", ec=BLUE, lw=0.9,
             z=z + 1)
    poly(ax, deck, fc=HULL, ec=BLUE, lw=1.6, z=z + 2)

    def box(f0, f1, w, h):
        """갑판 위 구조물. 윗면과 보이는 두 옆면만 그리면 입체로 보인다"""
        A, D_ = P(f0, -w), P(f0, w)
        C = P(f1, w)
        At, Dt, Ct = P(f0, -w, -h), P(f0, w, -h), P(f1, w, -h)
        Bt = P(f1, -w, -h)
        poly(ax, [D_, C, Ct, Dt], fc="#B9CBE6", ec=BLUE, lw=0.8, z=z + 3)   # 우현 면
        poly(ax, [A, D_, Dt, At], fc="#C7D6EE", ec=BLUE, lw=0.8, z=z + 3)   # 선미 면
        poly(ax, [At, Bt, Ct, Dt], fc=DECK, ec=BLUE, lw=0.9, z=z + 4)       # 윗면

    box(0.22, 0.36, 0.033, 0.030)                       # 격납고
    box(0.40, 0.50, 0.026, 0.038)                       # 연돌
    box(0.54, 0.72, 0.038, 0.050)                       # 함교
    box(0.80, 0.88, 0.026, 0.022)                       # 함포
    line(ax, P(0.18, -0.052), P(0.18, 0.052), color=BLUE, lw=0.9, z=z + 3)
    return P


def sea_line(ax, x0, x1, y, z=1, tick=None):
    """수면. 잔물결을 아래쪽에 몇 개 그어서 '옆에서 본 그림' 임을 알려 준다"""
    line(ax, (x0, y), (x1, y), color=WATER, lw=1.5, z=z)
    tick = tick or (x1 - x0) * 0.012
    step = (x1 - x0) / 22.0
    for i in range(22):
        if i % 3 == 1:
            continue
        xa = x0 + i * step
        line(ax, (xa, y - tick), (xa + step * 0.5, y - tick), color=WATER, lw=1.0, z=z)


def ground(ax, y, x0, x1, z=1, tick=None, color=FAINT):
    """설치면. 아래쪽으로 짧은 빗금을 그어 "여기에 볼트로 고정돼 있다" 를 알려 준다"""
    line(ax, (x0, y), (x1, y), color=color, lw=1.4, z=z)
    tick = tick or (x1 - x0) * 0.055
    n = max(int((x1 - x0) / (tick * 1.35)), 2)
    for i in range(n):
        xa = x0 + (x1 - x0) * (i + 0.5) / n
        line(ax, (xa, y), (xa - tick * 0.72, y - tick), color=color, lw=1.0, z=z)


def antenna_face(ax, c, deg, half=3.4, thick=1.5, z=8, color=BLUE):
    """안테나 면. deg 는 보어사이트 방향, 면은 거기 수직"""
    n = math.radians(deg)
    ux, uy = math.cos(n + math.pi / 2), math.sin(n + math.pi / 2)
    vx, vy = math.cos(n), math.sin(n)
    pts = [(c[0] + ux * half - vx * thick / 2, c[1] + uy * half - vy * thick / 2),
           (c[0] + ux * half + vx * thick / 2, c[1] + uy * half + vy * thick / 2),
           (c[0] - ux * half + vx * thick / 2, c[1] - uy * half + vy * thick / 2),
           (c[0] - ux * half - vx * thick / 2, c[1] - uy * half - vy * thick / 2)]
    poly(ax, pts, fc=color, ec=color, lw=1.0, z=z)


# ------------------------------------------------------ 레이다 안테나 한 대만
# 배가 아니라 안테나 자체를 기준으로 설명하는 그림(10장 · 13장)에서 쓴다.
# 세 함수 모두 같은 물체를 세 방향에서 본 것이고, 비율도 같게 잡아 한눈에 같은 물건으로 보인다.
# 크기는 s(배열면 반높이 정도)를 1 로 본 비율이고, 받침 한가운데가 기준점이다.
#
#              ┌───────────┐   배열면 (면 폭 1.24 s · 높이 0.70 s)
#              └─────┬─────┘   하우징 (면 뒤)
#                    │         기둥
#              ══════╧══════   받침  ← 기준점 base
ANT_BASE_HALF = 0.42        # 받침 반폭
ANT_BASE_H = 0.10           # 받침 두께
ANT_POST_HALF = 0.11        # 기둥 반폭
ANT_POST_TOP = 0.62         # 기둥 높이 (= 배열면 아랫변)
ANT_FACE_HALF = 0.62        # 배열면 반폭
ANT_FACE_TOP = 1.32         # 배열면 윗변
ANT_FACE_MID = 0.5 * (ANT_POST_TOP + ANT_FACE_TOP)      # 배열면 한가운데 높이
ANT_BACK = 0.34             # 하우징이 면 뒤로 나온 깊이
ANT_SKIN = 0.06             # 배열면 두께의 절반


def _rot(base, s, ang):
    """받침 한가운데를 축으로 ang 도 돌린 뒤 화면 좌표로 옮기는 함수를 만든다.

    ang 은 화면에서 시계 반대방향이 +. 뒤에서 본 그림(roll)처럼 시계방향이 + 인 곳은
    부르는 쪽에서 부호를 뒤집어 넘긴다.
    """
    ca, sa = math.cos(math.radians(ang)), math.sin(math.radians(ang))

    def P(a, b):
        dx, dy = a * s, b * s
        return (base[0] + dx * ca - dy * sa, base[1] + dx * sa + dy * ca)
    return P


def _ant_stand(ax, P, color, z):
    """받침과 기둥. 세 방향에서 모두 같은 모양이라 따로 뺐다"""
    poly(ax, [P(-ANT_BASE_HALF, 0.0), P(-ANT_BASE_HALF, ANT_BASE_H),
              P(ANT_BASE_HALF, ANT_BASE_H), P(ANT_BASE_HALF, 0.0)],
         fc=DECK, ec=color, lw=1.1, z=z)
    poly(ax, [P(-ANT_POST_HALF, ANT_BASE_H), P(-ANT_POST_HALF, ANT_POST_TOP),
              P(ANT_POST_HALF, ANT_POST_TOP), P(ANT_POST_HALF, ANT_BASE_H)],
         fc=DECK, ec=color, lw=1.1, z=z)


def antenna_front(ax, base, s, ang=0.0, z=6, color=BLUE, grid=True):
    """보어사이트 쪽에서 마주 본 레이다 안테나. 배열면이 직사각형으로 보인다.

    base 는 받침 한가운데. ang 은 그 점을 축으로 한 기울기(도)로, 화면 오른쪽이 내려가면 +.
    (뒤에서 본 그림에서 화면 오른쪽이 우현이므로 roll 의 + 방향과 그대로 맞는다)
    """
    P = _rot(base, s, -ang)
    _ant_stand(ax, P, color, z)
    poly(ax, [P(-ANT_FACE_HALF, ANT_POST_TOP), P(-ANT_FACE_HALF, ANT_FACE_TOP),
              P(ANT_FACE_HALF, ANT_FACE_TOP), P(ANT_FACE_HALF, ANT_POST_TOP)],
         fc=HULL, ec=color, lw=1.6, z=z + 1)
    if grid:                                            # 배열 소자 격자
        for i in range(1, 5):
            u = -ANT_FACE_HALF + 2 * ANT_FACE_HALF * i / 5.0
            line(ax, P(u, ANT_POST_TOP + 0.05), P(u, ANT_FACE_TOP - 0.05),
                 color=color, lw=0.7, z=z + 2)
        for j in range(1, 3):
            v = ANT_POST_TOP + (ANT_FACE_TOP - ANT_POST_TOP) * j / 3.0
            line(ax, P(-ANT_FACE_HALF + 0.04, v), P(ANT_FACE_HALF - 0.04, v),
                 color=color, lw=0.7, z=z + 2)
    return P


def antenna_profile(ax, base, s, ang=0.0, z=6, color=BLUE):
    """옆에서 본 레이다 안테나. 보어사이트는 화면 오른쪽이고 배열면은 얇게 선다.

    base 는 받침 한가운데. ang 은 그 점을 축으로 한 기울기(도)로, 보어사이트 쪽이 들리면 +.
    반환값 P 로 배열면 한가운데 P(ANT_SKIN, ANT_FACE_MID) 를 얻어 축의 원점으로 쓴다.
    """
    P = _rot(base, s, ang)
    _ant_stand(ax, P, color, z)
    poly(ax, [P(-ANT_BACK, ANT_POST_TOP + 0.06), P(-ANT_BACK, ANT_FACE_TOP - 0.06),
              P(-ANT_SKIN, ANT_FACE_TOP), P(-ANT_SKIN, ANT_POST_TOP)],
         fc=DECK, ec=color, lw=1.2, z=z + 1)            # 하우징 (면 뒤)
    poly(ax, [P(-ANT_SKIN, ANT_POST_TOP - 0.04), P(-ANT_SKIN, ANT_FACE_TOP + 0.04),
              P(ANT_SKIN, ANT_FACE_TOP + 0.04), P(ANT_SKIN, ANT_POST_TOP - 0.04)],
         fc=color, ec=color, lw=1.0, z=z + 2)           # 배열면
    return P


def antenna_plan(ax, base, s, ang=0.0, z=6, color=BLUE):
    """위에서 내려다본 레이다 안테나. 배열면이 막대로 보인다.

    base 는 받침 한가운데. ang 은 보어사이트가 향하는 각(도)으로 0 이면 화면 오른쪽.
    반환값 P 로 배열면 한가운데 P(ANT_SKIN, 0.0) 을 얻어 축의 원점으로 쓴다.
    """
    P = _rot(base, s, ang)
    ax.add_patch(Circle(P(-ANT_BACK - 0.06, 0.0), 0.26 * s, facecolor=DECK,
                        edgecolor=color, lw=1.1, zorder=z))     # 받침 (면 뒤에 가린다)
    poly(ax, [P(-ANT_BACK, -ANT_FACE_HALF + 0.20), P(-ANT_BACK, ANT_FACE_HALF - 0.20),
              P(-ANT_SKIN, ANT_FACE_HALF - 0.14), P(-ANT_SKIN, -ANT_FACE_HALF + 0.14)],
         fc=DECK, ec=color, lw=1.2, z=z + 1)            # 하우징 (면 뒤)
    poly(ax, [P(-ANT_SKIN, -ANT_FACE_HALF), P(-ANT_SKIN, ANT_FACE_HALF),
              P(ANT_SKIN, ANT_FACE_HALF), P(ANT_SKIN, -ANT_FACE_HALF)],
         fc=color, ec=color, lw=1.0, z=z + 2)           # 배열면
    return P


def person_top(ax, c, deg, look=26.0, color=BLUE, z=6, view=True):
    """위에서 내려다본 사람. deg 는 보고 있는 방향.

    시야 부채꼴 · 어깨 · 머리 순으로 겹쳐 그리면 "누가 어디를 보고 있다" 가 한눈에 읽힌다.
    반환값은 "앞" 방향 단위벡터로, 부르는 쪽에서 축 화살표를 그릴 때 쓴다.
    """
    u = (math.cos(math.radians(deg)), math.sin(math.radians(deg)))
    v = (-u[1], u[0])
    if view:
        ax.add_patch(Wedge(c, look, deg - 42.0, deg + 42.0, facecolor=color,
                           edgecolor="none", alpha=0.10, zorder=z - 1))

    def P(a, b):
        return (c[0] + u[0] * a + v[0] * b, c[1] + u[1] * a + v[1] * b)
    poly(ax, [P(-2.4, -2.6), P(-0.6, -2.0), P(-0.6, 2.0), P(-2.4, 2.6)],
         fc=HULL, ec=color, lw=1.3, z=z)                 # 어깨
    ax.add_patch(Circle(P(0.4, 0.0), 1.9, facecolor="white", edgecolor=color, lw=1.5,
                        zorder=z + 1))                   # 머리
    poly(ax, [P(2.1, -0.8), P(3.4, 0.0), P(2.1, 0.8)], fc=color, ec=color, lw=0.8,
         z=z + 2)                                        # 코 — 보는 쪽을 알려 준다
    return u


def bubble(ax, x, y, s, size=11.5, color=NAVY, fc="#FFFFFF", ec=None, z=12,
           ha="center", va="center", pad=0.5):
    """말풍선. 일반 예시 그림에서 사람이 하는 말을 그대로 얹는다"""
    return ax.text(x, y, s, fontproperties=BOLD, fontsize=size, color=color,
                   ha=ha, va=va, zorder=z,
                   bbox=dict(boxstyle="round,pad=%.2f" % pad, facecolor=fc,
                             edgecolor=(ec or color), linewidth=1.2))


def beam_wedge(ax, o, deg, length, half_angle=9.0, color=BLUE, z=2, alpha=0.13):
    """보어사이트를 감싸는 흐린 부채꼴. "보어사이트 = 빔 한가운데" 를 눈으로 보여 준다"""
    ax.add_patch(Wedge(o, length, deg - half_angle, deg + half_angle, facecolor=color,
                       edgecolor="none", alpha=alpha, zorder=z))


def axis_mark(ax, c, r, into=True, color=BLUE, lw=1.3, z=8):
    """화면을 뚫는 축 표시. into=True 면 들어가는 방향 ⊗, False 면 나오는 방향 ⊙"""
    ax.add_patch(Circle(c, r, facecolor="white", edgecolor=color, lw=lw, zorder=z))
    if into:
        d = r * 0.70
        line(ax, (c[0] - d, c[1] - d), (c[0] + d, c[1] + d), color=color, lw=lw, z=z + 1)
        line(ax, (c[0] - d, c[1] + d), (c[0] + d, c[1] - d), color=color, lw=lw, z=z + 1)
    else:
        ax.add_patch(Circle(c, r * 0.30, facecolor=color, edgecolor="none", zorder=z + 1))


def curve_arrow(ax, p0, p1, rad=0.35, color=ORANGE, lw=1.8, head=11, z=6):
    """회전 방향을 나타내는 굽은 화살표"""
    ax.add_patch(FancyArrowPatch(p0, p1, arrowstyle="-|>", mutation_scale=head, lw=lw,
                                 color=color, shrinkA=0, shrinkB=0, zorder=z,
                                 connectionstyle="arc3,rad=%.3f" % rad))


def num(v, sign=True):
    """천 단위마다 쉼표를 넣어 자릿수를 헷갈리지 않게 쓴다. 20000 -> 20,000"""
    body = "{:,.0f}".format(abs(v)) if float(v).is_integer() else "{:,}".format(abs(v))
    return ("-" if v < 0 else ("+" if sign else "")) + body


def blip(ax, c, r=1.5, color=None, z=5):
    """표적 점. 가운데 점에 흐린 후광을 둘러 어두운 배경에서도 눈에 띄게 한다"""
    color = color or DK_ORANGE
    ax.add_patch(Circle(c, r * 2.0, facecolor=color, edgecolor="none", alpha=0.30, zorder=z))
    ax.add_patch(Circle(c, r, facecolor=color, edgecolor="none", zorder=z + 1))


def save(fig, name, bg="white"):
    """bg=None 이면 배경 없이(투명하게) 저장한다"""
    if not os.path.isdir(OUT_DIR):
        sys.exit("figures 폴더를 못 찾음: %s" % OUT_DIR)
    path = os.path.join(OUT_DIR, name)
    if bg is None:
        fig.savefig(path, dpi=DPI, transparent=True)
    else:
        fig.savefig(path, dpi=DPI, facecolor=bg)
    plt.close(fig)
    print("생성: %s" % path)


# ============================================== 레이다 화면 vs 전시기 화면
# 도입부를 일반 예시로 바꾸면서 이 그림도 슬라이드에서 빠졌다. 생성기는 남겨 둔다.
SHIP_LLA = (127.307, 36.408)        # 과제 조건의 함선 위치
TGT_LLA = (127.3643, 36.2337)       # 같은 표적을 위경도로 옮긴 값 (31장 최종 출력)


def draw_intro():
    """같은 표적을 레이다는 부채꼴 화면 위 한 점으로, 전시기는 지도 위 한 점으로 찍는다.

    2장은 배경이 어두운 슬라이드라 배경을 비우고 밝은 색으로 그린다.
    왼쪽은 안테나 정면을 기준으로 잰 거리와 각도, 오른쪽은 지구를 기준으로 잰 위경도다.
    지도는 가로세로가 실제 거리 비율과 맞도록 경도 범위를 상자 모양에서 역산해 정한다.
    """
    W, H = 1955, 320
    fig, lay = canvas(W, H, bg=None)

    box = (0.012, 0.040, 0.415, 0.930)
    ar = (box[3] * H) / (box[2] * W)
    TOP = 100.0 * ar
    axA = stage(fig, list(box), (0, 100), (0, TOP))
    axB = stage(fig, [0.573, box[1], box[2], box[3]], (0, 100), (0, TOP))

    # ------------------------------------------------ 왼쪽 : 레이다가 보는 것
    O = (30.0, TOP * 0.82)
    R_OUT, R_IN, AZ = 44.0, 22.0, 30.0
    axA.add_patch(Wedge(O, R_OUT, -34, 6, facecolor=DK_BLUE, edgecolor="none",
                        alpha=0.14, zorder=1))
    for r, lab in ((R_IN, "10 km"), (R_OUT, "20 km")):
        axA.add_patch(Arc(O, 2 * r, 2 * r, theta1=-34, theta2=6, color=DK_GRID,
                          lw=1.0, zorder=2))
        text(axA, O[0] + r, O[1] + 1.6, lab, size=9, color=DK_GRID, ha="center",
             va="bottom")
    line(axA, O, at(O, R_OUT + 6.0, 0.0), color=DK_GRID, lw=1.0, dashes=(5, 4), z=2)
    text(axA, O[0] + R_OUT + 7.5, O[1], "안테나 정면", size=10, color=DK_MUTE, va="center")

    tgt = at(O, R_OUT, -AZ)
    arrow(axA, O, tgt, color=DK_ORANGE, lw=2.2, head=11, z=5)
    blip(axA, tgt)
    text(axA, tgt[0] + 3.6, tgt[1], "표적", size=11, color=DK_TEXT, bold=True, va="center")
    arcdeg(axA, O, 13.0, -AZ, 0.0, color=DK_ORANGE, lw=1.5)
    text(axA, *at(O, 18.5, -AZ / 2), s="30°", size=11, color=DK_ORANGE, bold=True,
         ha="center", va="center")
    text(axA, *at(O, R_OUT * 0.56, -AZ + 9.0), s="20 km", size=11, color=DK_ORANGE,
         bold=True, ha="center", va="center")

    antenna_face(axA, O, 0.0, half=3.0, thick=1.6, color=DK_BLUE)
    text(axA, O[0] - 4.5, O[1], "안테나", size=10, color=DK_BLUE, bold=True,
         ha="right", va="center")

    # ------------------------------------------------ 오른쪽 : 전시기가 보여주는 것
    X0, X1 = 18.0, 82.0
    Y0, Y1 = 0.11 * TOP, 0.89 * TOP
    LAT0, LAT1, LON_MID = 36.12, 36.48, 127.35
    lat_km = (LAT1 - LAT0) * 111.13
    lon_deg = (lat_km * (X1 - X0) / (Y1 - Y0)) / (111.32 * math.cos(math.radians(36.3)))
    LON0, LON1 = LON_MID - lon_deg / 2, LON_MID + lon_deg / 2

    def M(lon, lat):
        return (X0 + (lon - LON0) / (LON1 - LON0) * (X1 - X0),
                Y0 + (lat - LAT0) / (LAT1 - LAT0) * (Y1 - Y0))

    poly(axB, [(X0, Y0), (X1, Y0), (X1, Y1), (X0, Y1)], fc="#1B2E50", ec=DK_GRID,
         lw=1.2, z=1)
    ship, tb = M(*SHIP_LLA), M(*TGT_LLA)
    step = 0.2                                                 # 경도 눈금 간격
    for k in range(int(math.ceil(LON0 / step)), int(math.floor(LON1 / step)) + 1):
        lon = round(k * step, 2)
        line(axB, M(lon, LAT0), M(lon, LAT1), color=DK_GRID, lw=0.8, z=2)
        if abs(M(lon, LAT0)[0] - tb[0]) > 13.0:                # 표적 눈금과 겹치면 생략
            text(axB, M(lon, LAT0)[0], Y0 - 2.4, "%.1f°E" % lon, size=8.5,
                 color=DK_GRID, ha="center", va="top")
    for lat in (36.2, 36.3, 36.4):
        line(axB, M(LON0, lat), M(LON1, lat), color=DK_GRID, lw=0.8, z=2)
        if abs(M(LON0, lat)[1] - tb[1]) > 4.0:
            text(axB, X0 - 2.0, M(LON0, lat)[1], "%.1f°N" % lat, size=8.5,
                 color=DK_GRID, ha="right", va="center")

    line(axB, ship, tb, color=DK_MUTE, lw=1.0, dashes=(3, 3), z=3)
    line(axB, tb, (X0, tb[1]), color=DK_ORANGE, lw=1.0, dashes=(4, 3), z=3)
    line(axB, tb, (tb[0], Y0), color=DK_ORANGE, lw=1.0, dashes=(4, 3), z=3)

    ca, sa = math.cos(math.radians(-45.0)), math.sin(math.radians(-45.0))   # 함수 45°
    poly(axB, [(ship[0] + u * ca - v * sa, ship[1] + u * sa + v * ca)
               for u, v in HULL_MARKER], fc=DK_BLUE, ec=DK_BLUE, lw=1.0, z=5)
    text(axB, ship[0] - 3.6, ship[1] + 1.4, "우리 배", size=10, color=DK_BLUE, bold=True,
         ha="right", va="center")

    blip(axB, tb)
    text(axB, tb[0] + 3.6, tb[1] + 1.2, "표적", size=11, color=DK_TEXT, bold=True,
         ha="left", va="bottom")
    text(axB, X0 - 2.0, tb[1], "36.2337°N", size=10, color=DK_ORANGE, bold=True,
         ha="right", va="center")
    text(axB, tb[0], Y0 - 2.4, "127.3643°E", size=10, color=DK_ORANGE, bold=True,
         ha="center", va="top")

    # ------------------------------------------------ 사이 화살표
    arrow(lay, (0.462, 0.50), (0.538, 0.50), color=DK_MUTE, lw=2.4, head=16, z=5)

    save(fig, "fig15_scope_vs_chart.png", bg=None)


# ============================================================== 10장 : 극좌표
def draw_polar():
    """안테나 극좌표 R / Az / El.

    극좌표는 "안테나가 자기 정면(보어사이트)을 기준으로 잰 값" 이므로 배는 그리지 않는다.
    배를 같이 그리면 선수 · 선미가 눈에 먼저 들어와 Az 를 배 기준 각으로 읽게 된다.
    여기 나오는 각은 전부 안테나 기준이고, 배 기준으로 옮기는 일은 다음 단계(안테나→동체)다.

    축은 참고 문서(CIWS-II) 정의 그대로 : z 가 보어사이트(안테나 면에 수직), y 가 위, x 가 왼쪽.
    Az 는 오른손 법칙대로 위에서 보아 반시계(왼쪽)가 +, El 은 위가 +.

    (a) 는 안테나를 위에서 내려다본 그림이다. 보어사이트가 화면 오른쪽이면 안테나의 왼쪽은
    화면 위쪽이다. 그래서 Az 가 + 인 표적이 화면에서 위로 간다.

    (b) 는 같은 안테나를 안테나의 오른쪽, 즉 (a) 의 화면 아래쪽에서 본 그림이다. 그래야
    화면 오른쪽이 그대로 보어사이트가 되어 (a) 와 좌우가 맞고 El 도 제 각으로 그려진다.
    """
    W, H = 1615, 760
    fig, lay = canvas(W, H)

    box = (0.020, 0.028, 0.462, 0.812)
    ar = (box[3] * H) / (box[2] * W)
    TOP = 100.0 * ar
    axA = stage(fig, list(box), (0, 100), (0, TOP))
    axB = stage(fig, [0.518, box[1], box[2], box[3]], (0, 100), (0, TOP))
    line(lay, (0.501, 0.050), (0.501, 0.900), color="#E4E7ED", lw=1.0, z=1)

    S, OX = 13.5, 26.0                  # 안테나 크기, 안테나(원점) x — 두 패널 공통
    XAX, YAX, ZAX, RAY = 56.0, 34.0, 32.0, 46.0
    AZ, EL = 30.0, 25.0

    # ------------------------------------------------------ (a) 위에서 본 그림
    yc = TOP * 0.520                                   # 안테나 배열면 한가운데 (왼쪽 = 화면 위)
    O = (OX, yc)
    beam_wedge(axA, O, 0.0, XAX * 0.92)
    antenna_plan(axA, (OX - ANT_SKIN * S, yc), S)

    arrow(axA, O, (OX + XAX, yc), color=BLUE, lw=1.9, z=6)
    arrow(axA, O, (OX, yc + YAX), color=BLUE, lw=1.9, z=6)
    text(axA, OX + XAX, yc - 3.0, "z  보어사이트", size=11.5, color=BLUE, bold=True,
         ha="right", va="top")
    text(axA, OX + 3.0, yc + YAX - 1.5, "x  왼쪽", size=11.5, color=BLUE,
         bold=True, ha="left", va="center")

    tgt = at(O, RAY, AZ)
    arrow(axA, O, tgt, color=ORANGE, lw=2.2, head=11, z=7)
    axA.add_patch(Circle(tgt, 1.5, facecolor=ORANGE, edgecolor="none", zorder=8))
    text(axA, tgt[0] + 3.0, tgt[1] + 1.6, "표적", size=11.5, color=NAVY, bold=True, va="bottom")

    arcdeg(axA, O, 13.5, 0.0, AZ)
    text(axA, *at(O, 19.0, AZ / 2), s="Az", size=13, color=ORANGE, bold=True,
         ha="center", va="center")
    text(axA, *at(O, RAY * 0.58, AZ - 7.5), s="R", size=13, color=ORANGE, bold=True,
         ha="center", va="center")

    text(axA, OX - 1.05 * S, yc, "레이다\n안테나", size=10.5, color=BLUE,
         bold=True, ha="right", va="center", linespacing=1.45)

    arrow(axA, (52.0, 10.0), (52.0, 18.0), color=FAINT, lw=1.3, head=9, z=2)  # (b) 를 보는 방향
    text(axA, 52.0, 19.5, "(b)", size=10.5, color=GREY, bold=True, ha="center", va="bottom")
    text(axA, 2.0, 9.0, "원점은 안테나 배열면 한가운데.", size=10.5, color=GREY, va="center")
    text(axA, 2.0, 3.6, "(b) 는 같은 안테나를 화살표 방향에서 본 모습이다.", size=10.5,
         color=GREY, va="center")

    # ------------------------------------------------------ (b) 옆에서 본 그림
    ya = TOP * 0.560                                   # 배열면 한가운데
    Ob = (OX, ya)
    base_b = (OX - ANT_SKIN * S, ya - ANT_FACE_MID * S)
    ground(axB, base_b[1], 9.0, 43.0)
    beam_wedge(axB, Ob, 0.0, XAX * 0.92)
    antenna_profile(axB, base_b, S)
    text(axB, 45.0, base_b[1] - 4.6, "설치면 (갑판 · 마스트)", size=9.5, color=GREY,
         ha="left", va="center")

    arrow(axB, Ob, (OX + XAX, ya), color=BLUE, lw=1.9, z=9)
    arrow(axB, Ob, (OX, ya + ZAX), color=BLUE, lw=1.9, z=9, halo=True)
    text(axB, OX + XAX, ya - 3.0, "z  보어사이트 (수평)", size=11.5, color=BLUE, bold=True,
         ha="right", va="top")
    text(axB, OX + 3.4, ya + ZAX - 1.5, "y  위", size=11.5, color=BLUE, bold=True,
         ha="left", va="center")

    tb = at(Ob, RAY, EL)
    line(axB, (tb[0], ya), tb, color="#BFC5D0", lw=1.0, dashes=(4, 3), z=2)
    arrow(axB, Ob, tb, color=ORANGE, lw=2.2, head=11, z=7)
    axB.add_patch(Circle(tb, 1.5, facecolor=ORANGE, edgecolor="none", zorder=8))
    text(axB, tb[0] + 3.0, tb[1] + 1.6, "표적", size=11.5, color=NAVY, bold=True, va="bottom")

    arcdeg(axB, Ob, 13.5, 0.0, EL)
    text(axB, *at(Ob, 19.0, EL / 2), s="El", size=13, color=ORANGE, bold=True,
         ha="center", va="center")
    text(axB, *at(Ob, RAY * 0.58, EL + 7.5), s="R", size=13, color=ORANGE, bold=True,
         ha="center", va="center")

    text(axB, 2.0, 4.0, "y 는 위가 +.  표적이 수평면보다 위에 있으면 El 도 y 도 양수다.",
         size=10.5, color=GREY, va="center")

    # ------------------------------------------------------ 머리글
    text(lay, 0.038, 0.955, "(a) 위에서 본 그림 — 방위각 Az (Azimuth)", size=14.5, bold=True)
    text(lay, 0.038, 0.913, "위에서 내려다본 모습.  보어사이트에서 왼쪽(반시계)으로 잰 각이 +",
         size=11, color=GREY)
    text(lay, 0.536, 0.955, "(b) 옆에서 본 그림 — 고각 El (Elevation)", size=14.5, bold=True)
    text(lay, 0.536, 0.913, "같은 안테나를 옆에서 본 모습.  수평면에서 위로 잰 각",
         size=11, color=GREY)

    save(fig, "fig02_polar.png")


# ================================================= 좌표계의 필요성 (일반 예시)
def draw_observers():
    """같은 물체를 두 사람이 서로 다른 자리에서 본다.

    좌표계를 아직 하나도 소개하지 않은 자리에서 "왜 기준이 여럿인가" 를 보이는 그림이라,
    레이다도 배도 나오지 않는다. 사람 둘과 물체 하나면 충분하다.

    A 는 동쪽(0°)을, B 는 서쪽에 가까운 190° 를 본다. 물체는 두 사람 사이 아래쪽에 있어서
    A 에게는 오른쪽, B 에게는 왼쪽이 된다. 같은 물체인데 말이 정반대로 나온다는 것이 요점이다.
    """
    W, H = 1500, 760
    fig, lay = canvas(W, H)

    box = (0.030, 0.055, 0.940, 0.790)
    TOP = 100.0 * (box[3] * H) / (box[2] * W)
    ax = stage(fig, list(box), (0, 100), (0, TOP))

    TGT = (58.0, 7.0)
    A, A_DEG = (14.0, 24.0), 0.0
    B, B_DEG = (86.0, 30.0), 190.0

    # ------------------------------------------------------------------ 물체
    poly(ax, [(TGT[0] - 3.2, TGT[1] - 2.6), (TGT[0] + 3.2, TGT[1] - 2.6),
              (TGT[0] + 3.2, TGT[1] + 2.6), (TGT[0] - 3.2, TGT[1] + 2.6)],
         fc=ORANGE, ec=ORANGE, lw=1.2, z=8)
    text(ax, TGT[0], TGT[1] - 4.6, "물체 하나", size=12.5, color=ORANGE, bold=True,
         ha="center", va="top")

    for who, c, deg, side, dist in (("A", A, A_DEG, "오른쪽 21°", "9 m"),
                                    ("B", B, B_DEG, "왼쪽 29°", "7 m")):
        u = person_top(ax, c, deg, look=21.0)
        tip = (c[0] + u[0] * 16.0, c[1] + u[1] * 16.0)
        arrow(ax, (c[0] + u[0] * 4.2, c[1] + u[1] * 4.2), tip, color=BLUE, lw=1.7,
              head=10, z=9)                              # 머리에 겹치지 않게 띄운다
        text(ax, tip[0] + u[0] * 3.2, tip[1] + u[1] * 3.2, "앞", size=11, color=BLUE,
             bold=True, ha="center", va="center")
        line(ax, c, TGT, color="#C7CDD8", lw=1.2, dashes=(4, 3), z=3)
        q = arc_between(ax, c, tip, TGT, 12.0)
        text(ax, q[0], q[1], side, size=11.5, color=ORANGE, bold=True, ha="center",
             va="center")
        text(ax, c[0], c[1] + 4.6, "관측자 %s" % who, size=12.5, color=NAVY, bold=True,
             ha="center", va="bottom")
        mid = ((c[0] + TGT[0]) / 2.0, (c[1] + TGT[1]) / 2.0)
        text(ax, mid[0], mid[1] - 2.2, dist, size=11, color=GREY, ha="center", va="top")

    bubble(ax, 18.0, 36.5, "내 오른쪽에 있는데.", size=12, color=BLUE, fc="#F2F6FC")
    bubble(ax, 79.0, 40.0, "내 왼쪽에 있는데?", size=12, color=BLUE, fc="#F2F6FC")

    # ------------------------------------------------------------------ 글자
    text(lay, 0.030, 0.950, "같은 물체를 두 사람이 본다", size=14.5, bold=True)
    text(lay, 0.030, 0.905,
         "물체도 그 자리, 두 사람도 그 자리.  달라진 것은 어디에 서서 어디를 보느냐뿐이다.",
         size=11, color=GREY)

    save(fig, "fig18_observers.png")


# ============================================ 좌표변환의 기본 동작 (일반 예시)
def draw_two_frames():
    """좌표계 둘을 맞추는 데 필요한 동작은 두 가지뿐이라는 그림.

    구체적인 값(설치 방위 · 레버암)은 일부러 넣지 않는다. 실제 숫자는 02 실전 설계에서
    한 번에 따라가므로, 여기서는 "축을 돌린다 / 원점을 옮긴다" 두 그림이면 된다.
    """
    W, H = 1700, 425
    fig, lay = canvas(W, H)

    box = (0.045, 0.115, 0.400, 0.700)
    TOP = 100.0 * (box[3] * H) / (box[2] * W)
    axA = stage(fig, list(box), (0, 100), (0, TOP))
    axB = stage(fig, [0.545, box[1], box[2], box[3]], (0, 100), (0, TOP))
    line(lay, (0.500, 0.090), (0.500, 0.880), color="#E4E7ED", lw=1.0, z=1)

    def frame(ax, o, deg, tone, xlab, ylab, ln=30.0, lw=2.0, size=12):
        """직각인 축 두 개. deg 는 x 축이 향하는 각"""
        for a, lab in ((deg, xlab), (deg + 90.0, ylab)):
            tip = at(o, ln, a)
            arrow(ax, o, tip, color=tone, lw=lw, head=10, z=6)
            text(ax, *at(o, ln * 1.16, a), s=lab, size=size, color=tone, bold=True,
                 ha="center", va="center")
        ax.add_patch(Circle(o, 1.1, facecolor=tone, edgecolor="none", zorder=7))

    # --------------------------------------------------- ① 회전 : 원점은 같다
    O = (34.0, 8.0)
    frame(axA, O, 0.0, "#8FA8D2", "x", "y")
    frame(axA, O, 38.0, ORANGE, "x′", "y′")
    arcdeg(axA, O, 15.5, 0.0, 38.0, color=ORANGE, lw=1.8)
    text(axA, *at(O, 21.0, 19.0), s="θ", size=14, color=ORANGE, bold=True, ha="center",
         va="center")
    text(axA, O[0], O[1] - 4.4, "원점은 같다", size=11, color=GREY, ha="center", va="top")

    # ------------------------------------------ ② 평행이동 : 축 방향은 같다
    O1, O2 = (14.0, 8.0), (56.0, 17.0)
    frame(axB, O1, 0.0, "#8FA8D2", "x", "y", ln=20.0, lw=1.8, size=11)
    frame(axB, O2, 0.0, ORANGE, "x′", "y′", ln=20.0, lw=1.8, size=11)
    arrow(axB, O1, O2, color=GREEN, lw=2.2, head=12, z=9, halo=True)
    text(axB, (O1[0] + O2[0]) / 2 + 1.0, (O1[1] + O2[1]) / 2 + 3.2, "t", size=14,
         color=GREEN, bold=True, ha="center", va="center")
    text(axB, O1[0], O1[1] - 4.4, "축 방향은 같다", size=11, color=GREY, ha="center",
         va="top")

    # ------------------------------------------------------------------ 글자
    text(lay, 0.045, 0.955, "① 회전 (rotation) — 기준 방향이 다를 때", size=14, bold=True)
    text(lay, 0.045, 0.872, "두 좌표계의 축이 어긋난 만큼 θ 돌려서 맞춘다", size=11,
         color=GREY)
    text(lay, 0.545, 0.955, "② 평행이동 (translation) — 기준 점이 다를 때", size=14,
         bold=True)
    text(lay, 0.545, 0.872, "두 좌표계의 원점이 떨어진 만큼 t 옮겨서 맞춘다", size=11,
         color=GREY)

    save(fig, "fig19_two_frames.png")


# ========================================================== 8장 : 직교좌표 유도
# 나눠 담는 예시 각. 실제 과제 값(Az 30° · El 0°)은 El 이 0 이라 그림으로 못 쓴다.
# 사선 그림에서 x 축은 화면 40° 방향이므로, 표적 방향이 거기 겹치지 않게 Az 를 크게 잡는다
DEC_AZ, DEC_EL, DEC_R = 55.0, 20.0, 60.0


def draw_cartesian():
    """직교좌표 공식이 어디서 나오는지 — 사선 그림 한 장으로.

    축은 참고 문서(CIWS-II 2.1.9) 정의대로 z 보어사이트 · x 왼쪽 · y 위.
    표적 하나를 세 축에 나눠 담는 장면이고, 그 안에 직각삼각형이 두 개 들어 있다.
        ① 세로 삼각형 O-P-T  : 빗변 R,          각 El  ->  밑변 R cos(El), 높이 R sin(El) = y
        ② 가로 삼각형 O-Pz-P : 빗변 R cos(El),  각 Az  ->  밑변 z,         높이 x (왼쪽)

    투영은 이 그림 전용이다. 공용 ISO_RGT 는 가로 성분이 커서, 왼쪽(-ISO_RGT) 으로 가면
    앞으로 간 거리가 상쇄되어 표적이 원점 바로 위에 겹쳐 버린다. 그래서 여기서는
    보어사이트를 화면 오른쪽, 왼쪽 축을 화면 왼쪽 위(얕은 기울기) 로 두어 두 각이 다 벌어져 보이게 한다.
    """
    W, H = 1240, 660
    fig, lay = canvas(W, H)
    ax = stage(fig, [0.020, 0.125, 0.960, 0.855], (0, 100),
               (0, 100 * (0.855 * H) / (0.960 * W)))

    EZ = (1.00, 0.00)                                   # z 보어사이트 : 화면 오른쪽
    EX = (-0.45, 0.62)                                  # x 왼쪽       : 화면 왼쪽 위 (깊이)
    EY = (0.00, 1.00)                                   # y 위         : 화면 위

    def pt(o, z=0.0, x=0.0, y=0.0):
        return (o[0] + z * EZ[0] + x * EX[0] + y * EY[0],
                o[1] + z * EZ[1] + x * EX[1] + y * EY[1])

    AZ, EL, R = 35.0, 18.0, 46.0
    rh = R * math.cos(math.radians(EL))                 # 수평면에 눕힌 길이
    rv = R * math.sin(math.radians(EL))                 # 위로 올라간 길이 (y)
    fwd = rh * math.cos(math.radians(AZ))               # z 성분 (보어사이트)
    lft = rh * math.sin(math.radians(AZ))               # x 성분 (왼쪽)

    O = (22.0, 8.0)
    Pz = pt(O, z=fwd)                                   # z 만큼 간 자리
    P = pt(O, z=fwd, x=lft)                             # 표적을 수평면에 내린 발
    T = pt(O, z=fwd, x=lft, y=rv)                       # 표적

    # ------------------------------------------------------------ 수평면과 축
    poly(ax, [pt(O, -4, -8), pt(O, 46, -8), pt(O, 46, 34), pt(O, -4, 34)],
         fc="#F2F6FC", ec="#DCE3EE", lw=1.0, z=1)
    text(ax, *pt(O, 40, -6), s="수평면", size=10.5, color=FAINT, ha="center")
    text(ax, 1.5, 45.0, "z 보어사이트  ·  x 왼쪽  ·  y 위", size=10.5, color=GREY)

    ytip = pt(O, y=15.0)
    arrow(ax, O, ytip, color="#8FA8D2", lw=1.5, head=10, z=3)
    text(ax, ytip[0] - 1.2, ytip[1], "y축 (위)", size=10.5, color=GREY, ha="right", va="center")
    ztip = pt(O, z=45.0)
    arrow(ax, O, ztip, color="#8FA8D2", lw=1.5, head=10, z=3)
    text(ax, ztip[0] + 1.2, ztip[1], "z축 (보어사이트)", size=10.5, color=GREY, ha="left",
         va="center")
    xtip = pt(O, x=32.0)
    arrow(ax, O, xtip, color="#8FA8D2", lw=1.5, head=10, z=3)
    text(ax, xtip[0] + 0.4, xtip[1] + 1.4, "x축 (왼쪽)", size=10.5, color=GREY, ha="left",
         va="bottom")

    poly(ax, [O, Pz, P], fc="#E3EAF7", ec="none", lw=0, z=2)

    # ------------------------------------- ① 세로 삼각형 : R 을 El 로 나눈다
    line(ax, O, P, color=GREEN, lw=2.2, z=5)
    line(ax, P, T, color=GREEN, lw=2.2, ls="--", z=5)
    right_angle(ax, P, O, T, 2.6, color=GREEN)
    text(ax, T[0] + 1.8, 0.5 * (P[1] + T[1]) + 0.8, "R sin(El)", size=11.5, color=GREEN,
         bold=True, ha="left", va="center")
    text(ax, T[0] + 1.8, 0.5 * (P[1] + T[1]) - 3.4, "= y", size=10.5, color=GREY, ha="left",
         va="center")
    text(ax, O[0] + (P[0] - O[0]) * 0.82 - 1.6, O[1] + (P[1] - O[1]) * 0.82 - 3.4, "R cos(El)",
         size=11.5, color=GREEN, bold=True, ha="left", va="center")

    # -------------------------- ② 가로 삼각형 : 남은 수평 성분을 Az 로 나눈다
    line(ax, O, Pz, color=BLUE, lw=3.0, z=6)
    line(ax, Pz, P, color=BLUE, lw=3.0, z=6)
    right_angle(ax, Pz, O, P, 2.6, color=BLUE)
    text(ax, 0.5 * (O[0] + Pz[0]), O[1] - 3.6, "z", size=15, color=BLUE, bold=True,
         ha="center", va="center")
    text(ax, 0.5 * (Pz[0] + P[0]) + 2.8, 0.5 * (Pz[1] + P[1]), "x", size=15, color=BLUE,
         bold=True, ha="left", va="center")

    arrow(ax, O, T, color=ORANGE, lw=2.5, head=13, z=7, halo=True)
    ax.add_patch(Circle(T, 1.6, facecolor=ORANGE, edgecolor="none", zorder=8))
    text(ax, T[0], T[1] + 2.4, "표적", size=12, color=NAVY, bold=True, ha="center", va="bottom")
    text(ax, O[0] + (T[0] - O[0]) * 0.62 - 2.6, O[1] + (T[1] - O[1]) * 0.62 + 1.4, "R", size=15,
         color=ORANGE, bold=True, ha="right", va="center")

    lab = arc_between(ax, O, Pz, P, 13.0)
    text(ax, lab[0], lab[1] - 0.6, "② Az", size=12, color=ORANGE, bold=True, ha="center",
         va="center")
    lab = arc_between(ax, O, P, T, 26.0)
    text(ax, lab[0] + 1.0, lab[1], "① El", size=12, color=ORANGE, bold=True, ha="left",
         va="center")

    # ------------------------------------------------------ 그림 아래 두 줄
    text(lay, 0.024, 0.075,
         "직각삼각형 하나에서   밑변 = 빗변 × cos ,   높이 = 빗변 × sin .   "
         "이것을 두 번 쓴 것이 공식의 전부다", size=12, bold=True)
    text(lay, 0.024, 0.026,
         "①  R 을 El 로 눕혀  R cos(El) 과 R sin(El) = y 를 얻고,     "
         "②  눕힌 R cos(El) 을 Az 로  z 와 x 에 나눈다", size=10.5, color=GREY)

    save(fig, "fig17_cartesian.png")


# ================================================ 한 표적을 여러 기준에서 재기
# 들어가며를 3장으로 줄이면서 이 그림은 슬라이드에서 빠졌다.
# 카드 네 장이 같은 내용을 더 짧게 담고 있어서다. 되살릴 때를 대비해 생성기는 남겨 둔다.
def draw_one_target():
    """같은 표적 하나를 안테나 · 함선 · 진북 세 기준에서 재면 각도만 30 · 120 · 165 로
    달라진다는 것을 왼쪽에, 그 표적이 지구에서는 어디인지를 오른쪽에 그린다.
    """
    W, H = 1580, 910
    fig, lay = canvas(W, H)
    axL = stage(fig, [0.020, 0.050, 0.500, 0.900], (0, 100),
                (0, 100 * (0.900 * H) / (0.500 * W)))
    axR = stage(fig, [0.580, 0.190, 0.360, 0.620], (0, 100),
                (0, 100 * (0.620 * H) / (0.360 * W)))

    # ------------------------------------------------- 왼쪽 : 세 기준에서 잰 각
    CG, L = (30.0, 60.0), 24.0
    bow, bore = 45.0, -45.0                            # 화면 각도. 선수 45°, 보어사이트 우현
    los = bore - 30.0

    for ang_, lab in ((90.0, "N"), (0.0, "E")):
        arrow(axL, CG, at(CG, 18.0, ang_), color=GREEN, lw=1.8, head=10, z=5)
        text(axL, *at(CG, 20.5, ang_), s=lab, size=12, color=GREEN, bold=True,
             ha="center", va="center")
    line(axL, at(CG, 18.0, 90.0), at(CG, 25.0, 90.0), color=FAINT, lw=1.1,
         dashes=(4, 3), z=2)
    text(axL, CG[0], CG[1] + 26.0, "진북", size=11.5, color=NAVY, bold=True,
         ha="center", va="bottom")

    rad = math.radians(bow)
    P = ship_top(axL, CG[0] - 0.42 * L * math.cos(rad), CG[1] - 0.42 * L * math.sin(rad),
                 L, ang=bow, wake=False)
    # 축 이름은 호(弧) 바깥쪽에 두어 호와 글자가 겹치지 않게 한다
    arrow(axL, CG, at(CG, 35.0, bow), color=BLUE, lw=1.8, head=10, z=6)
    text(axL, *at(CG, 36.5, bow), s="x_b 선수", size=11.5, color=BLUE, bold=True,
         ha="left", va="bottom")
    tip = at(CG, 35.0, bore)                           # 보어사이트와 우현은 같은 방향이다
    arrow(axL, CG, tip, color=BLUE, lw=1.8, head=10, z=6)
    text(axL, tip[0] + 1.6, tip[1] + 1.4, "y_b 우현", size=11.5, color=BLUE, bold=True,
         ha="left", va="bottom")
    text(axL, tip[0] + 1.6, tip[1] - 1.4, "z_a 보어사이트", size=11.5, color=ORANGE,
         bold=True, ha="left", va="top")

    ant = P(0.34, -0.062)
    antenna_face(axL, ant, bore, half=2.4, thick=1.4, color=ORANGE)
    arrow(axL, ant, at(ant, 16.0, bore - 90.0), color=ORANGE, lw=1.8, head=10, z=6)
    text(axL, *at(ant, 17.5, bore - 90.0), s="y_a", size=11.5, color=ORANGE, bold=True,
         ha="right", va="center")

    tgt = at(ant, 42.0, los)
    arrow(axL, ant, tgt, color=NAVY, lw=2.4, head=12, z=6)
    axL.add_patch(Circle(tgt, 1.8, facecolor=NAVY, edgecolor="none", zorder=8))
    text(axL, tgt[0] + 3.2, tgt[1] + 1.0, "표적은 하나", size=12.5, color=NAVY, bold=True,
         ha="left", va="bottom")
    text(axL, tgt[0] + 3.2, tgt[1] - 1.0, "안테나에서 20 km", size=10, color=FAINT,
         ha="left", va="top")

    arcs = ((32.0, los, 90.0, GREEN, "진북에서 165°", 62.0),
            (25.0, los, bow, BLUE, "선수에서 120°", 54.0),
            (18.0, los, bore, ORANGE, "보어사이트에서 30°", 46.0))
    for r, a0, a1, col, lab, ly in arcs:                # 라벨은 오른쪽에 세로로 모아 둔다
        arcdeg(axL, CG, r, a0, a1, color=col, lw=1.6)
        text(axL, 68.0, ly, lab, size=11, color=col, bold=True, ha="left", va="center")

    # ------------------------------------------------------- 오른쪽 : 지구에서 보면
    TR = axR.get_ylim()[1]
    C, R = (46.0, TR * 0.52), 30.0
    axR.add_patch(Circle(C, R, facecolor=HULL, edgecolor="#3D4A5F", lw=1.6, zorder=2))
    axR.add_patch(Arc(C, R * 0.9, 2 * R, theta1=0, theta2=360, color="#8A93A8",
                      lw=1.0, linestyle=(0, (4, 3)), zorder=3))
    line(axR, (C[0] - R, C[1]), (C[0] + R, C[1]), color="#8A93A8", lw=1.0,
         dashes=(5, 4), z=3)
    text(axR, C[0] - R + 2.0, C[1] + 2.0, "적도", size=10, color=FAINT, va="bottom")
    text(axR, C[0], C[1] - R - 2.5, "그리니치 자오선", size=10, color=FAINT,
         ha="center", va="top")

    arrow(axR, C, (C[0], C[1] + R + 10.0), color=NAVY, lw=1.7, head=10, z=6)
    arrow(axR, C, (C[0] + R + 12.0, C[1]), color=NAVY, lw=1.7, head=10, z=6)
    text(axR, C[0], C[1] + R + 11.5, "Z  북극", size=11.5, color=NAVY, bold=True,
         ha="center", va="bottom")
    text(axR, C[0] + R + 13.5, C[1], "X", size=11.5, color=NAVY, bold=True, va="center")
    axR.add_patch(Circle(C, 1.0, facecolor=NAVY, edgecolor="none", zorder=7))
    text(axR, C[0] + 2.0, C[1] - 4.0, "지구 중심 = ECEF 원점", size=10, color=GREY,
         va="center")

    ptt = at(C, R, 36.0)
    line(axR, C, ptt, color=ORANGE, lw=1.2, dashes=(4, 3), z=5)
    arcdeg(axR, C, 13.0, 0.0, 36.0, color=ORANGE, lw=1.4)
    text(axR, *at(C, 17.0, 18.0), s="위도", size=10.5, color=ORANGE, bold=True,
         ha="left", va="center")
    axR.add_patch(Circle(ptt, 2.0, facecolor=ORANGE, edgecolor="none", zorder=8))
    text(axR, ptt[0] + 3.0, ptt[1] + 3.0, "배와 표적", size=11, color=NAVY, bold=True,
         ha="left", va="bottom")
    text(axR, ptt[0] + 3.0, ptt[1] + 1.0, "북위 36°, 동경 127°", size=10, color=GREY,
         ha="left", va="top")

    text(lay, 0.580, 0.905, "지구에서 보면", size=13, color=NAVY, bold=True)
    text(lay, 0.580, 0.175, "NED  :  배 위치에서 잰 북 · 동 · 아래", size=11, color=GREY)
    text(lay, 0.580, 0.118, "ECEF :  지구 중심에서 잰 (X, Y, Z) m", size=11, color=GREY)
    text(lay, 0.580, 0.061, "LLA  :  위도 · 경도 · 고도 (지도 좌표)", size=11, color=GREY)
    text(lay, 0.500, 0.014,
         "NED = North-East-Down    ·    ECEF = Earth-Centered Earth-Fixed"
         "    ·    LLA = Latitude-Longitude-Altitude",
         size=10, color=FAINT, ha="center")

    save(fig, "fig14_one_target.png")


# ========================================================== 8장 : 회전행렬 유도
def draw_derive():
    """삼각함수 덧셈정리에서 2차원 회전식이 나오는 과정.

    슬라이드에서 7.50 in 로 줄여 놓기 때문에 그림 안 글자는 13 pt 근처로 잡아야
    화면에서 12 pt 정도로 읽힌다. 슬라이드 제목과 겹치는 그림 안 머리글은 두지 않는다.
    """
    W, H = 1334, 689
    fig, lay = canvas(W, H)
    ax = stage(fig, [0.020, 0.055, 0.345, 0.890], (0, 100),
               (0, 100 * (0.890 * H) / (0.345 * W)))

    O, R, A, T = (14.0, 22.0), 72.0, 25.0, 42.0        # 원점, 반지름, 처음 각, 더 돌린 각
    p = at(O, R, A)
    q = at(O, R, A + T)

    arrow(ax, O, (96.0, O[1]), color=GREY, lw=1.3, head=9, z=3)
    arrow(ax, O, (O[0], 118.0), color=GREY, lw=1.3, head=9, z=3)
    text(ax, 97.5, O[1], "x", size=12, color=GREY, va="center")
    text(ax, O[0], 119.5, "y", size=12, color=GREY, ha="center", va="bottom")

    for pt_, col in ((p, "#C9CDD8"), (q, "#E2C3AE")):
        line(ax, pt_, (pt_[0], O[1]), color=col, lw=1.0, dashes=(4, 3), z=2)
        line(ax, pt_, (O[0], pt_[1]), color=col, lw=1.0, dashes=(4, 3), z=2)

    arrow(ax, O, p, color=GREY, lw=2.0, head=11, z=5)
    arrow(ax, O, q, color=ORANGE, lw=2.3, head=12, z=6)
    ax.add_patch(Circle(p, 1.6, facecolor=GREY, edgecolor="none", zorder=7))
    ax.add_patch(Circle(q, 1.6, facecolor=ORANGE, edgecolor="none", zorder=7))
    text(ax, p[0] + 2.4, p[1] - 2.0, "P (x, y)", size=12, color=NAVY, bold=True, va="top")
    text(ax, q[0] + 2.4, q[1] + 2.0, "P' (x', y')", size=12, color=ORANGE, bold=True,
         va="bottom")

    arcdeg(ax, O, 22.0, 0.0, A, color=GREY, lw=1.4)
    text(ax, *at(O, 27.0, A / 2), s="α", size=13, color=GREY, ha="center", va="center")
    arcdeg(ax, O, 46.0, A, A + T, color=ORANGE, lw=1.6)
    text(ax, *at(O, 51.5, A + T / 2), s="θ", size=13.5, color=ORANGE, bold=True,
         ha="center", va="center")

    # ---------------------------------------------------------------- 유도 세 단계
    x0, y = 0.415, 0.905
    steps = [
        (GREY, 11, "원래 점을 극좌표로 쓰면", False),
        (NAVY, 12.5, "x = r·cos α ,   y = r·sin α", False),
        (None, 0, "", False),
        (GREY, 11, "θ 만큼 더 돌리면 각이 α+θ 가 되므로", False),
        (NAVY, 12.5, "x' = r·cos(α+θ) = r·cosα·cosθ - r·sinα·sinθ", False),
        (NAVY, 12.5, "y' = r·sin(α+θ) = r·sinα·cosθ + r·cosα·sinθ", False),
        (None, 0, "", False),
        (GREY, 11, "r·cosα = x,  r·sinα = y  를 되돌려 넣으면", False),
        (ORANGE, 13, "x' = x·cos θ - y·sin θ", True),
        (ORANGE, 13, "y' = x·sin θ + y·cos θ", True),
        (None, 0, "", False),
        (GREY, 11, "이 두 줄을 표로 적은 것이 Rz(θ) 다.", False),
    ]
    for col, size, txt, bold in steps:
        if col is None:
            y -= 0.040
            continue
        text(lay, x0, y, txt, size=size, color=col, bold=bold, va="top")
        y -= 0.072

    save(fig, "fig04_rot_derive.png")


# ================================================ 10장 : 동체 좌표계와 설치 정렬
def draw_body():
    """동체(Body) 좌표계 FRD 와, 안테나가 그 좌표계 안에 어떻게 붙어 있는지.

    왼쪽은 좌표계 정의라 배를 그린다. 원점이 배의 무게중심이고 세 축이 배에 붙어 있다는 것이
    이 그림의 내용이기 때문이다.

    오른쪽은 안테나 -> 동체(2단계)에 필요한 값 세 가지다.
        설치 방위 : 선수를 기준으로 안테나가 보는 각      -> 위에서 본 그림
        백틸트    : 안테나 면이 위로 들린 각              -> 선미에서 본 그림
        레버암    : 무게중심에서 안테나까지의 거리        -> 두 그림에 x 와 z 를 나눠 표시

    자세각(roll · pitch · yaw)은 여기 그리지 않는다. 그것은 이 좌표계에서 나갈 때
    (동체 -> NED, 3단계) 쓰는 값이고, 안테나가 동체에 볼트로 고정돼 있어서 2단계에는
    아예 들어오지 않는다. 이름은 11장 INS 에서, 수식은 15장 변환 체인에서 나온다.

    판형은 슬라이드 본문 폭을 그대로 쓴다(12.10 x 3.52 in). 세 칸을 좁게 나누면 치수선과
    글자가 서로 밟기 때문이다. 그래서 평면도는 선수를 오른쪽으로 눕혀 가로로 길게 그리고,
    치수선은 배 위(평면도) 와 배 왼쪽(선미도) 으로 서로 다른 쪽에 뺀다.

    백틸트는 이번 과제 값이 0° 라 그대로 그리면 각이 보이지 않는다. 그림은 정의가 보이도록
    기울여 그리고, 실제 값은 아래 줄에 따로 적는다. (직교좌표 그림에서 El 을 20° 로 잡은 것과
    같은 이유다)
    """
    W, H = 1800, 523
    fig, lay = canvas(W, H)

    axL = stage(fig, [0.015, 0.175, 0.250, 0.560], (0, 100),
                (0, 100 * (0.560 * H) / (0.250 * W)))
    axA = stage(fig, [0.300, 0.175, 0.320, 0.560], (0, 100),
                (0, 100 * (0.560 * H) / (0.320 * W)))
    axB = stage(fig, [0.660, 0.175, 0.320, 0.560], (0, 100),
                (0, 100 * (0.560 * H) / (0.320 * W)))
    line(lay, (0.278, 0.115), (0.278, 0.900), color="#E4E7ED", lw=1.0, z=1)
    line(lay, (0.641, 0.165), (0.641, 0.740), color="#EDEFF3", lw=1.0, z=1)

    # ------------------------------------------------------- 왼쪽 : FRD 정의
    # 사선으로 그려야 z 가 아래로 내려가는 것이 기호 없이 그대로 보인다
    TL = axL.get_ylim()[1]
    cg = (32.0, TL * 0.56)                              # 무게중심 = 원점
    ship_iso(axL, cg, 44.0)
    axL.add_patch(Circle(cg, 1.1, facecolor=NAVY, edgecolor="none", zorder=9))
    for key, ln, lab, ha, va, k in (("fwd", 27.0, "x  선수(앞)", "left", "bottom", 1.06),
                                    ("rgt", 26.0, "y  우현(오른쪽)", "left", "center", 1.07),
                                    ("down", 21.0, "z  아래", "center", "top", 1.12)):
        d = {"fwd": ISO_FWD, "rgt": ISO_RGT, "down": (0.0, -1.0)}[key]
        arrow(axL, cg, (cg[0] + d[0] * ln, cg[1] + d[1] * ln), color=BLUE, lw=2.0,
              head=11, z=9)
        text(axL, cg[0] + d[0] * ln * k + (1.4 if ha == "left" else 0.0),
             cg[1] + d[1] * ln * k - (1.4 if va == "top" else 0.0),
             lab, size=10.5, color=BLUE, bold=True, ha=ha, va=va)

    # ------------------------------- 가운데 : 위에서 본 그림 (설치 방위 · 레버암 x)
    # 선수를 오른쪽으로 눕힌다. 이 시점에서 화면 아래쪽이 우현이라 보어사이트가 아래를 본다
    L, XS, YA = 60.0, 8.0, 30.0
    Ps = ship_top(axA, XS, YA, L, ang=0.0, wake=False)
    CG = Ps(0.45, 0.0)
    ANT = Ps(0.26, 0.0)                                 # 무게중심보다 선미 쪽
    HB = HALF_BEAM * L                                  # 선체 반폭

    arrow(axA, CG, (CG[0] + 40.0, YA), color=BLUE, lw=1.8, head=11, z=9)
    text(axA, CG[0] + 41.5, YA, "x_b  선수", size=10.5, color=BLUE, bold=True,
         ha="left", va="center")

    Pa = antenna_plan(axA, ANT, 8.4, ang=-90.0)         # 보어사이트가 아래쪽 = 우현
    face = Pa(ANT_SKIN, 0.0)
    arrow(axA, face, (face[0], face[1] - 15.0), color=ORANGE, lw=1.8, head=11, z=10)
    text(axA, face[0], face[1] - 16.5, "z_a  보어사이트", size=10.5, color=ORANGE,
         bold=True, ha="center", va="top")
    line(axA, ANT, (ANT[0] + 17.0, ANT[1]), color=FAINT, lw=1.0, dashes=(4, 3), z=8)
    arcdeg(axA, ANT, 11.0, -90.0, 0.0)
    text(axA, *at(ANT, 17.5, -44.0), s="설치 방위 90°", size=10.5, color=ORANGE,
         bold=True, ha="left", va="center")

    # 레버암의 선수 방향 성분은 배 위쪽에 치수선으로 뺀다
    yd = YA + HB + 8.0
    for x in (CG[0], ANT[0]):
        line(axA, (x, YA + HB + 1.5), (x, yd + 1.5), color=FAINT, lw=0.9, dashes=(3, 3),
             z=2)
    arrow(axA, (CG[0], yd), (ANT[0], yd), color=GREEN, lw=1.6, head=9, z=9)
    text(axA, 0.5 * (CG[0] + ANT[0]), yd + 2.6, "레버암  x = -30 m", size=10,
         color=GREEN, bold=True, ha="center", va="bottom")

    axA.add_patch(Circle(CG, 1.2, facecolor=NAVY, edgecolor="white", lw=0.8, zorder=10))
    line(axA, CG, (CG[0] + 8.0, YA + HB + 3.4), color=FAINT, lw=0.9, dashes=(3, 3), z=9)
    text(axA, CG[0] + 8.8, YA + HB + 3.6, "무게중심", size=9.5, color=NAVY, ha="left",
         va="center")

    # ----------------------------- 오른쪽 : 선미에서 본 그림 (백틸트 · 레버암 z)
    BEAM, XC, YW = 17.0, 46.0, 7.0
    ship_stern(axB, XC, YW, BEAM)                       # 화면 오른쪽이 우현
    cgB = (XC, YW + 0.10 * BEAM)
    baseB = (XC, YW + 1.58 * BEAM)                      # 마스트 꼭대기
    line(axB, cgB, baseB, color=GREEN, lw=1.0, dashes=(3, 3), z=9)
    axB.add_patch(Circle(cgB, 1.2, facecolor=NAVY, edgecolor="white", lw=0.8, zorder=10))
    line(axB, cgB, (XC + 9.5, cgB[1]), color=FAINT, lw=0.9, dashes=(3, 3), z=9)
    text(axB, XC + 10.5, cgB[1], "무게중심", size=9.5, color=NAVY, ha="left", va="center")

    TILT = 18.0                                         # 그림용 각. 실제 값은 0°
    Pt = antenna_profile(axB, baseB, 7.0, ang=TILT)
    faceB = Pt(ANT_SKIN, ANT_FACE_MID)
    line(axB, faceB, (faceB[0] + 21.0, faceB[1]), color=FAINT, lw=1.0, dashes=(4, 3), z=8)
    arrow(axB, faceB, at(faceB, 15.0, TILT), color=ORANGE, lw=1.8, head=11, z=10)
    arcdeg(axB, faceB, 10.5, 0.0, TILT)
    text(axB, *at(faceB, 15.5, 6.0), s="백틸트", size=10.5, color=ORANGE, bold=True,
         ha="left", va="center")

    # 레버암의 아래 방향 성분은 배 왼쪽에 뺀다. z 는 아래가 + 라 위에 달린 안테나가 음수다
    zd = XC - 0.5 * BEAM - 9.0
    for y in (cgB[1], baseB[1]):
        line(axB, (zd - 1.2, y), (XC - 2.0, y), color=FAINT, lw=0.9, dashes=(3, 3), z=2)
    arrow(axB, (zd, cgB[1]), (zd, baseB[1]), color=GREEN, lw=1.6, head=9, z=9)
    text(axB, zd - 2.2, 0.5 * (cgB[1] + baseB[1]), "레버암\nz = -10 m", size=10,
         color=GREEN, bold=True, ha="right", va="center", linespacing=1.35)

    # ------------------------------------------------------------------ 글자
    text(lay, 0.015, 0.945, "동체(Body) 좌표계 — FRD", size=13, bold=True)
    text(lay, 0.015, 0.900,
         "FRD = Forward-Right-Down.\n원점은 무게중심. 세 축은 배에 붙어 함께 움직인다.",
         size=9.5, color=FAINT, va="top", linespacing=1.5)
    text(lay, 0.300, 0.945, "안테나는 이 좌표계 안에 이렇게 붙어 있다", size=13, bold=True)
    text(lay, 0.300, 0.893, "설치 방위 · 백틸트 · 레버암 — 이 셋이 안테나 → 동체 변환의 전부다",
         size=11.5, color=ORANGE, bold=True)
    text(lay, 0.300, 0.845,
         "볼트로 고정된 값이라 배가 흔들려도 변하지 않는다 — 도크에서 한 번 재면 끝이다.",
         size=9.5, color=FAINT)

    text(lay, 0.460, 0.120, "위에서 본 그림  (선수가 오른쪽)", size=10.5, color=NAVY,
         bold=True, ha="center")
    text(lay, 0.820, 0.120, "선미에서 본 그림  (우현이 오른쪽)", size=10.5, color=NAVY,
         bold=True, ha="center")
    text(lay, 0.300, 0.055,
         "이번 과제 값 :  설치 방위 90°  ·  백틸트 0° (그림은 정의가 보이도록 기울여 그렸다)"
         "  ·  레버암 (-30, 0, -10) m", size=10, color=NAVY)

    save(fig, "fig06_body.png")


# ====================================================== 25장 : 세 각이 더해지는 그림
def draw_layout():
    """진북에서 선수 45°, 선수에서 안테나 90°, 보어사이트에서 측정 30°.
    셋을 더하면 진북 기준 165° 가 된다는 것을 한 그림에서 보인다.
    """
    W, H = 918, 850
    fig, lay = canvas(W, H)
    ax = stage(fig, [0.030, 0.030, 0.940, 0.940], (0, 100),
               (0, 100 * (0.940 * H) / (0.940 * W)))

    CG, L = (40.0, 59.0), 34.0
    HEAD, MOUNT, AZ = 45.0, 90.0, 30.0                 # 선수 방위 · 설치 방위 · 측정 방위
    bow = 90.0 - HEAD                                  # 화면 각도로 바꾼 값
    bore = bow - MOUNT
    los = bore - AZ

    line(ax, CG, (CG[0], CG[1] + 24.0), color=FAINT, lw=1.2, dashes=(5, 4), z=2)
    arrow(ax, (CG[0], CG[1] + 20.0), (CG[0], CG[1] + 25.0), color=GREY, lw=1.4, head=9, z=3)
    text(ax, CG[0], CG[1] + 26.5, "진북", size=12, color=NAVY, bold=True, ha="center",
         va="bottom")

    rad = math.radians(bow)
    P = ship_top(ax, CG[0] - 0.42 * L * math.cos(rad), CG[1] - 0.42 * L * math.sin(rad),
                 L, ang=bow, wake=False)
    ax.add_patch(Circle(CG, 1.1, facecolor=BLUE, edgecolor="none", zorder=8))
    line(ax, CG, at(CG, 0.62 * L, bow), color=FAINT, lw=1.1, dashes=(5, 4), z=2)

    ant = P(0.34, -0.062)                              # 우현 뱃전
    antenna_face(ax, ant, bore, half=3.0, thick=1.6, color=ORANGE)

    # ① 과 ② 는 같은 반지름으로 이어 그려 진북 -> 선수 -> 보어사이트 가 한 흐름으로 보이게 한다
    arcdeg(ax, CG, 25.0, bow, 90.0, color=GREY, lw=1.5)
    text(ax, *at(CG, 27.0, 70.0), s="① 선수 45°", size=12, color=NAVY, bold=True,
         ha="left", va="bottom")
    arcdeg(ax, CG, 25.0, bore, bow, color=GREY, lw=1.5)
    text(ax, *at(CG, 27.0, 14.0), s="② 안테나 설치 90°", size=12, color=NAVY, bold=True,
         ha="left", va="center")

    # 보어사이트 화살표 위쪽에 글자를 놓아 시선(파란 화살표)과 겹치지 않게 한다
    arrow(ax, ant, at(ant, 24.0, bore), color=ORANGE, lw=1.8, head=11, z=6, ls=(0, (5, 3)))
    text(ax, *at(ant, 25.5, bore + 4.0), s="안테나가 보는 방향", size=11.5, color=ORANGE,
         bold=True, ha="left", va="bottom")
    arcdeg(ax, ant, 17.0, los, bore, color=ORANGE, lw=1.6)
    text(ax, *at(ant, 19.0, (los + bore) / 2), s="③ 측정 Az 30°", size=12,
         color=ORANGE, bold=True, ha="left", va="top")

    tgt = at(ant, 48.0, los)
    arrow(ax, ant, tgt, color=BLUE, lw=2.2, head=12, z=6)
    ax.add_patch(Circle(tgt, 1.6, facecolor=BLUE, edgecolor="none", zorder=8))
    text(ax, tgt[0] + 3.0, tgt[1] - 0.5, "표적  20 km", size=12, color=NAVY, bold=True,
         va="center")

    save(fig, "fig10_layout.png")


# ================================================================ 회전의 의미
# 들어가며를 줄이면서 이 그림은 슬라이드에서 빠졌다. 되살릴 때를 대비해 생성기는 남겨 둔다.
def draw_rotate():
    """같은 배 · 같은 표적을 안테나 축과 함선 축에서 각각 읽으면 숫자가 어떻게 달라지는가"""
    W, H = 1496, 760
    fig, lay = canvas(W, H)

    box = (0.024, 0.180, 0.452, 0.688)
    ar = (box[3] * H) / (box[2] * W)
    TOP = 100.0 * ar
    axA = stage(fig, list(box), (0, 100), (0, TOP))
    axB = stage(fig, [0.524, box[1], box[2], box[3]], (0, 100), (0, TOP))

    L = 26.0                            # 배 길이
    OX, OY = 26.0, TOP * 0.50           # 안테나 = 두 그림 공통 원점
    AXL, AXS = 60.0, 33.0               # 긴 축 / 짧은 축
    SC = 50.0 / 17320.0                 # m 를 그림 단위로
    TX, TY = OX + 17320.0 * SC, OY - 10000.0 * SC

    def scene(ax):
        """두 그림에 똑같이 들어가는 것 : 배, 안테나, 표적까지의 화살표"""
        ship_top(ax, OX - 1.6 * HALF_BEAM * L, OY - 0.44 * L, L, ang=90.0, wake=False)
        antenna_face(ax, (OX, OY), 0.0, half=2.2, thick=1.3, color=ORANGE)
        arrow(ax, (OX, OY), (TX, TY), color=NAVY, lw=2.4, head=11, z=7)
        ax.add_patch(Circle((TX, TY), 1.6, facecolor=NAVY, edgecolor="none", zorder=8))
        text(ax, TX + 3.0, TY - 1.6, "표적", size=12, color=NAVY, bold=True, va="top")
        text(ax, OX - 4.4, OY, "안테나", size=10, color=ORANGE, bold=True, ha="right",
             va="center")
        text(ax, OX - 1.6 * HALF_BEAM * L - 3.2, OY + 0.46 * L, "선수", size=10, color=GREY,
             ha="right", va="center")

    def guides(ax, tone, ylab, note=None, back=False):
        """표적에서 두 축으로 수선을 내리고 읽은 값을 붙인다.
        back=True 면 성분이 축 화살표와 반대쪽이라는 뜻이라 그 구간을 점선으로 덧그린다"""
        line(ax, (TX, TY), (TX, OY), color="#D0D5DE", lw=1.0, dashes=(4, 3), z=2)
        line(ax, (TX, TY), (OX, TY), color="#D0D5DE", lw=1.0, dashes=(4, 3), z=2)
        if back:
            arrow(ax, (OX, OY), (OX, TY), color=tone, lw=1.6, head=9, z=6, ls=(0, (4, 3)))
        text(ax, (OX + TX) / 2 + 6.0, OY + 2.4, "+17,320", size=12, color=tone, bold=True,
             ha="center", va="bottom")
        text(ax, OX - 4.6, (OY + TY) / 2, ylab, size=12, color=tone, bold=True,
             ha="right", va="center")
        if note:
            text(ax, OX - 4.6, (OY + TY) / 2 - 4.8, note, size=9.5, color=GREY,
                 ha="right", va="center")

    # ------------------------------------------------- (a) 안테나 축에서 읽으면
    scene(axA)
    arrow(axA, (OX, OY), (OX + AXL, OY), color=ORANGE, lw=1.9, z=6)
    arrow(axA, (OX, OY), (OX, OY - AXS), color=ORANGE, lw=1.9, z=6)
    text(axA, OX + AXL + 2.0, OY, "x_a", size=12.5, color=ORANGE, bold=True, va="center")
    text(axA, OX + 2.6, OY - AXS + 1.2, "y_a", size=12.5, color=ORANGE, bold=True,
         ha="left", va="center")
    guides(axA, ORANGE, "+10,000")

    # ------------------------------------------------- (b) 함선 축에서 읽으면
    scene(axB)
    arrow(axB, (OX, OY), (OX, OY + AXS), color=BLUE, lw=1.9, z=6)
    arrow(axB, (OX, OY), (OX + AXL, OY), color=BLUE, lw=1.9, z=6)
    text(axB, OX + 2.6, OY + AXS - 1.2, "x_b", size=12.5, color=BLUE, bold=True,
         ha="left", va="center")
    text(axB, OX + AXL + 2.0, OY, "y_b", size=12.5, color=BLUE, bold=True, va="center")
    guides(axB, BLUE, "-10,000", note="(선수 기준 뒤쪽)", back=True)

    # ------------------------------------------------------ 머리글 · 결론 · 캡션
    text(lay, 0.040, 0.950, "(a) 안테나 축에서 읽으면", size=14.5, bold=True)
    text(lay, 0.040, 0.901, "x_a = 보어사이트 (우현 쪽),   y_a = 그 오른쪽 (선미 쪽)",
         size=11, color=GREY)
    text(lay, 0.540, 0.950, "(b) 함선 축에서 읽으면", size=14.5, bold=True)
    text(lay, 0.540, 0.901, "x_b = 선수,   y_b = 우현    (x_a 와 y_b 는 같은 방향)",
         size=11, color=GREY)
    text(lay, 0.040, 0.134, "읽은 값 :  x_a = +17,320 m,   y_a = +10,000 m",
         size=12.5, color=ORANGE, bold=True)
    text(lay, 0.540, 0.134, "읽은 값 :  x_b = -10,000 m,   y_b = +17,320 m",
         size=12.5, color=BLUE, bold=True)
    text(lay, 0.500, 0.052, "배도 표적도 그대로다.  축만 90° 돌아가 있다.", size=12.5,
         color=NAVY, ha="center")

    save(fig, "fig05_same_target.png")


# ==================================================== 17장 : NED 와 ENU 는 같은 점
NED_TGT = (-19340.0, 5155.0, -10.0)     # 과제 값 (N, E, D). 26장 계산 결과를 반올림한 값


def draw_ned_enu():
    """NED 와 ENU 는 같은 점을 부르는 순서와 셋째 축의 방향만 다르다.

    북 · 동 두 축은 두 칸에 똑같이 그린다. 가리키는 방향이 실제로 같기 때문이다.
    셋째 축만 아래(D) 와 위(U) 로 뒤집히는데, 그것이 눈에 보이도록 사선으로 그린다.
    표적은 배보다 10 m 위에 있어 NED 에서는 D 가 음수, ENU 에서는 U 가 양수가 된다.
    """
    W, H = 1325, 553
    fig, lay = canvas(W, H)
    boxes = ([0.035, 0.205, 0.400, 0.595], [0.545, 0.205, 0.400, 0.595])
    TOP = 100 * (0.595 * H) / (0.400 * W)
    axA, axB = (stage(fig, list(b_), (0, 100), (0, TOP)) for b_ in boxes)

    O, L = (44.0, 0.53 * TOP), 19.0
    SC = 17.0 / 20000.0                                 # 20 km 를 평면 위 17 칸으로 본다
    RISE = 4.2                                          # 표적 높이. 보이라고 크게 부풀린 값

    def panel(order, ax, tone):
        # 국지 수평면(북 · 동이 놓인 평면)을 평행사변형으로 깔아 준다
        a_, b_ = 1.24 * L, 0.90 * L
        quad = [iso(O, sf * b_, sr * a_) for sf, sr in ((1, 1), (1, -1), (-1, -1), (-1, 1))]
        poly(ax, quad, fc="#EFF3FA", ec="#C7CFDD", lw=1.1, z=2)
        text(ax, quad[3][0] + 1.2, quad[3][1] - 1.4, "국지 수평면", size=9, color=FAINT,
             ha="left", va="top")

        for i, (key, lab) in enumerate(order):
            col = GREEN if key in ("fwd", "rgt") else tone
            iso_axis(ax, O, key, L, col, lab, num="%d" % (i + 1), tone=tone)
        ax.add_patch(Circle(O, 1.1, facecolor=NAVY, edgecolor="none", zorder=9))

        # 표적. 평면 위 자리까지 안내선을 긋고, 높이만큼 띄워 점을 찍는다
        foot = iso(O, NED_TGT[0] * SC, NED_TGT[1] * SC)
        line(ax, O, foot, color="#B9C2D0", lw=1.0, dashes=(4, 3), z=4)
        head = (foot[0], foot[1] + RISE)
        line(ax, foot, head, color=ORANGE, lw=1.3, dashes=(3, 2), z=5)
        ax.add_patch(Circle(foot, 0.9, facecolor="#9AA6B8", edgecolor="none", zorder=5))
        ax.add_patch(Circle(head, 1.9, facecolor=ORANGE, edgecolor="none", zorder=8))
        text(ax, head[0] - 2.8, head[1], "표적", size=11, color=ORANGE, bold=True,
             ha="right", va="center")

    panel((("fwd", "N  북"), ("rgt", "E  동"), ("down", "D  아래")), axA, BLUE)
    panel((("rgt", "E  동"), ("fwd", "N  북"), ("up", "U  위")), axB, ORANGE)

    n, e, d = NED_TGT
    text(lay, boxes[0][0], 0.938, "NED  (항공우주 · 항법 · 무기체계)", size=12.5, bold=True)
    text(lay, boxes[0][0], 0.888, "North-East-Down.  북 → 동 → 아래 순서, 셋째 축이 아래로 +",
         size=10, color=FAINT)
    text(lay, boxes[1][0], 0.938, "ENU  (측지 · 측량 · GIS · 로보틱스)", size=12.5, bold=True)
    text(lay, boxes[1][0], 0.888, "East-North-Up.  동 → 북 → 위 순서, 셋째 축이 위로 +", size=10,
         color=FAINT)

    text(lay, boxes[0][0], 0.128, "(N %s,  E %s,  D %s) m" % (num(n), num(e), num(d)),
         size=12.5, color=BLUE, bold=True)
    text(lay, boxes[1][0], 0.128, "(E %s,  N %s,  U %s) m" % (num(e), num(n), num(-d)),
         size=12.5, color=ORANGE, bold=True)
    text(lay, 0.520, 0.045, "북 · 동은 같은 방향이다. 셋째 축만 뒤집히고, 둘 다 오른손 좌표계다.",
         size=11.5, color=NAVY, ha="center")
    text(lay, 0.035, 0.045, "표적 높이는 부풀린 그림", size=9, color=FAINT)

    save(fig, "fig07_ned_enu.png")


# ================================================== 18장 : ECEF 와 ECI 의 차이
def draw_ecef_eci():
    """북극에서 내려다본 그림 두 칸으로 '축이 도느냐 마느냐' 를 보인다.

    ECEF 는 X 축과 건물이 같이 돌아 사이 각(경도)이 세 시각 모두 같고,
    ECI 는 X 축이 별에 묶여 있어 건물만 하루에 한 바퀴 돈다.
    한 시각만 그리면 두 칸이 똑같아 보이므로 세 시각을 겹쳐 그린다.
    """
    W, H = 1378, 619
    fig, lay = canvas(W, H)
    ax = stage(fig, [0.015, 0.250, 0.970, 0.575], (0, 100),
               (0, 100 * (0.575 * H) / (0.970 * W)))

    R, LON = 9.8, 40.0                                # 지구 반지름(칸), 건물의 경도
    HOURS = (0.0, 8.0, 16.0)                           # 겹쳐 그릴 시각
    SPIN = 360.0 / 24.0                                # 한 시간에 도는 각
    CY = 0.50 * ax.get_ylim()[1]

    def globe(c):
        ax.add_patch(Circle(c, R, facecolor=HULL, edgecolor="#3D4A5F", lw=1.5, zorder=2))
        axis_mark(ax, c, 1.7, into=False, color="#3D4A5F")
        text(ax, c[0], c[1] - 3.0, "Z 북극 ⊙", size=9, color=FAINT, ha="center", va="top")

    def spoke(c, deg, solid):
        # 원점에서 조금 띄워 시작해야 세 시각의 축이 한 줄로 붙어 보이지 않는다
        ax.add_patch(FancyArrowPatch(at(c, 2.8, deg), at(c, R + 3.0, deg), arrowstyle="-|>",
                                     mutation_scale=9, lw=2.0 if solid else 1.3,
                                     color=BLUE, shrinkA=0, shrinkB=0,
                                     alpha=1.0 if solid else 0.38,
                                     zorder=6 if solid else 4))
        if solid:
            q = at(c, R + 4.6, deg)
            text(ax, q[0], q[1], "X", size=11, color=BLUE, bold=True, ha="left",
                 va="center")

    def build(c, deg, solid, tag=None, tag_r=None):
        p = at(c, R, deg)
        ax.add_patch(Circle(p, 1.8 if solid else 1.5, facecolor=ORANGE, edgecolor="none",
                            alpha=1.0 if solid else 0.32, zorder=7))
        if tag:                                        # 시각표는 지구 바깥에 둔다 (각 표시와 안 겹치게)
            q = at(c, tag_r or (R + 5.0), deg)
            text(ax, q[0], q[1], tag, size=9.5, color=ORANGE if solid else FAINT,
                 bold=solid, ha="center", va="center")

    # ------------------------------------------- 왼쪽 : ECEF, 축과 건물이 같이 돈다
    CA = (24.0, CY)
    globe(CA)
    for h in HOURS:
        solid = h == 0.0
        spoke(CA, h * SPIN, solid)
        build(CA, h * SPIN + LON, solid, tag="%d시" % int(h),
              tag_r=R + (3.6 if h == 16.0 else 5.0))
        arcdeg(ax, CA, 5.6, h * SPIN, h * SPIN + LON, color=GREY if solid else "#B9C0CC",
               lw=1.5 if solid else 1.2)
    text(ax, *at(CA, 8.4, LON / 2), s="경도", size=9.5, color=GREY, bold=True,
         ha="center", va="center")

    # ------------------------------------------- 오른쪽 : ECI, 축은 멈춰 있다
    CB = (76.0, CY)
    globe(CB)
    ax.add_patch(Circle(CB, R + 2.4, facecolor="none", edgecolor=ORANGE, lw=1.0,
                        linestyle=(0, (4, 3)), zorder=3))
    spoke(CB, 0.0, True)
    for h in HOURS:
        build(CB, h * SPIN + LON, h == 0.0, tag="%d시" % int(h),
              tag_r=R + (4.2 if h == 16.0 else 6.4))
    arcdeg(ax, CB, 5.6, 0.0, LON, color=ORANGE, lw=1.5)
    text(ax, *at(CB, 8.6, LON / 2), s="θ", size=11.5, color=ORANGE, bold=True,
         ha="center", va="center")
    # 건물이 없는 쪽(왼쪽 아래)에 도는 방향 표시를 둔다
    curve_arrow(ax, at(CB, R + 2.4, 196.0), at(CB, R + 2.4, 238.0), rad=-0.26,
                color=ORANGE, lw=1.6, head=10)
    text(ax, *at(CB, R + 5.6, 217.0), s="하루 한 바퀴", size=9.5, color=ORANGE, bold=True,
         ha="right", va="center")

    # ------------------------------------------- 가운데 : 두 좌표계를 잇는 회전
    for dy, lab, back in ((4.2, "Rz(θ)", False), (-4.2, "Rz(θ)^T", True)):
        p0, p1 = (42.0, CY + dy), (58.0, CY + dy)
        arrow(ax, p1 if back else p0, p0 if back else p1, color=GREY, lw=1.6, head=11, z=5)
        text(ax, 50.0, CY + dy + (1.6 if dy > 0 else -1.6), lab, size=11, color=GREY,
             bold=True, ha="center", va="bottom" if dy > 0 else "top")

    # 머리글 세 줄 : 이름 / 약어를 푼 영어 / 그림 읽는 법
    for fx, ttl, eng, how in (
            (0.030, "ECEF — 지구와 함께 돈다", "Earth-Centered, Earth-Fixed",
             "북극에서 내려다본 그림. 0시 · 8시 · 16시를 겹쳐 그렸다."),
            (0.545, "ECI — 별에 고정, 돌지 않는다", "Earth-Centered Inertial",
             "같은 세 시각. 원점은 둘 다 지구 중심으로 같다.")):
        text(lay, fx, 0.958, ttl, size=13, bold=True)
        text(lay, fx, 0.914, eng, size=10, color=GREY)
        text(lay, fx, 0.872, how, size=9.5, color=FAINT)

    text(lay, 0.030, 0.168, "X 축 = 그리니치 자오선. 지구와 함께 돈다.", size=11,
         color=BLUE, bold=True)
    text(lay, 0.030, 0.108, "축과 건물이 같이 도니 사이 각(경도)이 세 시각 모두 같다",
         size=11, color=GREY)
    text(lay, 0.545, 0.168, "X 축 = 별(춘분점)에 고정. 돌지 않는다.", size=11,
         color=BLUE, bold=True)
    text(lay, 0.545, 0.108, "축이 멈춰 있으니 건물만 하루에 한 바퀴 돈다",
         size=11, color=GREY)
    text(lay, 0.500, 0.038,
         "θ = GMST (Greenwich Mean Sidereal Time).  시각이 1 ms 어긋나면 적도에서 0.47 m 어긋난다.",
         size=11.5, color=ORANGE, bold=True, ha="center")

    save(fig, "fig09_ecef_eci.png")


# =============================================== 32장 : 배가 기울면 빔도 같이 기운다
def draw_roll_beam():
    """우현 안테나는 배에 볼트로 붙어 있어 배가 기울면 보어사이트도 같이 기운다.

    보어사이트가 우현 정횡이라 배 뒤에서 선수 쪽을 본 단면이 곧 빔이 놓인 평면이다.
    그래서 화면 오른쪽이 우현이고, 고각을 화면에서 그대로 잴 수 있다.
    """
    W, H = 1300, 484
    fig, lay = canvas(W, H)
    ax = stage(fig, [0.015, 0.045, 0.970, 0.900], (0, 100),
               (0, 100 * (0.900 * H) / (0.970 * W)))

    TOP = ax.get_ylim()[1]
    ROLL, SEA, BEAM = 20.0, 0.495 * TOP, 10.0
    ax.add_patch(Polygon([(0.0, 0.0), (100.0, 0.0), (100.0, SEA), (0.0, SEA)],
                         closed=True, facecolor="#EAF0F9", edgecolor="none", zorder=0))
    sea_line(ax, 0.0, 100.0, SEA, z=1)
    text(ax, 1.5, SEA - 2.0, "해수면", size=10.5, color=FAINT, va="top")

    P = ship_stern(ax, 15.0, SEA, BEAM, ang=ROLL)
    ant = P(0.50, 0.42)                                 # 우현 뱃전
    antenna_face(ax, ant, -ROLL, half=2.2, thick=1.3, color=ORANGE)
    text(ax, ant[0] + 3.6, ant[1] + 3.2, "안테나 (우현)", size=11.5, color=ORANGE,
         bold=True, ha="left", va="bottom")
    line(ax, (ant[0] + 3.4, ant[1] + 3.0), (ant[0] + 1.2, ant[1] + 1.2), color=FAINT,
         lw=0.9, z=7)

    line(ax, ant, (ant[0] + 42.0, ant[1]), color="#B9C2D0", lw=1.2, dashes=(6, 4), z=2)
    text(ax, ant[0] + 20.0, ant[1] + 1.4, "수평 기준선 = 고각 0°", size=11, color=FAINT,
         va="bottom")

    tip = at(ant, 42.0, -ROLL)
    arrow(ax, ant, tip, color=ORANGE, lw=2.4, head=13, z=6)
    arcdeg(ax, ant, 26.0, -ROLL, 0.0, color=NAVY, lw=1.4)
    text(ax, *at(ant, 29.0, -ROLL / 2), s="20°", size=13, color=NAVY, bold=True,
         ha="left", va="center")

    # 오른쪽 빈 자리에 '보고한 값' 과 '실제 값' 을 나란히 적는다
    text(ax, 66.0, 14.0, "레이다가 보고한 고각", size=11, color=GREY)
    text(ax, 97.0, 14.0, "0°", size=12.5, color=GREY, bold=True, ha="right")
    text(ax, 66.0, 9.5, "실제 빔의 고각", size=11, color=ORANGE)
    text(ax, 97.0, 9.5, "-20°", size=13.5, color=ORANGE, bold=True, ha="right")
    line(ax, (66.0, 6.9), (97.0, 6.9), color="#C7CDD8", lw=1.0, z=2)
    text(ax, 66.0, 4.7, "빔은 하늘이 아니라 바다 쪽으로 나간다", size=11, color=NAVY,
         bold=True, va="center")

    save(fig, "fig12_roll.png")


# ================================================== 12장 : UV (방향코사인) 이란
def draw_uv():
    """방향은 반지름 1 인 반구 위의 한 점 — 그 한 장면으로만 설명한다.

    u, v, w 는 그 점의 세 좌표다. (u, v) 는 점을 안테나 면에 내린 그림자이고,
    남은 높이가 w 다. u² + v² + w² = 1 이라 셋 중 둘을 정하면 나머지는 따라온다.

    보기 규칙 — 굵기와 선 모양으로 역할을 가른다 :
        표적 방향   굵은 주황 실선 (흰 테두리)   ← 주인공
        u · v · w   중간 굵기 색 실선            ← 그 방향의 세 성분
        좌표축      가는 회색 점선               ← 눈금 노릇만

    시점은 안테나 면을 눕히고 고도 22° 에서 본 정투영이다. 고도를 낮게 잡아야 표적
    화살표가 시선축과 나란해져 짧게 눌리는 일이 없다 (지금 배치에서 반지름의 53 %).
    """
    W, H = 1156, 663
    fig, lay = canvas(W, H)
    axD = stage(fig, [0.008, 0.150, 0.545, 0.790], (0, 100),
                (0, 100 * (0.790 * H) / (0.545 * W)))

    EL, AZ = 35.0, 20.0

    BETA = 22.0
    SB, CB = math.sin(math.radians(BETA)), math.cos(math.radians(BETA))
    K = math.sqrt(0.5)
    EU, EV, EW = (-K, -SB * K), (K, -SB * K), (0.0, CB)
    C, RS = (46.0, 32.0), 38.0
    N = 144

    def p3(u, v, w):
        return (C[0] + RS * (u * EU[0] + v * EV[0] + w * EW[0]),
                C[1] + RS * (u * EU[1] + v * EV[1] + w * EW[1]))

    def ring(rad, w, n=N):
        return [p3(rad * math.cos(2 * math.pi * i / n), rad * math.sin(2 * math.pi * i / n), w)
                for i in range(n + 1)]

    def curve(ax, pts, color, lw, z, dashes=None):
        ln, = ax.plot([q[0] for q in pts], [q[1] for q in pts], color=color, lw=lw,
                      zorder=z, solid_capstyle="round")
        if dashes:
            ln.set_dashes(dashes)

    # 바닥 원판 = 안테나 면
    poly(axD, ring(1.0, 0.0)[:-1], fc="#EDF2FA", ec="#9FB2CE", lw=1.3, z=2)
    for phi in range(0, 360, 30):                       # 반구 자오선
        cp, sp = math.cos(math.radians(phi)), math.sin(math.radians(phi))
        curve(axD, [p3(math.sin(math.radians(a)) * cp, math.sin(math.radians(a)) * sp,
                       math.cos(math.radians(a))) for a in range(0, 91, 3)],
              "#D8E1EE", 0.8, 3)
    for ang, lab in ((30.0, "30°"), (60.0, "60°")):
        rr, hh = math.sin(math.radians(ang)), math.cos(math.radians(ang))
        curve(axD, ring(rr, hh), "#B4C4DA", 1.0, 4)
        curve(axD, ring(rr, 0.0), "#CFD6E0", 0.9, 3, dashes=(4, 3))
        q = p3(0.0, rr, hh)
        text(axD, q[0] + 1.6, q[1] + 0.4, lab, size=9, color=FAINT, ha="left", va="bottom")
    curve(axD, ring(1.0, 0.0), "#8FA3BE", 1.5, 5)

    # ── 좌표축 : 가는 회색 점선. 눈금 노릇만 하므로 뒤로 물린다
    for vec, lab, ha, va, dx, dy in (((0.0, 0.0, 1.28), "w  보어사이트", "center", "bottom", 0.0, 1.8),
                                     ((1.24, 0.0, 0.0), "u  왼쪽", "left", "top", 1.6, -1.4),
                                     ((0.0, 1.24, 0.0), "v  위", "right", "top", -1.6, -1.4)):
        tq = p3(*vec)
        arrow(axD, C, tq, color="#9AA4B4", lw=1.1, head=8, z=6, ls=(0, (5, 3)))
        text(axD, tq[0] + dx, tq[1] + dy, lab, size=11, color=GREY, bold=True, ha=ha, va=va)

    ce = math.cos(math.radians(EL))
    uu, vv, ww = ce * math.sin(math.radians(AZ)), math.sin(math.radians(EL)), \
        ce * math.cos(math.radians(AZ))
    Pp, Sp, Up = p3(uu, vv, ww), p3(uu, vv, 0.0), p3(uu, 0.0, 0.0)
    HALO = [pe.withStroke(linewidth=3.4, foreground="white")]

    # ── 세 성분 : 중간 굵기 색 실선
    arrow(axD, C, Up, color=GREEN, lw=2.3, head=10, z=8)
    arrow(axD, Up, Sp, color=GREEN, lw=2.3, head=10, z=8)
    arrow(axD, Sp, Pp, color=BLUE, lw=2.3, head=10, z=9)
    right_angle(axD, Sp, C, Pp, 2.8, color=BLUE)
    # u 는 화살표 왼쪽 위, v 는 화살표 아래 — 서로 붙지 않게 반대쪽으로 뗀다
    text(axD, 0.5 * (C[0] + Up[0]) - 1.2, 0.5 * (C[1] + Up[1]) + 2.4, "u",
         size=15, color=GREEN, bold=True, ha="right", va="bottom", path_effects=HALO)
    text(axD, 0.5 * (Up[0] + Sp[0]), 0.5 * (Up[1] + Sp[1]) - 2.4, "v", size=15,
         color=GREEN, bold=True, ha="center", va="top", path_effects=HALO)
    text(axD, Sp[0] + 2.2, Sp[1] + 0.70 * (Pp[1] - Sp[1]), "w", size=15, color=BLUE,
         bold=True, ha="left", va="center", path_effects=HALO)

    # ── 표적 방향 : 가장 굵은 주황 실선. 흰 테두리로 격자 위에서도 끊기지 않게
    arrow(axD, C, Pp, color=ORANGE, lw=3.6, head=17, z=10, halo=True)
    axD.add_patch(Circle(Pp, 3.0, facecolor=ORANGE, edgecolor="white", lw=1.4, zorder=11))
    text(axD, Pp[0] - 3.8, Pp[1] + 2.6, "표적 방향", size=12.5, color=ORANGE, bold=True,
         ha="right", va="bottom", path_effects=HALO)
    text(axD, Pp[0] - 3.8, Pp[1] + 0.4, "길이 1", size=10.5, color=GREY,
         ha="right", va="top", path_effects=HALO)
    axD.add_patch(Circle(Sp, 2.4, facecolor="white", edgecolor=GREEN, lw=1.8, zorder=11))
    text(axD, Sp[0] + 3.2, Sp[1] - 1.0, "그림자 (u, v)", size=11, color=GREEN, bold=True,
         ha="left", va="top", path_effects=HALO)

    text(axD, 46.0, 3.0, "안테나 면을 눕혀 그린 그림 · w 는 면에서 똑바로 나오는 방향",
         size=9, color=FAINT, ha="center", va="center")

    # ------------------------------------------------------------------ 글자
    X = 0.565
    text(lay, X, 0.912, "방향은 반구 위의 한 점", size=13, bold=True)
    text(lay, X, 0.868, "u, v, w 는 그 점의 세 좌표다", size=10, color=GREY)

    for i, s_ in enumerate(["u  =  x / R  =  cos(El)·sin(Az)",
                            "v  =  y / R  =  sin(El)",
                            "w  =  z / R  =  cos(El)·cos(Az)"]):
        text(lay, X, 0.762 - i * 0.064, s_, size=12, color=NAVY, bold=True)

    text(lay, X, 0.512, "u² + v² + w²  =  1", size=13, color=ORANGE, bold=True)
    text(lay, X, 0.462, "셋 중 둘을 정하면 나머지는 따라온다", size=10, color=GREY)

    text(lay, X, 0.366, "El 35° · Az 20° 이면", size=10, color=GREY)
    text(lay, X, 0.310, "u 0.280    v 0.574    w 0.770", size=11.5, color=NAVY, bold=True)

    # 순수하게 기하로만 적는다. 위상배열 이야기는 자료에서 뺐다
    text(lay, X, 0.218, "그림자 (u, v) 는 u² + v² ≤ 1 인 원판", size=10, color=GREY)
    text(lay, X, 0.174, "안에만 놓인다 — 그 밖의 (u, v) 에", size=10, color=GREY)
    text(lay, X, 0.130, "해당하는 방향은 없다", size=10, color=GREY)

    text(lay, 0.018, 0.052,
         "앞 장 직교좌표 (x, y, z) 를 거리 R 로 나눈 것이다 — 거리를 지우고 방향만 남겼다.",
         size=10.5, color=GREY)

    save(fig, "fig03_uv.png")


# ============================================== 회전 + 평행이동을 행렬로
# 들어가며를 3장으로 줄이면서 이 그림도 슬라이드에서 빠졌다.
# "새 좌표 = R · 옛 좌표 + t" 한 줄로 대신했다. 되살릴 때를 대비해 생성기는 남겨 둔다.
def draw_two_ops():
    """새 좌표 = R · 옛 좌표 + t. (빼기는 나눔 글꼴에도 있는 en dash 로 적는다) 왼쪽은 일반형, 오른쪽은 예제(안테나 → 동체)의 실제 숫자"""
    W_IN, H_IN = 12.1, 2.25
    fig, lay = canvas(int(W_IN * DPI), int(H_IN * DPI))

    def X(v):
        return v / W_IN

    def Y(v):
        return v / H_IN

    def matrix(x, yc, rows, col_w, row_h=0.34, size=11.5, color=NAVY, bold=False):
        """대괄호 행렬. x 는 왼쪽 괄호 위치(in), 돌려주는 값은 오른쪽 괄호 바깥(in)"""
        n, w = len(rows), sum(col_w)
        top, bot = yc + n * row_h / 2, yc - n * row_h / 2
        for bx, d in ((x, 1), (x + w + 0.16, -1)):
            lay.plot([X(bx), X(bx)], [Y(bot), Y(top)], color=color, lw=1.3, solid_capstyle="butt")
            lay.plot([X(bx), X(bx + d * 0.09)], [Y(top), Y(top)], color=color, lw=1.3)
            lay.plot([X(bx), X(bx + d * 0.09)], [Y(bot), Y(bot)], color=color, lw=1.3)
        for i, row in enumerate(rows):
            cy, cx = top - (i + 0.5) * row_h, x + 0.08
            for j, cell in enumerate(row):
                text(lay, X(cx + col_w[j] / 2), Y(cy), cell, size=size, color=color, bold=bold, ha="center")
                cx += col_w[j]
        return x + w + 0.16

    def op(x, yc, s, size=15):
        text(lay, X(x), Y(yc), s, size=size, color=NAVY, ha="center")

    def label(x0, x1, y, s, color=GREY, bold=False, size=9.5):
        text(lay, X((x0 + x1) / 2), Y(y), s, size=size, color=color, bold=bold, ha="center")

    YC, YL, YT = 1.12, 0.30, 2.02

    # ── 왼쪽 : 일반형
    text(lay, X(0.30), Y(YT), "일반형 :  새 좌표  =  R · 옛 좌표  +  t", size=11.5, color=NAVY, bold=True)
    x0 = 0.30
    x1 = matrix(x0, YC, [["$x'$"], ["$y'$"], ["$z'$"]], [0.40], bold=True)
    label(x0, x1, YL, "새 좌표")
    op(x1 + 0.22, YC, "=")
    x2 = x1 + 0.44
    x3 = matrix(x2, YC, [["$r_{11}$", "$r_{12}$", "$r_{13}$"], ["$r_{21}$", "$r_{22}$", "$r_{23}$"],
                         ["$r_{31}$", "$r_{32}$", "$r_{33}$"]], [0.50] * 3, color=BLUE)
    label(x2, x3, YL, "회전  R  (3×3 행렬)", color=BLUE, bold=True)
    op(x3 + 0.20, YC, "·", size=17)
    x4 = x3 + 0.40
    x5 = matrix(x4, YC, [["$x$"], ["$y$"], ["$z$"]], [0.40])
    label(x4, x5, YL, "옛 좌표")
    op(x5 + 0.22, YC, "+")
    x6 = x5 + 0.44
    x7 = matrix(x6, YC, [["$t_x$"], ["$t_y$"], ["$t_z$"]], [0.50], color=ORANGE)
    label(x6, x7, YL, "평행이동  t  (3×1)", color=ORANGE, bold=True)

    # ── 가운데 구분선
    lay.plot([X(5.85), X(5.85)], [Y(0.25), Y(2.05)], color=FAINT, lw=0.8)

    # ── 오른쪽 : 예제 (안테나 → 동체)
    text(lay, X(6.25), Y(YT), "예제 :  안테나 → 동체   (설치 방위 90°,  레버암 (–30, 0, –10) m)",
         size=11.5, color=NAVY, bold=True)
    x0 = 6.25
    x1 = matrix(x0, YC, [["–10,030.0"], ["17,320.5"], ["–10.0"]], [0.95], bold=True)
    label(x0, x1, YL, "동체 좌표 (m)")
    op(x1 + 0.22, YC, "=")
    x2 = x1 + 0.44
    x3 = matrix(x2, YC, [["0", "–1", "0"], ["1", "0", "0"], ["0", "0", "1"]], [0.36] * 3, color=BLUE)
    label(x2, x3, YL, "Rz(90°)", color=BLUE, bold=True)
    op(x3 + 0.20, YC, "·", size=17)
    x4 = x3 + 0.40
    x5 = matrix(x4, YC, [["17,320.5"], ["10,000.0"], ["0.0"]], [0.95])
    label(x4, x5, YL, "안테나 좌표 (m)")
    op(x5 + 0.22, YC, "+")
    x6 = x5 + 0.44
    x7 = matrix(x6, YC, [["–30"], ["0"], ["–10"]], [0.55], color=ORANGE)
    label(x6, x7, YL, "레버암 (m)", color=ORANGE, bold=True)

    save(fig, "fig16_two_ops.png")


# ================================================= 11장 : INS 설치 오차 회전행렬
# 현재 미사용. 11장을 "INS 가 주는 값(자세 · 위치)" 중심으로 다시 짜면서 슬라이드에서 빠졌다.
# 설치 오차각을 행렬로 보여야 할 일이 생기면 그대로 되살려 쓰면 된다.
# Rz(Δψ)·Ry(Δθ)·Rx(Δφ) 를 펼쳐 1차항만 남긴 것. 설치 오차각은 각분 단위로 아주 작아서
# 이 근사가 그대로 통하고, "거의 항등행렬" 이 눈으로 보인다.
# 슬라이드에 글자로 적으면 글꼴 치환 때문에 대괄호가 어긋나서 그림으로 그린다.
def draw_ins_matrix():
    """[ 1  -Δψ  Δθ ; Δψ  1  -Δφ ; -Δθ  Δφ  1 ] 을 대각선과 나머지를 다른 색으로 그린다"""
    W, H = 810, 300
    fig, lay = canvas(W, H, bg=None)          # 카드 위에 얹으므로 배경을 비운다
    ax = stage(fig, [0.02, 0.05, 0.96, 0.90], (0, 100),
               (0, 100 * (0.90 * H) / (0.96 * W)))
    TOP = ax.get_ylim()[1]

    cells = [["1", "-Δψ", "Δθ"],
             ["Δψ", "1", "-Δφ"],
             ["-Δθ", "Δφ", "1"]]
    x0, dx = 22.0, 26.0                                 # 열 중심
    y0, dy = TOP * 0.78, TOP * 0.28                     # 행 중심
    for i, row in enumerate(cells):
        for j, s in enumerate(row):
            diag = (i == j)
            text(ax, x0 + j * dx, y0 - i * dy, s, size=24 if diag else 21,
                 color=NAVY if diag else ORANGE, bold=True, ha="center", va="center")

    # 대괄호. 세로선과 위아래 갈고리로 그린다
    top, bot = y0 + dy * 0.62, y0 - 2 * dy - dy * 0.62
    for bx, tick in ((x0 - dx * 0.82, +1), (x0 + 2 * dx + dx * 0.82, -1)):
        line(ax, (bx, bot), (bx, top), color=NAVY, lw=2.0, z=5)
        for yy in (top, bot):
            line(ax, (bx, yy), (bx + tick * 5.0, yy), color=NAVY, lw=2.0, z=5)

    save(fig, "fig20_ins_matrix.png", bg=None)


# ============================================== 11장 : INS 가 주는 자세각 세 가지
# 이번 과제 값은 roll 0° · pitch 0° · yaw 45° 다. roll 과 pitch 는 0 이라 그대로 그리면
# 각이 보이지 않으므로 정의가 보이도록 기울여 그리고 실제 값은 따로 적는다. yaw 만 실제
# 값 45° 그대로 그린다 — 그 각이 3단계 회전행렬 Rz(45°) 가 되기 때문이다.
def draw_attitude():
    """roll · pitch · yaw 세 칸. 각각 뒤에서 · 옆에서 · 위에서 본 그림이다"""
    W, H = 1800, 400
    fig, lay = canvas(W, H)
    axR, axP, axY = [stage(fig, [fx, 0.300, 0.300, 0.660], (0, 100),
                           (0, 100 * (0.660 * H) / (0.300 * W)))
                     for fx in (0.020, 0.350, 0.680)]
    base = 17.0                                          # 흘수선 · 기준선

    # ------------------------------------------------- roll : 뒤에서 본 그림
    line(axR, (22.0, base), (78.0, base), color="#C7CDD8", lw=1.0, dashes=(4, 3), z=1)
    ship_stern(axR, 50.0, base, 18.0, ang=22.0)          # 우현(화면 오른쪽)이 내려간 모습
    curve_arrow(axR, (30.0, base - 4.0), (70.0, base - 8.0), rad=-0.34)

    # ------------------------------------------------- pitch : 옆에서 본 그림
    line(axP, (16.0, base), (84.0, base), color="#C7CDD8", lw=1.0, dashes=(4, 3), z=1)
    ship_side(axP, 25.0, base, 50.0, ang=14.0)           # 선수(화면 오른쪽)가 들린 모습
    curve_arrow(axP, (28.0, base - 9.0), (72.0, base - 4.0), rad=0.34)

    # ------------------------------------------------- yaw : 위에서 본 그림
    # 동체 축(x_b · y_b)과 NED 축(N · E)을 한 그림에 겹친다. yaw 를 알면 "북이 동체 축
    # 기준으로 어느 쪽인가" 가 정해진다는 것 — 그것이 회전행렬이 북을 아는 이유다.
    O = (50.0, base + 4.0)                               # 배 한가운데 = 축의 원점
    L, YAW = 30.0, 45.0
    c45, s45 = math.cos(math.radians(YAW)), math.sin(math.radians(YAW))
    ship_top(axY, O[0] - 0.45 * L * c45, O[1] - 0.45 * L * s45, L, ang=YAW, wake=False)
    for deg, ln, lab, col, ha, va, dx, dy in (
            (90.0, 21.0, "N  북", GREEN, "center", "bottom", 0.0, 1.2),
            (0.0, 21.0, "E  동", GREEN, "left", "center", 1.4, 0.0),
            (YAW, 25.0, "x_b  선수", BLUE, "left", "bottom", 1.0, 0.8),
            (YAW - 90.0, 17.0, "y_b  우현", BLUE, "left", "top", 1.0, -0.8)):
        tip = at(O, ln, deg)
        arrow(axY, O, tip, color=col, lw=1.8, head=10, z=9)
        text(axY, tip[0] + dx, tip[1] + dy, lab, size=10, color=col, bold=True, ha=ha, va=va)
    axY.add_patch(Circle(O, 1.1, facecolor=NAVY, edgecolor="white", lw=0.8, zorder=10))
    arcdeg(axY, O, 13.0, YAW, 90.0, lw=1.8)
    text(axY, *at(O, 17.5, 67.5), s="45°", size=12, color=ORANGE, bold=True,
         ha="center", va="center")
    text(axY, 3.0, 8.0, "북은 선수에서\n왼쪽으로 45°", size=9.5, color=ORANGE, bold=True,
         ha="left", va="center", linespacing=1.35)

    # ------------------------------------------------------------------ 글자
    cols = (("roll   (횡동요)", "뒤에서 본 그림 · x축(선수축) 둘레 · 우현이 내려가면 +",
             "이번 과제  0°", GREY, 0.170),
            ("pitch  (종동요)", "옆에서 본 그림 · y축(우현축) 둘레 · 선수가 들리면 +",
             "이번 과제  0°", GREY, 0.500),
            ("yaw   (선수방위)", "위에서 본 그림 · z축(아래축) 둘레 · 진북에서 시계방향 +",
             "이번 과제  45°", ORANGE, 0.830))
    for label, sub, val, color, fx in cols:
        text(lay, fx, 0.215, label, size=13, color=NAVY, bold=True, ha="center")
        text(lay, fx, 0.135, sub, size=9.5, color=FAINT, ha="center")
        text(lay, fx, 0.045, val, size=12, color=color, bold=True, ha="center")

    save(fig, "fig21_attitude.png")





# ==================================== 12장 : 동체 -> 로컬 NED 계산을 행렬 그대로
# 현재 미사용. 계산 과정은 설계 단계(19장)에서 서술하기로 해서 12장에서 빠졌다.
# 이번 과제 값이다. 표적의 동체 좌표 (-10,030.0, +17,320.5, -10.0) 에 C_ned<-body = Rz(45°) 를
# 곱하면 (N -19,339.7, E +5,155.2, D -10.0) 이 된다. 글자로 적으면 글꼴 치환 때문에
# 대괄호가 어긋나서 그림으로 그린다 (fig20 과 같은 이유).
def draw_ned_calc():
    """[N;E;D] = [Rz(45°)] · [p_body] = [p_ned] 를 한 줄에 그린다. 산수는 슬라이드 글로 둔다"""
    W, H = 1150, 380
    fig, lay = canvas(W, H, bg=None)
    ax = stage(fig, [0.01, 0.17, 0.98, 0.79], (0, 76),
               (0, 76 * (0.79 * H) / (0.98 * W)))
    TOP = ax.get_ylim()[1]
    y0, dy = TOP * 0.80, TOP * 0.27                       # 행 중심 (위에서 아래로)

    def bracket(x0, x1, color=NAVY):
        top, bot = y0 + dy * 0.60, y0 - 2 * dy - dy * 0.60
        for bx, tick in ((x0, +1), (x1, -1)):
            line(ax, (bx, bot), (bx, top), color=color, lw=1.8, z=5)
            for yy in (top, bot):
                line(ax, (bx, yy), (bx + tick * 1.0, yy), color=color, lw=1.8, z=5)

    def block(x0, cols, half, cells, color, size=12.5, bold=False, label=None):
        for i, row in enumerate(cells):
            for c, s_ in zip(cols, row):
                text(ax, x0 + c, y0 - i * dy, s_, size=size, color=color, bold=bold,
                     ha="center", va="center")
        bracket(x0 - half, x0 + cols[-1] + half)
        if label:
            text(ax, x0 + 0.5 * cols[-1], y0 - 2 * dy - dy * 1.15, label, size=10,
                 color=color, ha="center", va="top")

    def op(x, s_, size=16):
        text(ax, x, y0 - dy, s_, size=size, color=NAVY, ha="center", va="center")

    block(4.0, [0.0], 2.2, [["N"], ["E"], ["D"]], NAVY, size=14, bold=True)
    op(8.8, "=")
    block(16.0, [0.0, 8.6, 16.4], 5.0, [["0.7071", "-0.7071", "0"],
                                        ["0.7071", "0.7071", "0"],
                                        ["0", "0", "1"]], NAVY,
          label="C_ned←body = Rz(45°)")
    op(40.4, "·")
    block(48.0, [0.0], 6.4, [["-10,030.0"], ["+17,320.5"], ["-10.0"]], BLUE,
          label="p_body  (동체 좌표)")
    op(57.4, "=")
    block(65.5, [0.0], 6.4, [["-19,339.7"], ["+5,155.2"], ["-10.0"]], ORANGE, bold=True,
          label="p_ned  (로컬 NED, m)")

    save(fig, "fig22_ned_calc.png", bg=None)


# =============================================== 14장 : LLA — 위도 · 경도 · 고도
def draw_lla():
    """지구 위 한 점 P 를 (φ, λ, h) 세 수로 적는 법 — 두 칸.

    왼쪽 : 실제 비율(b/a = 0.9966)의 지구를 비스듬히 본 그림. 적도 · 그리니치 자오선 · P 의 자오선을
      그리고 λ 는 적도면 위의 각(초록), φ 는 적도면에서 수직선까지의 각(파랑), h 는 수직선을 따라
      면에서 P 까지(주황). 실제 지구는 거의 구라 수직선이 사실상 지구 중심을 지나므로 φ 를 중심에서
      잰 것처럼 보인다 — 보는 사람의 직관과 같다.
    오른쪽 : 자오선 단면을 편평률을 과장해 그려 "위도는 어디서 재나" 를 보인다. 위도 φ 는 타원체 면에
      수직인 선(연직선)이 적도면과 이루는 각(측지위도)이고, 이 선은 지구 중심이 아니라 그 아래
      G (OG = e² R_N sinφ) 에서 자전축과 만난다. 중심에서 잰 각 ψ(지심위도)는 다른 값이며 지도가
      쓰지 않는다. 경도는 수직선도 자오선 면 안에 있어 영향이 없다.
    3차원 → 화면은 직교 투영(시선 방위 az · 앙각 el). 화면 각은 실제 각과 다르지만 "여기가 그 각" 을
    가리키는 표시로는 충분하다 (arc_between 과 같은 태도).
    """
    from matplotlib.patches import Ellipse
    W, H = 1500, 619
    fig, lay = canvas(W, H)

    # ------------------------------------------------ 왼쪽 : 지구 (실제 비율)
    SW = 0.49
    ax = stage(fig, [0.0, 0.0, SW, 1.0], (-1.32, 1.68),
               (-1.18, 3.00 * H / (SW * W) - 1.18))
    A = 1.0
    B = A * (1.0 - 1.0 / 298.257223563)                # WGS-84 비율 그대로 (거의 구)
    E2 = 1.0 - (B / A) ** 2
    AZ, EL = math.radians(18.0), math.radians(26.0)    # 시선 방위 · 앙각
    LAT, LON, ALT = math.radians(42.0), math.radians(68.0), 0.55
    ca, sa, ce, se = math.cos(AZ), math.sin(AZ), math.cos(EL), math.sin(EL)

    def proj(v):
        """3차원 (x, y, z) → 화면 (X, Y). z 가 북극 방향, x 가 그리니치 방향"""
        x, y, z = v
        x1 = x * ca + y * sa
        return (-x * sa + y * ca, -x1 * se + z * ce)

    def front(v):
        x, y, z = v
        return (x * ca + y * sa) * ce + z * se > 0.0

    def curve(pts, color, lw, z=4, only=None):
        """3차원 점열을 앞면(실선) · 뒷면(점선) 으로 나눠 그린다. only='front' 면 앞면만"""
        seg, mode = [], None

        def flush():
            if len(seg) > 1 and not (only == "front" and mode is False):
                ax.plot([q[0] for q in seg], [q[1] for q in seg], color=color,
                        lw=lw if mode else lw * 0.8, ls="-" if mode else (0, (3, 3)),
                        zorder=z if mode else 2, alpha=1.0 if mode else 0.55,
                        solid_capstyle="round")
        for v in pts:
            f = front(v)
            if mode is not None and f != mode:
                flush()
                seg = seg[-1:]
            seg.append(proj(v))
            mode = f
        flush()

    def lerp(v0, v1, t):
        return tuple(v0[i] + (v1[i] - v0[i]) * t for i in range(3))

    def meridian(lon):
        out = []
        for d in range(-90, 91, 3):
            p_ = math.radians(d)
            n_ = A / math.sqrt(1.0 - E2 * math.sin(p_) ** 2)
            out.append((n_ * math.cos(p_) * math.cos(lon), n_ * math.cos(p_) * math.sin(lon),
                        n_ * (1.0 - E2) * math.sin(p_)))
        return out

    vb = math.sqrt(A * A * se * se + B * B * ce * ce)   # 직교 투영의 윤곽은 타원
    ax.add_patch(Ellipse((0, 0), 2 * A, 2 * vb, facecolor=HULL, edgecolor="#3D4A5F", lw=1.5,
                         zorder=1))
    curve([(A * math.cos(t), A * math.sin(t), 0.0) for t in
           [math.radians(d) for d in range(0, 361, 3)]], "#3D4A5F", 1.3)
    curve(meridian(0.0), GREY, 1.4, only="front")
    curve(meridian(LON), GREEN, 1.6, only="front")
    line(ax, proj((0, 0, -B)), proj((0, 0, B)), color=FAINT, lw=1.0, ls=(0, (3, 3)), z=3)
    arrow(ax, proj((0, 0, B)), proj((0, 0, B + 0.20)), color="#3D4A5F", lw=1.5, head=9, z=6)
    q = proj((0, 0, B + 0.23))
    text(ax, q[0], q[1], "북극", size=10.5, color="#3D4A5F", bold=True, ha="center", va="bottom")
    o = proj((0, 0, 0))
    ax.add_patch(Circle(o, 0.028, facecolor="#3D4A5F", edgecolor="none", zorder=7))
    text(ax, o[0] - 0.06, o[1] - 0.02, "지구 중심", size=10, color=GREY, ha="right", va="top")

    # 경도 λ : 적도면 위, 그리니치 방향에서 P 의 자오선 방향까지
    g0 = (A, 0.0, 0.0)
    e1 = (A * math.cos(LON), A * math.sin(LON), 0.0)
    line(ax, o, proj(g0), color=GREY, lw=1.1, z=4)
    line(ax, o, proj(e1), color=GREEN, lw=1.1, z=4)
    curve([(0.46 * math.cos(t), 0.46 * math.sin(t), 0.0) for t in
           [LON * i / 40.0 for i in range(41)]], GREEN, 1.8, z=6)
    q = proj((0.62 * math.cos(LON * 0.5), 0.62 * math.sin(LON * 0.5), 0.0))
    text(ax, q[0], q[1] - 0.02, "λ 경도", size=12, color=GREEN, bold=True, ha="center", va="top")
    foot = next(proj(v) for v in meridian(0.0)[::-1] if front(v))   # 자오선 아래쪽 앞면 끝
    text(ax, foot[0] - 0.10, -vb - 0.04, "그리니치 자오선", size=10, color=GREY, ha="center",
         va="top")
    q = proj((A * math.cos(math.radians(-62)), A * math.sin(math.radians(-62)), 0.0))
    text(ax, q[0] - 0.02, q[1] - 0.05, "적도", size=10, color="#3D4A5F", ha="right", va="top")

    # 위도 φ · 고도 h : 자오선 평면 위. 수직선은 자전축과 G 에서 만난다 (실제 비율이라 중심과 겹쳐 보임)
    N = A / math.sqrt(1.0 - E2 * math.sin(LAT) ** 2)
    nrm = (math.cos(LAT) * math.cos(LON), math.cos(LAT) * math.sin(LON), math.sin(LAT))
    Q = (N * math.cos(LAT) * math.cos(LON), N * math.cos(LAT) * math.sin(LON),
         N * (1.0 - E2) * math.sin(LAT))
    P = tuple(Q[i] + ALT * nrm[i] for i in range(3))
    G = (0.0, 0.0, -N * E2 * math.sin(LAT))
    line(ax, proj(G), proj(Q), color=BLUE, lw=1.2, ls=(0, (3, 2)), z=5)
    arrow(ax, proj(Q), proj(P), color=ORANGE, lw=2.2, head=11, z=8)
    ax.add_patch(Circle(proj(Q), 0.030, facecolor="white", edgecolor=BLUE, lw=1.4, zorder=8))
    ax.add_patch(Circle(proj(P), 0.045, facecolor=ORANGE, edgecolor="white", lw=1.2, zorder=9))
    u = (math.cos(LON), math.sin(LON), 0.0)
    curve([tuple(G[i] + 0.34 * (math.cos(t) * u[i] + math.sin(t) * (0, 0, 1)[i]) for i in range(3))
           for t in [LAT * k / 40.0 for k in range(41)]], BLUE, 1.8, z=7)
    q = proj(tuple(G[i] + 0.50 * (math.cos(LAT * 0.5) * u[i] + math.sin(LAT * 0.5) * (0, 0, 1)[i])
                   for i in range(3)))
    text(ax, q[0] + 0.02, q[1], "φ 위도", size=12, color=BLUE, bold=True, ha="left", va="center")
    m = proj(lerp(Q, P, 0.55))
    text(ax, m[0] + 0.05, m[1] - 0.06, "h 고도", size=12, color=ORANGE, bold=True, ha="left",
         va="top")
    pp = proj(P)
    text(ax, pp[0] + 0.07, pp[1] + 0.06, "P (φ, λ, h)", size=12.5, color=ORANGE, bold=True,
         ha="left", va="bottom")
    qq = proj(Q)
    text(ax, qq[0] - 0.05, qq[1] + 0.04, "타원체 면", size=9.5, color=BLUE, ha="right", va="bottom")

    # ------------------------------------------------ 오른쪽 : 자오선 단면 (편평률 과장)
    # 자오선 평면의 오른쪽 절반. 가로축 = 자전축에서 떨어진 거리, 세로축 = 자전축 방향
    RX, RW = 0.515, 0.40
    bx = stage(fig, [RX, 0.235, RW, 0.66], (-0.52, 2.30),
               (-0.86, 2.82 * (0.66 * H) / (RW * W) - 0.86))
    A2, B2 = 1.0, 0.80
    E22 = 1.0 - (B2 / A2) ** 2
    arc_pts = [(A2 * math.cos(math.radians(d)), B2 * math.sin(math.radians(d)))
               for d in range(-90, 91, 2)]
    poly(bx, [(0.0, -B2)] + arc_pts + [(0.0, B2)], fc=HULL, ec="none", lw=0, z=1)
    bx.plot([q[0] for q in arc_pts], [q[1] for q in arc_pts], color="#3D4A5F", lw=1.5, zorder=3)
    line(bx, (-0.50, 0.0), (1.50, 0.0), color=FAINT, lw=1.0, z=2)                  # 적도면
    line(bx, (0.0, -0.86), (0.0, 0.98), color=FAINT, lw=1.0, ls=(0, (3, 3)), z=2)  # 자전축
    text(bx, 1.52, 0.0, "적도면", size=9.5, color=GREY, ha="left", va="center")
    text(bx, 0.0, 1.00, "자전축", size=9.5, color=GREY, ha="center", va="bottom")
    O2 = (0.0, 0.0)
    bx.add_patch(Circle(O2, 0.025, facecolor="#3D4A5F", edgecolor="none", zorder=7))
    text(bx, -0.05, 0.05, "중심 O", size=10, color=GREY, ha="right", va="bottom")

    N2 = A2 / math.sqrt(1.0 - E22 * math.sin(LAT) ** 2)
    P2 = (N2 * math.cos(LAT), N2 * (1.0 - E22) * math.sin(LAT))    # 타원체 면 위의 점
    n2 = (math.cos(LAT), math.sin(LAT))                             # 면에 수직인 방향
    F2 = (E22 * N2 * math.cos(LAT), 0.0)                            # 수직선이 적도면과 만나는 점
    G2 = (0.0, -E22 * N2 * math.sin(LAT))                           # 수직선이 자전축과 만나는 점
    # 접선과 직각 표시 : "수직" 은 면에 수직이라는 뜻
    t2 = (-math.sin(LAT), math.cos(LAT))
    line(bx, (P2[0] - 0.30 * t2[0], P2[1] - 0.30 * t2[1]),
         (P2[0] + 0.30 * t2[0], P2[1] + 0.30 * t2[1]), color=GREY, lw=1.1, z=4)
    right_angle(bx, P2, (P2[0] + t2[0], P2[1] + t2[1]), (P2[0] + n2[0], P2[1] + n2[1]), 0.07,
                color=GREY, lw=1.0, z=6)
    # 수직선 : G → P 점선, P 에서 바깥으로 화살표
    line(bx, G2, P2, color=BLUE, lw=1.4, ls=(0, (3, 2)), z=5)
    tip = (P2[0] + 0.34 * n2[0], P2[1] + 0.34 * n2[1])
    arrow(bx, P2, tip, color=BLUE, lw=1.8, head=10, z=6)
    text(bx, tip[0] + 0.05, tip[1] + 0.01, "수직선  (타원체 면에 수직)", size=10, color=BLUE,
         bold=True, ha="left", va="center")
    # 중심에서 그은 선 : 비교용
    line(bx, O2, P2, color="#9AA3B5", lw=1.1, ls=(0, (2, 2)), z=4)
    text(bx, 0.30, -0.05, "중심에서 잰 각", size=9, color="#8A93A8", ha="left", va="top")
    # 점들
    bx.add_patch(Circle(P2, 0.038, facecolor=ORANGE, edgecolor="white", lw=1.2, zorder=9))
    bx.add_patch(Circle(G2, 0.028, facecolor=BLUE, edgecolor="white", lw=1.0, zorder=8))
    text(bx, P2[0] + 0.06, P2[1] - 0.03, "P", size=12, color=ORANGE, bold=True, ha="left",
         va="top")
    text(bx, G2[0] + 0.05, G2[1] - 0.01, "G", size=10.5, color=BLUE, bold=True, ha="left",
         va="top")
    text(bx, G2[0] + 0.19, G2[1] - 0.01, r"OG = $e^{2}R_N\,\sin\varphi$", size=9.5, color=BLUE,
         ha="left", va="top")
    # 각 두 개 : φ 는 수직선이 적도면과 이루는 각 (F 에서), ψ 는 중심에서 잰 각 (O 에서)
    arcdeg(bx, F2, 0.30, 0.0, math.degrees(LAT), color=BLUE, lw=1.8)
    lab = at(F2, 0.42, math.degrees(LAT) * 0.5)
    text(bx, lab[0] + 0.01, lab[1], "φ", size=13, color=BLUE, bold=True, ha="left", va="center")
    PSI = math.degrees(math.atan2(P2[1], P2[0]))
    arcdeg(bx, O2, 0.15, 0.0, PSI, color="#9AA3B5", lw=1.4)
    lab = at(O2, 0.22, PSI * 0.5)
    text(bx, lab[0], lab[1] + 0.01, "ψ", size=11, color="#8A93A8", bold=True, ha="left",
         va="center")

    # 머리글 · 설명
    text(lay, RX + 0.01, 0.955, "자오선 단면 — 위도 φ 는 어디서 재나  (편평률 과장)", size=12.5,
         bold=True)
    text(lay, RX + 0.01, 0.160, "φ 측지위도 = 적도면 ~ 수직선.   지도 · GPS · 이 공식의 위도가 이것",
         size=9.5, color=BLUE)
    text(lay, RX + 0.01, 0.105, "ψ 지심위도 = 적도면 ~ 중심에서 그은 선.   다른 값 — 지도에서 안 쓴다",
         size=9.5, color="#8A93A8")
    text(lay, RX + 0.01, 0.050,
         "실제 지구(왼쪽 비율)는 거의 구라 φ - ψ 는 최대 0.19°.   경도 λ 는 영향 없음",
         size=9.5, color=GREY)

    save(fig, "fig23_lla.png")


# =============================================== 14장 : LLA ↔ ECEF 변환 흐름도
# 공식 카드의 글이 너무 길어 안 읽혀서, 수식을 상자로 · 흐름을 화살표로 그린다.
# 카드 위에 얹으므로 배경은 비운다 (fig20 · fig22 와 같은 방식). 수식은 mathtext.
# 상자 폭은 글자를 실제로 재서 정한다 (데이터 좌표 = 화면 px 이라 잰 값을 그대로 쓴다).
def draw_lla_flow():
    """윗줄 LLA → ECEF (한 번에), 아랫줄 ECEF → LLA (λ 는 바로, φ 와 h 는 한 상자 안에서 대입 반복).

    아랫줄 반복은 참고 자료 2.2.2 순서도 = f_EcefToLla() 와 같은 식이다.
    """
    from matplotlib.patches import FancyBboxPatch
    W, H = 2400, 463
    fig, lay = canvas(W, H, bg=None)
    ax = stage(fig, [0, 0, 1, 1], (0, W), (0, H))
    plt.rcParams["mathtext.fontset"] = "dejavusans"
    rd = fig.canvas.get_renderer()
    LINE, LOOPC, WARMFC = "#C9D6EC", ORANGE, "#FDF0E7"
    MATH, NOTE, PADX, PADY, GAP = 13.0, 9.8, 22.0, 12.0, 46.0     # px 단위 (DPI 170)

    def put(x, y, s, kind, ha="center", va="center", color=None, z=6):
        if kind == "math":
            return ax.text(x, y, s, fontsize=MATH, color=color or NAVY, ha=ha, va=va, zorder=z)
        return text(ax, x, y, s, size=NOTE, color=color or GREY, bold=(kind == "kb"),
                    ha=ha, va=va, zorder=z)

    def width(s, kind):
        t = put(-9999, -9999, s, kind)
        w = t.get_window_extent(renderer=rd).width
        t.remove()
        return w

    def box(x0, cy, lines, fc="white", ec=LINE, lw=1.4, z=4):
        """x0 에서 시작하는 상자. 폭은 글에 맞추고, 오른쪽 끝 x 를 돌려준다"""
        w = max(width(s, k) for s, k in lines) + 2 * PADX
        steps = [MATH * 2.36 * 1.15 if k == "math" else NOTE * 2.36 * 1.25 for s, k in lines]
        h = sum(steps) + 2 * PADY
        ax.add_patch(FancyBboxPatch((x0, cy - h / 2), w, h,
                                    boxstyle="round,pad=0,rounding_size=9", facecolor=fc,
                                    edgecolor=ec, lw=lw, zorder=z))
        y = cy + h / 2 - PADY
        for (s, k), st in zip(lines, steps):
            put(x0 + w / 2, y - st / 2, s, k, z=z + 2)
            y -= st
        return x0 + w, h

    def arr(pts, color=GREY, lw=1.6, head=14, z=3):
        for a_, b_ in zip(pts[:-2], pts[1:-1]):
            line(ax, a_, b_, color=color, lw=lw, z=z)
        arrow(ax, pts[-2], pts[-1], color=color, lw=lw, head=head, z=z)

    text(ax, 0, H - 20, "변환 공식", size=13.9, color=BLUE, bold=True, ha="left", va="center")
    X0 = 215                                              # 상자들이 시작하는 x (줄 이름 뒤)

    # ------------------------------------------------ 윗줄 : LLA → ECEF
    Y1 = 352
    text(ax, 0, Y1, "LLA → ECEF", size=14, color=NAVY, bold=True, ha="left", va="center")
    x, _ = box(X0, Y1, [(r"LLA $(\varphi,\,\lambda,\,h)$", "math")], ec=BLUE)
    arr([(x, Y1), (x + GAP, Y1)])
    x, _ = box(x + GAP, Y1, [(r"$R_N = a\,/\,\sqrt{1 - e^2\sin^2\varphi}$", "math"),
                             ("그 위도의 곡률반경", "k")])
    arr([(x, Y1), (x + GAP, Y1)])
    x, _ = box(x + GAP, Y1, [(r"$X = (R_N + h)\cos\varphi\,\cos\lambda$", "math"),
                             (r"$Y = (R_N + h)\cos\varphi\,\sin\lambda$", "math"),
                             (r"$Z = \left(R_N(1 - e^2) + h\right)\sin\varphi$", "math")])
    arr([(x, Y1), (x + GAP, Y1)])
    xe0 = x + GAP
    x, _ = box(xe0, Y1, [(r"ECEF $(X,\,Y,\,Z)$", "math")], ec=BLUE)
    put((xe0 + x) / 2, Y1 - 42, "넣으면 바로 나온다", "k", va="top")
    put(x + 30, Y1 + 13, "회전이 아니라 같은 점을 다른 수로 적는 식이라 C 표기가 없다.", "k",
        ha="left", color=FAINT)
    put(x + 30, Y1 - 13, "거꾸로도 전치가 아니라 아래 흐름으로 푼다.", "k", ha="left", color=FAINT)

    # ------------------------------------------------ 아랫줄 : ECEF → LLA
    YC, YT, YB = 150, 228, 118                            # 시작·끝 상자 / λ 가지 / φ·h 가지 중심
    text(ax, 0, YC, "ECEF → LLA", size=14, color=NAVY, bold=True, ha="left", va="center")
    x, _ = box(X0, YC, [(r"ECEF $(X,\,Y,\,Z)$", "math")], ec=BLUE)
    xs = x + GAP
    arr([(x, YC + 9), (x + GAP / 2, YC + 9), (x + GAP / 2, YT), (xs, YT)])
    arr([(x, YC - 9), (x + GAP / 2, YC - 9), (x + GAP / 2, YB), (xs, YB)])
    xl, _ = box(xs, YT, [(r"$\lambda = \arctan(Y\,/\,X)$", "math")])
    put(xl + 14, YT, "바로 나온다", "k", ha="left")
    xp, _ = box(xs, YB, [(r"$p = \sqrt{X^2 + Y^2}$", "math")])
    put((xs + xp) / 2, YB - 34, "자전축까지의 거리", "k", va="top")
    x = max(xl, xp)
    arr([(xp, YB), (x + GAP, YB)])
    x, _ = box(x + GAP, YB, [(r"$\varphi_0 = \arctan(Z\,/\,p),\ \ h_0 = 0$", "math"),
                             ("지구를 구로 본 첫 값", "k")])
    arr([(x, YB), (x + GAP, YB)])
    xl0 = x + GAP
    # 참고 자료 2.2.2 순서도 = f_EcefToLla() 와 같은 반복. φ 와 h 를 한 상자 안에서 번갈아 구한다
    x, hl = box(xl0, YB, [(r"$R_N = a\,/\,\sqrt{1 - e^2\sin^2\varphi}$"
                           r"$\qquad h = p\,/\cos\varphi - R_N$", "math"),
                          (r"$\tan\varphi = Z\,(R_N + h)\ /\ "
                           r"\left[\,p\,(R_N(1 - e^2) + h)\,\right]"
                           r"\quad\rightarrow\quad\varphi$", "math"),
                          ("오른쪽에도 φ 가 있어 한 번에 못 푼다 → φ 와 h 를 번갈아 대입", "k")],
               fc=WARMFC, ec=LOOPC)
    yb = YB - hl / 2
    arr([(x - 60, yb), (x - 60, 22), (xl0 + 60, 22), (xl0 + 60, yb)], color=LOOPC, lw=1.7)
    put((xl0 + x) / 2 + 30, (yb + 22) / 2, "앞 값과 다르면 다시 넣는다  (이번 과제 6 번)", "kb",
        color=LOOPC)
    xj = x + GAP / 2
    arr([(x, YB), (xj, YB), (xj, YC - 9), (xj + GAP / 2, YC - 9)])
    arr([(xl + 150, YT), (xj, YT), (xj, YC + 9), (xj + GAP / 2, YC + 9)])
    x, _ = box(xj + GAP / 2, YC, [(r"LLA $(\varphi,\,\lambda,\,h)$", "math")], ec=BLUE)
    if x > W:
        print("경고: 아랫줄이 %d px 로 캔버스(%d)를 넘는다" % (x, W))

    save(fig, "fig24_lla_flow.png", bg=None)


# =============================================== 흐름도 공통 도구 (10 · 12 · 13쪽)
# fig24 와 같은 그림 문법 — 상자 · 화살표 · 상자 아래 값. 글 폭을 렌더러로 재서 상자 폭을 정한다.
# 한 줄은 토막(segment)들의 목록이다 : (글, 종류) 로, 종류는
#   "n" 보통 글  "b" 굵은 글  "s" 아래 첨자(작게 · 낮게)  "m" mathtext 수식
class Flow(object):
    def __init__(self, fig, ax, scale=1.0):
        self.fig, self.ax = fig, ax
        self.rd = fig.canvas.get_renderer()
        self.MAIN, self.NOTE = 13.0 * scale, 9.8 * scale
        self.PADX, self.PADY, self.GAP = 22.0 * scale, 12.0 * scale, 46.0 * scale
        plt.rcParams["mathtext.fontset"] = "dejavusans"

    def _seg(self, x, y, s, st, size, color, bold, z, va="center"):
        dy = {"center": 0.0, "top": -size * 1.25, "bottom": size * 1.25}[va]   # 줄 중심으로 환산
        y = y + dy
        if st == "m":
            t = self.ax.text(x, y, s, fontsize=size, color=color, ha="left", va="center",
                             zorder=z)
        elif st == "s":
            t = self.ax.text(x, y - size * 0.62, s, fontproperties=(BOLD if bold else REG),
                             fontsize=size * 0.70, color=color, ha="left", va="center", zorder=z)
        else:
            t = text(self.ax, x, y, s, size=size, color=color, bold=(bold or st == "b"),
                     ha="left", va="center", zorder=z)
        return t, t.get_window_extent(renderer=self.rd).width

    def width(self, segs, size, bold=False):
        w = 0.0
        for s, st in segs:
            t, w_ = self._seg(-9999, -9999, s, st, size, NAVY, bold, 1)
            t.remove()
            w += w_
        return w

    def rich(self, x, y, segs, size=None, color=NAVY, bold=False, ha="center", z=6, va="center"):
        """토막들을 한 줄로. 돌려주는 값은 줄의 폭"""
        size = size or self.MAIN
        if isinstance(segs, str):
            segs = [(segs, "n")]
        total = self.width(segs, size, bold)
        x0 = {"center": x - total / 2, "left": x, "right": x - total}[ha]
        for s, st in segs:
            _, w = self._seg(x0, y, s, st, size, color, bold, z, va=va)
            x0 += w
        return total

    def note(self, x, y, segs, color=GREY, ha="center", bold=False, z=6, size=None, va="center"):
        return self.rich(x, y, segs, size=size or self.NOTE, color=color, bold=bold, ha=ha, z=z,
                         va=va)

    def box(self, x0, cy, lines, fc="white", ec="#C9D6EC", lw=1.4, z=4, minw=0.0):
        """lines = [(segs, kind[, color])], kind 은 "main" | "note". x0 에서 시작, 오른쪽 끝 x 와
        높이를 돌려준다"""
        from matplotlib.patches import FancyBboxPatch
        spec = []
        for ln in lines:
            segs, kind = ln[0], ln[1]
            color = ln[2] if len(ln) > 2 else (NAVY if kind == "main" else GREY)
            size = self.MAIN if kind == "main" else self.NOTE
            if isinstance(segs, str):
                segs = [(segs, "b" if kind == "main" else "n")]
            spec.append((segs, size, color, kind == "main"))
        w = max(max(self.width(s, sz, b) for s, sz, c, b in spec) + 2 * self.PADX, minw)
        steps = [sz * 2.36 * (1.15 if b else 1.25) for s, sz, c, b in spec]
        h = sum(steps) + 2 * self.PADY
        self.ax.add_patch(FancyBboxPatch((x0, cy - h / 2), w, h,
                                         boxstyle="round,pad=0,rounding_size=9", facecolor=fc,
                                         edgecolor=ec, lw=lw, zorder=z))
        y = cy + h / 2 - self.PADY
        for (segs, sz, color, b), st in zip(spec, steps):
            self.rich(x0 + w / 2, y - st / 2, segs, size=sz, color=color, bold=b, z=z + 2)
            y -= st
        return x0 + w, h

    def arr(self, pts, color=GREY, lw=1.6, head=14, z=3):
        for a_, b_ in zip(pts[:-2], pts[1:-1]):
            line(self.ax, a_, b_, color=color, lw=lw, z=z)
        arrow(self.ax, pts[-2], pts[-1], color=color, lw=lw, head=head, z=z)

    def matrix(self, x0, cy, rows, size=None, color=NAVY, bold=False, z=6, label=None,
               label_color=GREY, label_ha="center"):
        """대괄호 행렬. rows = [[문자열, ...], ...] (숫자는 오른쪽 정렬). 오른쪽 끝 x 를 돌려준다.
        label 을 주면 행렬 아래 가운데에 작은 글씨로 단다"""
        size = size or self.MAIN
        ncol = len(rows[0])
        widths = [max(self.width([(r[c], "n")], size, bold) for r in rows) for c in range(ncol)]
        colgap = size * 1.7
        step = size * 2.36 * 1.12
        h = len(rows) * step
        top, bot = cy + h / 2, cy - h / 2
        tick = size * 0.55
        bx = x0 + 4
        for xx, sgn in ((bx, 1), (bx + 8 + sum(widths) + ncol * colgap, -1)):
            line(self.ax, (xx, bot), (xx, top), color=color, lw=1.5, z=z)
            line(self.ax, (xx, top), (xx + sgn * tick, top), color=color, lw=1.5, z=z)
            line(self.ax, (xx, bot), (xx + sgn * tick, bot), color=color, lw=1.5, z=z)
        x = bx + 8 + colgap / 2
        for c in range(ncol):
            for i, r in enumerate(rows):
                self.rich(x + widths[c], top - (i + 0.5) * step, [(r[c], "n")], size=size,
                          color=color, bold=bold, ha="right", z=z)
            x += widths[c] + colgap
        x1 = bx + 8 + sum(widths) + ncol * colgap + 4
        if label:
            lx = {"center": (x0 + x1) / 2, "left": x0 + 4}[label_ha]
            self.note(lx, bot - 6, label, color=label_color, va="top", ha=label_ha)
        return x1

    def op(self, x, cy, s, size=None, color=NAVY):
        """행렬 사이 연산자. 오른쪽 끝 x 를 돌려준다"""
        size = size or self.MAIN
        w = self.rich(x + size * 0.9, cy, [(s, "n")], size=size, color=color, ha="center")
        return x + size * 1.8

    def chain(self, x, cy, boxes, under=None, **kw):
        """상자들을 화살표로 이어 붙인다. under = 상자마다 아래에 적을 값 (없으면 None).
        돌려주는 값 : (오른쪽 끝 x, [(x0, x1, h) ...])"""
        spans = []
        for i, lines in enumerate(boxes):
            if i:
                self.arr([(x, cy), (x + self.GAP, cy)])
                x += self.GAP
            x0 = x
            x, h = self.box(x, cy, lines, **kw)
            spans.append((x0, x, h))
            if under and under[i]:
                s, color = under[i]
                self.note((x0 + x) / 2, cy - h / 2 - 6, s, color=color, bold=True, va="top")
        return x, spans


def _flow_canvas(W, H, scale=1.0):
    fig, lay = canvas(W, H, bg=None)
    ax = stage(fig, [0, 0, 1, 1], (0, W), (0, H))
    return fig, ax, Flow(fig, ax, scale)


def _vec(name, sub, tail=""):
    """p_동체 같은 벡터 이름 토막"""
    segs = [(name, "b"), (sub, "s")]
    if tail:
        segs.append((tail, "b"))
    return segs


# =============================================== 10장 : 안테나 → 동체 흐름도
def draw_body_flow():
    """[p_안테나] → 축 맞춤 A → Rz(설치방위) → Ry(백틸트) → + 레버암 → [p_동체].

    A 는 안테나 면 축을 동체 축에 겹쳐 놓는 상수 행렬이라 벡터에 가장 먼저 닿는다
    (식 Rz · Ry · A 의 맨 오른쪽). 그다음 설치 방위, 백틸트 순으로 돌리는데, 이쪽은
    12장 자세 회전 yaw → pitch → roll 과 같은 약속이라 식을 왼쪽부터 읽은 순서다.
    """
    W, H = 2400, 300
    fig, ax, F = _flow_canvas(W, H, scale=1.18)
    w0 = F.rich(0, H - 24, [("안테나 → 동체 변환", "b")], size=11.4 * 1.18, color=BLUE,
                ha="left")
    F.note(w0 + 18, H - 24, "축을 먼저 맞추고, 설치 방위 → 백틸트 순서로 돌린다", ha="left")
    Y = 178
    x, spans = F.chain(0, Y, [
        [(_vec("p", "안테나", "  (x 왼쪽, y 위, z 보어사이트)"), "main")],
        [("축 맞춤 A", "main")],
        [("Rz(설치방위)", "main")],
        [("Ry(백틸트)", "main")],
        [("+ 레버암", "main")],
        [(_vec("p", "동체"), "main")],
    # 상자에 이미 "+ 레버암" 이 있어 아래 라벨은 값만 적는다 (옆 라벨과 붙지 않게)
    ], under=[("이번 과제 값 →", ORANGE), ("Rz(-90°)·Rx(-90°)", GREY),
              ("설치 방위 90°", ORANGE), ("백틸트 0°", ORANGE),
              ("(-30, 0, -10) m", ORANGE), None],
        ec=BLUE)
    if x > W:
        print("경고: fig25 가 %d px 로 캔버스(%d)를 넘는다" % (x, W))
    F.note(0, 40, [("식으로 쓰면   ", "n")] + _vec("p", "동체", " = Rz(설치방위) · Ry(백틸트) · A · ")
           + _vec("p", "안테나", " + 레버암")
           + [("      A 는 안테나 면 기준 축을 동체 축으로 돌려 놓는 상수 행렬 (참고 자료 2.2.9)", "n")],
           ha="left", color=NAVY)
    _save_cropped(fig, "fig25_body_flow.png", pad=8)   # 오른쪽 빈 자리를 잘라 글씨를 키운다


# =============================================== 12장 : 동체 → NED 흐름도
def draw_ned_flow():
    """[p_동체] → Rz(yaw) → Ry(pitch) → Rx(roll) → [p_NED] → 축만 바꿔 적음 → [p_ENU].

    자세 회전은 yaw → pitch → roll 순서로 적용한다. 순서가 바뀌면 다른 값이 나오므로
    상자도 식(Rz · Ry · Rx)과 같은 순서로 왼쪽부터 늘어놓는다."""
    W, H = 2400, 339
    fig, ax, F = _flow_canvas(W, H)
    w0 = F.rich(0, H - 20, [("동체 → NED 변환 공식", "b")], size=11.4, color=BLUE, ha="left")
    F.note(w0 + 18, H - 20, "자세는 yaw → pitch → roll 순서로 돌린 것 · 상자도 식과 같은 순서", ha="left")
    Y = 190
    x, spans = F.chain(70, Y, [
        [(_vec("p", "동체"), "main")],
        [("Rz(yaw)", "main")],
        [("Ry(pitch)", "main")],
        [("Rx(roll)", "main")],
        [(_vec("p", "NED", "  (N, E, D)"), "main")],
        [("축만 바꿔 적음", "main"), ("(E, N, U) = (E, N, -D)", "note")],
        [(_vec("p", "ENU", "  (E, N, U)"), "main")],
    ], under=[("원점 = 무게중심", GREY), ("이번 과제  45°", ORANGE), ("이번 과제  0°", ORANGE),
              ("이번 과제  0°", ORANGE), ("원점 같음 → 평행이동 없음", GREY), None,
              ("같은 점, 다른 표기", GREY)],
        ec=BLUE)
    F.note(0, 62, [("식으로 쓰면   ", "n")] + _vec("p", "NED", " = Rz(yaw) · Ry(pitch) · Rx(roll) · ")
           + _vec("p", "동체")
           + [("   — yaw → pitch → roll (3-2-1) 순서다. 순서가 바뀌면 값이 달라진다", "n")],
           ha="left", color=NAVY)
    F.note(0, 26, [("이번 과제   ", "n")] + _vec("p", "NED", " = Rz(45°) · Ry(0°) · Rx(0°) · ")
           + _vec("p", "동체", " = Rz(45°) · ") + _vec("p", "동체"),
           ha="left", color=ORANGE)
    F.note(x + 30, Y + 14, [("줄임말 :  세 회전행렬의 곱을 C", "n"), ("ned←body", "s"), (" 로 쓴다", "n")],
           ha="left")
    F.note(x + 30, Y - 14, "\"동체 좌표를 NED 좌표로 바꾸는 행렬\" — 설계 장에서 이 줄임말을 쓴다", ha="left")
    save(fig, "fig26_ned_flow.png", bg=None)


# =============================================== 13장 : NED → ECEF · ECEF → ECI 흐름도
def draw_ecef_flow():
    """윗줄 [p_NED] → Ry(−90°−위도) → Rz(경도) → + P_배 → [p_ECEF], P_배 는 아랫줄 오른쪽 상자에서
    올라온다. 아랫줄 [p_ECEF] → Rz(θ) → [p_ECI]."""
    W, H = 2400, 384
    fig, ax, F = _flow_canvas(W, H)
    text(ax, 0, H - 20, "변환 공식", size=11.4, color=BLUE, bold=True, ha="left", va="center")
    X0 = 215
    Y1, Y2 = 265, 95
    text(ax, 0, Y1, "NED → ECEF", size=14, color=NAVY, bold=True, ha="left", va="center")
    x, spans = F.chain(X0, Y1, [
        [(_vec("p", "NED"), "main")],
        [("Ry(-90° - 위도)", "main")],
        [("Rz(경도)", "main")],
        [(_vec("+ P", "배"), "main")],
        [(_vec("p", "ECEF"), "main")],
    ], under=[None, ("지구 축에 맞춰 돌린다", GREY), None, None, None], ec=BLUE)
    F.note(x + 30, Y1 + 14, [("줄임말 :  Rz(경도) · Ry(-90° - 위도) = C", "n"), ("ecef←ned", "s")],
           ha="left")
    F.note(x + 30, Y1 - 14, [("R", "n"), ("N", "s"), (" 은 그 위도의 곡률반경, e² 은 WGS-84 상수 — 다음 장 LLA 에서", "n")],
           ha="left")
    plus = spans[3]                                    # + P_배 상자
    text(ax, 0, Y2, "ECEF → ECI", size=14, color=NAVY, bold=True, ha="left", va="center")
    x2, sp2 = F.chain(X0, Y2, [
        [(_vec("p", "ECEF"), "main")],
        [("Rz(θ)", "main")],
        [(_vec("p", "ECI"), "main")],
    ], under=[None, ("θ = 지구 자전각 (GMST) — 시각으로 정해진다", GREY), None], ec=BLUE)
    F.note(x2 + 30, Y2, "지구가 돈 각만큼 되돌리면 별 기준 좌표.  거꾸로는 전치.", ha="left")
    # P_배 상자 : 배의 위경도 · 고도 → ECEF
    xb0 = 1330
    xb, hb = F.box(xb0, Y2, [
        (_vec("P", "배", "  = 배의 위경도 · 고도 (φ, λ, h) 에서"), "main"),
        ([(r"$X = (R_N + h)\cos\varphi\cos\lambda,\quad Y = (R_N + h)\cos\varphi\sin\lambda,"
           r"\quad Z = \left(R_N(1 - e^2) + h\right)\sin\varphi$", "m")], "note", NAVY),
    ], ec=ORANGE, fc="#FDF0E7")
    if xb > W:
        print("경고: P_배 상자가 %d px 로 캔버스(%d)를 넘는다" % (xb, W))
    px = (plus[0] + plus[1]) / 2
    xc = (xb0 + xb) / 2
    YM = 180
    F.arr([(xc, Y2 + hb / 2), (xc, YM), (px, YM), (px, Y1 - plus[2] / 2)], color=ORANGE, lw=1.7)
    F.note(px + 12, (YM + Y1 - plus[2] / 2) / 2, "배의 ECEF 위치를 더한다", color=ORANGE,
           ha="left", bold=True)
    save(fig, "fig27_ecef_flow.png", bg=None)


# =============================================== 19 · 20장 : 단계별 계산의 행렬 부분
# 계산 과정은 슬라이드 글(복사 가능)로 적고, 글로 못 적는 대괄호 행렬식만 그림으로 뽑는다.
# 투명 배경, 내용 크기로 잘라 저장 (슬라이드에서는 높이로 크기를 맞춘다).
def _save_cropped(fig, name, pad=6):
    """그린 내용의 경계로 잘라 저장한다"""
    import io
    from PIL import Image
    buf = io.BytesIO()
    fig.savefig(buf, dpi=DPI, transparent=True)
    plt.close(fig)
    im = Image.open(buf)
    box = im.getbbox()
    box = (max(box[0] - pad, 0), max(box[1] - pad, 0), min(box[2] + pad, im.width),
           min(box[3] + pad, im.height))
    im.crop(box).save(os.path.join(OUT_DIR, name))
    print("생성: %s  (%dx%d)" % (os.path.join(OUT_DIR, name), box[2] - box[0], box[3] - box[1]))


def draw_steps():
    S = 1.35                                            # 글자 배율 (슬라이드에서 높이 0.60 in 정도)

    # ② 안테나 → 동체 : Rz(90°)·Ry(0°)·A · p_안테나 + 레버암 = p_동체
    fig, ax, F = _flow_canvas(2400, 260, S)
    cy = 130
    x = F.matrix(10, cy, [["1", "0", "0"], ["0", "0", "1"], ["0", "-1", "0"]])
    x = F.op(x, cy, "·")
    x = F.matrix(x, cy, [["+10,000.00"], ["0.00"], ["+17,320.51"]])
    x = F.op(x, cy, "=")
    x = F.matrix(x, cy, [["+10,000.00"], ["+17,320.51"], ["0.00"]])
    x = F.op(x, cy, "+")
    x = F.matrix(x, cy, [["-30"], ["0"], ["-10"]])
    x = F.op(x, cy, "=")
    x = F.matrix(x, cy, [["+9,970.00"], ["+17,320.51"], ["-10.00"]], color=ORANGE, bold=True)
    _save_cropped(fig, "fig29_step2.png")

    # ③ 동체 → NED : Rz(45°) · p_동체 = p_NED
    fig, ax, F = _flow_canvas(2400, 260, S)
    x = F.matrix(10, cy, [["cos 45°", "-sin 45°", "0"], ["sin 45°", "cos 45°", "0"], ["0", "0", "1"]])
    x = F.op(x, cy, "·")
    x = F.matrix(x, cy, [["+9,970.00"], ["+17,320.51"], ["-10.00"]])
    x = F.op(x, cy, "=")
    x = F.matrix(x, cy, [["-5,197.59"], ["+19,297.30"], ["-10.00"]], color=ORANGE, bold=True)
    _save_cropped(fig, "fig30_step3.png")

    # ④ NED → ECEF : C_ecef←ned (풀어 쓴 것) = (숫자) · p_NED = 상대 벡터
    fig, ax, F = _flow_canvas(2400, 260, S)
    x = F.matrix(10, cy, [["-sinφ cosλ", "-sinλ", "-cosφ cosλ"], ["-sinφ sinλ", "cosλ", "-cosφ sinλ"],
                          ["cosφ", "0", "-sinφ"]])
    x = F.op(x, cy, "=")
    x = F.matrix(x, cy, [["0.3597", "-0.7954", "0.4878"], ["-0.4721", "-0.6061", "-0.6401"],
                         ["0.8048", "0", "-0.5935"]])
    x = F.op(x, cy, "·")
    x = F.matrix(x, cy, [["-5,197.59"], ["+19,297.30"], ["-10.00"]])
    x = F.op(x, cy, "=")
    x = F.matrix(x, cy, [["-17,223.68"], ["-9,235.66"], ["-4,177.15"]], color=ORANGE, bold=True)
    _save_cropped(fig, "fig31_step4.png")



if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "all"
    names = ("all", "steps", "lla", "llaflow", "bodyflow", "nedflow", "ecefflow", "insmat", "attitude", "nedcalc", "intro", "observers", "frames", "target", "polar", "cartesian",
             "derive", "body", "layout", "rotate", "nedenu", "ecef", "roll", "uv",
             "twoops")
    if what not in names:
        sys.exit("사용법: python make_figures.py [%s]" % "|".join(names))
    if what in ("all", "lla"):
        draw_lla()
    if what in ("all", "llaflow"):
        draw_lla_flow()
    if what in ("all", "bodyflow"):
        draw_body_flow()
    if what in ("all", "nedflow"):
        draw_ned_flow()
    if what in ("all", "ecefflow"):
        draw_ecef_flow()
    if what in ("all", "steps"):
        draw_steps()
    if what in ("all", "intro"):
        draw_intro()
    if what in ("all", "target"):
        draw_one_target()
    if what in ("all", "polar"):
        draw_polar()
    if what in ("all", "cartesian"):
        draw_cartesian()
    if what in ("all", "observers"):
        draw_observers()
    if what in ("all", "frames"):
        draw_two_frames()
    if what in ("all", "derive"):
        draw_derive()
    if what in ("all", "body"):
        draw_body()
    if what in ("all", "layout"):
        draw_layout()
    if what in ("all", "rotate"):
        draw_rotate()
    if what in ("all", "nedenu"):
        draw_ned_enu()
    if what in ("all", "ecef"):
        draw_ecef_eci()
    if what in ("all", "roll"):
        draw_roll_beam()
    if what in ("all", "uv"):
        draw_uv()
    if what in ("all", "twoops"):
        draw_two_ops()
    if what in ("all", "insmat"):
        draw_ins_matrix()
    if what in ("all", "attitude"):
        draw_attitude()
    if what in ("all", "nedcalc"):
        draw_ned_calc()
