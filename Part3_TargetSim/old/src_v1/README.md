> **백업 (2026-09-12)** : 오른쪽 화면이 지도 하나뿐이던 1판. 현재 판(`../../src/`) 은 레이다 화면(PPI) 과 무작위 표적 10 개 버튼이 추가된 것. 이 폴더는 참고용이며 빌드 대상이 아니다.

# Part 3 표적 궤적 모의

과제 3 조건대로 표적을 60 초 동안 0.1 초 간격으로 움직여 위경도 궤적을 그린다.
좌표변환은 `ref/참고 소스 코드/` 의 `CoordinateTransform.c` · `matrixCalcLib.c` · `Define.h` 를 손대지 않고 그대로 쓴다.

| 폴더 | 내용 |
|---|---|
| `TargetSimCore/` | C 정적 라이브러리. `target_sim.h/.c` (표적 구조체, 한 스텝 갱신, 과제 조건) + 제공 코드 5 파일 |
| `TargetSimUI/` | MFC 대화상자. [실행] → 궤적 지도 · 시각 슬라이더 · [재생] (1~10 배속) · 위경도 표, [CSV 저장] |
| `tools/plot_trajectory.py` | 저장한 CSV 를 matplotlib 그림으로 (평면 궤적, 고도) |

Visual Studio 2022 에서 `TargetSim.sln` 을 x64 로 빌드하고 `build\x64\Release\TargetSimUI.exe` 를 실행한다.
"기동 예시 포함" 을 켜면 대공 표적 3 g 우선회(10~20 s) · 2 g 기수 들기(35~40 s), 대함 표적 0.1 g 좌선회(20~50 s) 가 붙는다.

한 스텝 (`f_StepTarget`) : 기동 판단 → 자세 갱신 (ω = n·g / V) → 속도 변환 (동체 [V,0,0] → NED → ECEF, 플랫폼 위치 인자 0) → 위치 적분 (ECEF, p += v·dt) → LLA 변환.
LLA 변환 결과가 다음 스텝의 NED 기준이 되어 수평 비행이 지구 곡률을 따라간다.

제공 코드에서 조심한 것 : `f_Trans_Lla_To_Ecef(Lon, Lat, Alt)` 는 경도가 첫째 인자, `f_Trans_Body_To_Ned(..., Roll, Yaw, Pitch)` 순서, 각도는 라디안, `Define.h` 의 `#define BOOL bool` 은 MFC 쪽에서 `#undef`.
