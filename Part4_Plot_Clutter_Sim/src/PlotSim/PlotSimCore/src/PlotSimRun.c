//
// @file	PlotSimRun.c
// @brief	설정 기본값과 스캔 루프.
//			스캔마다 표적 참값 -> 플롯 -> 클러터 순서로 만들고 표적을 1 스캔 시간(0.1 s)만큼 진행함.
// @author	hwan
// @date	2026.10.06.
//
#include <string.h>

#include "PlotSim.h"

// 표적 하나 초기값. 각도는 deg 로 받음.
static VOID f_Pcs_SetTarget(ST_TargetInit *st_Init, FLOAT64 latDeg, FLOAT64 lonDeg, FLOAT64 alt, FLOAT64 speed, FLOAT64 yawDeg)
{
	(VOID)memset(st_Init, 0, sizeof(*st_Init));
	st_Init->st_InitLla.Lat	= f_Deg_To_Rad(latDeg);
	st_Init->st_InitLla.Lon	= f_Deg_To_Rad(lonDeg);
	st_Init->st_InitLla.Alt	= alt;
	st_Init->st_InitAtt.Yaw	= f_Deg_To_Rad(yawDeg);
	st_Init->headingSpeed	= speed;
}

// 기동 하나 추가. gravityValue 부호가 회전 방향 (Yaw + 우선회).
static VOID f_Pcs_AddManeuver(ST_TargetInit *st_Init, EN_TurnType enTurnType, FLOAT64 gravityValue, FLOAT64 startTime, FLOAT64 endTime)
{
	ST_TargetManeuver *st_Maneuver;

	if (st_Init->nManeuverNum < TGT_MAX_MANEUVER_NUM)
	{
		st_Maneuver					= &st_Init->st_Maneuver[st_Init->nManeuverNum];
		st_Maneuver->enTurnType		= enTurnType;
		st_Maneuver->gravityValue	= gravityValue;
		st_Maneuver->startTime		= startTime;
		st_Maneuver->endTime		= endTime;
		st_Init->nManeuverNum++;
	}
}

//
// @brief	기본 설정. 시간, 플랫폼, 레이다 파라미터, 클러터는 과제 조건
// @param	st_Config	결과
// @param	enPreset	PCS_PRESET_AIR: 대공 표적 1개, PCS_PRESET_MULTI: 표적 5개
// @return	없음
// @author	hwan
//
VOID f_Pcs_DefaultConfig(ST_PcsConfig *st_Config, EN_PcsPreset enPreset)
{
	ST_SimConfig *st_Sim = &st_Config->st_Target;

	(VOID)memset(st_Config, 0, sizeof(*st_Config));

	st_Sim->durationTime	= PCS_SIM_TIME;
	st_Sim->stepTime		= PCS_SCAN_TIME;

	// 플랫폼 (레이다): 32.0도, 126.0도, 0 m, 정지
	st_Sim->st_Platform.st_InitLla.Lat	= f_Deg_To_Rad(32.0);
	st_Sim->st_Platform.st_InitLla.Lon	= f_Deg_To_Rad(126.0);
	st_Sim->st_Platform.st_InitLla.Alt	= 0.0;

	// 표적 1: 이전 차수 대공 표적
	f_Pcs_SetTarget(&st_Sim->st_Target[0], 32.12, 126.0, 300.0, 200.0, 180.0);
	st_Sim->nTargetNum = 1;

	if (enPreset == PCS_PRESET_MULTI)
	{
		f_Pcs_SetTarget(&st_Sim->st_Target[1], 32.125, 126.03, 0.0, 30.0, 270.0);		// 이전 차수 대함 표적
		f_Pcs_SetTarget(&st_Sim->st_Target[2], 32.05, 126.08, 1500.0, 150.0, 0.0);		// 대공, 10 ~ 40 s 좌선회
		f_Pcs_AddManeuver(&st_Sim->st_Target[2], TGT_TURN_YAW, -1.5, 10.0, 40.0);
		f_Pcs_SetTarget(&st_Sim->st_Target[3], 32.10, 125.93, 2000.0, 180.0, 110.0);	// 대공
		f_Pcs_SetTarget(&st_Sim->st_Target[4], 32.04, 125.96, 0.0, 20.0, 45.0);			// 대함
		st_Sim->nTargetNum = 5;
	}

	st_Config->st_Radar.snr			= PCS_SNR;
	st_Config->st_Radar.enSnrUnit	= PCS_SNR_LINEAR;
	st_Config->st_Radar.lightSpeed	= PCS_LIGHT_SPEED;
	st_Config->st_Radar.bandwidth	= PCS_BANDWIDTH;
	st_Config->st_Radar.beamAzi		= PCS_BEAM_AZI;
	st_Config->st_Radar.beamEle		= PCS_BEAM_ELE;
	st_Config->st_Radar.kmAzi		= PCS_KM_AZI;
	st_Config->st_Radar.kmEle		= PCS_KM_ELE;

	st_Config->st_Clutter.nClutterNum	= PCS_CLUTTER_NUM;
	st_Config->st_Clutter.enMode		= PCS_CLUTTER_FIXED;
	st_Config->st_Clutter.rngMin		= PCS_CLT_RNG_MIN;
	st_Config->st_Clutter.rngMax		= PCS_CLT_RNG_MAX;
	st_Config->st_Clutter.aziMin		= PCS_CLT_AZI_MIN;
	st_Config->st_Clutter.aziMax		= PCS_CLT_AZI_MAX;
	st_Config->st_Clutter.eleMin		= PCS_CLT_ELE_MIN;
	st_Config->st_Clutter.eleMax		= PCS_CLT_ELE_MAX;

	st_Config->seed = PCS_DEFAULT_SEED;
}

