# -*- coding: utf-8 -*-
"""TargetSimUI 의 [CSV 저장] 결과를 그림으로.

    python plot_trajectory.py <결과.csv> [-o 출력폴더] [--tag 이름]

<tag>_map.png : 평면 궤적 (경도 · 위도, 축 비율 1/cos(위도))
<tag>_alt.png : 고도 - 시간
"""
import argparse
import csv
import math
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import font_manager as fm
from matplotlib.ticker import MaxNLocator

FONT = fm.FontProperties(fname=r"C:\Windows\Fonts\malgun.ttf")
COLORS = ["#2B57A6", "#2F855A", "#C2521C", "#7B4FA3", "#B8860B", "#2A9D8F"]


def read(path):
    data = {}
    with open(path, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            d = data.setdefault(int(row["id"]), {"t": [], "lat": [], "lon": [], "alt": []})
            d["t"].append(float(row["t"]))
            d["lat"].append(float(row["lat_deg"]))
            d["lon"].append(float(row["lon_deg"]))
            d["alt"].append(float(row["alt_m"]))
    return data


def label(i):
    return "플랫폼" if i == 0 else "표적 %d" % i


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("-o", "--out", default=".")
    ap.add_argument("--tag", default="result")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    data = read(a.csv)

    # 평면 궤적. 축 비율 1/cos(위도) 를 지키면서 그림 상자를 채우도록 좁은 쪽 범위를 넓힌다
    fig = plt.figure(figsize=(5.0, 4.8), dpi=150)
    ax = fig.add_axes([0.17, 0.12, 0.79, 0.80])
    lons = [v for d in data.values() for v in d["lon"]]
    lats = [v for d in data.values() for v in d["lat"]]
    for i, d in sorted(data.items()):
        if i == 0:
            ax.plot(d["lon"][0], d["lat"][0], "^", ms=9, color=COLORS[0], label=label(i))
        else:
            ax.plot(d["lon"], d["lat"], color=COLORS[i % len(COLORS)], lw=2, label=label(i))
            ax.plot(d["lon"][0], d["lat"][0], "o", ms=5, color=COLORS[i % len(COLORS)])
    lat_c, lon_c = 0.5 * (min(lats) + max(lats)), 0.5 * (min(lons) + max(lons))
    asp = 1.0 / math.cos(math.radians(lat_c))
    lon_span = (max(lons) - min(lons)) * 1.3 + 0.01
    lat_span = (max(lats) - min(lats)) * 1.3 + 0.01
    bw, bh = 5.0 * 0.79, 4.8 * 0.80
    if lat_span * asp * bw / bh > lon_span:
        lon_span = lat_span * asp * bw / bh
    else:
        lat_span = lon_span / asp * bh / bw
    ax.set_xlim(lon_c - lon_span / 2, lon_c + lon_span / 2)
    ax.set_ylim(lat_c - lat_span / 2, lat_c + lat_span / 2)
    ax.set_aspect(asp)
    ax.ticklabel_format(useOffset=False)
    ax.xaxis.set_major_locator(MaxNLocator(5))
    ax.grid(color="#E6EBF3")
    ax.set_xlabel("경도 [deg]", fontproperties=FONT)
    ax.set_ylabel("위도 [deg]", fontproperties=FONT)
    ax.set_title("평면 궤적 (○ 시작)", fontproperties=FONT)
    ax.legend(prop=FONT, loc="lower left")
    fig.savefig(os.path.join(a.out, a.tag + "_map.png"))

    fig, ax = plt.subplots(figsize=(5.6, 3.2), dpi=150)
    for i, d in sorted(data.items()):
        if i:
            ax.plot(d["t"], d["alt"], color=COLORS[i % len(COLORS)], lw=2, label=label(i))
    ax.grid(color="#E6EBF3")
    ax.set_xlabel("시간 [s]", fontproperties=FONT)
    ax.set_ylabel("고도 [m]", fontproperties=FONT)
    ax.set_title("고도 - 시간", fontproperties=FONT)
    ax.legend(prop=FONT)
    fig.tight_layout()
    fig.savefig(os.path.join(a.out, a.tag + "_alt.png"))
    print("saved:", a.out)


if __name__ == "__main__":
    main()
