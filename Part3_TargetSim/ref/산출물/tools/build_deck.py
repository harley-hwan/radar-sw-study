# -*- coding: utf-8 -*-
"""Chapter 3 발표자료(.pptx)를 만든다.

    python build_deck.py              # ../Chapter3_TargetSim.pptx 생성
    python build_deck.py <출력.pptx>

회사 템플릿(ref/템플릿/Presentation_Template_Blank.pptx)의 색 · 글꼴 · 배치를 그대로 쓴다.
표지와 간지 배경은 템플릿에서 꺼낸 그림(figures/bg_cover.png)이다.
슬라이드 노트는 여기서 넣지 않는다 — 대본 md 를 원본으로 두고 sync_notes.py 가 밀어 넣는다.
"""
import os
import sys

from PIL import Image
from pptx import Presentation
from pptx.dml.color import RGBColor
from pptx.enum.shapes import MSO_SHAPE
from pptx.enum.text import MSO_ANCHOR, MSO_AUTO_SIZE, PP_ALIGN
from pptx.util import Emu, Inches, Pt

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.normpath(os.path.join(HERE, ".."))
FIG = os.path.join(OUT_DIR, "figures")
TEMPLATE = os.path.normpath(os.path.join(HERE, "..", "..", "..", "..", "ref", "템플릿",
                                         "Presentation_Template_Blank.pptx"))
DECK = os.path.join(OUT_DIR, "Chapter3_TargetSim.pptx")

# ── 템플릿에서 뽑은 색 ──────────────────────────────────────────────────
NAVY = RGBColor(0x1B, 0x27, 0x40)
NAVY2 = RGBColor(0x1E, 0x2C, 0x4F)
SLATE = RGBColor(0x4A, 0x5A, 0x7A)
MUTED = RGBColor(0x87, 0x94, 0xAC)
LIGHT = RGBColor(0xAE, 0xB9, 0xCC)
PALE = RGBColor(0xD2, 0xD9, 0xE5)
BG = RGBColor(0xED, 0xF0, 0xF6)
WHITE = RGBColor(0xFF, 0xFF, 0xFF)
BLUE = RGBColor(0x25, 0x63, 0xEB)
RED = RGBColor(0xDC, 0x26, 0x26)
GREEN = RGBColor(0x05, 0x96, 0x05 + 0x64)
OK = RGBColor(0x05, 0x96, 0x69)
AMBER = RGBColor(0xD9, 0x77, 0x06)
VIOLET = RGBColor(0x7C, 0x3A, 0xED)
CODE_BG = RGBColor(0xF7, 0xF9, 0xFC)

KO = "맑은 고딕"
MONO = "Consolas"

SW, SH = 13.333, 7.5
MX, MW = 0.70, 11.93            # 본문 좌우 여백과 폭 (템플릿 머리말 띠와 같은 자리)
HEAD_Y, HEAD_H = 0.62, 0.62
FOOT_Y = 6.92

TITLE = "Part 3  표적 궤적 모의"
AUTHOR = "Janghwan Kim (Harley)"

prs = Presentation(TEMPLATE)
prs.slide_width = Inches(SW)
prs.slide_height = Inches(SH)

# 템플릿에 들어 있던 예시 17장을 지운다. 레이아웃(DEFAULT)과 테마는 그대로 남는다.
_ids = prs.slides._sldIdLst
for _sld in list(_ids):
    prs.part.drop_rel(_sld.rId)
    _ids.remove(_sld)
BLANK = prs.slide_layouts[0]

_page = 0
_titles = []


# ── 낱개 도구 ───────────────────────────────────────────────────────────
def txt(slide, x, y, w, h, text, size=12, color=NAVY, bold=False, font=KO,
        align=PP_ALIGN.LEFT, anchor=MSO_ANCHOR.TOP, space=1.0, wrap=True):
    tb = slide.shapes.add_textbox(Inches(x), Inches(y), Inches(w), Inches(h))
    tf = tb.text_frame
    tf.word_wrap = wrap
    tf.auto_size = MSO_AUTO_SIZE.NONE
    tf.margin_left = tf.margin_right = tf.margin_top = tf.margin_bottom = 0
    tf.vertical_anchor = anchor
    for i, line in enumerate(str(text).split("\n")):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.alignment = align
        p.line_spacing = space
        r = p.add_run()
        r.text = line
        r.font.size = Pt(size)
        r.font.bold = bold
        r.font.name = font
        r.font.color.rgb = color
    return tb


