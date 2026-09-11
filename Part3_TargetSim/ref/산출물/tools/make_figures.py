# -*- coding: utf-8 -*-
"""Chapter 3 발표자료 그림 생성기. 개념 설명용 그림 18 장을 PNG 로 뽑는다.

    python make_figures.py              # 전부
    python make_figures.py f08 f11      # 이름 앞부분으로 골라서

결과는 ../figures/ 에 저장되고 같은 파일이 build_deck.py 로 pptx 에 들어간다.
그림 크기는 슬라이드에 놓일 상자와 같은 인치로 잡고 DPI 170 으로 저장하므로,
그림 안의 글자 크기(pt) 가 슬라이드 위에서도 같은 크기로 보인다.
좌표 단위는 1/100 인치. 글꼴·색은 Chapter 2 그림과 같은 규칙이다.
맑은 고딕에 없는 글자(✓ ✕ ▸ ⋮ −) 는 쓰지 않고 선으로 그리거나 다른 글자로 바꾼다.
"""
import math
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import font_manager as fm
from matplotlib import patheffects as pe
from matplotlib.patches import Arc, Circle, FancyArrowPatch, FancyBboxPatch, Polygon, Wedge, Rectangle

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.normpath(os.path.join(HERE, "..", "figures"))

FONT_CANDIDATES = [
    (r"C:\Windows\Fonts\malgun.ttf", r"C:\Windows\Fonts\malgunbd.ttf"),
    ("/usr/share/fonts/truetype/nanum/NanumBarunGothic.ttf",
     "/usr/share/fonts/truetype/nanum/NanumBarunGothicBold.ttf"),
]
for _reg, _bold in FONT_CANDIDATES:
    if os.path.exists(_reg) and os.path.exists(_bold):
        REG, BOLD = fm.FontProperties(fname=_reg), fm.FontProperties(fname=_bold)
        break
else:
    sys.exit("한글 글꼴을 못 찾음 (맑은 고딕 / 나눔바른고딕)")
MONO = fm.FontProperties(fname=r"C:\Windows\Fonts\consola.ttf") \
    if os.path.exists(r"C:\Windows\Fonts\consola.ttf") else REG

# Chapter 2 그림과 같은 색
NAVY = "#18202F"
GREY = "#5A6478"
FAINT = "#8A93A8"
BLUE = "#2B57A6"
HULL = "#E2EAF7"
DECK = "#C7D6EE"
ORANGE = "#C2521C"
GREEN = "#2F855A"
WATER = "#9FBADF"
SOFT = "#EEF1F7"
PEACH = "#FBEADF"
DPI = 170


# ---------------------------------------------------------------- 캔버스
def canvas(w_in, h_in, bg="white"):
    """그림 전체를 하나의 축으로 쓴다. 좌표 단위는 1/100 in, 가로세로 1:1."""
    fig = plt.figure(figsize=(w_in, h_in), dpi=DPI, facecolor=bg)
    ax = fig.add_axes([0, 0, 1, 1])
    W, H = w_in * 100.0, h_in * 100.0
    ax.set_xlim(0, W)
    ax.set_ylim(0, H)
    ax.set_aspect("equal", adjustable="box")
    ax.set_axis_off()
    ax.patch.set_alpha(0)
    return fig, ax, W, H


def T(ax, x, y, s, size=11, color=NAVY, bold=False, ha="left", va="center", mono=False, **kw):
    fp = MONO if mono else (BOLD if bold else REG)
    return ax.text(x, y, s, fontproperties=fp, fontsize=size, color=color, ha=ha, va=va, **kw)


def arrow(ax, p0, p1, color=BLUE, lw=1.9, head=10.0, z=5, ls="-", halo=False, style="-|>"):
    a = FancyArrowPatch(p0, p1, arrowstyle=style, mutation_scale=head, lw=lw, color=color,
                        linestyle=ls, shrinkA=0, shrinkB=0, zorder=z, joinstyle="miter")
    if halo:
        a.set_path_effects([pe.withStroke(linewidth=lw + 2.4, foreground="white")])
    ax.add_patch(a)
    return a


def curve(ax, p0, p1, rad=0.3, color=ORANGE, lw=1.8, head=11, z=6, ls="-", style="-|>"):
    a = FancyArrowPatch(p0, p1, arrowstyle=style, mutation_scale=head, lw=lw, color=color,
                        linestyle=ls, connectionstyle="arc3,rad=%s" % rad, shrinkA=0, shrinkB=0,
                        zorder=z)
    ax.add_patch(a)
    return a


def line(ax, p0, p1, color=FAINT, lw=1.0, ls="-", z=2, dashes=None):
    ln, = ax.plot([p0[0], p1[0]], [p0[1], p1[1]], color=color, lw=lw, ls=ls, zorder=z,
                  solid_capstyle="butt")
    if dashes:
        ln.set_dashes(dashes)
    return ln


def poly(ax, pts, fc=HULL, ec=BLUE, lw=1.6, z=3, closed=True):
    ax.add_patch(Polygon(pts, closed=closed, facecolor=fc, edgecolor=ec, lw=lw, zorder=z,
                         joinstyle="round"))


def box(ax, x, y, w, h, fc=SOFT, ec="none", lw=1.0, z=1, r=6):
    """둥근 사각형. (x, y) 는 왼쪽 아래."""
    b = FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=%s" % r,
                       facecolor=fc, edgecolor=ec, lw=lw, zorder=z)
    ax.add_patch(b)
    return b


def dot(ax, c, r=3.0, color=ORANGE, z=7, ec="white", lw=0.8):
    ax.add_patch(Circle(c, r, facecolor=color, edgecolor=ec, lw=lw, zorder=z))


def numcircle(ax, c, n, r=10, fc=BLUE, tc="white", size=10.5, z=9):
    ax.add_patch(Circle(c, r, facecolor=fc, edgecolor="none", zorder=z))
    T(ax, c[0], c[1] - 0.6, str(n), size=size, color=tc, bold=True, ha="center", va="center",
      zorder=z + 1)


def tri(ax, c, s=4.0, color="#C9D6EC", z=4):
    """작은 오른쪽 삼각표 (▸ 대신)."""
    poly(ax, [(c[0] - s * 0.6, c[1] + s), (c[0] - s * 0.6, c[1] - s), (c[0] + s * 0.8, c[1])],
         fc=color, ec=color, lw=0.5, z=z)


def mark_ok(ax, c, s=7.0, color=GREEN, lw=2.2, z=8):
    ax.plot([c[0] - s, c[0] - s * 0.25, c[0] + s], [c[1], c[1] - s * 0.8, c[1] + s * 0.8],
            color=color, lw=lw, zorder=z, solid_capstyle="round", solid_joinstyle="round")


def mark_no(ax, c, s=6.0, color=ORANGE, lw=2.2, z=8):
    ax.plot([c[0] - s, c[0] + s], [c[1] - s, c[1] + s], color=color, lw=lw, zorder=z, solid_capstyle="round")
    ax.plot([c[0] - s, c[0] + s], [c[1] + s, c[1] - s], color=color, lw=lw, zorder=z, solid_capstyle="round")


def vdots(ax, c, gap=7.0, r=1.6, color=GREY, z=6):
    for k in (-1, 0, 1):
        ax.add_patch(Circle((c[0], c[1] + k * gap), r, facecolor=color, edgecolor="none", zorder=z))


def at(c, r, deg):
    return (c[0] + r * math.cos(math.radians(deg)), c[1] + r * math.sin(math.radians(deg)))


def arcdeg(ax, c, r, a0, a1, color=ORANGE, lw=1.7, z=6, ls="-"):
    ax.add_patch(Arc(c, 2 * r, 2 * r, theta1=a0, theta2=a1, color=color, lw=lw, zorder=z, ls=ls))


# ---------------------------------------------------------------- 아이콘
HULL_TOP = [(0.00, -0.049), (0.00, 0.049), (0.13, 0.059), (0.56, 0.062),
            (0.73, 0.058), (0.87, 0.045), (0.955, 0.023), (1.00, 0.000),
            (0.955, -0.023), (0.87, -0.045), (0.73, -0.058), (0.56, -0.062),
            (0.13, -0.059)]


def ship_top(ax, c, length, heading=90.0, z=4, fc=HULL, ec=BLUE, wake=False):
    """위에서 본 배. c 는 배 한가운데, heading 은 선수 방향(0 오른쪽, 90 위쪽)."""
    ca, sa = math.cos(math.radians(heading)), math.sin(math.radians(heading))

    def P(u, v):
        dx, dy = (u - 0.5) * length, v * length
        return (c[0] + dx * ca - dy * sa, c[1] + dx * sa + dy * ca)

    poly(ax, [P(u, v) for u, v in HULL_TOP], fc=fc, ec=ec, lw=1.3, z=z)
    poly(ax, [P(0.22, -0.033), P(0.22, 0.033), P(0.36, 0.033), P(0.36, -0.033)], fc=DECK, ec=ec, lw=0.8, z=z + 1)
    poly(ax, [P(0.54, -0.038), P(0.54, 0.038), P(0.72, 0.038), P(0.72, -0.038)], fc=DECK, ec=ec, lw=0.8, z=z + 1)
    poly(ax, [P(0.80, -0.026), P(0.80, 0.026), P(0.88, 0.026), P(0.88, -0.026)], fc=DECK, ec=ec, lw=0.8, z=z + 1)
    if wake:
        for k in (1, 2):
            d = 0.045 * k
            line(ax, P(-0.01, 0.045), P(-0.13 - d, 0.070 + d), color=WATER, lw=1.0, z=z - 1)
            line(ax, P(-0.01, -0.045), P(-0.13 - d, -0.070 - d), color=WATER, lw=1.0, z=z - 1)
    return P


PLANE_TOP = [(0.00, 1.00), (0.08, 0.82), (0.10, 0.30), (0.62, -0.06), (0.62, -0.20), (0.10, -0.04),
             (0.08, -0.60), (0.30, -0.80), (0.30, -0.92), (0.03, -0.82), (0.00, -0.96),
             (-0.03, -0.82), (-0.30, -0.92), (-0.30, -0.80), (-0.08, -0.60), (-0.10, -0.04),
             (-0.62, -0.20), (-0.62, -0.06), (-0.10, 0.30), (-0.08, 0.82)]


def plane_top(ax, c, size, heading=90.0, z=5, fc=PEACH, ec=ORANGE, lw=1.3):
    """위에서 본 비행기. size 는 기수에서 꼬리까지 절반 길이. heading 0 오른쪽, 90 위쪽."""
    a = math.radians(heading - 90.0)
    ca, sa = math.cos(a), math.sin(a)
    pts = [(c[0] + (x * ca - y * sa) * size, c[1] + (x * sa + y * ca) * size) for x, y in PLANE_TOP]
    poly(ax, pts, fc=fc, ec=ec, lw=lw, z=z)


