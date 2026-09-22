# Chapter 3 발표 산출물

과제 3 (표적 궤적 모의) 발표 자료와 그것을 만드는 데 쓴 스크립트다.
자료에 나오는 숫자와 그림은 전부 `src/TargetSim` 의 Core 를 실제로 돌려 나온 값이다.

발표는 "표적 모의를 어떻게 구현했는가" 를 따라간다. 좌표계와 좌표변환 이론은
Part 2 에서 이미 다뤘으므로 다시 설명하지 않고, 단계마다 필요한 공식과 그 공식이
왜 필요한지만 짚는다.

## 산출물

| 파일 | 내용 |
|---|---|
| `Chapter3_TargetSim.pptx` | 발표자료 18장. 슬라이드 노트에 대본이 들어 있다 |
| `Chapter3_TargetSim.pdf` | 위 파일을 PDF 로 내보낸 것 (합본을 만드는 재료) |
| `Chapter3_발표대본.md` | 발표 대본 원본. 노트와 합본은 이 파일에서 나온다 |
| `Chapter3_발표자료_대본합본.pdf` | A4 한 장에 슬라이드 한 장과 그 장의 대본. 발표 직전에 손에 드는 용도 |

## 폴더

- `figures/` — 슬라이드에 들어간 기술 도면 (png). `tools/make_figures.py` 가 만든다
- `data/` — Core 를 돌려 받은 원본 결과 (csv / txt). 그림과 표의 숫자가 전부 여기서 나온다
- `tools/` — 위를 만드는 스크립트

## 슬라이드 구성 (18장)

| 쪽 | 제목 | 그림 |
|---|---|---|
| 1 | 표지 | `bg_cover.png` |
| 2 | 목차 | |
| 3 | 무엇을 왜 만들었는가 | `figS1_overview.png` |
| 4 | 만든 것 — 계산과 화면을 나눈다 | `fig02_system.png` |
| 5 | 표적을 어떤 값으로 적을 것인가 | `figS2_target.png` |
| 6 | 기동을 어떤 값으로 적을 것인가 | `figS3_maneuver.png` |
| 7 | 축을 바꾸면 궤적이 어떻게 달라지는가 | `fig08_turntype.png` |
| 8 | 코드로 보는 구조체 | (코드 2단) |
| 9 | 한 스텝에 하는 일 | `figS4_step.png` |
| 10 | 공식 ① — 속도 벡터를 만든다 | `figS5_velocity.png` |
| 11 | 코드 ① — 속도 벡터를 만든다 | (코드) |
| 12 | 공식 ② — 한 스텝 전진시킨다 | `figS6_midpoint.png` |
| 13 | 코드 ② — 한 스텝 전진시킨다 | (코드) |
| 14 | 시뮬레이션 조건 | (표) |
| 15 | 결과 ① — 명세 조건 60 초 | `fig17_result_map.png` |
| 16 | 결과 ② — 기동 시연과 운용 화면 | `figG_demo_ui.png` |
| 17 | 정리 | (표) |
| 18 | 감사합니다 | `bg_cover.png` |

`fig22_ui.png` 는 화면 도해로, 16쪽 그림(`figG`) 안에 들어가는 재료다.

좌표계 도해 · DCM 원소 전개 · 오차 해석 같은 자세한 낱장 그림은 `make_figures.py` 의
`EXTRA` 에 그대로 남겨 두었다. 발표에는 넣지 않고, 질의응답에서 깊게 들어올 때만
`python3 make_figures.py --all` 로 뽑아 쓴다.

## 다시 만드는 순서

대본만 고쳤다면 4번과 5번만 다시 돌리면 된다.