def rect(slide, x, y, w, h, fill=None, line=None, lw=1.0, shape=MSO_SHAPE.RECTANGLE):
    sh = slide.shapes.add_shape(shape, Inches(x), Inches(y), Inches(w), Inches(h))
    sh.shadow.inherit = False
    if fill is None:
        sh.fill.background()
    else:
        sh.fill.solid()
        sh.fill.fore_color.rgb = fill
    if line is None:
        sh.line.fill.background()
    else:
        sh.line.color.rgb = line
        sh.line.width = Pt(lw)
    sh.text_frame.word_wrap = True
    return sh


def hline(slide, x, y, w, color=PALE, lw=0.75):
    ln = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, Inches(x), Inches(y), Inches(w), Emu(0))
    ln.shadow.inherit = False
    ln.fill.background()
    ln.line.color.rgb = color
    ln.line.width = Pt(lw)
    return ln


def picture(slide, name, box, align="center"):
    """box = (x, y, w, h). 그림 비율을 지키며 상자 안에 넣는다."""
    path = os.path.join(FIG, name)
    iw, ih = Image.open(path).size
    bx, by, bw, bh = box
    s = min(bw / iw, bh / ih)
    w, h = iw * s, ih * s
    x = bx + (bw - w) / 2 if align == "center" else bx
    y = by + (bh - h) / 2
    return slide.shapes.add_picture(path, Inches(x), Inches(y), Inches(w), Inches(h))


# ── 쪽 틀 ───────────────────────────────────────────────────────────────
def new_slide():
    return prs.slides.add_slide(BLANK)


def content(chapter, heading, lead=None):
    """본문 쪽 — 머리말 띠 + 제목 + (한 줄 설명) + 꼬리말. 본문 상자의 y 를 돌려준다."""
    global _page
    _page += 1
    slide = new_slide()
    _titles.append(heading)

    rect(slide, MX, HEAD_Y, MW, HEAD_H, fill=NAVY)
    txt(slide, MX + 0.28, HEAD_Y, MW - 0.56, HEAD_H, chapter, size=13.5, color=WHITE,
        bold=True, anchor=MSO_ANCHOR.MIDDLE)

    txt(slide, MX + 0.25, 1.36, MW - 0.5, 0.44, heading, size=20, color=NAVY, bold=True)
    y = 1.94
    if lead:
        txt(slide, MX + 0.25, 1.90, MW - 0.5, 0.30, lead, size=11.5, color=SLATE)
        y = 2.34

    hline(slide, MX, FOOT_Y, MW, PALE)
    txt(slide, 6.07, 6.98, 1.20, 0.30, str(_page), size=9, color=MUTED,
        align=PP_ALIGN.CENTER, font="Arial")
    return slide, y


def cover():
    global _page
    _page += 1
    slide = new_slide()
    _titles.append("표지")
    slide.shapes.add_picture(os.path.join(FIG, "bg_cover.png"), 0, 0,
                             Inches(SW), Inches(SH))
    txt(slide, 9.00, 0.72, 3.30, 0.30, "Confidential", size=10, color=WHITE, bold=True,
        font="Arial", align=PP_ALIGN.RIGHT)
    txt(slide, 4.60, 2.74, 7.40, 0.50, "레이다 SW 학습 과제 3", size=14, color=LIGHT,
        align=PP_ALIGN.RIGHT)
    txt(slide, 4.60, 3.22, 7.40, 0.75, "표적 궤적 모의", size=38, color=WHITE, bold=True,
        align=PP_ALIGN.RIGHT)
    hline(slide, 8.60, 4.28, 3.40, LIGHT)
    txt(slide, 4.60, 4.44, 7.40, 0.32, "Target Trajectory Simulation", size=13,
        color=LIGHT, font="Arial", align=PP_ALIGN.RIGHT)
    txt(slide, 4.60, 5.60, 7.40, 0.32, AUTHOR, size=12, color=WHITE,
        align=PP_ALIGN.RIGHT)
    return slide


def section(no, ko, en, points):
    global _page
    _page += 1
    slide = new_slide()
    _titles.append("%s. %s" % (no, ko))
    slide.shapes.add_picture(os.path.join(FIG, "bg_cover.png"), 0, 0,
                             Inches(SW), Inches(SH))
    hline(slide, 0.82, 0.78, 11.70, SLATE)
    hline(slide, 0.82, 6.93, 11.70, SLATE)
    txt(slide, 0.94, 2.55, 3.00, 1.10, no, size=64, color=SLATE, bold=True, font="Arial")
    txt(slide, 2.60, 2.72, 8.00, 0.66, ko, size=32, color=WHITE, bold=True)
    txt(slide, 2.66, 3.50, 8.00, 0.34, en, size=13, color=LIGHT, font="Arial")
    for i, p in enumerate(points):
        txt(slide, 2.66, 4.30 + i * 0.42, 8.60, 0.34, "·   " + p, size=12.5, color=LIGHT)
    txt(slide, 6.07, 6.98, 1.20, 0.30, str(_page), size=9, color=MUTED,
        align=PP_ALIGN.CENTER, font="Arial")
    return slide