def plane_side(ax, c, size, pitch=0.0, z=5, fc=PEACH, ec=ORANGE, nose_right=True):
    """옆에서 본 비행기 (기수 오른쪽). pitch 는 기수가 들리는 각."""
    body = [(-1.0, 0.0), (-0.85, 0.10), (0.55, 0.12), (1.0, 0.0), (0.55, -0.12), (-0.85, -0.10)]
    fin = [(-0.95, 0.08), (-0.55, 0.08), (-0.45, 0.42), (-0.80, 0.42)]
    wing = [(-0.05, 0.02), (0.30, 0.02), (0.18, -0.16), (-0.12, -0.16)]
    a = math.radians(pitch)
    ca, sa = math.cos(a), math.sin(a)
    sx = 1.0 if nose_right else -1.0

    def P(pts):
        return [(c[0] + (sx * x * ca - y * sa) * size, c[1] + (sx * x * sa + y * ca) * size) for x, y in pts]

    poly(ax, P(fin), fc="#F6D9C8", ec=ec, lw=1.1, z=z)
    poly(ax, P(body), fc=fc, ec=ec, lw=1.3, z=z + 1)
    poly(ax, P(wing), fc="#F6D9C8", ec=ec, lw=1.0, z=z + 2)


def plane_rear(ax, c, size, roll=0.0, z=5, fc=PEACH, ec=ORANGE):
    """뒤에서 본 비행기. roll 은 오른쪽 날개가 내려가는 쪽이 +."""
    a = math.radians(-roll)
    ca, sa = math.cos(a), math.sin(a)

    def P(pts):
        return [(c[0] + (x * ca - y * sa) * size, c[1] + (x * sa + y * ca) * size) for x, y in pts]

    poly(ax, P([(-1.0, 0.02), (1.0, 0.02), (1.0, -0.08), (-1.0, -0.08)]), fc="#F6D9C8", ec=ec, lw=1.0, z=z)
    poly(ax, P([(-0.06, 0.10), (0.06, 0.10), (0.04, 0.55), (-0.04, 0.55)]), fc="#F6D9C8", ec=ec, lw=1.0, z=z)
    ax.add_patch(Circle(c, 0.16 * size, facecolor=fc, edgecolor=ec, lw=1.3, zorder=z + 1))


def globe(ax, c, r, z=2, fc="#EAF0FA", ec=BLUE, grid=True, lw=1.3):
    ax.add_patch(Circle(c, r, facecolor=fc, edgecolor=ec, lw=lw, zorder=z))
    if grid:
        for k in (-0.55, 0.0, 0.55):                       # 위도선
            hh = r * 0.32 * (1.0 - abs(k) * 0.55)
            y = c[1] + k * r
            hw = math.sqrt(max(r * r - (k * r) ** 2, 0))
            ax.add_patch(Arc((c[0], y), 2 * hw, 2 * hh, theta1=180, theta2=360, color=ec, lw=0.7,
                             zorder=z + 1, alpha=0.75))
        for k in (-0.5, 0.0, 0.5):                          # 경도선
            ax.add_patch(Arc(c, 2 * r * abs(k) if k else 0.5, 2 * r, theta1=0, theta2=360, color=ec,
                             lw=0.7, zorder=z + 1, alpha=0.75))
        line(ax, (c[0] - r, c[1]), (c[0] + r, c[1]), color=ec, lw=0.7, z=z + 1)


def save(fig, name):
    os.makedirs(OUT_DIR, exist_ok=True)
    path = os.path.join(OUT_DIR, name)
    fig.savefig(path, dpi=DPI, facecolor=fig.get_facecolor())
    plt.close(fig)
    print("  " + name)


# ================================================================ 그림들
def f01_overview():
    """3장. 60초 동안 플랫폼과 표적 둘이 놓인 모습 (대략 축척, 17 unit = 1 km)."""
    fig, ax, W, H = canvas(7.4, 3.75)
    ox, oy = 262.0, 58.0                        # 플랫폼 위치
    K = 17.0                                    # unit / km
    box(ax, 20, 15, 500, 350, fc="#F5F8FC", ec="#D0D9E8", lw=0.8, z=0, r=8)
    for k in range(1, 8):
        line(ax, (30, 15 + k * 44), (510, 15 + k * 44), color="#E3E9F3", lw=0.7, z=0)
    for k in range(1, 11):
        line(ax, (20 + k * 46, 25), (20 + k * 46, 355), color="#E3E9F3", lw=0.7, z=0)
    # 플랫폼
    ship_top(ax, (ox, oy), 50, heading=90, z=4)
    T(ax, ox + 34, oy + 5, "플랫폼 (우리 배)", size=10.5, color=BLUE, bold=True)
    T(ax, ox + 34, oy - 11, "32.0°N  126.0°E · 정지", size=9, color=GREY)
    # 대공 표적
    ax_, ay_ = ox, oy + 13.3 * K
    plane_top(ax, (ax_, ay_), 15, heading=270)
    arrow(ax, (ax_, ay_ - 22), (ax_, oy + 1.3 * K + 14), color=ORANGE, lw=2.0, head=11, ls="--")
    T(ax, ax_ - 20, ay_ + 9, "표적 2 · 대공", size=10.5, color=ORANGE, bold=True, ha="right")
    T(ax, ax_ - 20, ay_ - 6, "고도 300 m · 200 m/s · 남쪽으로", size=9, color=GREY, ha="right")
    T(ax, ax_ + 10, oy + 7.2 * K, "60초 동안 12 km", size=9.5, color=ORANGE, bold=True)
    T(ax, ax_ + 10, oy + 6.4 * K, "(0.1초마다 20 m)", size=8.5, color=GREY)
    # 대함 표적
    sx_, sy_ = ox + 2.83 * K, oy + 13.86 * K
    ship_top(ax, (sx_, sy_), 36, heading=180, z=4, fc="#E6F3EA", ec=GREEN)
    arrow(ax, (sx_ - 20, sy_), (sx_ - 1.8 * K - 20, sy_), color=GREEN, lw=2.0, head=11, ls="--")
    T(ax, sx_ + 24, sy_ + 13, "표적 1 · 대함", size=10.5, color=GREEN, bold=True)
    T(ax, sx_ + 24, sy_ - 2, "수면 · 30 m/s · 서쪽으로", size=9, color=GREY)
    T(ax, sx_ + 24, sy_ - 16, "60초 동안 1.8 km", size=8.5, color=GREEN)
    # 거리 표시
    dx = ox - 120
    line(ax, (dx, oy), (dx, ay_), color=FAINT, lw=0.9, z=2, dashes=(4, 3))
    line(ax, (dx - 6, oy), (dx + 6, oy), color=FAINT, lw=0.9)
    line(ax, (dx - 6, ay_), (dx + 6, ay_), color=FAINT, lw=0.9)
    T(ax, dx - 8, (oy + ay_) / 2, "북쪽으로 13.3 km", size=9, color=GREY, ha="right", rotation=90, va="center")
    T(ax, 30, 350, "위에서 내려다본 그림 (대략 축척)", size=9, color=FAINT, va="top")
    # 오른쪽: 시간 눈금
    bx = 545
    box(ax, bx, 30, 175, 320, fc=SOFT, ec="none", z=0, r=8)
    T(ax, bx + 87, 330, "시뮬레이션 시간", size=11, color=NAVY, bold=True, ha="center")
    T(ax, bx + 87, 306, "60 초 · 0.1 초 간격", size=10, color=GREY, ha="center")
    y = 268
    for i, t in enumerate((0.0, 0.1, 0.2, 0.3)):
        dot(ax, (bx + 30, y), r=4.0, color=ORANGE if i else BLUE)
        T(ax, bx + 45, y, "t = %.1f s" % t, size=10, color=NAVY, mono=True)
        y -= 28
    vdots(ax, (bx + 30, y + 6))
    y -= 28
    dot(ax, (bx + 30, y), r=4.0, color=ORANGE)
    T(ax, bx + 45, y, "t = 60.0 s", size=10, color=NAVY, mono=True)
    box(ax, bx + 12, 44, 151, 46, fc=PEACH, ec="none", z=1, r=6)
    T(ax, bx + 87, 74, "위치 갱신 600 번", size=11, color=ORANGE, bold=True, ha="center")
    T(ax, bx + 87, 56, "표적마다 · 위경도로 기록", size=9, color=GREY, ha="center")
    save(fig, "fig01_overview.png")


def f02_p2_vs_p3():
    """4장. 과제 2(한 점 한 번) 와 과제 3(매 스텝 반복) 비교."""
    fig, ax, W, H = canvas(11.5, 2.9)
    box(ax, 15, 20, 540, 250, fc="#F7F9FC", ec="#DCE3EE", lw=0.8, z=0, r=10)
    T(ax, 35, 248, "과제 2  ·  한 점을 한 번 변환", size=12.5, color=BLUE, bold=True)
    box(ax, 35, 120, 120, 78, fc=SOFT, ec="none", z=1, r=6)
    T(ax, 95, 178, "측정값", size=10.5, color=NAVY, bold=True, ha="center")
    T(ax, 95, 158, "거리 R", size=9.5, color=GREY, ha="center")
    T(ax, 95, 143, "방위각 Az · 고각 El", size=9.5, color=GREY, ha="center")
    T(ax, 95, 128, "(한 번)", size=9, color=FAINT, ha="center")
    arrow(ax, (160, 159), (185, 159), color=FAINT, lw=1.4, head=9)
    x = 190
    for i, nm in enumerate(["안테나", "동체", "NED", "ECEF", "LLA"]):
        box(ax, x, 146, 52, 26, fc=SOFT, ec="#C9D6EC", lw=0.8, z=1, r=5)
        T(ax, x + 26, 159, nm, size=9, color=GREY, ha="center")
        x += 52
        if i < 4:
            tri(ax, (x + 5, 159))
            x += 10
    arrow(ax, (x + 2, 159), (x + 24, 159), color=FAINT, lw=1.4, head=9)
    dot(ax, (x + 44, 159), r=6, color=ORANGE)
    T(ax, x + 44, 132, "위경도 한 점", size=9.5, color=ORANGE, bold=True, ha="center")
    T(ax, 35, 60, "입력이 한 번 들어오고, 변환 체인을 한 번 지나, 점 하나가 나온다.", size=10, color=GREY)
    T(ax, 35, 40, "시간이라는 축이 없다.", size=10, color=GREY)
    # 오른쪽
    box(ax, 590, 20, 545, 250, fc="#FFF9F5", ec="#F1D9CA", lw=0.8, z=0, r=10)
    T(ax, 610, 248, "과제 3  ·  매 0.1 초마다, 60 초 동안 반복", size=12.5, color=ORANGE, bold=True)
    box(ax, 610, 112, 120, 92, fc=SOFT, ec="none", z=1, r=6)
    T(ax, 670, 190, "초기값", size=10.5, color=NAVY, bold=True, ha="center")
    T(ax, 670, 171, "위경도 · 고도", size=9.5, color=GREY, ha="center")
    T(ax, 670, 156, "속력 · 자세각", size=9.5, color=GREY, ha="center")
    T(ax, 670, 141, "기동 목록", size=9.5, color=GREY, ha="center")
    T(ax, 670, 124, "(처음 한 번)", size=9, color=FAINT, ha="center")
    arrow(ax, (735, 158), (762, 158), color=FAINT, lw=1.4, head=9)
    cx, cy, r = 855, 152, 62
    ax.add_patch(Circle((cx, cy), r, facecolor="white", edgecolor=ORANGE, lw=1.6, zorder=1))
    for ang in (45, 135, 225, 315):
        curve(ax, at((cx, cy), r, ang + 70), at((cx, cy), r, ang + 8), rad=-0.35, color=ORANGE, lw=1.4, head=9, z=3)
    for nm, ang in (("기동", 90), ("자세", 0), ("속도", 270), ("위치", 180)):      # 시계 방향
        p = at((cx, cy), r, ang)
        box(ax, p[0] - 22, p[1] - 11, 44, 22, fc=PEACH, ec="none", z=4, r=5)
        T(ax, p[0], p[1], nm, size=9.5, color=ORANGE, bold=True, ha="center", zorder=5)
    T(ax, cx, cy + 4, "× 600", size=12, color=NAVY, bold=True, ha="center")
    T(ax, cx, cy - 14, "0.1 s 마다", size=8.5, color=GREY, ha="center")
    arrow(ax, (cx + r + 26, cy), (cx + r + 46, cy), color=FAINT, lw=1.4, head=9)
    x0 = cx + r + 56
    for i in range(9):
        dot(ax, (x0 + i * 17, cy + 24 * math.sin(i * 0.5) - 4), r=4.2, color=ORANGE)
    T(ax, x0 + 68, cy - 44, "위경도 600 점 = 궤적", size=9.5, color=ORANGE, bold=True, ha="center")
    T(ax, 610, 60, "초기값을 한 번 넣고, 같은 계산을 600 번 돌린다.  매번 점이 하나씩 찍혀 선이 된다.", size=10, color=GREY)
    T(ax, 610, 40, "새로 들어온 것은 \"시간\" 이다.", size=10, color=GREY)
    save(fig, "fig02_p2_vs_p3.png")


