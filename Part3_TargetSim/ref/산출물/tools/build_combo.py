# -*- coding: utf-8 -*-
"""슬라이드 PDF + 발표대본 md  ->  대본 합본 PDF.

    python build_combo.py            # 산출물 폴더의 세 파일을 그대로 쓴다
    python build_combo.py <슬라이드.pdf> <대본.md> <출력.pdf>

A4 한 장에 슬라이드 한 장과 그 장의 대본을 같이 얹는다. 발표 직전에 손에 들고 읽는 용도다.
슬라이드는 이미지가 아니라 PDF 페이지를 그대로 얹으므로(show_pdf_page) 확대해도 글자가 깨지지 않는다.
Chapter 2 의 build_combo.py 와 같은 판형이다.
"""
import io
import os
import re
import sys

import pymupdf as fitz

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.normpath(os.path.join(HERE, ".."))
SLIDES_PDF = os.path.join(OUT_DIR, "Chapter3_target_simulation.pdf")
SCRIPT_MD = os.path.join(OUT_DIR, "Chapter3_발표대본.md")
COMBO_PDF = os.path.join(OUT_DIR, "Chapter3_발표자료_대본합본.pdf")

TITLE = "Chapter 3 표적 궤적 모의"
AUTHOR = "Janghwan Kim (Harley)"

FONT_CANDIDATES = [
    (r"C:\Windows\Fonts\malgun.ttf", r"C:\Windows\Fonts\malgunbd.ttf"),
    ("/usr/share/fonts/truetype/nanum/NanumGothic.ttf",
     "/usr/share/fonts/truetype/nanum/NanumGothicBold.ttf"),
]
for REG, BOLD in FONT_CANDIDATES:
    if os.path.exists(REG) and os.path.exists(BOLD):
        break
else:
    sys.exit("한글 글꼴을 못 찾음 (맑은 고딕 / 나눔고딕)")
FONT = {"mg": fitz.Font(fontfile=REG), "mgb": fitz.Font(fontfile=BOLD)}

DARK = (0.075, 0.137, 0.247)        # 머리글 · 쪽번호 배지 (13233F)
BODY = (0.239, 0.267, 0.318)
GOLD = (0.541, 0.369, 0.063)
GREY = (0.478, 0.510, 0.561)
PANEL = (0.949, 0.957, 0.969)

PAGE_W, PAGE_H = 595.0, 842.0
BADGE = fitz.Rect(40, 34, 84, 59)
HEAD_Y = 51.0
SLIDE_BOX = fitz.Rect(40, 70.4, 556, 360.6)
PANEL_BOX = fitz.Rect(40, 381, 556, 797)
LABEL_Y = 400.0
BODY_X, BODY_W = 58.0, 479.0
BODY_Y0 = 420.0
LEAD, PARA_GAP = 14.5, 22.5
FOOT_Y = 812.0

SUBST = {"\u2212": "-"}


def plain(text):
    for src, dst in SUBST.items():
        text = text.replace(src, dst)
    return text


def parse(md):
    """## N. 제목  ->  {번호: (제목, 구간·시간, [문단, ...])}"""
    out = {}
    for blk in re.split(r"(?m)^## ", md)[1:]:
        head = re.match(r"(\d+)\.\s+(.+)", blk)
        if not head:
            continue
        num, title = int(head.group(1)), plain(head.group(2).strip())
        tag, body, fence = "", [], False
        for raw in blk.split("\n")[1:]:
            line = raw.strip()
            if line.startswith("```"):
                fence = not fence
                continue
            if fence or not line or line.startswith(("|", "#", "---", "![", ">")):
                continue
            mark = re.match(r"\[(.+?)\]$", line)
            if mark and not tag:
                tag = plain(mark.group(1))
                continue
            body.append(plain(re.sub(r"\*\*(.+?)\*\*", r"\1", line)))
        out[num] = (title, tag, body)
    return out


def wrap(text, font, size, width):
    lines, cur = [], ""
    for ch in text:
        if cur and font.text_length(cur + ch, size) > width:
            lines.append(cur)
            cur = "" if ch == " " else ch
        else:
            cur += ch
    if cur.strip():
        lines.append(cur)
    return lines


def build(slides_pdf, md_path, out_pdf):
    src = fitz.open(slides_pdf)
    secs = parse(io.open(md_path, encoding="utf-8").read())
    doc = fitz.open()
    total = src.page_count

    for i in range(total):
        num = i + 1
        title, tag, body = secs.get(num, ("", "", []))
        pg = doc.new_page(width=PAGE_W, height=PAGE_H)
        for name, path in (("mg", REG), ("mgb", BOLD)):
            pg.insert_font(fontname=name, fontfile=path)

        shape = pg.new_shape()
        shape.draw_rect(BADGE)
        shape.finish(fill=DARK, color=None)
        shape.draw_rect(PANEL_BOX)
        shape.finish(fill=PANEL, color=None)
        shape.commit()

        label = str(num)
        pg.insert_text(fitz.Point(BADGE.x0 + BADGE.width / 2
                                  - FONT["mgb"].text_length(label, 12) / 2, HEAD_Y),
                       label, fontname="mgb", fontsize=12, color=(1, 1, 1))
        pg.insert_text(fitz.Point(96, HEAD_Y), title, fontname="mgb", fontsize=12, color=DARK)
        pg.show_pdf_page(SLIDE_BOX, src, i)

        pg.insert_text(fitz.Point(BODY_X, LABEL_Y), "발표 대본", fontname="mgb", fontsize=10.5, color=DARK)
        if tag:
            pg.insert_text(fitz.Point(537 - FONT["mg"].text_length(tag, 9.5), LABEL_Y),
                           tag, fontname="mg", fontsize=9.5, color=GOLD)

        y = BODY_Y0
        for para in body:
            for line in wrap(para, FONT["mg"], 10, BODY_W):
                pg.insert_text(fitz.Point(BODY_X, y), line, fontname="mg", fontsize=10, color=BODY)
                y += LEAD
            y += PARA_GAP - LEAD

        pg.insert_text(fitz.Point(40, FOOT_Y), "%s  ·  %s" % (TITLE, AUTHOR),
                       fontname="mg", fontsize=8.5, color=GREY)
        page_no = "%d / %d" % (num, total)
        pg.insert_text(fitz.Point(556 - FONT["mg"].text_length(page_no, 8.5), FOOT_Y),
                       page_no, fontname="mg", fontsize=8.5, color=GREY)

    doc.set_metadata({"title": "%s · 대본 합본" % TITLE, "author": AUTHOR, "producer": "build_combo.py"})
    doc.subset_fonts()
    doc.save(out_pdf, deflate=True, deflate_images=True, garbage=4, clean=True)
    doc.close()
    src.close()
    print("생성: %s (%d 쪽)" % (out_pdf, total))


if __name__ == "__main__":
    args = sys.argv[1:]
    if not args:
        args = [SLIDES_PDF, SCRIPT_MD, COMBO_PDF]
    if len(args) != 3:
        sys.exit("사용법: python build_combo.py [슬라이드.pdf 대본.md 출력.pdf]")
    build(*args)
