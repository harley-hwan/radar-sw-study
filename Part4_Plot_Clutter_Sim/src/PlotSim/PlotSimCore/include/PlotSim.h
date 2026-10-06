//
// @file	PlotSim.h
// @brief	플롯 / 클러터 모의 Core.
//			표적 참값은 Part 3 TargetSim 으로 진행하고, 레이다 기준 거리 / 방위각 / 고각으로 바꾼 뒤 잡음을 더해 플롯을 만듦.
//			클러터는 FOV 안에서 균등분포로 뽑음. 참값, 플롯, 클러터 모두 전시용 LLA 를 같이 둠.
//			단위: 거리 m, 방위각 / 고각 deg, 위경도 rad, 시간 s
// @author	hwan
// @date	2026.10.06.
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

#define PCS_PASS					0
#define PCS_FAIL					(-1)
#define PCS_END						1						// 스캔을 다 돎

#define PCS_MAX_TARGET				TGT_MAX_TARGET_NUM
#define PCS_MAX_CLUTTER				50						// 스캔당 클러터 최대 개수
#define PCS_HIST_MAX_BIN			100

// 과제 조건
#define PCS_SIM_TIME				60.0					// [s]
#define PCS_SCAN_TIME				0.1						// [s] 1 스캔 시간
#define PCS_SNR						20.0
#define PCS_LIGHT_SPEED				299792458.0				// [m/s]
#define PCS_BANDWIDTH				5.0e6					// [Hz]
#define PCS_BEAM_AZI				5.14					// [deg] 방위각 빔폭
#define PCS_BEAM_ELE				5.31					// [deg] 고각 빔폭
#define PCS_KM_AZI					1.7						// 모노펄스 기울기 상수
#define PCS_KM_ELE					1.7
#define PCS_CLUTTER_NUM				5						// 스캔당 클러터 수
#define PCS_CLT_RNG_MIN				200.0					// [m]
#define PCS_CLT_RNG_MAX				15000.0
#define PCS_CLT_AZI_MIN				(-60.0)					// [deg]
#define PCS_CLT_AZI_MAX				60.0
#define PCS_CLT_ELE_MIN				0.0						// [deg]
#define PCS_CLT_ELE_MAX				30.0
#define PCS_RAND_TEST_NUM			10000					// 난수 검증 표본 수

#define PCS_UNIFORM_BIN_NUM			10						// [0, 1] 을 0.1 간격
#define PCS_GAUSS_BIN_NUM			32						// [-4, 4] 를 0.25 간격
#define PCS_GAUSS_BIN_MIN			(-4.0)
#define PCS_GAUSS_BIN_MAX			4.0
#define PCS_DEFAULT_SEED			12345					// NoiseGeneration.c 의 SEED 초기값

typedef enum
{
	PCS_KIND_TRUE				= 0,						// 참값
	PCS_KIND_PLOT				= 1,						// 플롯
	PCS_KIND_CLUTTER			= 2							// 클러터
} EN_PcsKind;

typedef enum
{
	PCS_SNR_LINEAR				= 0,
	PCS_SNR_DB					= 1							// 선형값 = 10^(SNR / 10)
} EN_PcsSnrUnit;

typedef enum
{
	PCS_CLUTTER_FIXED			= 0,						// 매 스캔 nClutterNum 개
	PCS_CLUTTER_RANDOM			= 1							// 매 스캔 0 ~ nClutterNum 개
} EN_PcsClutterMode;

typedef enum
{
	PCS_PRESET_AIR				= 0,						// 대공 표적 1개 (과제 조건)
	PCS_PRESET_MULTI			= 1							// 표적 5개 (대공 3, 대함 2)
} EN_PcsPreset;

//
// @struct	ST_PcsHist
// @brief	난수 검증용 히스토그램. [binMin, binMax] 를 nBin 개로 나누고 PDF 로 정규화.
//
typedef struct
{
	INT32				nSample;							// 표본 수
	INT32				nBin;
	FLOAT64				binMin;
	FLOAT64				binMax;
	FLOAT64				binWidth;							// (binMax - binMin) / nBin
	INT32				nCount[PCS_HIST_MAX_BIN];			// bin 별 개수
	FLOAT64				pdf[PCS_HIST_MAX_BIN];				// nCount / (nSample * binWidth)
	INT32				nOutside;							// 구간 밖 개수
	FLOAT64				mean;
	FLOAT64				std;								// N 으로 나눈 표준편차
	FLOAT64				minValue;
	FLOAT64				maxValue;
} ST_PcsHist;

//
// @struct	ST_PcsRadar
// @brief	측정 표준편차 계산에 쓰는 레이다 파라미터.
//
typedef struct
{
	FLOAT64				snr;
	EN_PcsSnrUnit		enSnrUnit;
	FLOAT64				lightSpeed;							// [m/s]
	FLOAT64				bandwidth;							// [Hz]
	FLOAT64				beamAzi;							// [deg]
	FLOAT64				beamEle;							// [deg]
	FLOAT64				kmAzi;
	FLOAT64				kmEle;
} ST_PcsRadar;