def f03_four_frames():
    """6장. 이번에 쓰는 좌표계 넷을 한 장에."""
    fig, ax, W, H = canvas(8.4, 3.95)
    panels = [("LLA  위도 · 경도 · 고도", "\"지도 주소\" — 사람이 읽는 자리"),
              ("ECEF  지구 중심 xyz", "\"지구에 박힌 자\" — 계산하는 자리"),
              ("NED  북 · 동 · 아래", "\"내 발밑 나침반\" — 수평면 기준"),
              ("동체  앞 · 오른쪽 · 아래", "\"표적의 몸\" — 속도가 적힌 자리")]
    pw = 205.0
    for i, (title, sub) in enumerate(panels):
        x0 = 8 + i * (pw + 4)
        box(ax, x0, 8, pw, 379, fc="#F7F9FC", ec="#DCE3EE", lw=0.8, z=0, r=8)
        T(ax, x0 + pw / 2, 368, title, size=11, color=NAVY, bold=True, ha="center")
        T(ax, x0 + pw / 2, 346, sub, size=9, color=GREY, ha="center")
        cx, cy = x0 + pw / 2, 190
        if i == 0:
            globe(ax, (cx, cy), 78)
            p = at((cx, cy), 78, 40)
            dot(ax, p, r=4.5, color=ORANGE)
            arrow(ax, p, at((cx, cy), 108, 40), color=ORANGE, lw=1.6, head=9)
            T(ax, p[0] + 8, p[1] + 30, "고도", size=9, color=ORANGE, bold=True)
            arcdeg(ax, (cx, cy), 44, 0, 40, color=GREEN, lw=1.5)
            T(ax, cx + 50, cy + 10, "위도", size=9, color=GREEN, bold=True)
            line(ax, (cx, cy), at((cx, cy), 78, 40), color=FAINT, lw=0.8, dashes=(3, 3))
            line(ax, (cx, cy), (cx + 78, cy), color=FAINT, lw=0.8, dashes=(3, 3))
            T(ax, cx, cy - 92, "경도는 자오선에서 잰 각", size=8.5, color=GREY, ha="center")
            T(ax, cx, cy - 106, "고도는 타원체 면에서 잰 높이", size=8.5, color=GREY, ha="center")
        elif i == 1:
            globe(ax, (cx, cy), 78, grid=False)
            for k in (-0.5, 0.0, 0.5):
                ax.add_patch(Arc((cx, cy), 2 * 78 * abs(k) if k else 0.5, 2 * 78, color=BLUE, lw=0.6, zorder=3, alpha=0.6))
            dot(ax, (cx, cy), r=3.5, color=NAVY)
            arrow(ax, (cx, cy), (cx, cy + 110), color=BLUE, lw=1.9, head=10, z=6)
            arrow(ax, (cx, cy), (cx + 108, cy), color=BLUE, lw=1.9, head=10, z=6)
            arrow(ax, (cx, cy), (cx - 62, cy - 62), color=BLUE, lw=1.9, head=10, z=6)
            T(ax, cx + 10, cy + 108, "Z 북극", size=9, color=BLUE, bold=True)
            T(ax, cx + 96, cy + 12, "Y", size=9, color=BLUE, bold=True)
            T(ax, cx - 70, cy - 74, "X 그리니치", size=9, color=BLUE, bold=True, ha="center")
            T(ax, cx + 8, cy - 14, "원점 = 지구 중심", size=8.5, color=NAVY, bold=True)
            p = at((cx, cy), 78, 40)
            dot(ax, p, r=4.5, color=ORANGE)
            T(ax, cx, cy - 106, "지구와 함께 도는 직교좌표 (m)", size=8.5, color=GREY, ha="center")
        elif i == 2:
            surf = [(cx - 90, cy - 30), (cx - 30, cy + 20), (cx + 95, cy + 20), (cx + 35, cy - 30)]
            poly(ax, surf, fc="#E9F1FB", ec="#B9C8E0", lw=1.0, z=1)
            o = (cx + 2, cy - 5)
            dot(ax, o, r=4.5, color=ORANGE)
            arrow(ax, o, (o[0] - 58, o[1] + 50), color=GREEN, lw=1.9, head=10, z=6)
            arrow(ax, o, (o[0] + 84, o[1] + 8), color=GREEN, lw=1.9, head=10, z=6)
            arrow(ax, o, (o[0], o[1] - 88), color=GREEN, lw=1.9, head=10, z=6)
            T(ax, o[0] - 70, o[1] + 60, "N 북", size=9, color=GREEN, bold=True, ha="center")
            T(ax, o[0] + 90, o[1] + 16, "E 동", size=9, color=GREEN, bold=True)
            T(ax, o[0] + 12, o[1] - 86, "D 아래", size=9, color=GREEN, bold=True)
            T(ax, cx, cy + 46, "원점 = 표적이 서 있는 자리", size=8.5, color=NAVY, bold=True, ha="center")
            T(ax, cx, cy - 116, "표적이 움직이면 같이 따라간다", size=8.5, color=GREY, ha="center")
        else:
            o = (cx - 10, cy + 6)
            plane_top(ax, o, 52, heading=90)
            arrow(ax, (o[0], o[1] + 40), (o[0], o[1] + 100), color=ORANGE, lw=1.9, head=10, z=6)
            arrow(ax, (o[0] + 10, o[1]), (o[0] + 96, o[1]), color=ORANGE, lw=1.9, head=10, z=6)
            ax.add_patch(Circle(o, 7, facecolor="white", edgecolor=ORANGE, lw=1.3, zorder=7))
            ax.add_patch(Circle(o, 2, facecolor=ORANGE, edgecolor="none", zorder=8))
            T(ax, o[0] + 10, o[1] + 96, "x 앞 = 속도 방향", size=9, color=ORANGE, bold=True)
            T(ax, o[0] + 56, o[1] - 14, "y 오른쪽", size=9, color=ORANGE, bold=True)
            T(ax, o[0] + 12, o[1] - 40, "z 아래 (화면 속으로)", size=8.5, color=ORANGE, bold=True)
            T(ax, cx, cy - 106, "속력 V 는 늘 x 축 위에만 있다", size=8.5, color=GREY, ha="center")
    save(fig, "fig03_four_frames.png")


def f04_why_ecef():
    """7장. 위경도는 더할 수 없고 ECEF 는 더하면 끝."""
    fig, ax, W, H = canvas(7.4, 3.75)
    T(ax, 20, 355, "위경도 격자  (북위 32° 부근)", size=11.5, color=NAVY, bold=True)
    T(ax, 20, 336, "\"1도\" 라는 같은 눈금인데 실제 길이가 다르다", size=9.5, color=GREY)
    gx, gy = 70, 100
    cw, ch = 60, 71                                     # 94 km : 111 km 비율
    for r in range(3):
        for c in range(3):
            ax.add_patch(Rectangle((gx + c * cw, gy + r * ch), cw, ch, facecolor="#F3F6FB", edgecolor="#9DB3D6", lw=1.0, zorder=1))
    arrow(ax, (gx, gy - 14), (gx + cw, gy - 14), color=BLUE, lw=1.4, head=8, style="<|-|>")
    T(ax, gx + cw / 2, gy - 30, "경도 1° = 94 km", size=9.5, color=BLUE, bold=True, ha="center")
    arrow(ax, (gx + 3 * cw + 14, gy), (gx + 3 * cw + 14, gy + ch), color=GREEN, lw=1.4, head=8, style="<|-|>")
    T(ax, gx + 3 * cw + 24, gy + ch / 2, "위도 1°\n= 111 km", size=9.5, color=GREEN, bold=True, va="center")
    box(ax, 22, 12, 330, 36, fc=PEACH, ec="none", z=1, r=6)
    mark_no(ax, (42, 30))
    T(ax, 60, 30, "위도 + 속도 × 시간 ?   각도에 미터를 더할 수 없다", size=9.8, color=ORANGE, bold=True)
    line(ax, (385, 20), (385, 355), color="#D0D9E8", lw=1.0, dashes=(5, 4))
    # 오른쪽 : ECEF 직교좌표
    T(ax, 410, 355, "ECEF 직교좌표  (미터)", size=11.5, color=NAVY, bold=True)
    T(ax, 410, 336, "세 축이 모두 미터라 벡터를 그대로 더한다", size=9.5, color=GREY)
    o = (480, 110)
    arrow(ax, o, (o[0] + 235, o[1]), color=BLUE, lw=1.6, head=9)
    arrow(ax, o, (o[0], o[1] + 200), color=BLUE, lw=1.6, head=9)
    arrow(ax, o, (o[0] - 40, o[1] - 40), color=BLUE, lw=1.6, head=9)
    T(ax, o[0] + 240, o[1] + 10, "Y", size=9.5, color=BLUE, bold=True)
    T(ax, o[0] + 8, o[1] + 198, "Z", size=9.5, color=BLUE, bold=True)
    T(ax, o[0] - 54, o[1] - 42, "X", size=9.5, color=BLUE, bold=True)
    p = (o[0] + 60, o[1] + 60)
    q = (o[0] + 165, o[1] + 125)
    arrow(ax, o, p, color=FAINT, lw=1.2, head=8, ls="--")
    dot(ax, p, r=5, color=ORANGE)
    T(ax, p[0] - 8, p[1] + 14, "p (지금 위치)", size=9.5, color=ORANGE, bold=True, ha="right")
    arrow(ax, p, q, color=ORANGE, lw=2.2, head=11)
    T(ax, (p[0] + q[0]) / 2 + 10, (p[1] + q[1]) / 2 - 16, "v × 0.1 s", size=9.5, color=ORANGE, bold=True)
    dot(ax, q, r=5, color=ORANGE)
    T(ax, q[0] - 4, q[1] + 18, "p' (다음 위치)", size=9.5, color=ORANGE, bold=True, ha="center")
    arrow(ax, o, q, color=FAINT, lw=1.2, head=8, ls="--")
    box(ax, 400, 12, 325, 36, fc="#E6F3EA", ec="none", z=1, r=6)
    mark_ok(ax, (420, 30))
    T(ax, 438, 30, "p' = p + v × Δt    더하기 한 줄이면 끝", size=9.8, color=GREEN, bold=True)
    save(fig, "fig04_why_ecef.png")


