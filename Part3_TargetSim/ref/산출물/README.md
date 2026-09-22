# Chapter 3 발표 산출물

과제 3 (표적 궤적 모의) 발표 자료와 그것을 만드는 데 쓴 스크립트다.
자료에 나오는 숫자와 그림은 전부 `src/TargetSim` 의 Core 를 실제로 돌려 나온 값이다.

## 산출물

| 파일 | 내용 |
|---|---|
| `Chapter3_TargetSim.pptx` | 발표자료 32장. 슬라이드 노트에 대본이 들어 있다 |
| `Chapter3_TargetSim.pdf` | 위 파일을 PDF 로 내보낸 것 (합본을 만드는 재료) |
| `Chapter3_발표대본.md` | 발표 대본 원본. 노트와 합본은 이 파일에서 나온다 |
| `Chapter3_발표자료_대본합본.pdf` | A4 한 장에 슬라이드 한 장과 그 장의 대본. 발표 직전에 손에 드는 용도 |

## 폴더

- `figures/` — 슬라이드에 들어간 그림 (png). `tools/make_figures.py` 가 만든다
- `data/` — Core 를 돌려 받은 원본 결과 (csv / txt). 그림과 표의 숫자가 전부 여기서 나온다
- `tools/` — 위를 만드는 스크립트

## 다시 만드는 순서

대본만 고쳤다면 3번과 5번만 다시 돌리면 된다.

```bash
cd tools
SRC=../../../src/TargetSim/TargetSimCore

# 1) 시뮬레이션 결과 (data/*.csv, data/conv.txt)
gcc -O2 -std=c99 -I"$SRC/include" run_sim.c \
    "$SRC/src/TargetSim.c" "$SRC/src/CoordinateTransform.c" "$SRC/src/matrixCalcLib.c" \
    -lm -o run_sim && (cd ../data && ../tools/run_sim)

gcc -O2 -std=c99 -I"$SRC/include" run_convergence.c \
    "$SRC/src/TargetSim.c" "$SRC/src/CoordinateTransform.c" "$SRC/src/matrixCalcLib.c" \
    -lm -o run_conv && ./run_conv > ../data/conv.txt

# 2) 그림 (figures/*.png)
python3 make_figures.py            # 번호를 주면 그것만: python3 make_figures.py 15 17

# 3) 발표자료 (Chapter3_TargetSim.pptx)
python3 build_deck.py

# 4) 대본을 슬라이드 노트로 (대본이 원본, 노트가 사본)
python3 sync_notes.py              # --check 를 주면 쓰지 않고 어긋난 장만 알려 준다

# 5) 합본 PDF
#    먼저 pptx 를 PDF 로 내보낸다 (PowerPoint 의 "다른 이름으로 저장" 이어도 된다)
soffice --headless --convert-to pdf --outdir .. ../Chapter3_TargetSim.pptx
python3 build_combo.py
```

필요한 것: `python-pptx`, `pymupdf`, `matplotlib`, `numpy`, `Pillow`.
글꼴은 윈도우면 맑은 고딕, 리눅스면 나눔고딕을 찾아 쓴다.

## 스크립트

| 파일 | 하는 일 |
|---|---|
| `run_sim.c` | 명세 / 기동 시연 / 회전축 비교 세 시나리오를 돌려 CSV 로 떨군다 |
| `run_convergence.c` | 갱신 간격을 1 s ~ 0.01 s 로 바꿔 가며 60 s 뒤 위치 오차를 잰다 |
| `make_figures.py` | 슬라이드 그림 19장을 그린다 |
| `build_deck.py` | 회사 템플릿 색·글꼴·배치로 pptx 32장을 만든다 |
| `sync_notes.py` | 대본 md 를 슬라이드 노트로 밀어 넣는다 |
| `build_combo.py` | 슬라이드 PDF + 대본 md → A4 합본 PDF |

`build_deck.py` 는 `ref/템플릿/Presentation_Template_Blank.pptx` 를 열어 예시 슬라이드만
지우고 쓴다. 테마와 레이아웃, 표지 배경 그림이 템플릿에서 그대로 온다.
