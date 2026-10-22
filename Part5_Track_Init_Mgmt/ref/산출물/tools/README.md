# 과제 5 산출물 다시 만드는 순서

폴더 구성

- `Chapter5_TrackInitMgmt.pptx` 발표자료 (틀은 과제 4 발표자료의 레이아웃, 슬라이드 노트에 대본)
- `Chapter5_발표대본.md` 슬라이드 노트와 같은 대본
- `data/` 시뮬레이션 결과 (truth, plots, tracks, scanlog CSV) 와 검증 로그 `verify_log.txt`
- `figures/` 발표 그림 (`fig_*.png`), 프로그램 캡처 (`ui_*.png`)
- `tools/` 아래 스크립트

## 1. 빌드 · 실행 · 캡처 (Windows)

`tools/start_capture_agent.bat` 을 더블클릭하면 최소화된 PowerShell 도우미가 뜨고, 같은 폴더의 `request.txt` 에 적힌 명령을 처리합니다.

| 명령 | 하는 일 |
| --- | --- |
| `build` | `src/TargetSim/TargetSim.sln` 을 Release x64 로 다시 빌드 (`build_log.txt`) |
| `run` | `build/x64/Release/TargetSimUI.exe` 실행 |
| `view 0/1/2`, `scan <n>`, `size <w> <h>` | 주 화면 보기 · 스캔 · 창 크기 |
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

## 3. 그림과 발표자료 (Python 3, matplotlib, pandas, python-pptx)

    python make_figures.py ../data ../figures
    python build_deck.py <과제4 Chapter4_PlotClutterSim.pptx> ../figures ../../../src/TargetSim ../Chapter5_TrackInitMgmt.pptx ../Chapter5_발표대본.md

- 대본은 `notes.py` 에서 고칩니다. 슬라이드 노트와 대본 md 가 같이 바뀝니다.
- 코드 발췌는 실제 소스에서 줄을 찾아 읽으므로 줄 번호가 늘 소스와 같습니다.
- 4.2 · 4.5 슬라이드의 확대 캡처 (`ui_crop_*.png`) 는 `ui_big_s1159_*.png`, `ui_big_s701_*.png` 를 잘라 만든 것입니다.