def f05_pos_vs_vel():
    """8장. 위치는 원점이 필요하고 속도는 화살표만 있으면 된다."""
    fig, ax, W, H = canvas(11.5, 2.9)
    ang = 28
    # 왼쪽 : 위치
    box(ax, 12, 16, 548, 258, fc="#F7F9FC", ec="#DCE3EE", lw=0.8, z=0, r=10)
    T(ax, 30, 252, "위치  —  \"어디에 있나\" 는 원점이 있어야 말이 된다", size=11.5, color=BLUE, bold=True)
    oa = (70, 78)
    arrow(ax, oa, (oa[0] + 150, oa[1]), color=BLUE, lw=1.6, head=9)
    arrow(ax, oa, (oa[0], oa[1] + 130), color=BLUE, lw=1.6, head=9)
    T(ax, oa[0] - 6, oa[1] - 12, "원점 A", size=9, color=BLUE, bold=True, ha="right")
    ob = (330, 62)
    arrow(ax, ob, at(ob, 140, ang), color=GREEN, lw=1.6, head=9)
    arrow(ax, ob, at(ob, 120, ang + 90), color=GREEN, lw=1.6, head=9)
    T(ax, ob[0] + 14, ob[1] - 10, "원점 B", size=9, color=GREEN, bold=True)
    p = (250, 190)
    dot(ax, p, r=5.5, color=ORANGE)
    T(ax, p[0], p[1] + 18, "같은 점 P", size=10, color=ORANGE, bold=True, ha="center")
    arrow(ax, oa, p, color=BLUE, lw=1.3, head=8, ls="--")
    arrow(ax, ob, p, color=GREEN, lw=1.3, head=8, ls="--")
    T(ax, 140, 150, "A 에서 본 좌표", size=9, color=BLUE, ha="center", rotation=32)
    T(ax, 305, 140, "B 에서 본 좌표", size=9, color=GREEN, ha="center", rotation=-58)
    T(ax, 440, 205, "원점이 다르면 숫자가 다르고", size=9.5, color=NAVY, bold=True, ha="center")
    T(ax, 440, 187, "축이 돌아가 있으면 또 다르다", size=9.5, color=NAVY, bold=True, ha="center")
    box(ax, 372, 112, 165, 50, fc=SOFT, ec="none", z=1, r=6)
    T(ax, 454, 146, "p_new = R · p + t", size=10.5, color=NAVY, bold=True, ha="center", mono=True)
    T(ax, 454, 125, "회전  +  평행이동", size=9, color=GREY, ha="center")
    T(ax, 30, 30, "그래서 위치를 바꿀 때는 회전행렬 R 로 돌리고, 원점 차이 t 를 더한다.", size=9.5, color=GREY)
    # 오른쪽 : 속도
    box(ax, 590, 16, 548, 258, fc="#FFF9F5", ec="#F1D9CA", lw=0.8, z=0, r=10)
    T(ax, 608, 252, "속도  —  \"어느 쪽으로 얼마나 빨리\" 는 화살표 하나면 된다", size=11.5, color=ORANGE, bold=True)
    oa = (640, 62)
    arrow(ax, oa, (oa[0] + 105, oa[1]), color=BLUE, lw=1.6, head=9)
    arrow(ax, oa, (oa[0], oa[1] + 120), color=BLUE, lw=1.6, head=9)
    T(ax, oa[0] - 6, oa[1] - 12, "원점 A", size=9, color=BLUE, bold=True, ha="right")
    ob = (1050, 62)
    arrow(ax, ob, at(ob, 100, ang), color=GREEN, lw=1.6, head=9)
    arrow(ax, ob, at(ob, 92, ang + 90), color=GREEN, lw=1.6, head=9)
    T(ax, ob[0] + 14, ob[1] - 10, "원점 B", size=9, color=GREEN, bold=True)
    v = (70, 40)
    for s in ((700, 118), (770, 178), (850, 122), (930, 182)):
        arrow(ax, s, (s[0] + v[0], s[1] + v[1]), color=ORANGE, lw=2.2, head=11)
    T(ax, 830, 232, "어디에 그려도 같은 화살표 v", size=10, color=ORANGE, bold=True, ha="center")
    T(ax, 1085, 214, "원점을 옮겨도", size=9.5, color=NAVY, bold=True, ha="center")
    T(ax, 1085, 196, "화살표는 그대로", size=9.5, color=NAVY, bold=True, ha="center")
    box(ax, 762, 44, 175, 50, fc=PEACH, ec="none", z=1, r=6)
    T(ax, 849, 78, "v_new = R · v", size=10.5, color=ORANGE, bold=True, ha="center", mono=True)
    T(ax, 849, 57, "회전만  (평행이동 없음)", size=9, color=GREY, ha="center")
    T(ax, 608, 30, "축이 돌아간 만큼만 돌려 주면 된다.  t 를 더하는 순간 속도가 아니게 된다.", size=9.5, color=GREY)
    save(fig, "fig05_pos_vs_vel.png")


def f06_vel_chain():
    """9장. 동체 속도 → NED → ECEF. 회전 두 번."""
    fig, ax, W, H = canvas(11.5, 2.2)
    bw, bh, y0 = 280, 150, 40
    xs = [22, 445, 868]
    heads = [("① 동체 좌표의 속도", "앞으로만 간다", "v_body = [ V, 0, 0 ]", "대공 표적 : ( 200,  0,  0 )"),
             ("② NED 좌표의 속도", "북·동·아래 성분으로", "v_ned = C_ned←body · v_body", "( -200,  0,  0 )   남쪽으로 200"),
             ("③ ECEF 좌표의 속도", "지구 중심 축 성분으로", "v_ecef = C_ecef←ned · v_ned", "( -62.5,  86.0,  -169.4 )  크기 200.0")]
    cols = [ORANGE, GREEN, BLUE]
    for (x, (h1, h2, f, ex), c) in zip(xs, heads, cols):
        box(ax, x, y0, bw, bh, fc="#F7F9FC", ec="#DCE3EE", lw=0.8, z=0, r=8)
        T(ax, x + 14, y0 + bh - 20, h1, size=11.5, color=c, bold=True)
        T(ax, x + 14, y0 + bh - 40, h2, size=9.5, color=GREY)
        box(ax, x + 14, y0 + 48, bw - 28, 34, fc="white", ec="#D0D9E8", lw=0.8, z=1, r=5)
        T(ax, x + bw / 2, y0 + 65, f, size=9.5, color=NAVY, bold=True, ha="center", mono=True)
        T(ax, x + 14, y0 + 26, ex, size=9.2, color=c, bold=True)
    for i in range(2):
        xa = xs[i] + bw + 8
        xb = xs[i + 1] - 8
        xm = (xa + xb) / 2
        arrow(ax, (xa, y0 + 80), (xb, y0 + 80), color=NAVY, lw=2.0, head=12)
        if i == 0:
            T(ax, xm, y0 + 120, "자세각으로 회전", size=10, color=NAVY, bold=True, ha="center")
            T(ax, xm, y0 + 102, "roll · pitch · yaw", size=9, color=GREY, ha="center")
        else:
            T(ax, xm, y0 + 120, "위도·경도로 회전", size=10, color=NAVY, bold=True, ha="center")
            T(ax, xm, y0 + 102, "표적 자신의 자리 기준", size=9, color=GREY, ha="center")
        T(ax, xm, y0 + 54, "회전만", size=9, color=ORANGE, bold=True, ha="center")
        T(ax, xm + 6, y0 + 36, "위치 더하기", size=9, color=ORANGE, ha="center")
        mark_no(ax, (xm - 42, y0 + 36), s=4.5, lw=1.8)
    T(ax, 22, 16, "속도는 화살표라서 두 번 다 회전만 한다.  ③ 에서 위도·경도는 플랫폼이 아니라 표적이 지금 있는 자리의 값이다.", size=9.5, color=GREY)
    save(fig, "fig06_vel_chain.png")


def f07_euler():
    """10장. 0.1 초마다 v × Δt 만큼 옮긴다."""
    fig, ax, W, H = canvas(7.4, 3.2)
    T(ax, 40, 300, "자동차로 치면 : 시속 720 km 로 0.1 초 가면 20 m.  그걸 600 번 더하면 12 km.", size=10.5, color=NAVY)
    T(ax, 40, 278, "속도의 방향은 매 스텝 다시 계산하므로 (앞 장의 회전 두 번), 곡선도 이 방식으로 그려진다.", size=9.5, color=GREY)
    y = 150
    line(ax, (40, y), (640, y), color="#D0D9E8", lw=1.2, z=1)
    xs = [70 + i * 62 for i in range(8)]
    plane_top(ax, (xs[0] - 28, y + 58), 13, heading=0)
    for i, x in enumerate(xs):
        dot(ax, (x, y), r=5, color=BLUE if i == 0 else ORANGE)
        T(ax, x, y - 22, "%.1f s" % (i * 0.1), size=9, color=GREY, ha="center")
    T(ax, 640, y - 22, "...", size=11, color=GREY, ha="center")
    dot(ax, (690, y), r=5, color=ORANGE)
    T(ax, 690, y - 22, "60.0 s", size=9, color=GREY, ha="center")
    T(ax, 690, y - 40, "600 번째", size=8.5, color=FAINT, ha="center")
    arrow(ax, (xs[0], y + 24), (xs[1], y + 24), color=ORANGE, lw=1.8, head=10)
    T(ax, (xs[0] + xs[1]) / 2, y + 40, "한 스텝", size=9, color=ORANGE, bold=True, ha="center")
    T(ax, xs[1] + 10, y + 62, "속도 × 시간 = 200 m/s × 0.1 s = 20 m", size=10.5, color=ORANGE, bold=True)
    T(ax, xs[1] + 10, y + 44, "다음 스텝도 같은 계산, 방향만 다시 정한다", size=9, color=GREY)
    box(ax, 40, 26, 660, 60, fc=SOFT, ec="none", z=0, r=8)
    T(ax, 370, 68, "새 위치  =  지금 위치  +  속도 × 0.1 s", size=13, color=NAVY, bold=True, ha="center")
    T(ax, 370, 42, "p(t + Δt) = p(t) + v(t) · Δt      한 스텝 안에서는 속도가 일정하므로 이 한 줄로 충분하다", size=9.5, color=GREY, ha="center")
    save(fig, "fig07_euler.png")


