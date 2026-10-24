# 과제 6 산출물 다시 만드는 순서

폴더 구성

- `Chapter6_TrackFilter.pptx` 발표자료 (틀은 과제 4 발표자료의 레이아웃, 슬라이드 노트에 대본)
- `Chapter6_발표대본.md` 슬라이드 노트와 같은 대본
- `data/` 시뮬레이션 결과 CSV, 검증 로그 (`verify_log.txt`, `uiview_log.txt`). 만든 방법 · 열 설명 · 그림별 쓰는 데이터는 `data/README.md`
- `figures/` 발표 그림 (`fig_*.png`), 프로그램 화면 (`pv_*.png` 리눅스 미리보기, `ui_*.png` Windows 캡처) 과 그 컨트롤 위치 (`*.json`)
- `tools/` 아래 스크립트
- `versions/` 버전마다 남긴 발표자료 · 대본 · 소스 (`versions/버전기록.md`)

## 1. 결과 데이터와 검증 (Linux / WSL)

`tools/verify/harness_main.c` 는 Core 소스를 그대로 (PlotLink.c 의 UDP 송수신 쓰레드, TrackMgr.c 의 Singer CMKF 포함) 리눅스에서 돌립니다.
`winstub/` 은 Win32 API 를 pthread · BSD 소켓으로 바꿔 주는 검증용 헤더입니다.

    C=../../../src/TargetSim/TargetSimCore
    gcc -O2 -std=gnu11 -I verify/winstub -I $C/include -o harness verify/harness_main.c \
        $C/src/PlotLink.c $C/src/TrackMgr.c $C/src/TargetSim.c $C/src/PlotSim.c \
        $C/src/CoordinateTransform.c $C/src/matrixCalcLib.c $C/src/NoiseGeneration.c -lm -lpthread
    (cd ../data && ../tools/harness . all > verify_log.txt)        # unit + runs + sweep, 약 1 분

| 모드 | 하는 일 | 결과 |
| --- | --- | --- |
| `unit` | Singer F (행렬 지수 함수 대비), Q (60 자리 기준값 · Simpson 적분 대비, 양의 정부호, 급수 ↔ 닫힌 식 전환), q12 오타 확인, CMKF 갱신 (독립 구현 대비), 게이트 밖 · 발산 판정, 연관 가정, 파라미터 범위, 과제 5 로직 유지 | `verify_log.txt` |
| `runs` | 대함 · 대공 × 과제 값 · 조정 값 4 번 (프로그램과 같은 잡음) | `<run>/truth · plots · tracks · scanlog.csv`, `runs.csv` |
| `sweep` | 잡음 seed 30 개 × (q 10 단계 × R σ 0.3° · 0.5° + τ 6 단계) × 2 시나리오 = 1,560 번 | `sweep.csv` |

마지막 줄이 `FAILURES: 0` 이어야 합니다. 난수는 고정 SEED 의 LCG 라 Windows 실행 결과와 같은 값이 나옵니다.
조정 값 (`harness_main.c` 의 s_Tuned) 은 TargetSimUI `Scenario.cpp` 의 s_Tuned 와 같아야 합니다.

## 2. 화면 미리보기와 화면 검증 (`verify/uiview`, Linux)

