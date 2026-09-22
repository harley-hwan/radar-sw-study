# -*- coding: utf-8 -*-
"""발표자료에 들어갈 그림을 전부 다시 그린다.

    python make_figures.py            # 전부
    python make_figures.py 15 17      # 번호만 골라서

결과 그림은 ../figures/ 에 png 로 떨어진다. 궤적과 정확도 그림은 ../data/ 의
CSV 를 읽는데, 그 CSV 는 run_sim.c / run_convergence.c 를 실제 Core 와 함께
빌드해 돌린 결과다 (README 참고). 그림에 쓰인 숫자는 전부 그 실행 결과다.
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
from matplotlib.ticker import FuncFormatter, NullFormatter
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch, Circle, Arc, Polygon, Rectangle, Wedge

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "figures"))
DATA = os.path.normpath(os.path.join(HERE, "..", "data"))
os.makedirs(OUT, exist_ok=True)

# ── 회사 템플릿에서 뽑은 색 ─────────────────────────────────────────────
NAVY = "#1B2740"
NAVY2 = "#1E2C4F"
SLATE = "#4A5A7A"
MUTED = "#8794AC"
LIGHT = "#AEB9CC"
PALE = "#D2D9E5"
BG = "#EDF0F6"
WHITE = "#FFFFFF"

# ── UiCommon.h 의 객체 색을 그대로 쓴다 (화면과 그림의 색이 같도록) ────
C_PF = "#374151"        # 플랫폼
C_SHIP = "#2563EB"      # 표적 1 (대함)
C_AIR = "#DC2626"       # 표적 2 (대공)
C_OK = "#059669"
C_WARN = "#D97706"
C_VIOLET = "#7C3AED"

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
    "figure.facecolor": WHITE,
    "savefig.facecolor": WHITE,
    "axes.edgecolor": LIGHT,
    "axes.labelcolor": NAVY,
    "text.color": NAVY,
    "xtick.color": SLATE,
    "ytick.color": SLATE,
    "grid.color": "#E3E8F0",
    "mathtext.fontset": "dejavusans",
})

DPI = 220


def save(fig, name):
    path = os.path.join(OUT, name)
    fig.savefig(path, dpi=DPI, bbox_inches="tight", pad_inches=0.06)
    plt.close(fig)
    print("  %s" % name)


def box(ax, x, y, w, h, text, fill=WHITE, edge=LIGHT, fc=NAVY, size=11,
        bold=False, radius=0.035, lw=1.4, va="center", ha="center", zorder=2):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=%g" % radius,
                                facecolor=fill, edgecolor=edge, linewidth=lw, zorder=zorder))
    if text:
        tx = x + w / 2 if ha == "center" else x + 0.16
        ax.text(tx, y + h / 2, text, ha=ha, va=va, fontsize=size, color=fc,
                fontweight="bold" if bold else "normal", zorder=zorder + 1, linespacing=1.45)


def arrow(ax, p0, p1, color=SLATE, lw=1.6, style="-|>", ms=11, ls="-", zorder=3, rad=0.0):
    ax.add_patch(FancyArrowPatch(p0, p1, arrowstyle=style, mutation_scale=ms, color=color,
                                 linewidth=lw, linestyle=ls, zorder=zorder,
                                 connectionstyle="arc3,rad=%g" % rad,
                                 shrinkA=0, shrinkB=0))


def canvas(w, h):
    """가로 · 세로 모두 0 ~ 100. 원을 그리지 않는 그림은 이 쪽이 배치하기 쉽다."""
    fig, ax = plt.subplots(figsize=(w, h))
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100)
    ax.axis("off")
    return fig, ax


def blank(w, h):
    fig, ax = plt.subplots(figsize=(w, h))
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100 * h / w)
    ax.axis("off")
    return fig, ax


def read_csv(name):
    with open(os.path.join(DATA, name), newline="", encoding="utf-8") as fp:
        return list(csv.DictReader(fp))


def col(rows, key, cast=float):
    return np.array([cast(r[key]) for r in rows])


# 플랫폼 기준 동-북 [m]. 화면 궤적 그림과 같은 방식으로 간단히 환산한다.
def to_en(lat, lon, lat0, lon0):
    m_lat = 111132.0
    m_lon = 111320.0 * math.cos(math.radians(lat0))
    return (lon - lon0) * m_lon, (lat - lat0) * m_lat


# ═══════════════════════════════════════════════════════════════════════
# 01. 왜 표적을 모의하는가
# ═══════════════════════════════════════════════════════════════════════
def fig01():
    fig, ax = canvas(11, 4.4)
    ax.text(50, 96, "같은 데이터를 얻는 두 가지 길", ha="center", va="top",
            fontsize=13, fontweight="bold", color=NAVY)

    box(ax, 2, 24, 42, 60, "", fill="#FBF7F0", edge="#E8D9BE", radius=1.0)
    box(ax, 2, 71, 42, 13, "", fill="#F3E7CE", edge="#E8D9BE", radius=1.0)
    ax.text(23, 77.5, "실제 해상 시험", ha="center", va="center", fontsize=12.5,
            fontweight="bold", color="#8A6A2B")
    for i, t in enumerate(["함정과 항공기를 실제로 띄워야 한다",
                           "같은 상황을 두 번 만들 수 없다",
                           "날씨 · 공역 · 비용에 전부 묶인다",
                           "정답(참값)을 정확히 알 수 없다"]):
        ax.text(6, 63 - i * 9.5, "·  " + t, ha="left", va="center", fontsize=10.5,
                color="#6B5326")

    box(ax, 56, 24, 42, 60, "", fill="#F1F6FF", edge="#C8DAF6", radius=1.0)
    box(ax, 56, 71, 42, 13, "", fill="#DDE9FB", edge="#C8DAF6", radius=1.0)
    ax.text(77, 77.5, "표적 모의  (이번 과제)", ha="center", va="center", fontsize=12.5,
            fontweight="bold", color="#1D4ED8")
    for i, t in enumerate(["노트북 한 대에서 60초가 1초 만에",
                           "같은 시나리오를 몇 번이든 똑같이",
                           "표적 10개 · 기동 30번까지 자유롭게",
                           "내가 넣은 값이 곧 참값이다"]):
        ax.text(60, 63 - i * 9.5, "·  " + t, ha="left", va="center", fontsize=10.5,
                color="#1E40AF")

    arrow(ax, (45.2, 54), (54.8, 54), color=NAVY, lw=2.4, ms=18)

    box(ax, 8, 3, 84, 15,
        "데이터처리 소프트웨어가 받는 입력은 어느 쪽이든 똑같다\n"
        "— 시각마다 찍힌 표적의 위도 · 경도 · 고도",
        fill=NAVY, edge=NAVY, fc=WHITE, size=11.5, bold=True, radius=1.0)
    save(fig, "fig01_why.png")



# ═══════════════════════════════════════════════════════════════════════
# 02. 만든 것 — Core 와 UI
# ═══════════════════════════════════════════════════════════════════════
def fig02():
    fig, ax = canvas(11, 4.8)

    box(ax, 1.5, 12, 42, 82, "", fill="#F7F9FC", edge=PALE, radius=1.0)
    ax.text(22.5, 88, "TargetSimUI   (C++ / MFC)", ha="center", va="center", fontsize=12.5,
            fontweight="bold", color=NAVY)
    ui = [("CTargetSimUIDlg", "화면 배치 · 실행 · 재생"),
          ("CScenario", "입력값 보관 · 범위 검사"),
          ("CTrajectoryPlot", "궤적 · 고도 그림 (GDI+)"),
          ("CGridCtrl", "표 편집")]
    for i, (n, d) in enumerate(ui):
        y = 65 - i * 14
        box(ax, 4.0, y, 36, 11.5, "", fill=WHITE, edge=PALE, radius=0.6)
        ax.text(6.4, y + 7.6, n, ha="left", va="center", fontsize=10.5, fontweight="bold",
                color=NAVY, family=MONO)
        ax.text(6.4, y + 3.4, d, ha="left", va="center", fontsize=9.5, color=SLATE)

    box(ax, 56.5, 12, 42, 82, "", fill="#F1F5FC", edge="#C8D5EA", radius=1.0)
    ax.text(77.5, 88, "TargetSimCore   (C / DLL)", ha="center", va="center", fontsize=12.5,
            fontweight="bold", color=NAVY)
    core = [("TargetSim.c", "표적 상태 갱신 — 이번 과제의 본체"),
            ("CoordinateTransform.c", "좌표 변환 (제공받은 코드 그대로)"),
            ("matrixCalcLib.c", "행렬 곱셈 (제공받은 코드 그대로)"),
            ("TargetSim.h", "구조체와 함수 공개 — UI 는 이것만 본다")]
    for i, (n, d) in enumerate(core):
        y = 65 - i * 14
        box(ax, 59.0, y, 36, 11.5, "", fill="#E8F0FE" if i == 0 else WHITE,
            edge="#C8D5EA", radius=0.6)
        ax.text(61.4, y + 7.6, n, ha="left", va="center", fontsize=10.5, fontweight="bold",
                color=NAVY, family=MONO)
        ax.text(61.4, y + 3.4, d, ha="left", va="center", fontsize=9.3, color=SLATE)

    arrow(ax, (44.3, 62), (55.7, 62), color=C_SHIP, lw=2.2, ms=13)
    ax.text(50, 67.5, "ST_SimConfig", ha="center", va="center", fontsize=8.0, color=C_SHIP,
            family=MONO, fontweight="bold",
            bbox=dict(boxstyle="round,pad=0.25", fc=WHITE, ec="none"))
    arrow(ax, (55.7, 46), (44.3, 46), color=C_OK, lw=2.2, ms=13)
    ax.text(50, 40.5, "ST_SimSample", ha="center", va="center", fontsize=8.0, color=C_OK,
            family=MONO, fontweight="bold",
            bbox=dict(boxstyle="round,pad=0.25", fc=WHITE, ec="none"))

    box(ax, 1.5, 1.5, 97, 8,
        "UI 는 Core 의 구조체와 함수 여섯 개만 알면 된다 — 화면을 바꿔도 계산은 그대로다",
        fill=BG, edge=PALE, fc=SLATE, size=10, radius=0.6)
    save(fig, "fig02_system.png")



# ═══════════════════════════════════════════════════════════════════════
# 03. 표적 하나를 숫자로 적으면
# ═══════════════════════════════════════════════════════════════════════
def fig03():
    fig, ax = canvas(11, 4.4)
    ax.text(50, 96, "표적 하나는 숫자 일곱 개로 적힌다", ha="center", va="top",
            fontsize=13, fontweight="bold", color=NAVY)

    groups = [("어디에 있나", "위도 · 경도 · 고도", "STRUCT_Coord_Lla", C_SHIP),
              ("어디를 보나", "Roll · Pitch · Yaw", "STRUCT_Coord_Attitude", C_WARN),
              ("얼마나 빠른가", "속력 한 개  V_heading", "FLOAT64", C_OK)]
    for i, (title, detail, ctype, color) in enumerate(groups):
        y = 62 - i * 24
        box(ax, 2, y, 32, 20, "", fill=WHITE, edge=color, radius=0.8, lw=1.8)
        ax.text(4.8, y + 14.8, title, ha="left", va="center", fontsize=11.5,
                fontweight="bold", color=color)
        ax.text(4.8, y + 9.0, detail, ha="left", va="center", fontsize=10.5, color=NAVY)
        ax.text(4.8, y + 3.8, ctype, ha="left", va="center", fontsize=8.6, color=MUTED,
                family=MONO)
        arrow(ax, (34.8, y + 10.0), (44, 45), color=color, lw=1.5, ms=10, rad=-0.10)

    cx, cy, k = 53, 45, 1.55
    body = np.array([[0.0, 10.0], [1.5, 2.6], [8.5, -0.4], [8.5, -2.4], [1.5, -4.4],
                     [3.4, -8.6], [3.4, -10.2], [0.0, -8.4], [-3.4, -10.2], [-3.4, -8.6],
                     [-1.5, -4.4], [-8.5, -2.4], [-8.5, -0.4], [-1.5, 2.6]])
    ax.add_patch(Polygon(np.column_stack([cx + body[:, 0] * k * 0.62, cy + body[:, 1] * k]),
                         closed=True, facecolor=C_AIR, edgecolor="none", zorder=4))

    box(ax, 66, 14, 32, 65, "", fill="#F7F9FC", edge=PALE, radius=0.9)
    ax.text(68.5, 72, "예 — 명세의 대공 표적", ha="left", fontsize=11, fontweight="bold",
            color=NAVY)
    for i, (k1, v1) in enumerate([("위도", "32.12 °"), ("경도", "126.00 °"), ("고도", "300 m"),
                                  ("Roll", "0 °"), ("Pitch", "0 °"), ("Yaw", "180 °"),
                                  ("속력", "200 m/s")]):
        y = 63 - i * 7.2
        ax.text(70, y, k1, ha="left", va="center", fontsize=10, color=SLATE, family=MONO)
        ax.text(95, y, v1, ha="right", va="center", fontsize=10, color=NAVY, family=MONO,
                fontweight="bold")

    box(ax, 2, 2, 96, 8,
        "이 일곱 개가 표적 하나의 \"초기 설정\"이다. 여기에 \"언제 어떻게 꺾을지\"를 적은 기동표가 붙는다.",
        fill=NAVY, edge=NAVY, fc=WHITE, size=10.5, radius=0.6)
    save(fig, "fig03_state7.png")



# ═══════════════════════════════════════════════════════════════════════
# 04. 위도 · 경도 · 고도 (LLA)
# ═══════════════════════════════════════════════════════════════════════
def fig04():
    fig, ax = blank(10, 4.2)
    H = 100 * 4.2 / 10
    cx, cy, R = 28, 20, 15.5

    ax.add_patch(Circle((cx, cy), R, facecolor="#E8F0FB", edgecolor=SLATE, lw=1.5, zorder=1))
    for k in (-0.62, -0.32, 0.0, 0.32, 0.62):
        rr = R * math.cos(math.asin(k)) if abs(k) < 1 else 0
        ax.plot([cx - rr, cx + rr], [cy + R * k, cy + R * k], color=LIGHT, lw=0.9, zorder=2)
    ax.plot([cx - R, cx + R], [cy, cy], color=SLATE, lw=1.4, zorder=2)
    for k in (-0.65, -0.33, 0.0, 0.33, 0.65):
        th = np.linspace(-np.pi / 2, np.pi / 2, 120)
        ax.plot(cx + R * k * np.cos(th), cy + R * np.sin(th), color=LIGHT, lw=0.9, zorder=2)
    ax.plot([cx, cx], [cy - R - 3.5, cy + R + 3.5], color=MUTED, lw=1.0, ls=(0, (4, 3)), zorder=1)

    la, lo = math.radians(33), math.radians(26)
    px = cx + R * math.sin(lo) * math.cos(la)
    py = cy + R * math.sin(la)
    ax.plot([cx, px], [cy, py], color=C_AIR, lw=1.5, zorder=4)
    ax.plot([px], [py], "o", ms=8, color=C_AIR, zorder=5)
    ax.plot([px, px + 2.6], [py, py + 4.0], color=C_AIR, lw=1.6, ls=(0, (2, 2)), zorder=4)
    ax.plot([px + 2.6], [py + 4.0], "^", ms=9, color=C_AIR, zorder=5)
    ax.text(px + 4.2, py + 4.6, "고도", fontsize=10, color=C_AIR, fontweight="bold")

    ax.add_patch(Arc((cx, cy), R * 1.0, R * 1.0, angle=0, theta1=0, theta2=33,
                     color=C_SHIP, lw=1.6, zorder=4))
    ax.text(cx + 9.5, cy + 3.6, "위도", fontsize=10, color=C_SHIP, fontweight="bold")
    ax.add_patch(Arc((cx, cy), R * 0.62, R * 0.62, angle=0, theta1=-38, theta2=0,
                     color=C_OK, lw=1.6, zorder=4))
    ax.text(cx + 5.4, cy - 5.2, "경도", fontsize=10, color=C_OK, fontweight="bold")

    rows = [("위도 (Latitude)", "적도에서 남북으로 잰 각", "-90 ° ~ +90 °", C_SHIP),
            ("경도 (Longitude)", "영국 그리니치에서 동서로 잰 각", "-180 ° ~ +180 °", C_OK),
            ("고도 (Altitude)", "지표면에서 위로 잰 높이", "미터", C_AIR)]
    for i, (n, d, r, c) in enumerate(rows):
        y = 32 - i * 10.5
        ax.add_patch(Rectangle((52, y - 1.2), 0.9, 7.6, facecolor=c, edgecolor="none"))
        ax.text(54.5, y + 4.6, n, ha="left", fontsize=11, fontweight="bold", color=NAVY)
        ax.text(54.5, y + 1.8, d, ha="left", fontsize=9.8, color=SLATE)
        ax.text(54.5, y - 0.7, r, ha="left", fontsize=9, color=MUTED)

    box(ax, 52, 1.5, 46, 6.5,
        "사람이 읽기 좋은 좌표 — 그래서 입력과 출력은 LLA 로 받고 쓴다",
        fill=BG, edge=PALE, fc=NAVY, size=10, radius=0.6)
    save(fig, "fig04_lla.png")


# ═══════════════════════════════════════════════════════════════════════
# 05. 속도를 왜 크기 하나로만 받는가
# ═══════════════════════════════════════════════════════════════════════
def fig05():
    fig, ax = canvas(11, 4.2)
    ax.text(50, 96, "속도를 어떻게 입력받을 것인가", ha="center", va="top", fontsize=13,
            fontweight="bold", color=NAVY)

    box(ax, 1.5, 8, 45, 76, "", fill="#FDF6F6", edge="#F0D5D5", radius=0.9)
    ax.text(24, 78, "x, y, z 세 숫자로 받으면", ha="center", va="center", fontsize=12,
            fontweight="bold", color="#B45151")
    for i, t in enumerate(["\"북서쪽으로 초속 30 m\" 를",
                           "사람이 직접 세 숫자로 쪼개야 한다",
                           "세 숫자를 조금 잘못 넣으면",
                           "속력이 30 이 아니라 31.4 가 된다",
                           "자세각과 따로 놀아서, 기수는 서쪽인데",
                           "속도는 동쪽인 표적도 만들어진다"]):
        gap = 4 if i % 2 == 0 else 0
        ax.text(5, 63 - i * 9.5 - gap * 0, t, ha="left", va="center", fontsize=10,
                color="#8A4B4B")

    box(ax, 53.5, 8, 45, 76, "", fill="#F1F8F4", edge="#C6E4D2", radius=0.9)
    ax.text(76, 78, "속력 하나 + 자세각으로 받으면", ha="center", va="center", fontsize=12,
            fontweight="bold", color="#0F7A52")
    for i, t in enumerate(["\"기수 방향으로 초속 30 m\" 그대로 적는다",
                           "조종사가 계기판에서 읽는 값과 같다",
                           "회전만 시키므로 속력은 절대 안 변한다",
                           "(회전은 길이를 바꾸지 않는다)",
                           "기동은 자세각만 돌리면 되고",
                           "속도 방향은 저절로 따라온다"]):
        ax.text(57, 63 - i * 9.5, t, ha="left", va="center", fontsize=10, color="#0B5C3E")

    ax.text(50, 3, "명세도 \"동체 속도(Heading 방향, V_heading) 성분으로 입력받도록\" 을 요구한다",
            ha="center", va="center", fontsize=10, color=MUTED)
    save(fig, "fig05_speed.png")



# ═══════════════════════════════════════════════════════════════════════
# 06. 기동 = 언제부터 언제까지 어느 축으로 몇 G
# ═══════════════════════════════════════════════════════════════════════
def fig06():
    fig, ax = canvas(11, 4.3)
    ax.text(4, 96, "기동 한 줄에 들어가는 것 네 가지", ha="left", va="top", fontsize=13,
            fontweight="bold", color=NAVY)

    items = [("TurnType", "어느 축으로", "0 없음 / 1 Roll\n2 Yaw / 3 Pitch", C_VIOLET),
             ("Gravity Value", "얼마나 세게", "중력가속도의 몇 배\n부호가 회전 방향", C_WARN),
             ("StartTime", "언제부터", "[초]", C_OK),
             ("EndTime", "언제까지", "[초]", C_OK)]
    for i, (n, h, d, c) in enumerate(items):
        x = 4 + i * 23.5
        box(ax, x, 56, 21.5, 30, "", fill=WHITE, edge=c, radius=0.7, lw=1.7)
        ax.text(x + 10.75, 80, n, ha="center", va="center", fontsize=10.4,
                fontweight="bold", color=c, family=MONO)
        ax.text(x + 10.75, 72, h, ha="center", va="center", fontsize=10.6, color=NAVY)
        ax.text(x + 10.75, 63, d, ha="center", va="center", fontsize=9, color=SLATE,
                linespacing=1.6)

    ax.text(4, 47, "표적 하나에 이런 줄을 최대 30개까지 — 걸린 구간에서만 기수가 돌아가고, 나머지 시간은 직진이다",
            ha="left", va="center", fontsize=10.2, color=NAVY)

    y0 = 22
    ax.plot([6, 94], [y0, y0], color=SLATE, lw=1.6, zorder=2)
    for t, lab in [(0, "0"), (15, "15"), (30, "30"), (45, "45"), (60, "60")]:
        x = 6 + 88 * t / 60.0
        ax.plot([x, x], [y0 - 2.2, y0 + 2.2], color=SLATE, lw=1.2, zorder=2)
        ax.text(x, y0 - 6.5, lab, ha="center", fontsize=9.5, color=SLATE)
    ax.text(94, y0 + 5.5, "[초]", ha="left", fontsize=9.5, color=MUTED)

    for s, e, lab, c in [(2.8, 6.7, "-8.2 G", C_AIR), (13.3, 17.2, "+8.2 G", C_AIR),
                         (18.6, 22.5, "+8.2 G", C_AIR), (36.2, 40.1, "-8.2 G", C_AIR),
                         (41.5, 45.4, "-8.2 G", C_AIR), (53.5, 60.0, "+8.2 G", C_AIR)]:
        x0 = 6 + 88 * s / 60.0
        x1 = 6 + 88 * e / 60.0
        ax.add_patch(Rectangle((x0, y0 + 1.5), x1 - x0, 13, facecolor=c, alpha=0.18,
                               edgecolor=c, lw=1.2, zorder=2))
        ax.text((x0 + x1) / 2, y0 + 8, lab, ha="center", va="center", fontsize=7.6,
                color=c, fontweight="bold")
    ax.text(4, 5, "↑  기동 시연 시나리오에서 대공 표적에 걸어 둔 여섯 줄", ha="left",
            va="center", fontsize=9.4, color=MUTED)
    save(fig, "fig06_maneuver.png")



# ═══════════════════════════════════════════════════════════════════════
# 07. G 값이 회전 속도가 되는 원리
# ═══════════════════════════════════════════════════════════════════════
def fig07():
    fig, ax = blank(11, 4.3)
    H = 100 * 4.3 / 11

    cx, cy, R = 24, 20, 13.0
    th = np.linspace(20, 250, 200)
    ax.plot(cx + R * np.cos(np.radians(th)), cy + R * np.sin(np.radians(th)),
            color=LIGHT, lw=1.6, ls=(0, (5, 4)), zorder=2)
    ax.plot([cx], [cy], "o", ms=6, color=MUTED, zorder=3)
    ax.text(cx - 1.5, cy - 3.6, "선회 중심", fontsize=9, color=MUTED, ha="center")

    a = math.radians(58)
    px, py = cx + R * math.cos(a), cy + R * math.sin(a)
    ax.plot([cx, px], [cy, py], color=MUTED, lw=1.2, ls=(0, (3, 3)), zorder=2)
    ax.text(cx + 2.6, cy + 7.4, "R", fontsize=11, color=MUTED, fontweight="bold",
            style="italic", ha="center")
    ax.plot([px], [py], "o", ms=9, color=C_AIR, zorder=5)
    arrow(ax, (px, py), (px + 10 * math.cos(a + math.pi / 2), py + 10 * math.sin(a + math.pi / 2)),
          color=C_AIR, lw=2.2, ms=13)
    ax.text(px - 12.0, py + 8.6, "속도 V", fontsize=10.5, color=C_AIR, fontweight="bold")
    arrow(ax, (px, py), (px + 8.5 * math.cos(a + math.pi), py + 8.5 * math.sin(a + math.pi)),
          color=C_WARN, lw=2.2, ms=13)
    ax.text(px + 1.5, py - 10.5, "구심가속도 = n × g", fontsize=10, color=C_WARN,
            fontweight="bold", ha="center")

    x0, w = 50, 48
    ax.text(x0, H - 3.5, "원운동은 식 한 줄이면 끝난다", ha="left", va="center", fontsize=12.5,
            fontweight="bold", color=NAVY)
    box(ax, x0, H - 14.0, w, 8.0, "", fill=BG, edge=PALE, radius=0.6)
    ax.text(x0 + w / 2, H - 10.0, "V × ω  =  n × g", ha="center", va="center", fontsize=15,
            color=NAVY, fontweight="bold", family=MONO)
    ax.text(x0, H - 17.0, "속력 × 각속도 = 중력가속도의 n 배", ha="left", va="center",
            fontsize=9.6, color=MUTED)

    box(ax, x0, H - 28.5, w, 8.5, "", fill="#F1F6FF", edge="#C8DAF6", radius=0.6)
    ax.text(x0 + w / 2, H - 24.2, "ω  =  ( n × 9.80665 ) ÷ V", ha="center", va="center",
            fontsize=13.5, color=C_SHIP, fontweight="bold", family=MONO)
    ax.text(x0, 6.5, "G 값 하나만 받으면 초당 몇 도를 돌지가 정해진다.", ha="left",
            va="center", fontsize=10.2, color=NAVY)
    ax.text(x0, 2.6, "omega = (gravityValue * G_FORCE) / headingSpeed;", ha="left",
            va="center", fontsize=8.8, color=MUTED, family=MONO)
    save(fig, "fig07_gforce.png")



# ═══════════════════════════════════════════════════════════════════════
# 08. 회전축 세 가지 — 같은 2 G 를 세 축에 각각 걸어 본 실제 결과
# ═══════════════════════════════════════════════════════════════════════
def fig08():
    rows = read_csv("turn.csv")
    t = col(rows, "time")
    lat0, lon0 = 32.0, 126.0
    fig = plt.figure(figsize=(11, 4.0))
    gs = fig.add_gridspec(1, 3, width_ratios=[1.05, 1.0, 1.0], wspace=0.30)

    names = [("2 Yaw — 기수를 좌우로", C_SHIP), ("3 Pitch — 기수를 위아래로", C_WARN),
             ("1 Roll — 몸통을 옆으로", C_VIOLET)]

    ax = fig.add_subplot(gs[0, 0])
    for i, (n, c) in enumerate(names, start=1):
        e, nn = to_en(col(rows, "t%d_lat" % i), col(rows, "t%d_lon" % i), lat0, lon0)
        ax.plot(e / 1000.0, nn / 1000.0, color=c, lw=2.0, label=n.split(" —")[0])
    ax.plot([0], [0], "^", ms=9, color=C_PF)
    ax.set_xlabel("동쪽 [km]", fontsize=9.5)
    ax.set_ylabel("북쪽 [km]", fontsize=9.5)
    ax.set_title("위에서 본 궤적", fontsize=10.5, color=NAVY, pad=7)
    ax.grid(True, lw=0.6)
    ax.legend(fontsize=8.2, loc="lower left", framealpha=0.95)
    ax.tick_params(labelsize=8.5)
    ax.set_aspect("equal", adjustable="datalim")

    ax = fig.add_subplot(gs[0, 1])
    for i, (n, c) in enumerate(names, start=1):
        ax.plot(t, col(rows, "t%d_alt" % i), color=c, lw=2.0)
    ax.set_xlabel("시각 [s]", fontsize=9.5)
    ax.set_ylabel("고도 [m]", fontsize=9.5)
    ax.set_title("고도 변화", fontsize=10.5, color=NAVY, pad=7)
    ax.grid(True, lw=0.6)
    ax.tick_params(labelsize=8.5)
    ax.axvspan(10, 20, color=PALE, alpha=0.55, zorder=0)
    ax.text(15, ax.get_ylim()[1] * 0.93, "기동 구간", ha="center", fontsize=8.2, color=SLATE)

    ax = fig.add_subplot(gs[0, 2])
    ax.axis("off")
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 100)
    ax.text(0, 95, "같은 표적 · 같은 2 G · 같은 10초", fontsize=10, color=NAVY, fontweight="bold")
    facts = [("Yaw", "기수가 옆으로 돌아 궤적이 휘었다.\n고도는 그대로.", C_SHIP),
             ("Pitch", "기수가 들려 7.5 km 를 올라갔다.\n수평 방향은 거의 안 변함.", C_WARN),
             ("Roll", "몸통만 돌았다. 속도 방향이 곧\n회전축이라 궤적은 직진 그대로.", C_VIOLET)]
    for i, (n, d, c) in enumerate(facts):
        y = 76 - i * 26
        ax.add_patch(Rectangle((0, y - 10), 1.8, 18, facecolor=c, edgecolor="none"))
        ax.text(5, y + 4, n, fontsize=11, fontweight="bold", color=c, family=MONO)
        ax.text(5, y - 5, d, fontsize=9.2, color=SLATE, linespacing=1.6, va="center")

    fig.savefig(os.path.join(OUT, "fig08_turntype.png"), dpi=DPI, bbox_inches="tight",
                pad_inches=0.08)
    plt.close(fig)
    print("  fig08_turntype.png")


# ═══════════════════════════════════════════════════════════════════════
# 09. 구조체 계층
# ═══════════════════════════════════════════════════════════════════════
def fig09():
    fig, ax = canvas(11, 4.8)
    ax.text(24, 96, "입력  —  내가 적는 것", ha="center", va="center", fontsize=11.5,
            fontweight="bold", color=C_SHIP)
    ax.text(76, 96, "출력  —  Core 가 채우는 것", ha="center", va="center", fontsize=11.5,
            fontweight="bold", color=C_OK)

    def tree(items, x0, fill0, edge):
        for i, (n, d, lv) in enumerate(items):
            y = 76 - i * 16.5
            x = x0 + lv * 4.5
            w = 43 - lv * 4.5
            box(ax, x, y, w, 13, "", fill=fill0 if lv == 0 else WHITE, edge=edge,
                radius=0.5, lw=1.5)
            if n:
                ax.text(x + 2.4, y + 8.6, n, ha="left", va="center", fontsize=10,
                        fontweight="bold", color=NAVY, family=MONO)
                ax.text(x + 2.4, y + 3.8, d, ha="left", va="center", fontsize=8.8, color=SLATE)
            else:
                ax.text(x + 2.4, y + 6.5, d, ha="left", va="center", fontsize=8.8, color=SLATE)
            if lv > 0:
                ax.plot([x - 2.2, x - 2.2, x - 0.4], [y + 19.9, y + 6.5, y + 6.5],
                        color=LIGHT, lw=1.2, zorder=1)

    tree([("ST_SimConfig", "시뮬레이션 시간 · 갱신 간격 · 표적 수", 0),
          ("ST_PlatformInit", "플랫폼 초기값 (기동 없음)", 1),
          ("ST_TargetInit", "표적 초기값 + 기동 개수          × 최대 10", 1),
          ("ST_TargetManeuver", "축 · G · 시작 · 종료              × 최대 30", 2)],
         2.5, "#F1F6FF", "#C8DAF6")
    tree([("ST_SimState", "진행 상태 — 총 스텝 수 · 설정 사본", 0),
          ("ST_SimSample", "한 시각의 전원 상태 (스텝 번호 · 시각)", 1),
          ("ST_TargetState", "한 객체의 상태                    × 1 + 10", 2),
          ("", "ECEF 위치 · ECEF 속도 · LLA · 자세각", 3)],
         54.5, "#F1F8F4", "#C6E4D2")

    arrow(ax, (46.6, 60), (53.4, 60), color=NAVY, lw=2.0, ms=13)
    ax.text(50, 68, "f_Tgt_InitSim\nf_Tgt_StepSim", ha="center", va="center", fontsize=8.4,
            color=NAVY, family=MONO, fontweight="bold", linespacing=1.6)

    box(ax, 2.5, 2, 95, 9,
        "설정과 상태를 따로 둔 이유 — 설정은 처음 한 번만 읽고, 매 스텝 갱신되는 것은 상태뿐이다",
        fill=BG, edge=PALE, fc=SLATE, size=10, radius=0.5)
    save(fig, "fig09_struct.png")



# ═══════════════════════════════════════════════════════════════════════
# 10. 좌표계 셋 — LLA · ECEF · NED
# ═══════════════════════════════════════════════════════════════════════
def fig10():
    fig, axes = plt.subplots(1, 3, figsize=(11, 3.5))
    titles = [("LLA", "사람이 읽는 좌표", "위도 · 경도 · 고도\n입력과 출력에 쓴다", C_SHIP),
              ("ECEF", "지구에 박힌 직교좌표", "지구 중심이 원점\n위치 갱신을 여기서 한다", C_OK),
              ("NED", "발밑의 평면 좌표", "북 · 동 · 아래\n기수 방향을 여기서 푼다", C_WARN)]

    ax = axes[0]
    ax.set_xlim(0, 100); ax.set_ylim(0, 100); ax.axis("off")
    ax.add_patch(Circle((50, 45), 30, facecolor="#E8F0FB", edgecolor=SLATE, lw=1.4))
    for k in (-0.55, 0.0, 0.55):
        rr = 30 * math.cos(math.asin(k))
        ax.plot([50 - rr, 50 + rr], [45 + 30 * k, 45 + 30 * k], color=LIGHT, lw=0.9)
    for k in (-0.6, 0.0, 0.6):
        th = np.linspace(-np.pi / 2, np.pi / 2, 100)
        ax.plot(50 + 30 * k * np.cos(th), 45 + 30 * np.sin(th), color=LIGHT, lw=0.9)
    ax.plot([62], [62], "o", ms=9, color=C_AIR)
    ax.text(66, 66, "(위도, 경도, 고도)", fontsize=8.6, color=C_AIR, fontweight="bold")

    ax = axes[1]
    ax.set_xlim(0, 100); ax.set_ylim(0, 100); ax.axis("off")
    ax.add_patch(Circle((50, 45), 30, facecolor="#EAF6EF", edgecolor=SLATE, lw=1.4))
    arrow(ax, (50, 45), (50, 88), color=C_OK, lw=1.8)
    ax.text(52, 86, "Z  (북극)", fontsize=8.8, color=C_OK, fontweight="bold")
    arrow(ax, (50, 45), (93, 45), color=C_OK, lw=1.8)
    ax.text(84, 40, "Y", fontsize=8.8, color=C_OK, fontweight="bold")
    arrow(ax, (50, 45), (24, 20), color=C_OK, lw=1.8)
    ax.text(20, 22, "X  (경도 0 °)", fontsize=8.8, color=C_OK, fontweight="bold")
    ax.plot([62], [62], "o", ms=9, color=C_AIR)
    ax.text(66, 66, "(x, y, z)  [m]", fontsize=8.6, color=C_AIR, fontweight="bold")

    ax = axes[2]
    ax.set_xlim(0, 100); ax.set_ylim(0, 100); ax.axis("off")
    th = np.linspace(np.pi * 0.72, np.pi * 0.28, 120)
    ax.plot(50 + 88 * np.cos(th), -30 + 88 * np.sin(th), color=SLATE, lw=1.4)
    ax.fill_between(50 + 88 * np.cos(th), -30 + 88 * np.sin(th), 0, color="#FDF3E4")
    ox, oy = 50, 55
    arrow(ax, (ox, oy), (ox, oy + 26), color=C_WARN, lw=1.8)
    ax.text(ox + 2.5, oy + 24, "N  북", fontsize=8.8, color=C_WARN, fontweight="bold")
    arrow(ax, (ox, oy), (ox + 30, oy - 8), color=C_WARN, lw=1.8)
    ax.text(ox + 24, oy - 14, "E  동", fontsize=8.8, color=C_WARN, fontweight="bold")
    arrow(ax, (ox, oy), (ox - 20, oy - 20), color=C_WARN, lw=1.8)
    ax.text(ox - 30, oy - 23, "D  아래", fontsize=8.8, color=C_WARN, fontweight="bold")
    ax.plot([ox], [oy], "o", ms=7, color=C_PF)
    ax.text(ox - 5, oy + 5, "지금 내 자리", fontsize=8.2, color=SLATE, ha="right")

    for ax, (n, s, d, c) in zip(axes, titles):
        ax.text(50, 98, n, ha="center", fontsize=13, fontweight="bold", color=c)
        ax.text(50, 90, s, ha="center", fontsize=9.6, color=NAVY)
        ax.text(50, 10, d, ha="center", va="top", fontsize=9.2, color=SLATE, linespacing=1.6)

    fig.subplots_adjust(wspace=0.05)
    fig.savefig(os.path.join(OUT, "fig10_frames.png"), dpi=DPI, bbox_inches="tight",
                pad_inches=0.08)
    plt.close(fig)
    print("  fig10_frames.png")


# ═══════════════════════════════════════════════════════════════════════
# 11. 왜 ECEF 에서 갱신하는가
# ═══════════════════════════════════════════════════════════════════════
def fig11():
    fig, ax = blank(11, 4.0)
    H = 100 * 4.0 / 11

    box(ax, 1.5, 4.5, 45, H - 11, "", fill="#FDF6F6", edge="#F0D5D5", radius=0.9)
    ax.text(24, H - 10.0, "LLA 에서 바로 더하면", ha="center", fontsize=12,
            fontweight="bold", color="#B45151")
    cx, cy = 24, 21
    th = np.linspace(np.pi * 0.78, np.pi * 0.22, 120)
    ax.plot(cx + 46 * np.cos(th), cy - 34 + 46 * np.sin(th), color="#C99", lw=1.6)
    ax.plot([cx - 16, cx + 16], [cy + 8.5, cy + 8.5], color="#B45151", lw=2.0, ls=(0, (4, 3)))
    arrow(ax, (cx - 16, cy + 8.5), (cx + 16, cy + 8.5), color="#B45151", lw=2.0)
    ax.plot([cx + 16, cx + 16], [cy + 8.5, cy + 2.7], color="#B45151", lw=1.3, ls=(0, (2, 2)))
    ax.text(cx + 17.5, cy + 5.6, "벌어진 만큼\n고도가 뜬다", fontsize=8.8, color="#B45151",
            ha="left", va="center", linespacing=1.5)
    ax.text(24, 8.5, "\"위도 1도 = 몇 m\" 가 위도마다 달라서\n곧게 날려도 고도가 슬금슬금 오른다",
            ha="center", fontsize=9.6, color="#8A4B4B", linespacing=1.6)

    box(ax, 53.5, 4.5, 45, H - 11, "", fill="#F1F8F4", edge="#C6E4D2", radius=0.9)
    ax.text(76, H - 10.0, "ECEF 에서 더하면", ha="center", fontsize=12,
            fontweight="bold", color="#0F7A52")
    box(ax, 58, 19, 36, 9.5, "", fill=WHITE, edge="#C6E4D2", radius=0.5)
    ax.text(76, 23.8, "새 위치 = 옛 위치 + 속도 × Δt", ha="center", va="center",
            fontsize=11.5, color="#0B5C3E", fontweight="bold")
    ax.text(76, 14.0, "미터 단위 직교좌표라 그냥 더하면 된다.\n"
                      "지구가 둥근 것은 좌표계가 이미 품고 있다.",
            ha="center", va="center", fontsize=9.6, color="#0B5C3E", linespacing=1.6)
    ax.text(76, 7.5, "출력할 때만 f_Trans_Ecef_To_Lla 로 되돌린다", ha="center",
            fontsize=8.8, color=MUTED, family=MONO)

    save(fig, "fig11_whyecef.png")


# ═══════════════════════════════════════════════════════════════════════
# 12. V_heading 을 ECEF 속도로 — 두 번 돌리면 끝
# ═══════════════════════════════════════════════════════════════════════
def fig12():
    fig, ax = canvas(11, 4.3)
    ax.text(50, 96, "회전 두 번 — 그래서 속력은 처음 그대로 보존된다", ha="center",
            va="top", fontsize=13, fontweight="bold", color=NAVY)

    steps = [("동체 좌표", "( V, 0, 0 )", "기수 방향으로 V.\n옆도 아래도 0.", C_AIR, 2.0),
             ("NED 좌표", "( V_N, V_E, V_D )", "자세각 Roll · Pitch · Yaw 로\n한 번 돌린다.", C_WARN, 35.0),
             ("ECEF 좌표", "( V_x, V_y, V_z )", "그 지점의 위도 · 경도로\n한 번 더 돌린다.", C_OK, 68.0)]
    for name, val, desc, c, x in steps:
        box(ax, x, 28, 30, 44, "", fill=WHITE, edge=c, radius=0.8, lw=1.8)
        ax.text(x + 15, 64, name, ha="center", va="center", fontsize=11.5,
                fontweight="bold", color=c)
        ax.text(x + 15, 53, val, ha="center", va="center", fontsize=12.5, color=NAVY,
                family=MONO, fontweight="bold")
        ax.text(x + 15, 39, desc, ha="center", va="center", fontsize=9.4, color=SLATE,
                linespacing=1.6)

    for x0, x1, fn in [(32.5, 34.5, "f_Trans_Body_To_Ned"), (65.5, 67.5, "f_Trans_Ned_To_Ecef")]:
        arrow(ax, (x0, 50), (x1, 50), color=NAVY, lw=2.2, ms=14)
        ax.text((x0 + x1) / 2, 78, fn, ha="center", va="center", fontsize=8.6, color=NAVY,
                family=MONO, fontweight="bold")
        ax.plot([(x0 + x1) / 2, (x0 + x1) / 2], [72.5, 75.5], color=LIGHT, lw=1.0)

    box(ax, 4, 4, 92, 16,
        "실제로 60초 내내 재보면 —  대함 30.000000000 m/s,  대공 200.000000000 m/s\n"
        "소수점 아홉 자리까지 넣은 값 그대로였다.",
        fill=BG, edge=PALE, fc=SLATE, size=10.2, radius=0.6)
    save(fig, "fig12_velocity.png")



# ═══════════════════════════════════════════════════════════════════════
# 13. 오일러법 vs 중점법
# ═══════════════════════════════════════════════════════════════════════
def fig13():
    fig, axes = plt.subplots(1, 2, figsize=(11, 3.7))
    for ax in axes:
        ax.set_xlim(0, 100); ax.set_ylim(0, 72); ax.axis("off")

    def curve(ax, color):
        t = np.linspace(0, 1, 200)
        x = 12 + 74 * t
        y = 16 + 40 * np.sin(t * 1.25)
        ax.plot(x, y, color=color, lw=2.0, ls=(0, (5, 4)), zorder=2)
        return x, y

    ax = axes[0]
    ax.text(50, 68, "오일러법 — 출발할 때의 방향으로 쭉", ha="center", fontsize=11.5,
            fontweight="bold", color="#B45151")
    xs, ys = curve(ax, LIGHT)
    px, py = 12.0, 16.0
    for i in range(4):
        t0 = i * 0.25
        dx, dy = 74 * 0.25, 40 * 1.25 * math.cos(t0 * 1.25) * 0.25
        ax.plot([px, px + dx], [py, py + dy], color="#B45151", lw=2.2, zorder=3)
        ax.plot([px], [py], "o", ms=6, color="#B45151", zorder=4)
        px, py = px + dx, py + dy
    ax.plot([px], [py], "o", ms=6, color="#B45151", zorder=4)
    ax.annotate("", xy=(px, py), xytext=(xs[-1], ys[-1]),
                arrowprops=dict(arrowstyle="<->", color=SLATE, lw=1.2))
    ax.text(px + 1.5, (py + ys[-1]) / 2, "벌어짐", fontsize=9, color=SLATE, ha="left")
    ax.text(50, 3, "굽은 길을 직선 네 토막으로 근사 — 바깥쪽으로 밀린다",
            ha="center", fontsize=9.4, color=SLATE)

    ax = axes[1]
    ax.text(50, 68, "중점법 — 반 걸음 가보고, 거기 방향으로 한 걸음", ha="center",
            fontsize=11.5, fontweight="bold", color="#0F7A52")
    xs, ys = curve(ax, LIGHT)
    px, py = 12.0, 16.0
    for i in range(4):
        t0 = i * 0.25
        tm = t0 + 0.125
        dxh, dyh = 74 * 0.125, 40 * 1.25 * math.cos(t0 * 1.25) * 0.125
        ax.plot([px, px + dxh], [py, py + dyh], color=MUTED, lw=1.3, ls=(0, (2, 2)), zorder=3)
        ax.plot([px + dxh], [py + dyh], "o", ms=4.5, color=MUTED, zorder=4)
        dx, dy = 74 * 0.25, 40 * 1.25 * math.cos(tm * 1.25) * 0.25
        ax.plot([px, px + dx], [py, py + dy], color="#0F7A52", lw=2.2, zorder=3)
        ax.plot([px], [py], "o", ms=6, color="#0F7A52", zorder=4)
        px, py = px + dx, py + dy
    ax.plot([px], [py], "o", ms=6, color="#0F7A52", zorder=4)
    ax.text(50, 3, "같은 0.1초인데 오차는 스텝 간격의 제곱으로 줄어든다",
            ha="center", fontsize=9.4, color=SLATE)

    fig.savefig(os.path.join(OUT, "fig13_midpoint.png"), dpi=DPI, bbox_inches="tight",
                pad_inches=0.08)
    plt.close(fig)
    print("  fig13_midpoint.png")


# ═══════════════════════════════════════════════════════════════════════
# 14. 한 스텝에 벌어지는 일
# ═══════════════════════════════════════════════════════════════════════
def fig14():
    fig, ax = blank(11, 4.9)
    H = 100 * 4.9 / 11

    items = [
        ("1", "지금 시각에 걸린\n기동을 찾는다", "f_Tgt_GetAttRate", C_SHIP),
        ("2", "G 값을 각속도로\n바꾼다", "omega = n·g / V", C_SHIP),
        ("3", "자세각을 반 스텝\n돌려 본다", "Att + rate × Δt/2", C_WARN),
        ("4", "그 자세로 ECEF\n속도를 만든다", "f_Tgt_GetVelEcef", C_WARN),
        ("5", "그 속도로 한 스텝\n위치를 옮긴다", "Pos + Vel × Δt", C_OK),
        ("6", "ECEF 를 LLA 로\n되돌려 함께 보관", "f_Trans_Ecef_To_Lla", C_OK),
    ]
    for i, (n, d, code, c) in enumerate(items):
        x = 1.5 + i * 16.4
        box(ax, x, 12.5, 14.6, 24.0, "", fill=WHITE, edge=c, radius=0.7, lw=1.6)
        ax.add_patch(Circle((x + 7.3, 32.6), 2.7, facecolor=c, edgecolor="none", zorder=4))
        ax.text(x + 7.3, 32.6, n, ha="center", va="center", fontsize=10, color=WHITE,
                fontweight="bold", zorder=5)
        ax.text(x + 7.3, 24.4, d, ha="center", va="center", fontsize=9.2, color=NAVY,
                linespacing=1.6)
        ax.text(x + 7.3, 15.6, code, ha="center", va="center", fontsize=7.4, color=MUTED,
                family=MONO)
        if i < len(items) - 1:
            arrow(ax, (x + 15.0, 24.5), (x + 16.0, 24.5), color=LIGHT, lw=1.6, ms=10)

    box(ax, 1.5, 3.0, 97, 9.0,
        "여섯 단계 어느 하나라도 숫자가 무한대나 NaN 이 되면 — 상태를 건드리지 않고 오류 코드만 돌려준다\n"
        "(사본에서 전부 계산한 뒤, 다 성공했을 때만 한 번에 반영)",
        fill=NAVY, edge=NAVY, fc=WHITE, size=9.6, radius=0.6)
    ax.text(50, H - 3.0, "f_Tgt_StepSim 한 번  =  플랫폼 1 개 + 표적 N 개에 대해 아래를 반복",
            ha="center", va="center", fontsize=11.5, color=NAVY, fontweight="bold")
    save(fig, "fig14_stepflow.png")


# ═══════════════════════════════════════════════════════════════════════
# 15. 명세 시나리오 결과 — 60 초 궤적
# ═══════════════════════════════════════════════════════════════════════
def _map_axes(ax, rows, lat0, lon0, mark_every=100, show_alt_label=True):
    e1, n1 = to_en(col(rows, "t1_lat"), col(rows, "t1_lon"), lat0, lon0)
    e2, n2 = to_en(col(rows, "t2_lat"), col(rows, "t2_lon"), lat0, lon0)
    ax.plot(e1 / 1000.0, n1 / 1000.0, color=C_SHIP, lw=2.2, label="표적 1  대함 30 m/s", zorder=4)
    ax.plot(e2 / 1000.0, n2 / 1000.0, color=C_AIR, lw=2.2, label="표적 2  대공 200 m/s", zorder=4)
    ax.plot(e1[0] / 1000.0, n1[0] / 1000.0, "o", ms=8, mfc=WHITE, mec=C_SHIP, mew=2, zorder=6)
    ax.plot(e2[0] / 1000.0, n2[0] / 1000.0, "o", ms=8, mfc=WHITE, mec=C_AIR, mew=2, zorder=6)
    ax.plot(e1[-1] / 1000.0, n1[-1] / 1000.0, "s", ms=7, color=C_SHIP, zorder=6)
    ax.plot(e2[-1] / 1000.0, n2[-1] / 1000.0, "s", ms=7, color=C_AIR, zorder=6)
    ax.plot([0], [0], "^", ms=11, color=C_PF, zorder=6,
            label="플랫폼  32.0 °, 126.0 °  정지", ls="none")
    for e, n, c in ((e1, n1, C_SHIP), (e2, n2, C_AIR)):
        ax.plot(e[mark_every::mark_every] / 1000.0, n[mark_every::mark_every] / 1000.0,
                "o", ms=3.2, color=c, zorder=5)
    ax.set_xlabel("플랫폼 기준 동쪽 [km]", fontsize=9.5)
    ax.set_ylabel("플랫폼 기준 북쪽 [km]", fontsize=9.5)
    ax.grid(True, lw=0.6)
    ax.tick_params(labelsize=8.5)
    ax.set_aspect("equal", adjustable="box")
    return (e1, n1, e2, n2)


def fig15():
    rows = read_csv("spec.csv")
    t = col(rows, "time")
    fig = plt.figure(figsize=(11, 4.3))
    gs = fig.add_gridspec(2, 2, width_ratios=[1.32, 1.0], height_ratios=[1, 1],
                          wspace=0.26, hspace=0.55)

    ax = fig.add_subplot(gs[:, 0])
    e1, n1, e2, n2 = _map_axes(ax, rows, 32.0, 126.0)
    ax.set_xlim(-6.0, 6.0)
    ax.set_ylim(-1.2, 15.2)
    ax.set_title("위에서 본 궤적  (점 = 10초 간격)", fontsize=10.5, color=NAVY, pad=8)
    ax.legend(fontsize=7.8, loc="lower left", framealpha=0.95, handlelength=1.4)

    # 대함 궤적은 대공의 1/7 크기라 그대로 두면 점으로 보인다. 같은 축척으로 잘라 확대해 얹는다.
    sub = ax.inset_axes([0.05, 0.60, 0.44, 0.24])
    sub.plot(e1 / 1000.0, n1 / 1000.0, color=C_SHIP, lw=2.0)
    sub.plot(e1[0] / 1000.0, n1[0] / 1000.0, "o", ms=6, mfc=WHITE, mec=C_SHIP, mew=1.8)
    sub.plot(e1[-1] / 1000.0, n1[-1] / 1000.0, "s", ms=5.5, color=C_SHIP)
    sub.plot(e1[100::100] / 1000.0, n1[100::100] / 1000.0, "o", ms=3, color=C_SHIP)
    sub.set_xlim(e1.min() / 1000.0 - 0.25, e1.max() / 1000.0 + 0.25)
    sub.set_ylim(n1.mean() / 1000.0 - 0.45, n1.mean() / 1000.0 + 0.45)
    sub.set_xticks([]); sub.set_yticks([])
    sub.set_facecolor("#FBFCFE")
    for sp in sub.spines.values():
        sp.set_color(C_SHIP)
        sp.set_linewidth(1.1)
    sub.set_title("표적 1 확대  (서쪽으로 1.8 km)", fontsize=7.6, color=C_SHIP, pad=3)

    ax = fig.add_subplot(gs[0, 1])
    ax.plot(t, col(rows, "t1_alt"), color=C_SHIP, lw=2.0)
    ax.plot(t, col(rows, "t2_alt"), color=C_AIR, lw=2.0)
    ax.set_ylim(-60, 420)
    ax.set_ylabel("고도 [m]", fontsize=9)
    ax.set_xlabel("시각 [s]", fontsize=9)
    ax.grid(True, lw=0.6)
    ax.tick_params(labelsize=8)
    ax.set_title("고도 — 60초 내내 평평하다", fontsize=9.8, color=NAVY, pad=6)

    ax = fig.add_subplot(gs[1, 1])
    ax.plot(t, col(rows, "t1_spd"), color=C_SHIP, lw=2.0)
    ax.plot(t, col(rows, "t2_spd"), color=C_AIR, lw=2.0)
    ax.set_ylim(-20, 260)
    ax.set_ylabel("속력 [m/s]", fontsize=9)
    ax.set_xlabel("시각 [s]", fontsize=9)
    ax.grid(True, lw=0.6)
    ax.tick_params(labelsize=8)
    ax.set_title("ECEF 속도 크기 — 넣은 값 그대로", fontsize=9.8, color=NAVY, pad=6)

    fig.savefig(os.path.join(OUT, "fig15_result_map.png"), dpi=DPI, bbox_inches="tight",
                pad_inches=0.08)
    plt.close(fig)
    print("  fig15_result_map.png")


# ═══════════════════════════════════════════════════════════════════════
# 16. 결과를 숫자로 검산
# ═══════════════════════════════════════════════════════════════════════
def fig16():
    rows = read_csv("spec.csv")
    A, E = 6378137.0, 0.08181919

    def lla2ecef(lat, lon, alt):
        la, lo = np.radians(lat), np.radians(lon)
        N = A / np.sqrt(1 - E * E * np.sin(la) ** 2)
        return np.column_stack([(N + alt) * np.cos(la) * np.cos(lo),
                                (N + alt) * np.cos(la) * np.sin(lo),
                                (N * (1 - E * E) + alt) * np.sin(la)])

    path = []
    for i in (1, 2):
        p = lla2ecef(col(rows, "t%d_lat" % i), col(rows, "t%d_lon" % i), col(rows, "t%d_alt" % i))
        path.append(float(np.sum(np.linalg.norm(np.diff(p, axis=0), axis=1))))

    fig, ax = canvas(11, 4.6)
    cols = ["검산 항목", "이론값 (손으로 계산)", "시뮬레이션 결과", "차이"]
    data = [
        ("대함 표적이 60초 동안 간 거리", "30 m/s × 60 s = 1,800 m", "%.5f m" % path[0],
         "%+.5f m" % (path[0] - 1800.0)),
        ("대공 표적이 60초 동안 간 거리", "200 m/s × 60 s = 12,000 m", "%.5f m" % path[1],
         "%+.5f m" % (path[1] - 12000.0)),
        ("대함 고도 (수평 서진)", "0 m 유지", "0.0000 ~ 0.0000 m", "0"),
        ("대공 고도 (수평 남진)", "300 m 유지", "300.0000 ~ 300.0000 m", "0"),
        ("대함 위도 (정서쪽으로만)", "32.125 ° 유지", "32.125000000 ° 고정", "0"),
        ("대공 경도 (정남쪽으로만)", "126.0 ° 유지", "126.000000000 ° 고정", "0"),
    ]
    xs = [3.0, 33.0, 58.0, 84.0]
    hdr_y, row_h = 88.0, 10.2
    ax.add_patch(Rectangle((2, hdr_y), 96, 9.0, facecolor=NAVY, edgecolor="none"))
    for x, c in zip(xs, cols):
        ax.text(x, hdr_y + 4.5, c, ha="left", va="center", fontsize=10, color=WHITE,
                fontweight="bold")
    for i, r in enumerate(data):
        y = hdr_y - (i + 1) * row_h
        if i % 2 == 0:
            ax.add_patch(Rectangle((2, y), 96, row_h, facecolor="#F7F9FC", edgecolor="none"))
        for j, (x, v) in enumerate(zip(xs, r)):
            ax.text(x, y + row_h / 2, v, ha="left", va="center", fontsize=9.6,
                    color=NAVY if j == 0 else (C_OK if j == 3 else SLATE),
                    fontweight="bold" if j == 3 else "normal",
                    family=MONO if j > 0 else KO)

    box(ax, 2, 4.0, 96, 16.0,
        "경로 길이는 0.1초마다 찍힌 601개 점 사이 거리를 전부 더해서 잰 값이다.\n"
        "12 km 를 날아 오차가 0.05 mm 수준이면, 남은 오차는 계산 방법이 아니라 double 형이 표현할 수 있는 한계에 가깝다.",
        fill=BG, edge=PALE, fc=SLATE, size=9.8, radius=0.5)
    save(fig, "fig16_verify.png")


# ═══════════════════════════════════════════════════════════════════════
# 17. 스텝 간격을 바꿔 보면 — 중점법의 차수 확인
# ═══════════════════════════════════════════════════════════════════════
def fig17():
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

    fig, axes = plt.subplots(1, 2, figsize=(11, 3.7), gridspec_kw={"width_ratios": [1.0, 1.05]})

    ax = axes[0]
    ax.loglog(dts, errs, "o-", color=C_SHIP, lw=2.0, ms=7, label="실제 오차")
    ref = errs[0] * (dts / dts[0]) ** 2
    ax.loglog(dts, ref, "--", color=MUTED, lw=1.5, label="간격² 에 비례하는 선")
    ax.axvline(0.1, color=C_WARN, lw=1.4, ls=(0, (4, 3)))
    ax.text(0.105, errs[0] * 0.6, "과제 조건\n0.1 s", fontsize=8.6, color=C_WARN,
            fontweight="bold", linespacing=1.5)
    ax.set_xlabel("갱신 간격 Δt [s]", fontsize=9.5)
    ax.set_ylabel("60초 뒤 위치 오차 [m]", fontsize=9.5)
    ax.grid(True, which="both", lw=0.6)
    ax.tick_params(labelsize=8.5)
    ax.legend(fontsize=8.4, framealpha=0.95)
    ax.set_title("Yaw 2 G 로 20초 선회하는 표적", fontsize=10.2, color=NAVY, pad=7)
    ax.xaxis.set_major_formatter(FuncFormatter(lambda v, _: ("%g" % v)))
    ax.xaxis.set_minor_formatter(NullFormatter())
    ax.set_xticks([0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0])
    ax.yaxis.set_major_formatter(FuncFormatter(
        lambda v, _: ("%g" % v) if v >= 0.01 else ("%.0e" % v).replace("e-0", "e-")))

    ax = axes[1]
    ax.axis("off")
    ax.set_xlim(0, 100); ax.set_ylim(0, 100)
    ax.text(0, 95, "읽는 법", fontsize=11.5, fontweight="bold", color=NAVY)
    lines = [
        ("두 선이 나란하다", "간격을 절반으로 줄이면 오차가 1/4 로 준다.\n중점법이 제대로 2차 정확도로 돌고 있다는 뜻.", C_SHIP),
        ("0.1초에서 1.4 cm", "20초를 2 G 로 꺾는 동안 쌓인 오차가 1.4 cm.\n레이다 거리 분해능(수 m)에 비하면 없는 값이다.", C_OK),
        ("직진이면 더 작다", "기동이 없을 때는 오차가 10 나노미터 수준 —\n사실상 계산 오차만 남는다.", C_WARN),
    ]
    for i, (h, d, c) in enumerate(lines):
        y = 76 - i * 27
        ax.add_patch(Rectangle((0, y - 11), 1.8, 19, facecolor=c, edgecolor="none"))
        ax.text(5, y + 4.5, h, fontsize=10.6, fontweight="bold", color=c)
        ax.text(5, y - 5, d, fontsize=9.2, color=SLATE, linespacing=1.6, va="center")

    fig.subplots_adjust(wspace=0.22)
    fig.savefig(os.path.join(OUT, "fig17_convergence.png"), dpi=DPI, bbox_inches="tight",
                pad_inches=0.08)
    plt.close(fig)
    print("  fig17_convergence.png")


# ═══════════════════════════════════════════════════════════════════════
# 18. 기동을 얹으면 — 지그재그 시연
# ═══════════════════════════════════════════════════════════════════════
def fig18():
    rows = read_csv("demo.csv")
    t = col(rows, "time")
    fig = plt.figure(figsize=(11, 4.2))
    gs = fig.add_gridspec(2, 2, width_ratios=[1.25, 1.0], hspace=0.55, wspace=0.26)

    ax = fig.add_subplot(gs[:, 0])
    _map_axes(ax, rows, 32.0, 126.0)
    ax.set_title("기동 시연 — 표적 2개에 기동 10줄", fontsize=10.5, color=NAVY, pad=8)
    ax.legend(fontsize=7.8, loc="lower center", framealpha=0.95, handlelength=1.4,
              borderpad=0.5, labelspacing=0.5)

    for i, (key, c, name, ax_pos) in enumerate([("t1_yaw", C_SHIP, "표적 1 대함", 0),
                                                ("t2_yaw", C_AIR, "표적 2 대공", 1)]):
        ax = fig.add_subplot(gs[ax_pos, 1])
        yaw = col(rows, key) % 360.0
        ax.plot(t, yaw, color=c, lw=2.0)
        ax.set_ylim(-20, 380)
        ax.set_yticks([0, 90, 180, 270, 360])
        ax.set_ylabel("기수 방위 [°]", fontsize=9)
        ax.set_xlabel("시각 [s]", fontsize=9)
        ax.grid(True, lw=0.6)
        ax.tick_params(labelsize=8)
        ax.set_title("%s — 계단이 곧 기동 구간" % name, fontsize=9.8, color=NAVY, pad=6)

    fig.savefig(os.path.join(OUT, "fig18_zigzag.png"), dpi=DPI, bbox_inches="tight",
                pad_inches=0.08)
    plt.close(fig)
    print("  fig18_zigzag.png")


# ═══════════════════════════════════════════════════════════════════════
# 19. TargetSim UI 화면 구성 (IDD_TARGETSIMUI_DIALOG 배치 그대로)
# ═══════════════════════════════════════════════════════════════════════
def fig19():
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
    data = [("플랫폼", "32.0", "126.0", "0", "0", "0", "0", "0", "", C_PF),
            ("표적 1", "32.125", "126.03", "0", "30", "0", "0", "270", "", C_SHIP),
            ("표적 2", "32.12", "126.0", "300", "200", "0", "0", "180", "", C_AIR)]
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

    for e, n, c in ((e1, n1, C_SHIP), (e2, n2, C_AIR)):
        X, Y = P(e, n)
        ax.plot(X, Y, color=c, lw=1.5, zorder=5)
        ax.plot(X[0], Y[0], "o", ms=4.5, mfc=WHITE, mec=c, mew=1.4, zorder=6)
        ax.plot(X[300], Y[300], "o", ms=5, color=c, zorder=7)
    X, Y = P(np.array([0.0]), np.array([0.0]))
    ax.plot(X, Y, "^", ms=7, color=C_PF, zorder=6)

    ax.text(366, 232, "고도 [m]", fontsize=6.4, color=SUB, va="center")
    ax.add_patch(Rectangle((px0, 236), pw, 44, facecolor="#FCFCFD", edgecolor=BORDER, lw=0.8, zorder=3))
    tt = col(rows, "time")
    a2 = col(rows, "t2_alt")
    ax.plot(px0 + tt / tt.max() * pw, 280 - a2 / 400.0 * 40, color=C_AIR, lw=1.4, zorder=5)
    ax.plot(px0 + tt / tt.max() * pw, 280 - col(rows, "t1_alt") / 400.0 * 40, color=C_SHIP,
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

    ax.text(14, 464, "601 표본 × 3 객체 생성 — 4.8 ms", fontsize=6.4, color=C_OK, va="center")

    fig.savefig(os.path.join(OUT, "fig19_ui.png"), dpi=190, bbox_inches="tight", pad_inches=0.05)
    plt.close(fig)
    print("  fig19_ui.png")


FIGS = {1: fig01, 2: fig02, 3: fig03, 4: fig04, 5: fig05, 6: fig06, 7: fig07,
        8: fig08, 9: fig09, 10: fig10, 11: fig11, 12: fig12, 13: fig13, 14: fig14,
        15: fig15, 16: fig16, 17: fig17, 18: fig18, 19: fig19}

if __name__ == "__main__":
    want = [int(a) for a in sys.argv[1:]] or sorted(FIGS)
    print("그림 생성 -> %s" % OUT)
    for n in want:
        if n not in FIGS:
            sys.exit("그림 번호 %d 없음 (1 ~ %d)" % (n, max(FIGS)))
        FIGS[n]()
    print("끝 (%d 장)" % len(want))
