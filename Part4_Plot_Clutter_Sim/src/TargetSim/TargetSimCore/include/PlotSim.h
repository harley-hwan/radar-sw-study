#ifndef PLOT_SIM_H
#define PLOT_SIM_H

#include "TargetSim.h"

#ifdef __cplusplus
extern "C" {
#endif

// 레이다 (과제 조건). 레이다는 Part 3 의 플랫폼 (위치, 자세) 을 그대로 씀.
#define PCS_BANDWIDTH			5.0e6					// [Hz]
#define PCS_BEAM_AZI			5.14					// [deg] 방위각 빔폭
#define PCS_BEAM_ELE			5.31					// [deg] 고각 빔폭
#define PCS_KM					1.7						// 모노펄스 기울기 상수 (방위각, 고각 같음)

// 클러터 FOV (레이다 안테나 기준)
#define PCS_CLT_RNG_MIN			200.0					// [m]
#define PCS_CLT_RNG_MAX			15000.0
#define PCS_CLT_AZI_MIN			(-60.0)					// [deg]
#define PCS_CLT_AZI_MAX			60.0
#define PCS_CLT_ELE_MIN			0.0						// [deg]
#define PCS_CLT_ELE_MAX			30.0
#define PCS_MAX_CLUTTER			20						// 스캔당 클러터 최대 개수
#define PCS_SEED				12345					// NoiseGeneration.c 의 SEED 초기값

// 난수 검증
#define PCS_RAND_NUM			10000					// 표본 수
#define PCS_UNIFORM_BIN			10						// [0, 1] 을 0.1 간격
#define PCS_GAUSS_BIN			32						// [-4, 4] 를 0.25 간격
#define PCS_GAUSS_MIN			(-4.0)
#define PCS_GAUSS_MAX			4.0

// 난수 히스토그램. 막대 넓이 합이 1 이 되게 PDF 로 맞춤.
typedef struct
{
	INT32				nBin;
	FLOAT64				binMin;
	FLOAT64				binWidth;
	FLOAT64				pdf[PCS_GAUSS_BIN];				// 개수 / (표본 수 * bin 폭)
	FLOAT64				mean;
	FLOAT64				std;
} ST_PcsHist;

// 스캔 하나의 측정 결과. 구좌표는 레이다 안테나 좌표계 (방위각 왼쪽 +, 고각 위쪽 +), 거리 m, 각도 rad.
typedef struct
{
	FLOAT64				time;							// [s]
	INT32				nTargetNum;
	STRUCT_Coord_Sph	st_True[TGT_MAX_TARGET_NUM];	// 참값 x. [0] 이 표적 1
	STRUCT_Coord_Sph	st_Plot[TGT_MAX_TARGET_NUM];	// 플롯 z = x + v
	STRUCT_Coord_Lla	st_PlotLla[TGT_MAX_TARGET_NUM];	// 전시용. 위도, 경도 [rad], 고도 [m]
	INT32				nClutterNum;
	STRUCT_Coord_Sph	st_Clutter[PCS_MAX_CLUTTER];
	STRUCT_Coord_Lla	st_ClutterLla[PCS_MAX_CLUTTER];
} ST_PcsScan;

TSCORE_API VOID				f_Pcs_TestRand(ST_PcsHist *st_Uniform, ST_PcsHist *st_Gauss);
TSCORE_API STRUCT_Coord_Sph	f_Pcs_CalcSigma(FLOAT64 snr);
TSCORE_API VOID				f_Pcs_Measure(ST_PcsScan *st_Scan, const ST_SimSample *st_Sample, INT32 nScanNum, FLOAT64 snr, INT32 nClutterNum);

#ifdef __cplusplus
}
#endif

#endif
