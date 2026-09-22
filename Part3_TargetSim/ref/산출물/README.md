# Chapter 3 발표 산출물

과제 3 (표적 궤적 모의) 발표 자료와 그것을 만드는 데 쓴 스크립트다.
자료에 나오는 숫자와 그림은 전부 `src/TargetSim` 의 Core 를 실제로 돌려 나온 값이다.

## 산출물

| 파일 | 내용 |
|---|---|
| `Chapter3_TargetSim.pptx` | 발표자료 20장. 슬라이드 노트에 대본이 들어 있다 |
| `Chapter3_TargetSim.pdf` | 위 파일을 PDF 로 내보낸 것 (합본을 만드는 재료) |
| `Chapter3_발표대본.md` | 발표 대본 원본. 노트와 합본은 이 파일에서 나온다 |
| `Chapter3_발표자료_대본합본.pdf` | A4 한 장에 슬라이드 한 장과 그 장의 대본. 발표 직전에 손에 드는 용도 |

## 폴더

- `figures/` — 슬라이드에 들어간 기술 도면 12장 (png). `tools/make_figures.py` 가 만든다
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
python3 make_figures.py            # 발표용 12장. 번호를 주면 그것만: python3 make_figures.py 2 3
python3 make_figures.py --all      # 질의응답 예비 그림(101~116)까지 전부

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

## 그림 구성

발표자료는 관련 내용을 한 장에 묶는 통합 그림을 쓴다. 자세한 낱장 그림은
`EXTRA` 로 따로 두어, 질의응답에서 깊게 들어올 때만 꺼낸다.

| 번호 | 파일 | 슬라이드 | 묶은 내용 |
|---|---|---|---|
| 1 | `fig02_system.png` | 4 | 계층 구성 · 인터페이스 구조체 · 호출 순서 |
| 2 | `figA_state_frames.png` | 5 | ECEF/LLA · NED/Body · 자세각 3종 · 상태 벡터 · 초기값 매핑 |
| 3 | `figB_maneuver_g.png` | 6 | 기동 4필드 · 구간 규칙 · G→ω/R 유도 · 수치표 · 실측 응답 |
| 4 | `fig08_turntype.png` | 7 | 세 회전축의 궤적·고도 응답 (실측) |
| 5 | `fig09_struct.png` | 8 | 입력·출력 구조체 관계도와 설계 결정 |
| 6 | `figC_model_ecef.png` | 10 | 지배 방정식 3줄 · 모델 가정 3 · ECEF 선택 근거와 수치 |
| 7 | `figD_velocity_dcm.png` | 11 | 2단 회전 파이프라인 · 두 DCM 원소 · 위치/속도 차이 |
| 8 | `fig14_midpoint.png` | 13 | 오일러법 vs 중점법 기하와 절단오차 차수 |
| 9 | `fig17_result_map.png` | 16 | 명세 시나리오 궤적 · 고도 · 속력 |
| 10 | `figF_verify.png` | 17 | 검증 3갈래 표 + 수렴 차수 그래프 |
| 11 | `fig22_ui.png` | (재료) | TargetSim 화면 — figG 안에 들어간다 |
| 12 | `figG_demo_ui.png` | 18 | 기동 시연 궤적·방위각 + 운용 화면 |

## 그림 작도 규칙

기술보고서 그림 관례를 따랐다.

- 축은 실선, 가려진 축과 보조선은 파선
- 벡터는 굵은 실선 + 화살촉, 각은 호로 표시하고 기호를 붙인다
- 좌표계 표기는 `C^{to}_{from}`, 위도는 `L`, 경도는 `λ`, 자세각은 `φ θ ψ`
- 색은 뜻이 있을 때만 쓴다 (객체 구분 · 강조), 나머지는 회색조
- 지구 도해는 정사영(`Ortho` 클래스)으로 그려 위도선·경도선의 앞뒤를 구분한다

## 스크립트

| 파일 | 하는 일 |
|---|---|
| `run_sim.c` | 명세 / 기동 시연 / 회전축 비교 세 시나리오를 돌려 CSV 로 떨군다 |
| `run_convergence.c` | 갱신 간격을 1 s ~ 0.01 s 로 바꿔 가며 60 s 뒤 위치 오차를 잰다 |
| `make_figures.py` | 발표용 그림 12장 + 질의응답 예비 16장 (구면 정사영 · DCM 행렬 · 오차 해석) |
| `build_deck.py` | 회사 템플릿 색·글꼴·배치로 pptx 20장을 만든다 |
| `sync_notes.py` | 대본 md 를 슬라이드 노트로 밀어 넣는다 |
| `build_combo.py` | 슬라이드 PDF + 대본 md → A4 합본 PDF |

`build_deck.py` 는 `ref/템플릿/Presentation_Template_Blank.pptx` 를 열어 예시 슬라이드만
지우고 쓴다. 테마와 레이아웃, 표지 배경 그림이 템플릿에서 그대로 온다.