# ── 본문에서 자주 쓰는 덩어리 ───────────────────────────────────────────
def card(slide, x, y, w, h, title, body, accent=BLUE, tsize=12.5, bsize=10.5):
    rect(slide, x, y, w, h, fill=WHITE, line=PALE, lw=1.0)
    rect(slide, x, y, 0.055, h, fill=accent)
    txt(slide, x + 0.30, y + 0.20, w - 0.55, 0.30, title, size=tsize, color=accent, bold=True)
    txt(slide, x + 0.30, y + 0.58, w - 0.55, h - 0.72, body, size=bsize, color=SLATE,
        space=1.28)


def code(slide, x, y, w, h, lines, size=10.0, lead=1.30, title=None):
    """소스 코드 상자. lines 는 (글자, 종류) 목록 — 종류: '', 'c'(주석), 'k'(강조)."""
    rect(slide, x, y, w, h, fill=CODE_BG, line=PALE, lw=1.0)
    if title:
        txt(slide, x + 0.22, y + 0.14, w - 0.44, 0.26, title, size=9.5, color=MUTED,
            font="Arial")
        y0 = y + 0.50
    else:
        y0 = y + 0.20
    tb = slide.shapes.add_textbox(Inches(x + 0.22), Inches(y0), Inches(w - 0.44),
                                  Inches(h - (y0 - y) - 0.16))
    tf = tb.text_frame
    tf.word_wrap = True
    tf.auto_size = MSO_AUTO_SIZE.NONE
    tf.margin_left = tf.margin_right = tf.margin_top = tf.margin_bottom = 0
    for i, item in enumerate(lines):
        text, kind = item if isinstance(item, tuple) else (item, "")
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.line_spacing = lead
        p.alignment = PP_ALIGN.LEFT
        r = p.add_run()
        r.text = text if text else " "
        r.font.size = Pt(size)
        r.font.name = MONO
        if kind == "c":
            r.font.color.rgb = MUTED
        elif kind == "k":
            r.font.color.rgb = BLUE
            r.font.bold = True
        elif kind == "r":
            r.font.color.rgb = RED
            r.font.bold = True
        else:
            r.font.color.rgb = NAVY
    return tb


def banner(slide, x, y, w, h, text, fill=NAVY, color=WHITE, size=12):
    rect(slide, x, y, w, h, fill=fill)
    txt(slide, x + 0.30, y, w - 0.60, h, text, size=size, color=color, bold=True,
        anchor=MSO_ANCHOR.MIDDLE, space=1.3)


def table(slide, x, y, w, cols, rows, widths, hsize=10.5, bsize=10.5, rh=0.42):
    """가벼운 표 — 머리줄은 남색 띠, 본문은 줄무늬."""
    rect(slide, x, y, w, rh, fill=NAVY)
    cx = x
    for c, cw in zip(cols, widths):
        txt(slide, cx + 0.16, y, cw - 0.2, rh, c, size=hsize, color=WHITE, bold=True,
            anchor=MSO_ANCHOR.MIDDLE)
        cx += cw
    for i, row in enumerate(rows):
        ry = y + rh + i * rh
        if i % 2 == 0:
            rect(slide, x, ry, w, rh, fill=BG)
        cx = x
        for j, (v, cw) in enumerate(zip(row, widths)):
            bold = j == 0
            col = NAVY if j == 0 else SLATE
            fnt = KO
            if v.startswith("~"):
                v, col, fnt, bold = v[1:], OK, MONO, True
            elif v.startswith("`"):
                v, fnt = v[1:], MONO
            txt(slide, cx + 0.16, ry, cw - 0.2, rh, v, size=bsize, color=col, bold=bold,
                font=fnt, anchor=MSO_ANCHOR.MIDDLE)
            cx += cw
    return y + rh + len(rows) * rh


CH0 = "들어가며"
CH1 = "01. 표적 구조체 설계"
CH2 = "02. 표적 궤적 모의 구현"
CH3 = "03. 시뮬레이션 결과 및 검증"


def body_box(y, bottom=6.78):
    return (MX, y, MW, bottom - y)


# ═══════════════════════════════════════════════════════════════════════
# 1. 표지
# ═══════════════════════════════════════════════════════════════════════
cover()

