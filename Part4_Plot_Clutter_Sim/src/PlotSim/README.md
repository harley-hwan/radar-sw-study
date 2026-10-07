# PlotSim

Part 4 플롯 / 클러터 모의.
Part 3 TargetSim 으로 대공 표적 참값을 진행하고, 레이다 안테나 기준 거리 / 방위각 / 고각에 가우시안 잡음을 더해 플롯을 만든다.
클러터는 FOV 안에서 균등분포로 뽑는다. 참값, 플롯, 클러터는 LLA 로 바꿔 색을 다르게 전시한다.

## 구성

- PlotSimCore (C, DLL)
  - PlotSim.h, PlotSim.c : 난수 검증, 측정 표준편차, 600 스캔 모의 (참값 -> 플롯 -> 클러터)
  - NoiseGeneration, CoordinateTransform, matrixCalcLib : 참고 소스 그대로
  - CommonDefine.h : NoiseGeneration.h 가 include 하는 헤더. 참고 소스에 없어서 Define.h 연결용으로 추가
  - Define.h, TargetSim.h, TargetSim.c : Part 3 그대로
- PlotSimUI (MFC) : SNR, 클러터 수 입력, 결과 요약, LLA 전시, 난수 히스토그램

## 빌드, 실행

- VS2022, Debug | x64 로 솔루션 빌드. 출력은 build/x64/Debug
- PlotSimUI 가 시작 프로젝트

## 조건

- 60 s, 0.1 s 간격 (600 스캔)
- 플랫폼 32.0도 / 126.0도 / 0 m, 정지, 자세 0
- 표적: 이전 차수 대공 표적 (32.12도 / 126.0도 / 300 m, 200 m/s, 기수 180도)
- SNR 20 (선형)
- 대역폭 5 MHz, 빔폭 5.14도 / 5.31도, K_M 1.7
- 클러터 스캔당 5개, 거리 200 ~ 15000 m, 방위각 -60 ~ 60도, 고각 0 ~ 30도
- seed 12345 (난수 검증, 모의 시작마다 다시 맞춤)

## 메모

- 표준편차 식은 과제 원문의 square(2 * SNR) 대신 sqrt(2 * SNR) 로 구현. 제곱이면 p_rng 가 0.019 m 로 잡음이 거의 없어짐
- 구좌표는 참고 소스 안테나 좌표계: 방위각은 조준축 기준 왼쪽 +, 고각은 위쪽 +. 플랫폼 자세가 0 이라 조준축은 북쪽이고 방위각 +60도 클러터는 서쪽에 찍힘
- NoiseGeneration.c 는 C4244 경고만 프로젝트 설정에서 끔
- windows.h 가 필요하면 PlotSim.h 보다 먼저 include (Define.h 의 INT32, VOID 가 매크로라서)