def f08_curvature():
    """11장. 처음 속도로 직진하면 떠오른다. 매 스텝 수평면을 다시 잡으면 고도가 유지된다."""
    fig, ax, W, H = canvas(7.4, 3.75)
    C = (370.0, -560.0)
    R = 700.0
    alt = 50.0
    ax.add_patch(Wedge(C, R, 50, 130, facecolor="#E6EEF9", edgecolor=BLUE, lw=1.4, zorder=1))
    T(ax, 370, 70, "지구 (반지름 6,371 km · 크게 과장한 그림)", size=9, color=BLUE, ha="center")
    a0, a1 = 112.0, 80.0
    S = at(C, R + alt, a0)
    angs = [a0 - k * (a0 - a1) / 60.0 for k in range(61)]
    pts = [at(C, R + alt, a) for a in angs]
    ax.plot([p[0] for p in pts], [p[1] for p in pts], color=GREEN, lw=2.4, zorder=5)
    E_b = pts[-1]
    ta = math.radians(a0 - 90.0)
    L = (R + alt) * math.radians(a0 - a1)
    E_a = (S[0] + L * math.cos(ta), S[1] + L * math.sin(ta))
    line(ax, S, E_a, color=ORANGE, lw=2.4, ls="--", z=5)
    plane_side(ax, (S[0] - 34, S[1] - 6), 18, pitch=22)
    dot(ax, S, r=4.5, color=NAVY)
    T(ax, 26, S[1] + 40, "출발 · 고도 300 m", size=9.5, color=NAVY, bold=True)
    dot(ax, E_a, r=4.5, color=ORANGE)
    dot(ax, E_b, r=4.5, color=GREEN)
    line(ax, E_b, E_a, color=FAINT, lw=1.0, dashes=(3, 3))
    T(ax, E_a[0] + 14, E_a[1] + 8, "A  처음 속도 그대로 직진", size=10, color=ORANGE, bold=True)
    T(ax, E_a[0] + 14, E_a[1] - 10, "60 초 뒤 고도 311.3 m  (11 m 떠오름)", size=9.5, color=ORANGE)
    T(ax, E_b[0] + 14, E_b[1] + 4, "B  매 스텝 수평면을 다시 잡음", size=10, color=GREEN, bold=True)
    T(ax, E_b[0] + 14, E_b[1] - 14, "60 초 뒤 고도 300.0 m", size=9.5, color=GREEN)
    for a in (a0, a0 - 11, a0 - 22):
        p = at(C, R + alt, a)
        t = math.radians(a - 90.0)
        line(ax, (p[0] - 30 * math.cos(t), p[1] - 30 * math.sin(t)), (p[0] + 30 * math.cos(t), p[1] + 30 * math.sin(t)),
             color=GREEN, lw=1.0, z=4, dashes=(2, 2))
    T(ax, 150, 122, "지금 자리의 수평면 (매 스텝 다시 계산)", size=8.5, color=GREEN)
    box(ax, 22, 316, 700, 52, fc=PEACH, ec="none", z=2, r=7)
    T(ax, 372, 352, "왜 떠오르나 :  지구가 둥글어서 접선 방향으로 12 km 가면 땅이 11 m 아래로 내려간다  ( d² / 2R )", size=9.8, color=ORANGE, bold=True, ha="center")
    T(ax, 372, 331, "그래서 매 스텝 표적의 현재 위경도로 NED 수평면을 다시 잡고, 그 위에서 속도 방향을 다시 만든다", size=9.5, color=NAVY, ha="center")
    save(fig, "fig08_curvature.png")


def f09_step_loop():
    """12장. 표적 하나의 한 스텝, 다섯 단계."""
    fig, ax, W, H = canvas(11.5, 3.0)
    steps = [("기동 판단", "지금 시각 t 가", "어느 기동 구간인가"),
             ("자세 갱신", "각속도 ω × 0.1 s", "만큼 자세각을 돌림"),
             ("속도 변환", "동체 → NED → ECEF", "회전 두 번"),
             ("위치 적분", "p ← p + v × 0.1 s", "ECEF 에서 더함"),
             ("LLA 변환", "위경도로 기록하고", "다음 스텝 수평면 기준")]
    cols = [ORANGE, ORANGE, GREEN, BLUE, BLUE]
    bw, bh, y0, gap = 196, 132, 92, 40
    x = 20
    for i, ((t1, t2, t3), c) in enumerate(zip(steps, cols)):
        box(ax, x, y0, bw, bh, fc="#F7F9FC", ec="#DCE3EE", lw=0.9, z=1, r=9)
        numcircle(ax, (x + 24, y0 + bh - 26), i + 1, r=11, fc=c)
        T(ax, x + 44, y0 + bh - 26, t1, size=12, color=c, bold=True)
        T(ax, x + bw / 2, y0 + 56, t2, size=10, color=NAVY, ha="center", mono=(i == 3))
        T(ax, x + bw / 2, y0 + 34, t3, size=9.5, color=GREY, ha="center")
        if i < 4:
            arrow(ax, (x + bw + 4, y0 + bh / 2), (x + bw + gap - 4, y0 + bh / 2), color=NAVY, lw=1.8, head=11)
        x += bw + gap
    xe = 20 + 5 * bw + 4 * gap
    p0 = (xe - bw / 2, y0 - 6)
    p1 = (20 + bw / 2, y0 - 6)
    line(ax, (p0[0], p0[1]), (p0[0], p0[1] - 30), color=FAINT, lw=1.6, z=3)
    line(ax, (p0[0], p0[1] - 30), (p1[0], p0[1] - 30), color=FAINT, lw=1.6, z=3)
    arrow(ax, (p1[0], p0[1] - 30), (p1[0], y0 - 4), color=FAINT, lw=1.6, head=11, z=3)
    T(ax, (p0[0] + p1[0]) / 2, y0 - 50, "t ← t + 0.1 s  ·  60 초까지 반복  ·  표적마다 같은 순서", size=10, color=NAVY, bold=True, ha="center")
    T(ax, 20, 272, "표적 하나의 한 스텝 (0.1 초)", size=12, color=NAVY, bold=True)
    T(ax, 20, 250, "왼쪽부터 순서대로.  ① ② 는 기동이 없으면 건너뛰고,  ③ ④ ⑤ 는 매 스텝 반드시 한다.", size=9.5, color=GREY)
    save(fig, "fig09_step_loop.png")


def f10_gload():
    """14장. 커브를 도는 자동차와 g."""
    fig, ax, W, H = canvas(7.4, 3.6)
    C = (150, 50)
    Rr = 200
    ax.add_patch(Wedge(C, Rr + 34, 20, 100, width=68, facecolor="#EDF1F7", edgecolor="#C9D2E0", lw=1.0, zorder=1))
    arcdeg(ax, C, Rr, 20, 100, color="#C9D2E0", lw=1.0, ls="--")
    a_car = 62
    P = at(C, Rr, a_car)
    tdir = math.radians(a_car + 90)
    ndir = math.radians(a_car + 180)
    ct, st = math.cos(tdir), math.sin(tdir)
    cn, sn = math.cos(ndir), math.sin(ndir)

    def Q(u, v):
        return (P[0] + u * ct + v * cn, P[1] + u * st + v * sn)
    poly(ax, [Q(-20, -11), Q(20, -11), Q(24, -4), Q(24, 4), Q(20, 11), Q(-20, 11)], fc=PEACH, ec=ORANGE, lw=1.3, z=4)
    poly(ax, [Q(-6, -8), Q(10, -8), Q(10, 8), Q(-6, 8)], fc="#F6D9C8", ec=ORANGE, lw=0.9, z=5)
    arrow(ax, Q(26, 0), Q(80, 0), color=NAVY, lw=2.0, head=11, z=6)
    T(ax, Q(62, -20)[0], Q(62, -20)[1], "속도 V", size=10, color=NAVY, bold=True, ha="center")
    arrow(ax, Q(0, 14), Q(0, 84), color=ORANGE, lw=2.2, head=12, z=6)
    T(ax, Q(0, 60)[0] - 14, Q(0, 60)[1] + 4, "안쪽으로 당기는 가속도", size=9.5, color=ORANGE, bold=True, ha="right")
    T(ax, Q(0, 60)[0] - 14, Q(0, 60)[1] - 12, "a = n × g", size=10.5, color=ORANGE, bold=True, ha="right", mono=True)
    T(ax, C[0], C[1] - 14, "회전 중심", size=9, color=GREY, ha="center")
    dot(ax, C, r=3.5, color=GREY)
    line(ax, C, P, color=FAINT, lw=1.0, dashes=(4, 3))
    T(ax, (C[0] + P[0]) / 2 + 14, (C[1] + P[1]) / 2 - 10, "반경 R", size=9.5, color=GREY)
    T(ax, 20, 340, "커브를 도는 자동차", size=11.5, color=NAVY, bold=True)
    T(ax, 20, 320, "속력은 그대로, 방향만 계속 바뀐다", size=9.5, color=GREY)
    x0 = 470
    T(ax, x0, 340, "하중배수 n  (\"몇 g\")", size=11.5, color=NAVY, bold=True)
    T(ax, x0, 320, "몸이 느끼는 힘이 평소 몸무게의 몇 배인가", size=9.5, color=GREY)
    line(ax, (x0 + 20, 40), (x0 + 20, 290), color=NAVY, lw=2.0)
    marks = [(0, "0 g", "무중력", FAINT), (1, "1 g", "서 있을 때 · 승용차 커브는 0.3 g", NAVY),
             (3, "3 g", "롤러코스터 바닥 · 전투기 급선회", ORANGE), (5, "5 g", "에어쇼 기동 · 시야가 흐려진다", ORANGE),
             (9, "9 g", "전투기 한계 · 대G복이 필요", "#8E3A12")]
    for n, lab, desc, col in marks:
        y = 40 + n * 27.5
        line(ax, (x0 + 12, y), (x0 + 28, y), color=NAVY, lw=1.6)
        T(ax, x0 + 34, y, lab, size=10.5, color=col, bold=True)
        T(ax, x0 + 74, y, desc, size=8.8, color=GREY)
    box(ax, x0 - 10, 6, 270, 26, fc=PEACH, ec="none", z=1, r=5)
    T(ax, x0 + 125, 19, "과제의 Gravity Value 가 바로 이 n 이다", size=9.5, color=ORANGE, bold=True, ha="center")
    save(fig, "fig10_gload.png")