```bash
cd tools
SRC=../../../src/TargetSim/TargetSimCore
CORE="$SRC/src/CoordinateTransform.c $SRC/src/matrixCalcLib.c"

# 1) 시뮬레이션 결과 (data/*.csv, data/conv.txt)
gcc -O2 -std=c99 -I"$SRC/include" run_sim.c "$SRC/src/TargetSim.c" $CORE \
    -lm -o run_sim && (cd ../data && ../tools/run_sim)

gcc -O2 -std=c99 -I"$SRC/include" run_convergence.c "$SRC/src/TargetSim.c" $CORE \
    -lm -o run_conv && ./run_conv > ../data/conv.txt

# 1-1) 적분 방법 비교 (data/method_mid.csv, data/method_euler.csv)
#      오일러판은 위치 갱신에 쓰는 속도만 중점 속도 → 스텝 시작 속도로 되돌린 사본이다.
sed 's/st_VelMid\.\([xyz]\) \* stepTime/st_VelStart.\1 * stepTime/g' \
    "$SRC/src/TargetSim.c" > /tmp/TargetSim_euler.c
gcc -O2 -std=c99 -I"$SRC/include" run_method.c "$SRC/src/TargetSim.c" $CORE \
    -lm -o run_mid   && ./run_mid   > ../data/method_mid.csv
gcc -O2 -std=c99 -I"$SRC/include" run_method.c /tmp/TargetSim_euler.c $CORE \
    -lm -o run_eul   && ./run_eul   > ../data/method_euler.csv

# 2) 그림 (figures/*.png)
python3 make_figures.py            # 발표용. 번호를 주면 그것만: python3 make_figures.py S4 17
python3 make_figures.py --all      # 질의응답 예비 그림까지 전부

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
글꼴은 윈도우면 맑은 고딕, 리눅스면 나눔고딕을 찾아 쓴다. 수식은 matplotlib mathtext(STIX) 로 그린다.

## 그림 작도 규칙

기술보고서 그림 관례를 따랐다.

- 축은 실선, 가려진 축과 보조선은 파선
- 벡터는 굵은 실선 + 화살촉, 각은 호로 표시하고 기호를 붙인다
- 좌표계 표기는 `C^{to}_{from}`, 위도는 `L` 또는 `φ`, 경도는 `λ`, 자세각은 `φ θ ψ`
- 색은 뜻이 있을 때만 쓴다 (객체 구분 · 강조), 나머지는 회색조
- 캔버스는 가로·세로 0~100 으로 두고, 원·호는 `circle()` / `arcp()` 로 그려 가로세로비를 보정한다

## 스크립트

| 파일 | 하는 일 |
|---|---|
| `run_sim.c` | 명세 / 기동 시연 / 회전축 비교 세 시나리오를 돌려 CSV 로 떨군다 |
| `run_convergence.c` | 갱신 간격을 1 s ~ 0.01 s 로 바꿔 가며 60 s 뒤 위치 오차를 잰다 |
| `run_method.c` | 같은 간격에서 오일러법 / 중점법의 60 s 뒤 위치를 비교한다 (두 번 빌드) |
| `make_figures.py` | 발표용 그림 + 질의응답 예비 그림 (`--all`) |
| `build_deck.py` | 회사 템플릿 색·글꼴·배치로 pptx 18장을 만든다 |
| `sync_notes.py` | 대본 md 를 슬라이드 노트로 밀어 넣는다 |
| `build_combo.py` | 슬라이드 PDF + 대본 md → A4 합본 PDF |

`build_deck.py` 는 `ref/템플릿/Presentation_Template_Blank.pptx` 를 열어 예시 슬라이드만
지우고 쓴다. 테마와 레이아웃, 표지 배경 그림이 템플릿에서 그대로 온다.

## 주요 실측값

발표에 쓴 숫자다. 전부 `data/` 의 CSV 에서 나왔다.

| 항목 | 값 | 출처 |
|---|---|---|
| 대함 60 s 이동 | 1800 m (= 30 × 60) | `spec.csv` |
| 대공 60 s 이동 | 12000 m (= 200 × 60) | `spec.csv` |
| 고도 · 속력 | 601 표본 내내 입력값 유지 | `spec.csv` |
| 0.1 s · 60 s 위치 오차 (중점법) | 1.36 cm | `method_mid.csv` |
| 0.1 s · 60 s 위치 오차 (오일러법) | 16.6 m | `method_euler.csv` |
| 수렴 차수 (중점법) | 회귀 기울기 2.002 | `conv.txt` |
| 실행 시간 | 601 표본 × 3 객체 = 4.8 ms | 화면 상태줄 |