//
// @struct	ST_PcsSigma
// @brief	거리 / 방위각 / 고각 측정 표준편차. SNR 이 고정이라 시작할 때 한 번만 계산.
//
typedef struct
{
	FLOAT64				snrLinear;							// 계산에 쓴 선형 SNR
	FLOAT64				rng;								// [m]
	FLOAT64				azi;								// [deg]
	FLOAT64				ele;								// [deg]
} ST_PcsSigma;

//
// @struct	ST_PcsSph
// @brief	레이다 기준 구좌표. 방위각은 진북 기준 시계방향 +, 고각은 수평면 위쪽 +.
//
typedef struct
{
	FLOAT64				rng;								// [m]
	FLOAT64				azi;								// [deg] -180 ~ 180
	FLOAT64				ele;								// [deg]
} ST_PcsSph;

//
// @struct	ST_PcsClutterCfg
// @brief	클러터 개수와 FOV.
//
typedef struct
{
	INT32				nClutterNum;						// FIXED 면 스캔당 개수, RANDOM 이면 최대 개수
	EN_PcsClutterMode	enMode;
	FLOAT64				rngMin;								// [m]
	FLOAT64				rngMax;
	FLOAT64				aziMin;								// [deg]
	FLOAT64				aziMax;
	FLOAT64				eleMin;								// [deg]
	FLOAT64				eleMax;
} ST_PcsClutterCfg;

//
// @struct	ST_PcsConfig
// @brief	시뮬레이션 설정. 시간, 플랫폼(레이다), 표적은 Part 3 설정 그대로 씀.
//
typedef struct
{
	ST_SimConfig		st_Target;
	ST_PcsRadar			st_Radar;
	ST_PcsClutterCfg	st_Clutter;
	INT32				seed;								// 1 ~ 2147483646
} ST_PcsConfig;

//
// @struct	ST_PcsPoint
// @brief	점 하나 (참값, 플롯, 클러터 공통).
//
typedef struct
{
	EN_PcsKind			enKind;
	INT32				nId;								// 표적 번호 (1 ~). 클러터는 스캔 안 순번
	FLOAT64				time;								// [s]
	ST_PcsSph			st_Sph;
	STRUCT_Coord_Lla	st_Lla;								// 위도, 경도 [rad], 고도 [m]
} ST_PcsPoint;

//
// @struct	ST_PcsScan
// @brief	스캔 하나의 결과.
//
typedef struct
{
	INT32				nScanIndex;
	FLOAT64				scanTime;							// [s]
	STRUCT_Coord_Lla	st_RadarLla;
	INT32				nTargetNum;
	ST_PcsPoint			st_True[PCS_MAX_TARGET];
	ST_PcsPoint			st_Plot[PCS_MAX_TARGET];
	INT32				nClutterNum;
	ST_PcsPoint			st_Clutter[PCS_MAX_CLUTTER];
} ST_PcsScan;

//
// @struct	ST_PcsState
// @brief	스캔 진행 상태.
//
typedef struct
{
	ST_PcsConfig		st_Config;
	ST_SimState			st_Sim;								// 표적 궤적 (Part 3)
	ST_PcsSigma			st_Sigma;
	INT32				nScanNum;							// 60 / 0.1 = 600
	INT32				nScanIndex;							// 다음 스캔 번호
} ST_PcsState;

// 난수
PCS_API VOID				f_Pcs_SetSeed(INT32 seed);
PCS_API INT32				f_Pcs_GetSeed(VOID);
PCS_API INT32				f_Pcs_TestUniform(ST_PcsHist *st_Hist, INT32 nSample, INT32 nBin);
PCS_API INT32				f_Pcs_TestGauss(ST_PcsHist *st_Hist, INT32 nSample, INT32 nBin, FLOAT64 binMin, FLOAT64 binMax);

// 플롯, 클러터
PCS_API INT32				f_Pcs_CalcSigma(ST_PcsSigma *st_Sigma, const ST_PcsRadar *st_Radar);
PCS_API ST_PcsSph			f_Pcs_EcefToSph(const STRUCT_Coord_Rect *st_TgtEcef, const ST_TargetState *st_Radar);
PCS_API STRUCT_Coord_Lla	f_Pcs_SphToLla(const ST_PcsSph *st_Sph, const ST_TargetState *st_Radar);
PCS_API ST_PcsSph			f_Pcs_MakePlot(const ST_PcsSph *st_True, const ST_PcsSigma *st_Sigma);
PCS_API INT32				f_Pcs_MakeClutter(ST_PcsSph *st_Clutter, const ST_PcsClutterCfg *st_Cfg);

// 시뮬레이션
PCS_API VOID				f_Pcs_DefaultConfig(ST_PcsConfig *st_Config, EN_PcsPreset enPreset);
PCS_API INT32				f_Pcs_InitSim(ST_PcsState *st_State, const ST_PcsConfig *st_Config);
PCS_API INT32				f_Pcs_StepScan(ST_PcsState *st_State, ST_PcsScan *st_Scan);

#ifdef __cplusplus
}
#endif

#endif
