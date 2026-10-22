# 결과 데이터 (발표 그림의 원본)

발표자료의 결과 그림은 두 가지로 만들었습니다. 슬라이드마다 그림 아래에 어느 쪽인지 표시했습니다.

| 표시 | 만든 방법 | 슬라이드 |
| --- | --- | --- |
| 프로그램 화면 | TargetSimUI 를 실행해 창을 캡처 (`../figures/ui_*.png`). 4.2 · 4.5 는 그림 영역만 잘라 키움 (`make_figures.py` crop_ui, 다른 편집 없음) | 4.1, 4.2, 4.4 오른쪽, 4.5 |
| 결과 데이터 | 이 폴더의 CSV 를 Python (`../tools/make_figures.py`) 으로 그림 | 4.3, 4.4 왼쪽 |

## CSV 를 만든 방법

- `../tools/verify/harness_main.c` 가 TargetSimCore 소스를 그대로 컴파일해 (PlotLink.c 의 UDP 송수신 쓰레드, TrackMgr.c 포함)
  TargetSimUI 와 같은 시나리오 · 조건으로 실행합니다. 지그재그 응용 시나리오, SNR 20, 스캔당 클러터 5, 116 s, 1,160 스캔입니다.
- 스캔을 처리할 때마다 `f_Lnk_CopyScan` 으로 결과를 꺼내 CSV 한 줄씩 씁니다.
  TargetSimUI 도 같은 함수로 결과를 꺼내 2D · 3D 그림과 트랙 표를 그립니다 (`TargetSimUI/PlotResult.cpp` CTrackResult::f_Poll).
- 난수는 고정 SEED 라 몇 번을 돌려도 같은 값이 나옵니다. UI 트랙 표와 대조했을 때 아래 값이 모두 같았습니다.
  - 스캔 701: 트랙 282 ~ 285
  - 스캔 1159: 트랙 453, 454
  - 대조한 항목: ID, 상태, 점수, 개시 스캔, 개시 플롯, 위치, 속도
- `verify_log.txt` 는 같은 실행의 단위 시험 · 송수신 · ID 범위 검사 기록입니다 (실패 0).

다시 만들기: `../tools/README.md` 2 장 (harness 빌드 · 실행) → 3 장 (`python make_figures.py ../data ../figures`).

## 파일과 열

좌표는 모두 플랫폼 시작점 기준 [m] 이고, 속도는 [m/s] 입니다.

| 파일 | 한 줄 | 열 |
| --- | --- | --- |
| `truth.csv` | 0.1 s 마다 객체 하나의 참값 | `step`, `time`, `obj` (0 플랫폼, 1 표적 1 대함, 2 표적 2 대공), `east`, `north`, `up`, `r`, `az_deg`, `el_deg` (레이다 기준 참값 구좌표, 플랫폼 줄과 마지막 스텝은 0) |
| `plots.csv` | 데이터 처리 쓰레드가 UDP 로 받은 플롯 하나 | `scan`, `time`, `plot_id`, `source` (0 클러터, 1 · 2 표적), `north`, `east`, `down` |
| `tracks.csv` | 스캔 하나의 트랙 표 한 줄 (처리 뒤) | `scan`, `time`, `track_id`, `state` (1 임시, 2 확정, 3 메모리, 4 삭제), `score` (TrackScore), `memory` (MemoryScore), `flag` (Assigned_Flag), `init_scan`, `plot_prev` · `plot_cur` (개시에 쓴 앞 · 이번 스캔 플롯 ID), `src_prev` · `src_cur` (그 플롯의 source), `north`, `east`, `down`, `vn`, `ve`, `vd`, `psn` · `pse` · `psd` (mat_EstP 위치 대각의 제곱근), `vsn` · `vse` · `vsd` (속도 대각의 제곱근) |
| `scanlog.csv` | 스캔 하나의 처리 기록 | `scan`, `time`, `plots` (받은 플롯 수), `cand` (앞 스캔에서 개시 후보로 남은 플롯 수), `pairs` (검사한 쌍 수), `gate_pass` (속도 게이트를 넘은 쌍 수), `created` · `confirmed` · `memory` · `deleted` (이번 스캔에 만든 · 확정된 · 메모리로 바뀐 · 삭제 상태가 된 트랙 수), `tracks` (표의 트랙 수), `msg_size` (플롯 메시지 크기 [B]) |

`source` 는 송신 쪽이 플롯을 만들 때 적어 둔 참값으로, 트랙 처리에는 쓰지 않고 화면 · 그림의 색 구분에만 씁니다.

## 그림과 쓰는 데이터

| 슬라이드 | 그림 | 쓰는 데이터 | 그리는 함수 |
| --- | --- | --- | --- |
| 4.3 | `fig_zoom.png` | `truth` 표적 2 궤적, `plots` 표적 2 의 스캔 1153 · 1154, `tracks` 스캔 1154 의 트랙 451 · 450 | `fig_zoom` |
| 4.3 | 오른쪽 트랙 표 | `tracks` 스캔 1154 두 줄 | `build_deck.py` 가 값을 표로 씀 |
| 4.4 | `fig_lifeline.png` | `tracks` 중 스캔 0 ~ 104 에 개시된 트랙 100 ~ 121 | `fig_lifeline` |
| 4.4 | 부제 · 대본의 숫자 | `tracks` 트랙 355 개를 개시 플롯의 source 로 나눔 (표적 1 117, 표적 2 235, 클러터끼리 3) | - |