//
// @brief	시뮬레이션 시작. 설정 확인, 표준편차 계산 (SNR 고정이라 한 번), 표적 궤적 초기화, seed 설정
// @param	st_State	진행 상태
// @param	st_Config	설정
// @return	PCS_PASS / PCS_FAIL (설정 값 오류)
// @author	hwan
//
INT32 f_Pcs_InitSim(ST_PcsState *st_State, const ST_PcsConfig *st_Config)
{
	const ST_PcsClutterCfg *st_Clutter = &st_Config->st_Clutter;

	if ((st_Config->st_Target.nTargetNum < 1) || (st_Config->st_Target.nTargetNum > PCS_MAX_TARGET)
		|| (st_Clutter->nClutterNum < 0) || (st_Clutter->nClutterNum > PCS_MAX_CLUTTER)
		|| (st_Clutter->rngMax < st_Clutter->rngMin) || (st_Clutter->aziMax < st_Clutter->aziMin) || (st_Clutter->eleMax < st_Clutter->eleMin)
		|| (st_Config->seed < 1) || (st_Config->seed > 2147483646))
	{
		return PCS_FAIL;
	}

	(VOID)memset(st_State, 0, sizeof(*st_State));
	st_State->st_Config = *st_Config;

	if (f_Pcs_CalcSigma(&st_State->st_Sigma, &st_State->st_Config.st_Radar) != PCS_PASS)
	{
		return PCS_FAIL;
	}

	// 궤적 결과는 0 ~ nStepNum 번이고 스캔은 앞의 nStepNum 개 (t = 0.0 ~ 59.9 s)
	f_Tgt_InitSim(&st_State->st_Sim, &st_State->st_Config.st_Target);
	st_State->nScanNum		= st_State->st_Sim.nStepNum;
	st_State->nScanIndex	= 0;

	f_Pcs_SetSeed(st_State->st_Config.seed);

	return PCS_PASS;
}

//
// @brief	스캔 하나 진행. 표적마다 참값 x 와 플롯 z, 그 다음 클러터를 만들고 표적을 진행함
// @param	st_State	진행 상태
// @param	st_Scan		이번 스캔 결과
// @return	PCS_PASS. 스캔을 다 돌았으면 PCS_END
// @author	hwan
//
INT32 f_Pcs_StepScan(ST_PcsState *st_State, ST_PcsScan *st_Scan)
{
	const ST_SimSample		*st_Sample = &st_State->st_Sim.st_Sample;
	const ST_TargetState	*st_Radar = &st_Sample->st_Platform;
	ST_PcsSph				st_Sph[PCS_MAX_CLUTTER];
	ST_PcsPoint				*st_Point;
	INT32					nTarget;
	INT32					nClutter;

	if (st_State->nScanIndex >= st_State->nScanNum)
	{
		return PCS_END;
	}

	(VOID)memset(st_Scan, 0, sizeof(*st_Scan));
	st_Scan->nScanIndex		= st_State->nScanIndex;
	st_Scan->scanTime		= st_Sample->simTime;
	st_Scan->st_RadarLla	= st_Radar->st_Lla;
	st_Scan->nTargetNum		= st_Sample->nTargetNum;

	for (nTarget = 0; nTarget < st_Sample->nTargetNum; nTarget++)
	{
		// 참값: 위치는 Part 3 결과, 구좌표는 레이다 기준으로 변환
		st_Point			= &st_Scan->st_True[nTarget];
		st_Point->enKind	= PCS_KIND_TRUE;
		st_Point->nId		= nTarget + 1;
		st_Point->time		= st_Scan->scanTime;
		st_Point->st_Sph	= f_Pcs_EcefToSph(&st_Sample->st_Target[nTarget].st_PosEcef, st_Radar);
		st_Point->st_Lla	= st_Sample->st_Target[nTarget].st_Lla;

		// 플롯: 참값 + 잡음
		st_Point			= &st_Scan->st_Plot[nTarget];
		st_Point->enKind	= PCS_KIND_PLOT;
		st_Point->nId		= nTarget + 1;
		st_Point->time		= st_Scan->scanTime;
		st_Point->st_Sph	= f_Pcs_MakePlot(&st_Scan->st_True[nTarget].st_Sph, &st_State->st_Sigma);
		st_Point->st_Lla	= f_Pcs_SphToLla(&st_Point->st_Sph, st_Radar);
	}

	// 클러터: 표적과 상관없이 매 스캔 새로 뽑음
	st_Scan->nClutterNum = f_Pcs_MakeClutter(st_Sph, &st_State->st_Config.st_Clutter);

	for (nClutter = 0; nClutter < st_Scan->nClutterNum; nClutter++)
	{
		st_Point			= &st_Scan->st_Clutter[nClutter];
		st_Point->enKind	= PCS_KIND_CLUTTER;
		st_Point->nId		= nClutter + 1;
		st_Point->time		= st_Scan->scanTime;
		st_Point->st_Sph	= st_Sph[nClutter];
		st_Point->st_Lla	= f_Pcs_SphToLla(&st_Point->st_Sph, st_Radar);
	}

	f_Tgt_StepSim(&st_State->st_Sim);
	st_State->nScanIndex++;

	return PCS_PASS;
}
