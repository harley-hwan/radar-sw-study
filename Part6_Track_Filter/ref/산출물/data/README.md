# 과제 6 결과 데이터

`tools/verify/harness_main.c` (Core C 소스를 그대로 리눅스에서 실행, UDP 루프백 포함) 가 만든 결과입니다. 만드는 방법은 `tools/README.md` 1 장.
플롯 잡음은 고정 SEED 의 LCG 라 TargetSimUI (Windows) 를 같은 시나리오 · 파라미터로 실행한 결과와 같은 값입니다.
TargetSimUI 의 화면 누적 값 (RMS, NIS, 발산 판정) 이 이 값과 같은지는 `uiview_log.txt` 의 '화면 누적 값 = 검증 harness 값' 항목에서 확인합니다.

| 파일 · 폴더 | 내용 |
| --- | --- |
| `ship_given/`, `ship_tuned/`, `air_given/`, `air_tuned/` | 대함 · 대공 × 과제 값 · 조정 값 한 번씩 (프로그램 기본 잡음, seed 0) |
| `runs.csv` | 위 네 실행의 요약 (한 줄 = 한 실행) |
| `sweep.csv` | 30 회 몬테카를로 (한 줄 = 한 설정의 30 회 평균) |
| `verify_log.txt` | 검증 harness 출력 (단위 시험 · 실행 · 스윕, 마지막 줄 `FAILURES: 0`) |
| `uiview_log.txt` | 화면 검증 (`tools/verify/uiview`) 출력 (`PASS 36, FAIL 0`) |

## 실행 폴더의 CSV

좌표는 VC-NED (레이다 원점, 북 · 동 · 아래) [m], 시각 [s]. 스캔 간격 0.1 s, 0 ~ 1159 스캔.

**truth.csv** 참값 (궤적 계산 표본, 0 ~ 116.0 s 의 1,161 개)

| 열 | 뜻 |
| --- | --- |
| step, time | 표본 번호 (= 스캔 번호), 시각 |
| north, east, down | 표적 위치 |
| lat_deg, lon_deg, alt_m | 표적 LLA |
| speed | 속력 [m/s] |
| turn | 1 = 시나리오의 선회 구간 안 |
| r_ant, az_ant_deg, el_ant_deg | 안테나 기준 거리 · 방위각 · 고각 참값. 스캔이 있는 표본만 (마지막 표본 116.0 s 는 빈 칸) |

**plots.csv** 수신 쓰레드가 받은 플롯 (스캔마다 1 개, 클러터 없음)

| 열 | 뜻 |
| --- | --- |
| scan, time, plot_id | 스캔, 시각, 플롯 ID (10000 부터) |
| source | 1 = 표적 1 (보내는 쪽이 만든 값. 받는 쪽은 모름) |
| north, east, down, lat_deg, lon_deg, alt_m | 받은 플롯을 좌표 변환한 위치 |

**tracks.csv** 스캔 처리 뒤 트랙 표 (한 줄 = 한 스캔의 트랙 하나)

| 열 | 뜻 |
| --- | --- |
| scan, time, track_id | 스캔, 시각, 트랙 ID |
| state, score, memory, flag | TrackState (1 임시, 2 확정, 3 메모리, 4 삭제), TrackScore, MemoryScore, Assigned_Flag |
| init_scan, plot_last, update_num | 개시 스캔, 마지막으로 갱신에 쓴 플롯 ID, 갱신 횟수 |
| north, east, down / vn, ve, vd / an, ae, ad | 추정 위치 / 속도 [m/s] / 가속도 [m/s²] (mat_EstX) |
| psn, pse, psd / vsn, vse, vsd / asn, ase, asd | P 대각의 표준편차: 위치 / 속도 / 가속도 |
| lat_deg, lon_deg, alt_m | 추정 위치의 LLA |
| true_north, true_east, true_down | 같은 스캔의 참값 위치 |
| err_n, err_e, err_d, err | 추정 − 참값, 그 크기 [m] |
| nis | 이번 스캔 NIS (= 잔차ᵀ S⁻¹ 잔차). 갱신하지 않은 스캔은 −1 |
| nees | 위치 NEES (= 오차ᵀ P위치⁻¹ 오차, 맞으면 평균 3) |
| gate_run, gate_sum, diverge_num | 게이트 밖 (NIS > 11.34) 연속 · 누적 횟수, 발산 판정 횟수 (연속 5 번마다 1) |
| innov_n, innov_e, innov_d | 마지막 갱신의 잔차 [m] |