# ═══════════════════════════════════════════════════════════════════════
# 2. 목차
# ═══════════════════════════════════════════════════════════════════════
_page += 1
s = new_slide()
_titles.append("목차")
rect(s, 0, 0, 4.60, SH, fill=NAVY)
txt(s, 0.85, 0.55, 2.00, 0.45, "INDEX", size=26, color=WHITE, bold=True, font="Arial")
hline(s, 2.30, 0.79, 2.30, SLATE)
hline(s, 4.60, 0.79, 7.95, PALE)
txt(s, 0.50, 3.00, 3.60, 1.30, "표적 궤적 모의\nTargetSim", size=18, color=WHITE,
    bold=True, align=PP_ALIGN.CENTER, space=1.45)
txt(s, 0.50, 6.60, 3.60, 0.30, AUTHOR, size=10, color=LIGHT, align=PP_ALIGN.CENTER)
_idx = [("01", "표적 구조체 설계", "상태 벡터와 좌표계\n기동을 G · 축 · 구간으로", "5 ~ 9"),
        ("02", "표적 궤적 모의 구현", "운동학 모델과 회전행렬\n속도 변환과 중점법 적분", "10 ~ 14"),
        ("03", "시뮬레이션 결과 및 검증", "명세 조건 60 s 결과\n세 갈래 검증과 시연", "15 ~ 18")]
for i, (n, t, d, pg) in enumerate(_idx):
    y = 1.85 + i * 1.62
    txt(s, 5.25, y, 1.00, 0.55, n, size=30, color=NAVY, bold=True, font="Arial")
    txt(s, 6.40, y + 0.10, 3.10, 0.40, t, size=15, color=NAVY, bold=True)
    txt(s, 9.75, y - 0.02, 2.40, 0.80, d, size=10, color=SLATE, space=1.42)
    txt(s, 12.20, y + 0.12, 0.45, 0.30, pg, size=9.5, color=MUTED, font="Arial",
        align=PP_ALIGN.RIGHT)
    if i < 2:
        hline(s, 5.25, y + 1.26, 7.35, PALE)
txt(s, 8.37, 6.98, 1.20, 0.30, str(_page), size=9, color=MUTED, align=PP_ALIGN.CENTER,
    font="Arial")

# ═══════════════════════════════════════════════════════════════════════
# 3. 과제 요구사항
# ═══════════════════════════════════════════════════════════════════════
s, y = content(CH0, "과제가 요구한 것",
               "「과제 3 내용」의 요구사항을 네 덩어리로 묶고, 발표도 그 순서로 갑니다.")
_req = [("1  표적 구조체 정의",
         "표적 최대 10 · 기동 최대 30.\n위치는 위도 · 경도 · 고도,\n속도는 동체 속도 한 개.\n기동은 G · 축 · 시작 · 종료.", "01 장", BLUE),
        ("2  표적 상태 갱신",
         "위치와 속도를 ECEF 에서 갱신.\n추가 고민 — 동체 속도를 ECEF\nVx, Vy, Vz 로 바꾸는 방법.", "02 장", AMBER),
        ("3  시뮬레이션 조건",
         "60 s · 0.1 s 간격.\n플랫폼 1 + 대함 1 + 대공 1.\n제공받은 좌표변환 코드 사용.", "03 장", VIOLET),
        ("4  결과 제시",
         "궤적을 LLA 로 출력하고 그림으로.\n구조체 · 구현 · 결과를\n코드와 함께 설명.", "03 장", OK)]
for i, (t, d, ch, c) in enumerate(_req):
    card(s, MX + i * 3.03, y + 0.18, 2.80, 2.62, t, d, accent=c, bsize=10)
    txt(s, MX + i * 3.03 + 0.30, y + 2.46, 2.20, 0.26, ch, size=9, color=c, bold=True)
banner(s, MX, y + 3.12, MW, 1.02,
       "표적 모의는 실장비 없이 데이터처리 입력을 공급하는 일입니다 — 같은 규격, 반복 가능, 그리고 내가 넣은 값이 곧 참값입니다.\n"
       "좌표변환과 행렬 연산은 제공받은 코드를 무수정으로 썼고, 새로 작성한 것은 TargetSim.c 하나입니다.")

# ═══════════════════════════════════════════════════════════════════════
# 4. 소프트웨어 구성
# ═══════════════════════════════════════════════════════════════════════
s, y = content(CH0, "만든 것 — 계산과 화면을 나눈다",
               "연산 엔진만 따로 빌드해 검증할 수 있게 했습니다. 오늘 숫자는 모두 그렇게 뽑은 것입니다.")
picture(s, "fig02_system.png", body_box(y))

# ═══════════════════════════════════════════════════════════════════════
# 5. 상태 벡터와 좌표계
# ═══════════════════════════════════════════════════════════════════════
s, y = content(CH1, "표적의 상태와 좌표계",
               "표적 하나는 아홉 개 성분으로 정해집니다. 그 값들이 어느 좌표계의 값인지부터 맞춥니다.")