TargetSimUI 의 실제 소스 (대화상자 · 그림 · 결과 보관) 와 Core 를 리눅스에서 빌드해 OnInitDialog → UDP 송수신 → 타이머 → 배치 → 그리기를 돌립니다.
MFC · GDI+ 는 `afx_stub.h` 가 흉내 내고, 배치 · 조작 · 화면 누적 값 (= 검증 harness 값) 을 검사한 뒤 창 전체 그리기 호출과 컨트롤 위치를 JSON 으로 남깁니다.

    cd verify/uiview
    sh build.sh /tmp/uiobj                                           # -> /tmp/uiobj/uiview
    /tmp/uiobj/uiview ../../../../../src/TargetSim/TargetSimUI out ../../../data/runs.csv   # PASS 36, FAIL 0
    python render.py --out ../../../figures out/ui_{ship,air}_{given,tuned}_{all,perf}.json out/ui_air_given_map.json
                                                                     # -> figures/pv_<이름>.png + pv_<이름>.json (발표자료가 쓰는 9 장)
    python render.py out/*.json                                      # (선택) 모든 기록을 out/ 에 PNG 로

미리보기는 글꼴 크기 · 글자 폭이 근사입니다. 발표자료에는 '화면 미리보기' (보라) 로 표시되고, 아래 3 의 Windows 캡처가 있으면 그것을 씁니다.

## 3. Windows 실제 화면으로 바꾸기 (선택)

1. Visual Studio 2022 가 있는 PC 에서 `tools/start_capture_agent.bat` 을 더블클릭 (최소화된 PowerShell 도우미가 뜸)
2. 같은 폴더에 `request.txt` 를 만들고 한 줄 명령을 적어 저장. 도우미가 읽고 지운 뒤 `agent_log.txt` 에 결과를 남김
3. `build` → (끝나면) `deck` : 발표자료가 쓰는 화면 9 장을 `figures/ui_<이름>.png` + `ui_<이름>.json` 으로 저장
4. 아래 4 의 `build_deck.py` 를 다시 돌리면 화면 그림이 Windows 캡처로 바뀌고 표시도 '프로그램 화면' (파랑) 이 됨

| 명령 | 하는 일 |
| --- | --- |
| `build` | `src/TargetSim/TargetSim.sln` 을 Release x64 로 다시 빌드 (`build_log.txt`) |
| `run` | `build/x64/Release/TargetSimUI.exe` 실행, 첫 실행이 끝날 때까지 기다림 |
| `deck` | 대함 · 대공 × 과제 값 · 조정 값을 돌리며 3D + 2D · 추적 성능 · 평면 보기를 차례로 캡처 |
| `scenario 0/1`, `given`, `tuned`, `param <r> <az> <el> <q> <tau>`, `apply` | 시나리오 · 파라미터 단추 · 입력 칸 · 적용 |
| `view 0..4`, `scan <n>`, `size <w> <h>`, `wait` | 보기 (3D + 2D / 3D / 2D / 평면 / 추적 성능) · 스캔 · 창 크기 · 실행 끝 기다리기 |
| `capture <이름>` | 주 화면 `figures/ui_<이름>.png` + 컨트롤 위치 `ui_<이름>.json` |
| `kill`, `quit` | 프로그램 · 도우미 종료 |

## 4. 그림과 발표자료 (Python 3, matplotlib, pandas, mpmath, Pillow, python-pptx)

    python make_figures.py ../data ../figures
    python build_deck.py <과제4 Chapter4_PlotClutterSim.pptx> ../figures ../../../src/TargetSim ../Chapter6_TrackFilter.pptx ../Chapter6_발표대본.md

- 대본은 `notes.py` 에서 고칩니다. 슬라이드 노트와 대본 md 가 같이 바뀝니다.
- 슬라이드의 수치 (위치 오차, NIS, 발산 판정, 30 회 결과) 는 `../data/runs.csv` · `sweep.csv` 에서 읽습니다 (6 번째 인자로 다른 데이터 폴더 지정 가능).
  대본의 수치가 데이터와 다르거나, 목차 · 간지의 쪽 번호가 실제 슬라이드와 다르면 `build_deck.py` 가 멈추고 어디가 다른지 알려 줍니다.
- 코드 발췌는 실제 소스에서 줄을 찾아 읽으므로 줄 번호가 늘 소스와 같습니다.
- 결과 그림 아래의 출처 표시 (`build_deck.py` src_tag): 파랑 '프로그램 화면' = TargetSimUI Windows 캡처, 보라 '화면 미리보기' = 같은 그리기 코드를 리눅스에서 실행한 그림,
  초록 '결과 데이터' = `data/` CSV 로 그린 그림.
- 그림 색: 과제 값 = 주황, 조정 값 = 파랑 (비교 그림 모두 같음), 참값 = 남색 선, 플롯 = 회색 점.