def f11_circle():
    """15장. 원운동 : 각속도와 반경."""
    fig, ax, W, H = canvas(6.0, 3.6)
    C = (200, 170)
    R = 118
    ax.add_patch(Circle(C, R, facecolor="none", edgecolor="#C9D2E0", lw=1.4, ls="--", zorder=1))
    dot(ax, C, r=3.5, color=GREY)
    T(ax, C[0] - 8, C[1] - 6, "중심", size=9, color=GREY, ha="right")
    a = 40
    P = at(C, R, a)
    plane_top(ax, P, 16, heading=a + 90)
    t = math.radians(a + 90)
    arrow(ax, (P[0] + 18 * math.cos(t), P[1] + 18 * math.sin(t)), (P[0] + 88 * math.cos(t), P[1] + 88 * math.sin(t)), color=NAVY, lw=2.0, head=11, z=6)
    T(ax, P[0] + 60 * math.cos(t) + 18, P[1] + 60 * math.sin(t) + 12, "V", size=11, color=NAVY, bold=True)
    n = math.radians(a + 180)
    arrow(ax, (P[0] + 14 * math.cos(n), P[1] + 14 * math.sin(n)), (P[0] + 70 * math.cos(n), P[1] + 70 * math.sin(n)), color=ORANGE, lw=2.2, head=12, z=6)
    T(ax, P[0] + 40 * math.cos(n) + 14, P[1] + 40 * math.sin(n) - 6, "a = n·g", size=10.5, color=ORANGE, bold=True)
    # 반지름은 다른 방향으로 따로 표시
    Pr = at(C, R, 205)
    line(ax, C, Pr, color=FAINT, lw=1.0, dashes=(4, 3))
    T(ax, (C[0] + Pr[0]) / 2 - 4, (C[1] + Pr[1]) / 2 + 12, "R", size=11, color=GREY, bold=True, ha="right")
    arcdeg(ax, C, 36, 0, a, color=GREEN, lw=1.6)
    line(ax, C, (C[0] + 52, C[1]), color=FAINT, lw=0.8, dashes=(3, 3))
    T(ax, C[0] + 44, C[1] + 16, "ω", size=11, color=GREEN, bold=True)
    curve(ax, at(C, R + 22, a + 28), at(C, R + 22, a + 62), rad=0.22, color=GREEN, lw=1.5, head=10)
    T(ax, C[0], C[1] - R - 24, "1 초에 ω 만큼 돌아간다", size=9.5, color=GREEN, ha="center")
    x0 = 370
    box(ax, x0, 40, 215, 275, fc=SOFT, ec="none", z=0, r=9)
    T(ax, x0 + 16, 292, "등속 원운동 공식", size=11, color=NAVY, bold=True)
    rows = [("구심가속도", "a = V² / R = n · g"), ("선회 반경", "R = V² / (n · g)"), ("각속도", "ω = V / R = n · g / V")]
    y = 250
    for k, (nm, f) in enumerate(rows):
        T(ax, x0 + 16, y, nm, size=9.5, color=GREY)
        T(ax, x0 + 16, y - 22, f, size=11.5, color=(ORANGE if k else NAVY), bold=True, mono=True)
        y -= 62
    T(ax, x0 + 16, 74, "g = 9.80665 m/s²", size=9.5, color=GREY, mono=True)
    T(ax, x0 + 16, 56, "V 는 변하지 않는다", size=9.5, color=GREY)
    save(fig, "fig11_circle.png")


def f12_axes():
    """16장. Roll · Yaw · Pitch 세 축."""
    fig, ax, W, H = canvas(8.4, 3.4)
    pw = 272
    items = [("Roll  (x 축 · 앞뒤 축)", "뒤에서 본 그림 · 날개가 기운다", "속도 방향은 그대로", 1),
             ("Pitch  (y 축 · 날개 축)", "옆에서 본 그림 · 기수가 들린다", "상승 · 강하", 3),
             ("Yaw  (z 축 · 위아래 축)", "위에서 본 그림 · 기수가 돌아간다", "좌우 선회", 2)]
    for i, (t1, t2, t3, tt) in enumerate(items):
        x0 = 8 + i * (pw + 6)
        box(ax, x0, 8, pw, 324, fc="#F7F9FC", ec="#DCE3EE", lw=0.8, z=0, r=8)
        T(ax, x0 + pw / 2, 312, t1, size=11, color=NAVY, bold=True, ha="center")
        T(ax, x0 + pw / 2, 292, t2, size=9, color=GREY, ha="center")
        c = (x0 + pw / 2, 168)
        if i == 0:
            plane_rear(ax, c, 62, roll=18)
            arcdeg(ax, c, 84, 200, 340, color=ORANGE, lw=1.8)
            arrow(ax, at(c, 84, 338), at(c, 84, 343), color=ORANGE, lw=1.8, head=11)
            line(ax, (c[0] - 96, c[1]), (c[0] + 96, c[1]), color=FAINT, lw=0.9, dashes=(4, 3))
            T(ax, c[0], c[1] - 62, "roll 각", size=9.5, color=ORANGE, bold=True, ha="center")
            ax.add_patch(Circle(c, 6, facecolor="white", edgecolor=NAVY, lw=1.2, zorder=9))
            ax.add_patch(Circle(c, 2, facecolor=NAVY, edgecolor="none", zorder=10))
            T(ax, c[0] + 14, c[1] + 62, "x 축 (화면 밖으로)", size=8.5, color=NAVY)
        elif i == 1:
            plane_side(ax, c, 62, pitch=18)
            line(ax, (c[0] - 96, c[1]), (c[0] + 96, c[1]), color=FAINT, lw=0.9, dashes=(4, 3))
            arcdeg(ax, c, 84, -8, 40, color=ORANGE, lw=1.8)
            arrow(ax, at(c, 84, 38), at(c, 84, 43), color=ORANGE, lw=1.8, head=11)
            T(ax, c[0] + 96, c[1] + 52, "pitch 각", size=9.5, color=ORANGE, bold=True, ha="center")
            ax.add_patch(Circle(c, 6, facecolor="white", edgecolor=NAVY, lw=1.2, zorder=9))
            T(ax, c[0], c[1] - 3, "×", size=9, color=NAVY, bold=True, ha="center", va="center", zorder=10)
            T(ax, c[0] - 96, c[1] - 56, "y 축 (화면 속으로)", size=8.5, color=NAVY)
        else:
            plane_top(ax, c, 62, heading=110)
            line(ax, (c[0], c[1] - 96), (c[0], c[1] + 96), color=FAINT, lw=0.9, dashes=(4, 3))
            arcdeg(ax, c, 84, 50, 116, color=ORANGE, lw=1.8)
            arrow(ax, at(c, 84, 114), at(c, 84, 118), color=ORANGE, lw=1.8, head=11)
            T(ax, c[0] - 66, c[1] + 98, "yaw 각", size=9.5, color=ORANGE, bold=True, ha="center")
            ax.add_patch(Circle(c, 6, facecolor="white", edgecolor=NAVY, lw=1.2, zorder=9))
            T(ax, c[0], c[1] - 3, "×", size=9, color=NAVY, bold=True, ha="center", va="center", zorder=10)
            T(ax, c[0] + 14, c[1] - 92, "z 축 (화면 속으로)", size=8.5, color=NAVY)
        box(ax, x0 + 16, 22, pw - 32, 30, fc=PEACH if tt != 1 else SOFT, ec="none", z=1, r=6)
        T(ax, x0 + pw / 2, 37, t3, size=10.5, color=ORANGE if tt != 1 else NAVY, bold=True, ha="center")
    save(fig, "fig12_axes.png")


def f13_timeline():
    """17장. 기동 시간표."""
    fig, ax, W, H = canvas(11.5, 2.6)
    x0, x1 = 120, 1110

    def X(t):
        return x0 + (x1 - x0) * t / 60.0
    yA, yB = 158, 88
    for y, nm, col in ((yA, "표적 2 · 대공", ORANGE), (yB, "표적 1 · 대함", GREEN)):
        line(ax, (x0, y), (x1, y), color="#D0D9E8", lw=1.4, z=1)
        T(ax, x0 - 12, y, nm, size=10, color=col, bold=True, ha="right")
    for t in range(0, 61, 10):
        line(ax, (X(t), yB - 30), (X(t), yA + 28), color="#E3E9F3", lw=0.8, z=0)
        T(ax, X(t), yB - 42, "%d s" % t, size=9, color=GREY, ha="center")
    blocks = [(yA, 10, 20, "3 g · Yaw", ORANGE, PEACH), (yA, 35, 40, "2 g · Pitch", BLUE, "#E3EBF8"),
              (yB, 20, 50, "0.1 g · Yaw", GREEN, "#E6F3EA")]
    for y, s, e, lab, ec, fc in blocks:
        box(ax, X(s), y - 16, X(e) - X(s), 32, fc=fc, ec=ec, lw=1.2, z=2, r=5)
        T(ax, (X(s) + X(e)) / 2, y, lab, size=9.5, color=ec, bold=True, ha="center", zorder=3)
    T(ax, (X(0) + X(10)) / 2, yA + 20, "직진", size=8.5, color=FAINT, ha="center")
    T(ax, (X(20) + X(35)) / 2, yA + 20, "직진 (선회 뒤 자세 유지)", size=8.5, color=FAINT, ha="center")
    T(ax, (X(40) + X(60)) / 2, yA + 20, "직진", size=8.5, color=FAINT, ha="center")
    tn = 15
    line(ax, (X(tn), yB - 34), (X(tn), yA + 36), color=NAVY, lw=1.6, z=4, dashes=(5, 3))
    box(ax, X(tn) - 46, yA + 36, 92, 22, fc=NAVY, ec="none", z=5, r=5)
    T(ax, X(tn), yA + 47, "지금 t = 15 s", size=9, color="white", bold=True, ha="center", zorder=6)
    T(ax, X(tn) + 54, yA + 47, "→  표적 2 는 3 g Yaw 구간 안,  표적 1 은 아직 직진", size=9.5, color=NAVY, bold=True)
    T(ax, 20, 242, "기동 하나 = { Gravity Value,  TurnType,  StartTime,  EndTime }     표적마다 최대 30 개", size=10.5, color=NAVY, bold=True)
    T(ax, 20, 20, "StartTime ≤ t < EndTime 인 기동이 있으면 그 기동을 적용하고, 없으면 TurnType 0 (직진) 으로 본다.", size=9.5, color=GREY)
    save(fig, "fig13_timeline.png")