picture(s, "figA_state_frames.png", body_box(y))

# ═══════════════════════════════════════════════════════════════════════
# 6. 기동 정의와 G
# ═══════════════════════════════════════════════════════════════════════
s, y = content(CH1, "기동을 네 개의 값으로",
               "\"12 초부터 16 초까지 오른쪽으로 8 G\" 라는 지시가 구조체 한 줄이 되고, G 하나가 회전 속도를 정합니다.")
picture(s, "figB_maneuver_g.png", body_box(y))

# ═══════════════════════════════════════════════════════════════════════
# 7. 회전축 세 가지 응답
# ═══════════════════════════════════════════════════════════════════════
s, y = content(CH1, "회전축 세 가지의 응답 — 실측",
               "같은 표적에 같은 2 G 를 10 초씩, 축만 바꿔 걸어 본 결과입니다.")
picture(s, "fig08_turntype.png", body_box(y))

# ═══════════════════════════════════════════════════════════════════════
# 8. 구조체 관계
# ═══════════════════════════════════════════════════════════════════════
s, y = content(CH1, "구조체 관계와 설계 결정")
picture(s, "fig09_struct.png", body_box(y, 6.80))

# ═══════════════════════════════════════════════════════════════════════
# 9. 코드로 보는 구조체 (입력 + 출력)
# ═══════════════════════════════════════════════════════════════════════
s, y = content(CH1, "코드로 보는 구조체")
code(s, MX, y + 0.02, 5.92, 3.52, [
    ("// 기동 1건. gravityValue 의 부호가 회전 방향이다.", "c"),
    ("typedef struct", "k"),
    "{",
    "    EN_TurnType   enTurnType;    // 0~3",
    "    FLOAT64       gravityValue;  // [G]",
    "    FLOAT64       startTime;     // [s]",
    "    FLOAT64       endTime;       // [s]",
    "} ST_TargetManeuver;",
    "",
    ("// 위치는 위/경/고도, 속도는 동체 x 축 방향 크기.", "c"),
    ("typedef struct", "k"),
    "{",
    "    STRUCT_Coord_Lla       st_InitLla;",
    "    STRUCT_Coord_Attitude  st_InitAtt;",
    ("    FLOAT64                headingSpeed;   // V_heading", "k"),
    "    INT32                  nManeuverNum;",
    "    ST_TargetManeuver      st_Maneuver[TGT_MAX_MANEUVER_NUM];",
    "} ST_TargetInit;",
], size=8.4, lead=1.14, title="입력 — TargetSim.h")
code(s, MX + 6.10, y + 0.02, 5.83, 3.52, [
    ("// 위치 · 속도는 ECEF 에서 갱신하고, LLA 는 출력과", "c"),
    ("// 다음 스텝의 NED 기준으로 함께 보관한다.", "c"),
    ("typedef struct", "k"),
    "{",
    "    FLOAT64                simTime;      // [s]",
    ("    STRUCT_Coord_Rect      st_PosEcef;   // [m]", "k"),
    ("    STRUCT_Coord_Rect      st_VelEcef;   // [m/s]", "k"),
    "    STRUCT_Coord_Lla       st_Lla;",
    "    STRUCT_Coord_Attitude  st_Att;",
    "} ST_TargetState;",
    "",
    ("// 한 시각의 플랫폼 · 표적 상태.", "c"),
    ("typedef struct", "k"),
    "{",
    "    FLOAT64         simTime;",
    "    INT32           nStepIndex;",
    "    ST_TargetState  st_Platform;",
    "    ST_TargetState  st_Target[TGT_MAX_TARGET_NUM];",
    "} ST_SimSample;",
], size=8.4, lead=1.14, title="출력 — TargetSim.h")
card(s, MX, y + 3.68, 3.85, 1.10, "구간은 [시작, 종료)",
     "끝과 다음 시작이 같아도 겹침이 아니다.\n실제로 겹치면 계산 전에 거부한다.", accent=BLUE,
     bsize=9.6)
card(s, MX + 4.05, y + 3.68, 3.85, 1.10, "ECEF 와 LLA 를 함께",
     "갱신은 ECEF, 출력은 LLA, 다음 스텝의\nNED 기준도 LLA — 한 번만 변환해 둔다.",
     accent=AMBER, bsize=9.6)
card(s, MX + 8.10, y + 3.68, 3.83, 1.10, "플랫폼도 같은 구조체",
     "기동이 없는 표적일 뿐이라 각속도만 0 으로\n넣어 같은 경로로 전진시킨다.", accent=OK,
     bsize=9.6)

