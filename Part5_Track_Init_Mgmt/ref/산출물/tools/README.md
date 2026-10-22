# 과제 5 산출물 다시 만드는 순서

폴더 구성

- `Chapter5_TrackInitMgmt.pptx` 발표자료 (틀은 과제 4 발표자료의 레이아웃, 슬라이드 노트에 대본)
- `Chapter5_발표대본.md` 슬라이드 노트와 같은 대본
- `data/` 시뮬레이션 결과 (truth, plots, tracks, scanlog CSV) 와 검증 로그 `verify_log.txt`. 만든 방법 · 열 설명 · 그림별 쓰는 데이터는 `data/README.md`
- `figures/` 발표 그림 (`fig_*.png`), 프로그램 캡처 (`ui_*.png`)
- `tools/` 아래 스크립트

## 1. 빌드 · 실행 · 캡처 (Windows)

`tools/start_capture_agent.bat` 을 더블클릭하면 최소화된 PowerShell 도우미가 뜨고, 같은 폴더의 `request.txt` 에 적힌 명령을 처리합니다.

| 명령 | 하는 일 |
| --- | --- |
| `build` | `src/TargetSim/TargetSim.sln` 을 Release x64 로 다시 빌드 (`build_log.txt`) |
| `run` | `build/x64/Release/TargetSimUI.exe` 실행 |
| `view 0/1/2/3`, `scan <n>`, `size <w> <h>` | 주 화면 보기 (2D + 3D / 2D / 3D / 트랙 수명) · 스캔 · 창 크기 |
| `wheel <x> <y> <delta>` | 창 그림의 (x, y) 에서 휠 (2D · 3D 확대, 트랙 수명은 가로 구간 길이) |
| `score`, `preset <n>`, `closescore` | 스코어 확인 창 열기 · 예시 고르기 · 닫기 |
| `capture <이름>` | TargetSimUI 창마다 `figures/ui_<이름>.png` 저장 |
| `kill`, `quit` | 프로그램 · 도우미 종료 |

## 2. 데이터 검증 (Linux / WSL, 선택)

`tools/verify/harness_main.c` 는 Core 소스를 그대로 (PlotLink.c 의 UDP 송수신 쓰레드 포함) 리눅스에서 돌려 단위 시험과 CSV 를 만듭니다. `winstub/` 은 Win32 API 를 pthread · BSD 소켓으로 바꿔 주는 검증용 헤더입니다.

    C=../../../src/TargetSim/TargetSimCore
    gcc -O2 -std=gnu11 -I verify/winstub -I $C/include -o harness verify/harness_main.c \
        $C/src/PlotLink.c $C/src/TrackMgr.c $C/src/TargetSim.c $C/src/PlotSim.c \
        $C/src/CoordinateTransform.c $C/src/matrixCalcLib.c $C/src/NoiseGeneration.c -lm -lpthread
    ./harness ../data

난수는 고정 SEED 의 LCG 라 Windows 실행 결과와 같은 값이 나옵니다 (트랙 355 개, 손실 0).

### 트랙 수명 그림 검증 (`verify/lifeview`)

TargetSimUI 의 트랙 수명 그림 (`TrajectoryPlot.cpp` 의 CTrackLifeView) 을 리눅스에서 돌려 봅니다. MFC · GDI+ 는 `afx_stub.h` 가 흉내 내고,
`extract.py` 가 실제 소스에서 이 그림 코드와 그것이 부르는 함수를 그대로 잘라 옵니다 (소스를 고치면 다시 실행).
`data/tracks.csv` 를 CTrackResult 에 같은 순서로 쌓아 줄 · 칸 · 구간 · 휠 · 손실 스캔 · 머리 줄 배치를 검사하고, 그리기 호출을 `out/*.json` 으로 남깁니다.

    cd verify/lifeview
    S=../../../../../src/TargetSim
    python extract.py $S/TargetSimUI life_tu.cpp
    g++ -std=c++17 -Wall -Wextra -Wshadow -I . -I $S/TargetSimUI -I $S/TargetSimCore/include -o lifeview life_tu.cpp harness_main.cpp
    ./lifeview ../../../data out
    python render.py out/*.json          # 미리보기 PNG (글꼴 크기 · 글자 폭은 근사. 실제 화면은 Windows 캡처로 확인)

## 3. 그림과 발표자료 (Python 3, matplotlib, pandas, python-pptx)

    python make_figures.py ../data ../figures
    python build_deck.py <과제4 Chapter4_PlotClutterSim.pptx> ../figures ../../../src/TargetSim ../Chapter5_TrackInitMgmt.pptx ../Chapter5_발표대본.md

- 대본은 `notes.py` 에서 고칩니다. 슬라이드 노트와 대본 md 가 같이 바뀝니다.
- 4.4 의 '위 그림을 그린 데이터' 표는 `../data/tracks.csv` 를 읽어 만듭니다 (6 번째 인자로 다른 데이터 폴더 지정 가능).
- 코드 발췌는 실제 소스에서 줄을 찾아 읽으므로 줄 번호가 늘 소스와 같습니다.
- 4.2 · 4.5 슬라이드의 확대 캡처 (`ui_crop_*.png`) 는 `ui_big_s1159_*.png`, `ui_big_s701_*.png` 를 잘라 만든 것입니다.
- 결과 그림 아래의 출처 표시 (`build_deck.py` src_tag): 파랑 '프로그램 화면' = TargetSimUI 캡처, 초록 '결과 데이터' = `data/` CSV 로 그린 그림.
