//
// @file	PlotSim.h
// @brief	플롯 / 클러터 모의 Core.
//			참값은 Part 3 TargetSim 의 표적 5개 (과제 3 대공 · 대함 표적 + 추가 3개), 플롯은 참값 + 가우시안 잡음,
//			클러터는 FOV 안 균등분포.
//			구좌표는 참고 소스 안테나 좌표계 그대로 씀 (방위각은 조준축 기준 왼쪽 +, 고각은 위쪽 +).
//			단위: 거리 m, 각도 rad, 시간 s
// @author	hwan
// @date	2026.10.07.
//
#ifndef PLOT_SIM_H
#define PLOT_SIM_H

#include "Define.h"
#include "CoordinateTransform.h"
#include "TargetSim.h"

// DLL 내보내기 / 가져오기. Core 빌드 시에만 PLOTSIMCORE_EXPORTS 정의.
#if defined(_WIN32)
	#ifdef PLOTSIMCORE_EXPORTS
		#define PCS_API		__declspec(dllexport)
	#else
		#define PCS_API		__declspec(dllimport)
	#endif
#else
	#define PCS_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 과제 조건
#define PCS_SCAN_NUM			600						// 60 s / 0.1 s
#define PCS_SCAN_TIME			0.1						// [s]
#define PCS_RADAR_LAT			32.0					// [deg] 레이다 (플랫폼, 정지)
#define PCS_RADAR_LON			126.0					// [deg]
#define PCS_BANDWIDTH			5.0e6					// [Hz]
#define PCS_BEAM_AZI			5.14					// [deg] 방위각 빔폭
#define PCS_BEAM_ELE			5.31					// [deg] 고각 빔폭
#define PCS_KM					1.7						// 모노펄스 기울기 상수 (방위각, 고각 같음)
#define PCS_CLT_RNG_MIN			200.0					// [m] 클러터 FOV
#define PCS_CLT_RNG_MAX			15000.0
#define PCS_CLT_AZI_MIN			(-60.0)					// [deg]
#define PCS_CLT_AZI_MAX			60.0
#define PCS_CLT_ELE_MIN			0.0						// [deg]
#define PCS_CLT_ELE_MAX			30.0
#define PCS_TARGET_NUM			5						// 표적 수. 1, 2 는 과제 3 대공 / 대함 표적, 3 ~ 5 는 추가
#define PCS_MAX_CLUTTER			20						// 스캔당 클러터 최대 개수
#define PCS_SEED				12345					// NoiseGeneration.c 의 SEED 초기값

// 난수 검증
#define PCS_RAND_NUM			10000					// 표본 수
#define PCS_UNIFORM_BIN			10						// [0, 1] 을 0.1 간격
#define PCS_GAUSS_BIN			32						// [-4, 4] 를 0.25 간격
#define PCS_GAUSS_MIN			(-4.0)
#define PCS_GAUSS_MAX			4.0

//
// @struct	ST_PcsHist
// @brief	난수 히스토그램. 막대 넓이 합이 1 이 되게 PDF 로 맞춤.
//
typedef struct
{
	INT32				nBin;
	FLOAT64				binMin;
	FLOAT64				binWidth;
	FLOAT64				pdf[PCS_GAUSS_BIN];				// 개수 / (표본 수 * bin 폭)
	FLOAT64				mean;
	FLOAT64				std;
} ST_PcsHist;

//
// @struct	ST_PcsScan
// @brief	스캔 하나의 결과. 표적은 [0] 이 표적 1. 구좌표는 레이다 안테나 기준, LLA 는 전시용.
//
typedef struct
{
	FLOAT64				time;							// [s]
	STRUCT_Coord_Sph	st_True[PCS_TARGET_NUM];		// 참값 x
	STRUCT_Coord_Sph	st_Plot[PCS_TARGET_NUM];		// 플롯 z = x + v
	STRUCT_Coord_Lla	st_TrueLla[PCS_TARGET_NUM];		// 위도, 경도 [rad], 고도 [m]
	STRUCT_Coord_Lla	st_PlotLla[PCS_TARGET_NUM];
	INT32				nClutterNum;
	STRUCT_Coord_Sph	st_Clutter[PCS_MAX_CLUTTER];
	STRUCT_Coord_Lla	st_ClutterLla[PCS_MAX_CLUTTER];
} ST_PcsScan;

PCS_API VOID				f_Pcs_TestRand(ST_PcsHist *st_Uniform, ST_PcsHist *st_Gauss);
PCS_API STRUCT_Coord_Sph	f_Pcs_CalcSigma(FLOAT64 snr);
PCS_API VOID				f_Pcs_Run(ST_PcsScan *st_Scan, FLOAT64 snr, INT32 nClutterNum);

#ifdef __cplusplus
}
#endif

#endif