# ═══════════════════════════════════════════════════════════════════════
# 10. 운동학 모델과 ECEF
# ═══════════════════════════════════════════════════════════════════════
s, y = content(CH2, "운동학 모델과 ECEF 적분",
               "구현을 시작하기 전에 방정식부터 세우고, 어느 좌표계에서 적분할지를 정합니다.")
picture(s, "figC_model_ecef.png", body_box(y))

# ═══════════════════════════════════════════════════════════════════════
# 11. 속도 변환과 회전행렬
# ═══════════════════════════════════════════════════════════════════════
s, y = content(CH2, "동체 속도를 ECEF 속도로 — 과제의 추가 고민",
               "\"동체 속도 값(V_heading)을 ECEF 좌표계 Vx, Vy, Vz 성분으로 변환하는 방법\" 에 대한 답입니다.")
picture(s, "figD_velocity_dcm.png", body_box(y))

# ═══════════════════════════════════════════════════════════════════════
# 12. 코드 — GetVelEcef
# ═══════════════════════════════════════════════════════════════════════
s, y = content(CH2, "코드 ① — 동체 속도를 ECEF 속도로")
code(s, MX, y + 0.05, 12.63 - MX, 3.20, [
    ("// 동체 속도 (V, 0, 0) 을 자세로 한 번, 위경도로 한 번 돌려 ECEF 속도로 만든다.", "c"),
    ("// 회전만 거치므로 속력은 그대로 보존된다.", "c"),
    ("static STRUCT_Coord_Rect f_Tgt_GetVelEcef(const STRUCT_Coord_Attitude *st_Att,", "k"),
    "        const STRUCT_Coord_Lla *st_Lla, FLOAT64 headingSpeed)",
    "{",
    "    STRUCT_Coord_Rect  st_VelNed;",
    "",
    ("    // 참고 소스의 f_Trans_Body_To_Ned 는 인자를 Roll, Yaw, Pitch 순서로 받는다.", "c"),
    ("    st_VelNed = f_Trans_Body_To_Ned(headingSpeed, 0.0, 0.0,", "k"),
    "            st_Att->Roll, st_Att->Yaw, st_Att->Pitch);",
    "",
    ("    // f_Trans_Ned_To_Ecef 는 기준점 이동까지 더한다. 방향만 옮길 때는 기준점을 원점으로 준다.", "c"),
    ("    return f_Trans_Ned_To_Ecef(st_VelNed.x, st_VelNed.y, st_VelNed.z,", "k"),
    "            0.0, 0.0, 0.0, st_Lla->Lat, st_Lla->Lon);",
    "}",
], size=9.4, lead=1.16, title="TargetSimCore/src/TargetSim.c")
card(s, MX, y + 3.38, 3.85, 1.42, "기준점을 0 으로 주는 이유",
     "f_Trans_Ned_To_Ecef 는 회전에 더해\n기준점만큼 평행이동까지 한다. 속도는\n\"위치\"가 아니라 \"방향\"이라 빼야 한다.",
     accent=RED, bsize=9.5)
card(s, MX + 4.05, y + 3.38, 3.85, 1.42, "인자 순서를 주석에 남긴 이유",
     "제공받은 함수가 Roll, Yaw, Pitch 순서로\n받는다. 자세각 구조체 필드 순서와 달라\n한 번 틀렸던 자리다.",
     accent=AMBER, bsize=9.5)
card(s, MX + 8.10, y + 3.38, 3.83, 1.42, "속력 보존은 성질, 시험은 확인",
     "회전행렬은 직교행렬이라 크기를 바꾸지\n않는다. 601 표본에서 속력이 소수점\n12 자리까지 입력값과 일치했다.",
     accent=OK, bsize=9.5)

# ═══════════════════════════════════════════════════════════════════════
# 13. 중점법
# ═══════════════════════════════════════════════════════════════════════
s, y = content(CH2, "0.1 초를 어떻게 전진시킬 것인가",
               "어느 지점의 기울기를 쓰느냐가 오차 차수를 가릅니다.")
picture(s, "fig14_midpoint.png", body_box(y))