**scanlog.csv** 스캔 처리 기록 (f_Trk_ProcessScan 의 ST_TrkScanLog)

| 열 | 뜻 |
| --- | --- |
| plots, updated, gate_out, diverged | 받은 플롯, 연관 (가정) 해 갱신한 트랙, 그 가운데 게이트 밖, 이번 스캔에 발산 판정이 난 트랙 |
| cand, pairs, gate_pass, created | 2-Point 개시: 앞 스캔 후보, 검사한 쌍, 속도 게이트 통과, 만든 트랙 |
| confirmed, memory, deleted, tracks | 확정 · 메모리 · 삭제 전환, 트랙 표의 트랙 수 |
| msg_size | 플롯 메시지 크기 [B] |

## runs.csv, sweep.csv 의 지표

통계는 갱신 20 번 (2 s) 뒤의 트랙 줄만 씁니다 (2-Point 개시 직후의 큰 오차를 빼려고). 선회 / 직진은 truth.csv 의 turn 으로 나눕니다.

| 열 | 뜻 |
| --- | --- |
| sigma_r, sigma_az_deg, sigma_el_deg, q, tau | R 표준편차 [m, °, °], Singer q [m/s²], τ [s] |
| proc, lost | 처리한 스캔, 잃은 메시지 (runs.csv) |
| ids, created, deleted | 나온 트랙 ID 수, 만든 · 지운 트랙 (sweep.csv 의 ids 는 30 회 평균) |
| init_time, confirm_time | 첫 트랙 개시 · 확정 시각 [s] |
| rms, rms_turn, rms_straight | 위치 오차 RMS: 전체 / 선회 / 직진 [m] |
| max_err, max_err_turn | 최대 위치 오차: 전체 / 선회 [m] |
| mean_nis, gate_out_pct | NIS 평균 (맞으면 3), 게이트 밖 비율 [%] |
| max_run | 게이트 밖 최대 연속 횟수 (sweep.csv 는 30 회 평균) |
| divergence | 발산 판정 횟수 (sweep.csv 는 한 번 실행당 평균) |
| diverged_seeds | 30 회 가운데 발산 판정이 한 번이라도 난 회차 수 (sweep.csv) |
| mean_nees | NEES 평균 (맞으면 3) |
| sweep, seeds, rms_sd | 실험 종류 (`q_sigma`: q × R σ, `tau`: 조정 값에서 τ 만), seed 수, 회차별 RMS 의 표준편차 |

sweep 의 seed k 는 12345 + 1000 k (k = 0 이 프로그램 기본 잡음).

## 발표 그림이 쓰는 데이터 (`tools/make_figures.py`)

| 그림 | 데이터 |
| --- | --- |
| fig_role (1.1) | air_tuned 의 truth · plots · tracks.csv, 22 ~ 34 s |
| fig_runs (1.6) | air_given/tracks.csv 의 nis · gate_run, 8.75 ~ 10.35 s |
| fig_scenarios (3.1) | 두 시나리오 truth.csv (위치, turn) |
| fig_accel_given (3.5) | air truth.csv 위치의 2 차 차분 (참값 가속도), air_given/tracks.csv 의 an · ae · ad |
| fig_lag_given, fig_lag_both (3.5, 4.6) | air_given · air_tuned 의 truth · plots · tracks.csv, 3 ~ 17.5 s |
| fig_plotnoise (4.2) | 두 시나리오 plots.csv − truth.csv (거리 · 방위각 · 고각 오차, 2,320 개) |
| fig_sweep_nis, fig_sweep_q, fig_sweep_tau (4.2, 4.3) | sweep.csv |
| fig_compare_air, fig_compare_ship (4.6) | 과제 값 · 조정 값 tracks.csv 의 err · nis, runs.csv |
| 개념 그림 (fig_kf_cycle, fig_hmatrix, fig_singer, fig_qprec, fig_chi2) | 데이터 없이 식으로 계산 (fig_qprec 는 mpmath 60 자리 기준값) |