def f14_target_card():
    """19장. 표적 구조체 = 표적 카드 한 장."""
    fig, ax, W, H = canvas(6.4, 4.3)
    box(ax, 12, 12, 616, 406, fc="white", ec="#B9C8E0", lw=1.4, z=0, r=12)
    box(ax, 12, 372, 616, 46, fc="#13233F", ec="none", z=1, r=12)
    ax.add_patch(Rectangle((12, 372), 616, 20, facecolor="#13233F", edgecolor="none", zorder=1))
    T(ax, 34, 395, "표적 카드  #2", size=13, color="white", bold=True)
    T(ax, 606, 395, "ST_Target", size=11, color="#C9D6EC", bold=True, ha="right", mono=True)
    lx, rx = 30, 346
    box(ax, lx, 34, 270, 322, fc=SOFT, ec="none", z=1, r=8)
    T(ax, lx + 14, 338, "입력  (사람이 적는 칸)", size=11, color=BLUE, bold=True)
    rows = [("초기 위치", "32.12°N  126.0°E  300 m", "위경도 · 고도로"),
            ("동체 속력", "200 m/s", "앞 방향 하나로"),
            ("초기 자세", "roll 0°  pitch 0°  yaw 180°", ""),
            ("기동 목록  (최대 30 개)", "", "")]
    y = 306
    for nm, v, note in rows:
        T(ax, lx + 14, y, nm, size=9.5, color=GREY)
        if v:
            T(ax, lx + 14, y - 18, v, size=10, color=NAVY, bold=True)
        if note:
            T(ax, lx + 256, y, note, size=8.5, color=FAINT, ha="right")
        y -= 50
    ty = 116
    hdr = ["g", "Type", "Start", "End"]
    cw = [44, 60, 66, 66]
    x = lx + 16
    for h, w in zip(hdr, cw):
        box(ax, x, ty, w - 3, 20, fc="#2B57A6", ec="none", z=2, r=3)
        T(ax, x + (w - 3) / 2, ty + 10, h, size=8.5, color="white", bold=True, ha="center", zorder=3)
        x += w
    data = [["3", "Yaw", "10", "20"], ["2", "Pitch", "35", "40"], ["...", "", "", ""]]
    for r, row in enumerate(data):
        x = lx + 16
        yy = ty - 22 * (r + 1)
        for v, w in zip(row, cw):
            box(ax, x, yy, w - 3, 20, fc="white", ec="#D0D9E8", lw=0.6, z=2, r=3)
            T(ax, x + (w - 3) / 2, yy + 10, v, size=8.5, color=NAVY, ha="center", zorder=3)
            x += w
    box(ax, rx, 34, 270, 322, fc=PEACH, ec="none", z=1, r=8)
    T(ax, rx + 14, 338, "상태  (프로그램이 갱신)", size=11, color=ORANGE, bold=True)
    rows = [("ECEF 위치", "x, y, z  [m]"), ("ECEF 속도", "vx, vy, vz  [m/s]"), ("현재 자세", "roll, pitch, yaw"), ("현재 위경도", "출력용 · 매 스텝 변환")]
    y = 306
    for nm, v in rows:
        T(ax, rx + 14, y, nm, size=9.5, color=GREY)
        T(ax, rx + 14, y - 18, v, size=10, color=NAVY, bold=True)
        y -= 50
    arrow(ax, (lx + 272, 250), (rx - 2, 250), color=NAVY, lw=1.6, head=10, z=5)
    T(ax, (lx + 270 + rx) / 2, 264, "초기화", size=8.5, color=NAVY, bold=True, ha="center")
    T(ax, (lx + 270 + rx) / 2, 236, "한 번", size=8, color=GREY, ha="center")
    c = (rx + 46, 84)
    arcdeg(ax, c, 22, 30, 330, color=ORANGE, lw=1.8)
    arrow(ax, at(c, 22, 32), at(c, 22, 26), color=ORANGE, lw=1.8, head=10)
    T(ax, c[0], c[1] - 2, "0.1 s", size=8, color=ORANGE, bold=True, ha="center")
    T(ax, c[0] + 34, c[1] + 6, "매 스텝 갱신 · 600 번", size=9.5, color=ORANGE, bold=True)
    T(ax, c[0] + 34, c[1] - 10, "입력 칸은 건드리지 않는다", size=8.5, color=GREY)
    save(fig, "fig14_target_card.png")


def f15_pipeline():
    """20장. 시나리오 → 초기화 → 루프 → CSV → 그림."""
    fig, ax, W, H = canvas(11.5, 2.8)
    items = [("시나리오", "플랫폼 1 + 표적 ≤ 10", "카드 묶음", BLUE),
             ("초기화", "위경도 → ECEF", "자세 · 속력 복사", BLUE),
             ("루프 600 회", "표적마다 5 단계", "0.1 s × 600 = 60 s", ORANGE),
             ("CSV 기록", "t · id · 위도 · 경도 · 고도", "+ 플랫폼 기준 거리 · 고각", GREEN),
             ("그림 3 장", "평면 궤적 · 고도 · 거리", "matplotlib", GREEN)]
    bw, bh, y0, gap = 190, 120, 84, 42
    x = 20
    for i, (t1, t2, t3, c) in enumerate(items):
        box(ax, x, y0, bw, bh, fc="#F7F9FC" if i != 2 else "#FFF9F5", ec="#DCE3EE" if i != 2 else "#F1D9CA", lw=0.9, z=1, r=9)
        T(ax, x + bw / 2, y0 + bh - 24, t1, size=12, color=c, bold=True, ha="center")
        T(ax, x + bw / 2, y0 + 52, t2, size=9.8, color=NAVY, ha="center")
        T(ax, x + bw / 2, y0 + 30, t3, size=9, color=GREY, ha="center")
        if i < 4:
            arrow(ax, (x + bw + 4, y0 + bh / 2), (x + bw + gap - 4, y0 + bh / 2), color=NAVY, lw=1.8, head=11)
        x += bw + gap
    lc = (20 + 2 * (bw + gap) + bw - 26, y0 + bh - 22)
    arcdeg(ax, lc, 11, 40, 320, color=ORANGE, lw=1.5)
    arrow(ax, at(lc, 11, 42), at(lc, 11, 36), color=ORANGE, lw=1.5, head=8)
    T(ax, 20, 252, "프로그램 전체 흐름", size=12, color=NAVY, bold=True)
    T(ax, 20, 230, "플랫폼도 속도 0 인 표적 카드로 두면, 갱신 함수 하나로 전부 처리된다.", size=9.5, color=GREY)
    T(ax, 20, 40, "루프 안 5 단계 :  기동 판단 → 자세 갱신 → 속도 변환 → 위치 적분 → LLA 변환   (12 장)", size=9.5, color=GREY)
    save(fig, "fig15_pipeline.png")


# 지도 그림 공통 (21 · 22 장)
KM_LAT = 111.0          # 위도 1도 [km]
KM_LON = 94.4           # 경도 1도 [km] (32°N)


def map_canvas(title):
    fig, ax, W, H = canvas(5.6, 4.3)
    mx0, mx1, my0, my1 = 62, 542, 52, 402
    lat_lo, lat_hi = 31.985, 32.150
    kmy = (my1 - my0) / ((lat_hi - lat_lo) * KM_LAT)
    lon_span_km = (mx1 - mx0) / kmy
    lon_c = 125.99
    lon_lo = lon_c - lon_span_km / KM_LON / 2
    lon_hi = lon_c + lon_span_km / KM_LON / 2

    def X(lon):
        return mx0 + (lon - lon_lo) / (lon_hi - lon_lo) * (mx1 - mx0)

    def Y(lat):
        return my0 + (lat - lat_lo) / (lat_hi - lat_lo) * (my1 - my0)
    ax.add_patch(Rectangle((mx0, my0), mx1 - mx0, my1 - my0, facecolor="#F5F8FC", edgecolor="#B9C8E0", lw=1.0, zorder=0))
    lat = 32.00
    while lat < lat_hi:
        if lat > lat_lo:
            line(ax, (mx0, Y(lat)), (mx1, Y(lat)), color="#E0E7F1", lw=0.8, z=0)
            T(ax, mx0 - 6, Y(lat), "%.2f°" % lat, size=8, color=GREY, ha="right")
        lat += 0.05
    lon = 125.85
    while lon < lon_hi:
        if lon > lon_lo:
            line(ax, (X(lon), my0), (X(lon), my1), color="#E0E7F1", lw=0.8, z=0)
            T(ax, X(lon), my0 - 12, "%.2f°" % lon, size=8, color=GREY, ha="center")
        lon += 0.05
    T(ax, mx0, my1 + 14, title, size=10.5, color=NAVY, bold=True)
    T(ax, mx1, my1 + 14, "북위 · 동경 (도)", size=8.5, color=FAINT, ha="right")
    sx = mx0 + 20
    line(ax, (sx, my0 + 14), (sx + 5 * kmy, my0 + 14), color=NAVY, lw=2.0, z=3)
    T(ax, sx + 2.5 * kmy, my0 + 26, "5 km", size=8.5, color=NAVY, ha="center")
    return fig, ax, X, Y, kmy


def f16_scenario_map():
    fig, ax, X, Y, kmy = map_canvas("시뮬레이션 조건 · 시작 위치")
    ship_top(ax, (X(126.0), Y(32.0)), 30, heading=90, z=5)
    T(ax, X(126.0) + 20, Y(32.0) + 4, "플랫폼", size=10, color=BLUE, bold=True)
    T(ax, X(126.0) + 20, Y(32.0) - 11, "32.0°N 126.0°E · 0 m · 정지", size=8.5, color=GREY)
    plane_top(ax, (X(126.0), Y(32.12)), 12, heading=270)
    T(ax, X(126.0) - 18, Y(32.12) + 2, "표적 2 · 대공", size=10, color=ORANGE, bold=True, ha="right")
    T(ax, X(126.0) - 18, Y(32.12) - 13, "32.12°N 126.0°E · 고도 300 m", size=8.5, color=GREY, ha="right")
    T(ax, X(126.0) - 18, Y(32.12) - 26, "200 m/s · yaw 180° (남쪽)", size=8.5, color=GREY, ha="right")
    ship_top(ax, (X(126.03), Y(32.125)), 24, heading=180, z=5, fc="#E6F3EA", ec=GREEN)
    T(ax, X(126.03) + 18, Y(32.125) + 12, "표적 1 · 대함", size=10, color=GREEN, bold=True)
    T(ax, X(126.03) + 18, Y(32.125) - 3, "32.125°N 126.03°E · 0 m", size=8.5, color=GREY)
    T(ax, X(126.03) + 18, Y(32.125) - 16, "30 m/s · yaw 270° (서쪽)", size=8.5, color=GREY)
    line(ax, (X(126.0), Y(32.0) + 18), (X(126.0), Y(32.12) - 14), color=FAINT, lw=1.0, dashes=(4, 3), z=2)
    T(ax, X(126.0) - 8, Y(32.06), "13.3 km", size=9, color=ORANGE, bold=True, ha="right")
    line(ax, (X(126.0) + 6, Y(32.0) + 6), (X(126.03) - 12, Y(32.125) - 6), color=FAINT, lw=1.0, dashes=(4, 3), z=2)
    T(ax, X(126.018) + 18, Y(32.06), "14.1 km", size=9, color=GREEN, bold=True)
    T(ax, X(126.018) + 18, Y(32.05), "(북 13.9 · 동 2.8)", size=8, color=GREY)
    save(fig, "fig16_scenario_map.png")