# ═══════════════════════════════════════════════════════════════════════
# 14. 코드 — Propagate
# ═══════════════════════════════════════════════════════════════════════
s, y = content(CH2, "코드 ② — 한 스텝 전진 (중점법)")
code(s, MX, y + 0.05, 7.62, 4.70, [
    ("    halfStep = 0.5 * stepTime;", ""),
    "",
    ("    // ① 반 스텝 뒤의 자세 — 각속도가 구간 내 상수라 바로 더한다", "c"),
    "    st_AttMid.Roll  = st_State->st_Att.Roll  + (st_Rate->Roll  * halfStep);",
    "    st_AttMid.Pitch = st_State->st_Att.Pitch + (st_Rate->Pitch * halfStep);",
    "    st_AttMid.Yaw   = st_State->st_Att.Yaw   + (st_Rate->Yaw   * halfStep);",
    "",
    ("    // ② 반 스텝 뒤의 위치 — 스텝 시작 속도로 예측", "c"),
    "    st_VelStart = f_Tgt_GetVelEcef(&st_State->st_Att, &st_State->st_Lla, headingSpeed);",
    "    st_PosMid.x = st_State->st_PosEcef.x + (st_VelStart.x * halfStep);",
    ("    ...", "c"),
    "",
    ("    // ③ 그 자리 · 그 자세에서 구한 속도가 이 스텝의 대표 속도", "c"),
    ("    st_LlaMid = f_Trans_Ecef_To_Lla(st_PosMid.x, st_PosMid.y, st_PosMid.z);", "k"),
    ("    st_VelMid = f_Tgt_GetVelEcef(&st_AttMid, &st_LlaMid, headingSpeed);", "k"),
    "",
    ("    // ④ 그 속도로 한 스텝을 옮긴다", "c"),
    ("    st_Next.st_PosEcef.x = st_State->st_PosEcef.x + (st_VelMid.x * stepTime);", "k"),
    ("    ...", "c"),
], size=8.8, lead=1.14, title="TargetSimCore/src/TargetSim.c   f_Tgt_Propagate (발췌)")
card(s, 8.52, y + 0.05, 4.11, 1.90, "② 를 빼먹었을 때 생긴 일",
     "자세만 반 스텝 돌리고 위치를 그대로 두면,\n표적은 앞으로 나아갔는데 \"북쪽\" 의 방향은\n"
     "뒤에 남는다. 그 차이가 매 스텝 d²/2R 만큼\n고도로 쌓인다.", accent=RED, bsize=9.2)
card(s, 8.52, y + 2.10, 4.11, 1.32, "계산이 깨져도 상태는 지킨다",
     "사본에서 전부 계산하고 유한성을 확인한 뒤\n모두 성공했을 때만 한 번에 반영한다.",
     accent=BLUE, bsize=9.2)
card(s, 8.52, y + 3.57, 4.11, 1.18, "알고도 남겨 둔 한계 — 짐벌락",
     "Pitch 가 ±90° 근처면 Yaw 축이 속도 방향과\n겹쳐 Yaw 기동이 궤적을 바꾸지 못한다.",
     accent=AMBER, bsize=9.2)

# ═══════════════════════════════════════════════════════════════════════
# 15. 시뮬레이션 조건
# ═══════════════════════════════════════════════════════════════════════
s, y = content(CH3, "시뮬레이션 조건", "「과제 3 내용」 2항의 값을 그대로 넣었습니다.")
end = table(s, MX, y + 0.08, MW,
            ["", "위도 [°]", "경도 [°]", "고도 [m]", "속력 [m/s]", "Roll [°]", "Pitch [°]", "Yaw [°]"],
            [["플랫폼", "32.0", "126.0", "0", "0", "0", "0", "0"],
             ["대함 표적", "32.125", "126.03", "0", "30", "0", "0", "270  (서)"],
             ["대공 표적", "32.12", "126.0", "300", "200", "0", "0", "180  (남)"]],
            [2.13, 1.40, 1.40, 1.40, 1.40, 1.40, 1.40, 1.40], rh=0.44)
card(s, MX, end + 0.30, 3.85, 1.42, "시간 60 s · 간격 0.1 s",
     "스텝 600 회, 표본 601 개.\n시각은 누적이 아니라 스텝 번호 × 간격으로\n"
     "매겨 반올림 오차가 쌓이지 않게 했다.", accent=BLUE, bsize=9.4)
card(s, MX + 4.05, end + 0.30, 3.85, 1.42, "검증하기 좋은 조건이다",
     "기동이 없으므로 답을 미리 알 수 있다.\n"
     "대함은 정서진 → 위도 불변,\n대공은 정남진 → 경도 불변, 둘 다 고도 불변.",
     accent=OK, bsize=9.4)
card(s, MX + 8.10, end + 0.30, 3.83, 1.42, "플랫폼 속력이 0 인 점",
     "각속도 계산에 속력이 분모로 들어가지만,\n그 계산은 기동 조회 안에만 있다.\n"
     "플랫폼은 기동이 없어 그 자리를 지나가지 않는다.", accent=AMBER, bsize=9.4)

# ═══════════════════════════════════════════════════════════════════════
# 16. 결과 ① 궤적
# ═══════════════════════════════════════════════════════════════════════
s, y = content(CH3, "결과 ① — 60 초 궤적",
               "플랫폼을 원점으로 잡은 동-북 평면입니다. 점 하나가 10 초입니다.")
picture(s, "fig17_result_map.png", body_box(y))

# ═══════════════════════════════════════════════════════════════════════
# 17. 결과 ② 검증 종합
# ═══════════════════════════════════════════════════════════════════════
s, y = content(CH3, "결과 ② — 세 가지 방법으로 검증",
               "각각이 다른 종류의 실수를 잡습니다 — 스케일, 방향, 그리고 적분 방법.")
picture(s, "figF_verify.png", body_box(y))

# ═══════════════════════════════════════════════════════════════════════
# 18. 결과 ③ 기동 시연과 화면
# ═══════════════════════════════════════════════════════════════════════
s, y = content(CH3, "결과 ③ — 기동 시연과 운용 화면",
               "초기값 · 시간 · 간격은 명세 그대로, 기동표 10줄만 추가했습니다.")
picture(s, "figG_demo_ui.png", body_box(y))

# ═══════════════════════════════════════════════════════════════════════
# 19. 정리
# ═══════════════════════════════════════════════════════════════════════
s, y = content(CH3, "정리", "요구사항과 결과를 한 장에 놓고 보겠습니다.")
end = table(s, MX, y + 0.05, MW,
            ["과제 요구사항", "구현", "근거"],
            [["표적 10 · 기동 30", "고정 배열로 상한, 범위 밖은 오류 코드", "~범위 검사"],
             ["위치 LLA · 속도 V_heading", "ST_TargetInit, 방향은 자세각이 담당", "~검증 통과"],
             ["기동 = G · 축 · 시작 · 종료", "ST_TargetManeuver 4 필드, 구간 겹침 검사", "~검증 통과"],
             ["ECEF 에서 상태 갱신", "위치 · 속도 ECEF, 출력용 LLA 병행 보관", "~고도 잔차 30 µm"],
             ["V_heading → Vx, Vy, Vz  (추가 고민)", "동체 → NED → ECEF, 회전 2회", "~속력 완전 보존"],
             ["제공 좌표변환 코드 사용", "CoordinateTransform.c / matrixCalcLib.c 원본", "~무수정"],
             ["결과를 LLA 로 출력하고 그림으로", "표 · CSV · 궤적 · 고도 · 방위 그림", "~601 표본 출력"]],
            [4.95, 5.40, 1.58], rh=0.365, bsize=9.8, hsize=10)
card(s, MX, end + 0.26, 5.85, 1.26, "남겨 둔 한계",
     "· Pitch ±90° 근처의 짐벌락 — 사원수로 전환하면 해소\n"
     "· 힘을 풀지 않는 운동학 모델 — 탄도 표적에는 부적합\n"
     "· 고도 잔차 30.8 µm 는 제공 코드의 ECEF→LLA 반복 근사",
     accent=AMBER, bsize=9.6)
card(s, MX + 6.08, end + 0.26, 5.85, 1.26, "이어 붙일 수 있는 것",
     "· 궤적에 측정 잡음을 얹으면 그대로 추적 필터 입력\n"
     "· Part 2 의 좌표변환을 붙이면 안테나 기준 R · Az · El\n"
     "· 표본 생성기를 바꾸면 RCS · 도플러까지 확장 가능",
     accent=BLUE, bsize=9.6)

# ═══════════════════════════════════════════════════════════════════════
# 20. 마무리
# ═══════════════════════════════════════════════════════════════════════
_page += 1
s = new_slide()
_titles.append("감사합니다")
s.shapes.add_picture(os.path.join(FIG, "bg_cover.png"), 0, 0, Inches(SW), Inches(SH))
hline(s, 0.82, 0.78, 11.70, SLATE)
hline(s, 0.82, 6.93, 11.70, SLATE)
txt(s, 0, 3.05, SW, 0.80, "감사합니다", size=34, color=WHITE, bold=True,
    align=PP_ALIGN.CENTER)
txt(s, 0, 4.05, SW, 0.34, "질문 주시면 답변드리겠습니다", size=13, color=LIGHT,
    align=PP_ALIGN.CENTER)
txt(s, 6.07, 6.98, 1.20, 0.30, str(_page), size=9, color=MUTED, align=PP_ALIGN.CENTER,
    font="Arial")


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else DECK
    prs.core_properties.title = TITLE
    prs.core_properties.author = AUTHOR
    prs.save(out)
    print("생성: %s (%d 장)" % (out, len(prs.slides._sldIdLst)))
    for i, t in enumerate(_titles, 1):
        print("  %2d. %s" % (i, t))