def f17_expected():
    fig, ax, X, Y, kmy = map_canvas("예상 궤적 (60 초) · 개념도")
    ship_top(ax, (X(126.0), Y(32.0)), 30, heading=90, z=5)
    T(ax, X(126.0) + 20, Y(32.0) + 2, "플랫폼", size=10, color=BLUE, bold=True)
    lat_end = 32.12 - 12.0 / KM_LAT
    arrow(ax, (X(126.0), Y(32.12) - 10), (X(126.0), Y(lat_end)), color=ORANGE, lw=2.4, head=12, z=4)
    plane_top(ax, (X(126.0), Y(32.12)), 12, heading=270)
    T(ax, X(126.0) + 8, Y(32.10), "대공 · 직진", size=9.5, color=ORANGE, bold=True)
    T(ax, X(126.0) + 8, Y(32.09), "12 km 남하", size=8.5, color=GREY)
    dot(ax, (X(126.0), Y(lat_end)), r=4, color=ORANGE)
    T(ax, X(126.0) + 12, Y(lat_end) + 4, "60 s : 1.3 km 앞", size=8.5, color=ORANGE, bold=True)
    pts = []
    x, y = 0.0, 0.0
    hdg = 180.0
    V, g = 200.0, 9.80665
    w = math.degrees(3 * g / V)
    for k in range(600):
        t = k * 0.1
        if 20.0 <= t < 30.0:
            hdg += w * 0.1
        x += V * math.sin(math.radians(hdg)) * 0.1
        y += V * math.cos(math.radians(hdg)) * 0.1
        pts.append((X(126.0 + x / 1000.0 / KM_LON), Y(32.12 + y / 1000.0 / KM_LAT)))
    ax.plot([p[0] for p in pts], [p[1] for p in pts], color=ORANGE, lw=1.6, ls="--", zorder=3)
    dot(ax, pts[-1], r=4, color=ORANGE)
    T(ax, X(125.905), Y(32.052), "기동 예시 : 20~30 s 에 3 g 우선회", size=8.5, color=ORANGE, bold=True)
    T(ax, X(125.905), Y(32.041), "반경 1.36 km · 84° 돌아 서쪽으로 직진", size=8, color=GREY)
    lon_end = 126.03 - 1.8 / KM_LON
    arrow(ax, (X(126.03) - 8, Y(32.125)), (X(lon_end), Y(32.125)), color=GREEN, lw=2.4, head=12, z=4)
    ship_top(ax, (X(126.03), Y(32.125)), 24, heading=180, z=5, fc="#E6F3EA", ec=GREEN)
    T(ax, X(126.03) + 18, Y(32.125) + 10, "대함 · 서쪽으로 1.8 km", size=9.5, color=GREEN, bold=True)
    T(ax, X(126.03) + 18, Y(32.125) - 4, "60 s 뒤 동쪽 1.0 km 지점", size=8.5, color=GREY)
    save(fig, "fig17_expected.png")


def f18_charts_mock():
    """24장. 결과 그림 세 장의 모양 (직진 가정으로 계산한 예상값)."""
    fig = plt.figure(figsize=(11.5, 3.0), dpi=DPI, facecolor="white")
    ts = [k * 0.1 for k in range(601)]
    airN = [13307.0 - 200.0 * t for t in ts]
    airR = [math.hypot(n, 300.0) for n in airN]
    airEl = [math.degrees(math.atan2(300.0, n)) for n in airN]
    shipN, shipE = 13861.0, [2831.0 - 30.0 * t for t in ts]
    shipR = [math.hypot(shipN, e) for e in shipE]
    specs = [(0.045, "평면 궤적 (위도 · 경도)"), (0.385, "고도 (m) · 시간 (s)"), (0.715, "플랫폼 기준 거리 (km) · 고각 (°)")]
    axes = []
    for k, (lx, title) in enumerate(specs):
        a = fig.add_axes([lx, 0.17, 0.26, 0.64])
        for sp in a.spines.values():
            sp.set_color("#B9C8E0")
            sp.set_linewidth(0.8)
        a.tick_params(labelsize=7.5, colors=GREY, length=2)
        a.grid(color="#E6EBF3", lw=0.6)
        a.set_title(title, fontproperties=BOLD, fontsize=10, color=NAVY, pad=6)
        axes.append(a)
    a = axes[0]
    a.plot([126.0, 126.0], [32.12, 32.12 - 12.0 / KM_LAT], color=ORANGE, lw=2.0)
    a.plot([126.03, 126.03 - 1.8 / KM_LON], [32.125, 32.125], color=GREEN, lw=2.0)
    a.plot([126.0], [32.0], marker="^", color=BLUE, ms=7)
    a.set_xlim(125.94, 126.06)
    a.set_ylim(31.99, 32.14)
    a.set_aspect(KM_LAT / KM_LON)
    a.set_xticks([125.95, 126.0, 126.05])
    a.ticklabel_format(useOffset=False)
    a.text(126.004, 32.02, "플랫폼", fontproperties=REG, fontsize=7.5, color=BLUE)
    a.text(126.004, 32.07, "대공", fontproperties=REG, fontsize=7.5, color=ORANGE)
    a.text(126.012, 32.128, "대함", fontproperties=REG, fontsize=7.5, color=GREEN)
    a = axes[1]
    a.plot(ts, [300.0] * len(ts), color=ORANGE, lw=2.0)
    a.plot(ts, [0.0] * len(ts), color=GREEN, lw=2.0)
    a.set_ylim(-40, 360)
    a.set_xlim(0, 60)
    a.text(30, 318, "대공 : 300 m 유지 (수평 비행)", fontproperties=REG, fontsize=7.5, color=ORANGE, ha="center")
    a.text(30, 18, "대함 : 0 m (수면)", fontproperties=REG, fontsize=7.5, color=GREEN, ha="center")
    a = axes[2]
    a.plot(ts, [r / 1000.0 for r in airR], color=ORANGE, lw=2.0)
    a.plot(ts, [r / 1000.0 for r in shipR], color=GREEN, lw=2.0)
    a.set_xlim(0, 60)
    a.set_ylim(0, 16)
    a.text(8, 14.7, "대함 14.1 → 13.9 km", fontproperties=REG, fontsize=7.5, color=GREEN)
    a.text(14, 4.2, "대공 13.3 → 1.3 km", fontproperties=REG, fontsize=7.5, color=ORANGE)
    b = a.twinx()
    b.plot(ts, airEl, color=ORANGE, lw=1.2, ls="--")
    b.set_ylim(0, 16)
    b.tick_params(labelsize=7.5, colors=ORANGE, length=2)
    for sp in b.spines.values():
        sp.set_color("#B9C8E0")
    b.text(33, 9.2, "고각 1.2° → 13.4° (점선)", fontproperties=REG, fontsize=7.5, color=ORANGE)
    fig.text(0.5, 0.02, "직진만 가정하고 계산한 예상 모양이다.  실제 시뮬레이션 결과로 갈아 끼울 자리.", fontproperties=REG, fontsize=8.5, color=FAINT, ha="center")
    save(fig, "fig18_charts_mock.png")


def f19_program():
    """13장. 프로그램 구성 : 제공 코드 → C 코어 → MFC 화면."""
    fig, ax, W, H = canvas(11.5, 2.7)
    bw, bh, y0 = 330, 176, 60
    xs = [20, 410, 800]
    heads = [("제공받은 참고 소스 코드", "수정 없이 그대로 (C)",
              [("CoordinateTransform.c/.h", "좌표변환 14 함수"), ("matrixCalcLib.c/.h", "행렬 연산"), ("Define.h", "타입 · 상수 (G_FORCE)")], BLUE, SOFT),
             ("TargetSimCore  (C 정적 라이브러리)", "이번에 짠 부분 · target_sim.h / .c",
              [("ST_Maneuver/Target/Scenario", "구조체 셋"), ("f_SetAssignment", "과제 조건 채우기"), ("f_StartScenario", "위경도 → ECEF 초기화"), ("f_StepScenario", "0.1 초 한 스텝")], ORANGE, PEACH),
             ("TargetSimUI  (MFC 대화상자)", "돌리고 보여 주기만 · C++",
              [("[실행]", "600 스텝 루프 → 위경도 배열"), ("지도", "축 비율 1/cos(위도), 슬라이더"), ("[재생]", "100 ms 타이머, 1 ~ 10 배속"), ("[CSV 저장]", "t · id · 위도 · 경도 · 고도")], GREEN, "#E6F3EA")]
    for x, (h1, h2, lines, c, fc) in zip(xs, heads):
        box(ax, x, y0, bw, bh, fc=fc, ec="none", z=0, r=9)
        T(ax, x + 16, y0 + bh - 22, h1, size=11.5, color=c, bold=True)
        T(ax, x + 16, y0 + bh - 42, h2, size=9.5, color=GREY)
        yy = y0 + bh - 70
        for tok, desc in lines:
            is_code = tok[0] in "CmDSf"
            T(ax, x + 16, yy, tok, size=9.5, color=NAVY, mono=is_code, bold=not is_code)
            if desc:
                T(ax, x + max(150, 24 + len(tok) * 8.4), yy, desc, size=9.5, color=GREY)
            yy -= 22
    for i in range(2):
        xa, xb = xs[i] + bw + 8, xs[i + 1] - 8
        arrow(ax, (xa, y0 + bh / 2), (xb, y0 + bh / 2), color=NAVY, lw=2.0, head=12)
        T(ax, (xa + xb) / 2, y0 + bh / 2 + 16, "링크" if i == 0 else "호출", size=9.5, color=NAVY, bold=True, ha="center")
    T(ax, 20, 28, "좌표변환은 전부 제공 코드가 하고, 코어는 그 함수를 순서대로 부르기만 한다.  MFC 는 계산을 하지 않는다.", size=9.5, color=GREY)
    save(fig, "fig19_program.png")


ALL = [f01_overview, f02_p2_vs_p3, f03_four_frames, f04_why_ecef, f05_pos_vs_vel, f06_vel_chain,
       f07_euler, f08_curvature, f09_step_loop, f10_gload, f11_circle, f12_axes, f13_timeline,
       f14_target_card, f15_pipeline, f16_scenario_map, f17_expected, f18_charts_mock, f19_program]

if __name__ == "__main__":
    want = sys.argv[1:]
    for fn in ALL:
        if want and not any(fn.__name__.startswith(w) for w in want):
            continue
        fn()
    print("저장 위치:", OUT_DIR)
