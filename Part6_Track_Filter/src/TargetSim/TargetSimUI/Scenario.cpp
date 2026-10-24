// 고정 시나리오 값 -> Core 입력, 궤적 계산 실행과 그림 좌표 계산
#include "pch.h"

#include <math.h>

#include "Scenario.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define SCN_MANEUVER_NUM		10

// 객체 하나의 초기값. 단위는 과제 명세 그대로 (도, m, m/s).
typedef struct
{
	FLOAT64				lat;					// [deg]
	FLOAT64				lon;					// [deg]
	FLOAT64				alt;					// [m]
	FLOAT64				speed;					// [m/s] 기수 방향 속력
	FLOAT64				roll;					// [deg]
	FLOAT64				pitch;					// [deg]
	FLOAT64				yaw;					// [deg]
} ST_ScnObject;

// 기동 1건과 그 시나리오
typedef struct
{
	INT32				nScenario;				// SCN_SHIP, SCN_AIR
	ST_TargetManeuver	st_Maneuver;
} ST_ScnManeuver;

static_assert(SCN_MANEUVER_NUM <= TGT_MAX_MANEUVER_NUM, "SCN_MANEUVER_NUM is too large");

// 과제 5 응용 시나리오의 두 표적을 하나씩: 대함 = 표적 1, 대공 = 표적 2 (고도 2,000 m)
static const ST_ScnObject	s_Platform = { 32.0, 126.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
static const ST_ScnObject	s_Target[SCN_NUM] =
{
	{ 32.125,	126.03,		0.0,		30.0,		0.0,	0.0,	270.0 },
	{ 32.12,	126.0,		2000.0,		200.0,		0.0,	0.0,	180.0 }
};

// G 부호 = 회전 방향 (+ 우선회). 대함: Yaw 1 G 45 도 x 4, 대공: Yaw 4 G 90 도 x 6 (마지막 150 도)
static const ST_ScnManeuver	s_Maneuver[SCN_MANEUVER_NUM] =
{
	{ SCN_SHIP, { TGT_TURN_YAW, -1.0, 28.4, 30.8 } },
	{ SCN_SHIP, { TGT_TURN_YAW, 1.0, 41.4, 43.8 } },
	{ SCN_SHIP, { TGT_TURN_YAW, 1.0, 72.2, 74.6 } },
	{ SCN_SHIP, { TGT_TURN_YAW, -1.0, 85.2, 87.6 } },
	{ SCN_AIR, { TGT_TURN_YAW, -4.0, 6.0, 14.0 } },
	{ SCN_AIR, { TGT_TURN_YAW, 4.0, 24.1, 32.1 } },
	{ SCN_AIR, { TGT_TURN_YAW, 4.0, 35.2, 43.2 } },
	{ SCN_AIR, { TGT_TURN_YAW, -4.0, 67.5, 75.5 } },
	{ SCN_AIR, { TGT_TURN_YAW, -4.0, 78.5, 86.5 } },
	{ SCN_AIR, { TGT_TURN_YAW, 4.0, 102.6, 116.0 } }
};

// 시나리오 이름, 설명
static const LPCTSTR		s_Name[SCN_NUM] = { _T("대함 표적"), _T("대공 표적") };
static const LPCTSTR		s_Note[SCN_NUM] =
{
	_T("고도 0 m, 30 m/s 서쪽, 1 G 선회 4 번 (45°)"),
	_T("고도 2,000 m, 200 m/s 남쪽, 4 G 선회 6 번 (90°)")
};

// 조정 값 (검증 프로그램의 30 개 잡음 시드 실험에서 발산 판정 0 번이고 위치 오차 RMS 가 가장 작은 값)
// R 의 각도 σ 는 실제 플롯 잡음 (SNR 20: 0.48°, 0.49°) 에 맞춰 0.5°, q 는 대공 표적 4 G 선회에 맞춰 20 m/s^2
static const ST_TrkParam	s_Tuned[SCN_NUM] =
{
	{ 10.0, 0.5, 0.5, 0.5, 10.0 },
	{ 10.0, 0.5, 0.5, 20.0, 10.0 }
};

// 초기값 -> Core 입력. 각도는 도 -> 라디안.
static VOID f_Scn_SetObject(const ST_ScnObject *st_Object, STRUCT_Coord_Lla *st_Lla, STRUCT_Coord_Attitude *st_Att, FLOAT64 *pt_Speed)
{
	st_Lla->Lat		= f_Deg_To_Rad(st_Object->lat);
	st_Lla->Lon		= f_Deg_To_Rad(st_Object->lon);
	st_Lla->Alt		= st_Object->alt;
	*pt_Speed		= st_Object->speed;
	st_Att->Roll	= f_Deg_To_Rad(st_Object->roll);
	st_Att->Pitch	= f_Deg_To_Rad(st_Object->pitch);
	st_Att->Yaw		= f_Deg_To_Rad(st_Object->yaw);
}

// 시나리오 하나 -> ST_SimConfig (표적 하나, 계산 기록 없음)
VOID f_Scn_BuildConfig(ST_SimConfig *st_Config, INT32 nScenario)
{
	const INT32		nScn = ((nScenario >= 0) && (nScenario < SCN_NUM)) ? nScenario : SCN_SHIP;
	ST_TargetInit	*st_Init = &st_Config->st_Target[0];
	INT32			nIndex;

	*st_Config				= {};
	st_Config->durationTime	= SCN_DURATION_TIME;
	st_Config->stepTime		= SCN_STEP_TIME;
	st_Config->nTargetNum	= 1;

	f_Scn_SetObject(&s_Platform, &st_Config->st_Platform.st_InitLla, &st_Config->st_Platform.st_InitAtt, &st_Config->st_Platform.headingSpeed);
	f_Scn_SetObject(&s_Target[nScn], &st_Init->st_InitLla, &st_Init->st_InitAtt, &st_Init->headingSpeed);

	for (nIndex = 0; nIndex < SCN_MANEUVER_NUM; nIndex++)
	{
		if (s_Maneuver[nIndex].nScenario == nScn)
		{
			st_Init->st_Maneuver[st_Init->nManeuverNum]	= s_Maneuver[nIndex].st_Maneuver;
			st_Init->nManeuverNum						= st_Init->nManeuverNum + 1;
		}
	}
}

// 시나리오 이름 ("대함 표적", "대공 표적")
LPCTSTR f_Scn_GetName(INT32 nScenario)
{
	return s_Name[((nScenario >= 0) && (nScenario < SCN_NUM)) ? nScenario : SCN_SHIP];
}

// 시나리오 설명 (고도, 속력, 기동)
LPCTSTR f_Scn_GetNote(INT32 nScenario)
{
	return s_Note[((nScenario >= 0) && (nScenario < SCN_NUM)) ? nScenario : SCN_SHIP];
}

// 시나리오의 조정 값
VOID f_Scn_GetTunedParam(ST_TrkParam *st_Param, INT32 nScenario)
{
	*st_Param = s_Tuned[((nScenario >= 0) && (nScenario < SCN_NUM)) ? nScenario : SCN_SHIP];
}

// 원점의 위도 · 경도 1 도 길이 (WGS-84 자오선 곡률 반경 M, 묘유선 곡률 반경 N)
VOID f_Scn_InitMap(ST_GeoMap *st_Map, const STRUCT_Coord_Lla *st_Origin)
{
	const FLOAT64 sinLat = sin(st_Origin->Lat);
	const FLOAT64 denom = 1.0 - (FirstEccentricitySquared * sinLat * sinLat);
	const FLOAT64 radiusN = R_1 / sqrt(denom);
	const FLOAT64 radiusM = (R_1 * (1.0 - FirstEccentricitySquared)) / (denom * sqrt(denom));

	st_Map->lat0		= f_Rad_To_Deg(st_Origin->Lat);
	st_Map->lon0		= f_Rad_To_Deg(st_Origin->Lon);
	st_Map->meterPerLat	= f_Deg_To_Rad(1.0) * radiusM;
	st_Map->meterPerLon	= f_Deg_To_Rad(1.0) * radiusN * cos(st_Origin->Lat);
}

// 위경도 [rad], 고도 -> 그림 좌표
ST_PlotPoint f_Scn_LlaToPoint(const ST_GeoMap *st_Map, const STRUCT_Coord_Lla *st_Lla)
{
	ST_PlotPoint st_Point;

	st_Point.east	= f_Scn_LonToEast(st_Map, f_Rad_To_Deg(st_Lla->Lon));
	st_Point.north	= f_Scn_LatToNorth(st_Map, f_Rad_To_Deg(st_Lla->Lat));
	st_Point.up		= st_Lla->Alt;

	return st_Point;
}

// 그림 좌표 북 [m] -> 위도 [deg]
FLOAT64 f_Scn_NorthToLat(const ST_GeoMap *st_Map, FLOAT64 north)
{
	return st_Map->lat0 + (north / st_Map->meterPerLat);
}

// 그림 좌표 동 [m] -> 경도 [deg]
FLOAT64 f_Scn_EastToLon(const ST_GeoMap *st_Map, FLOAT64 east)
{
	return st_Map->lon0 + (east / st_Map->meterPerLon);
}

// 위도 [deg] -> 그림 좌표 북 [m]
FLOAT64 f_Scn_LatToNorth(const ST_GeoMap *st_Map, FLOAT64 latDeg)
{
	return (latDeg - st_Map->lat0) * st_Map->meterPerLat;
}

// 경도 [deg] -> 그림 좌표 동 [m]
FLOAT64 f_Scn_LonToEast(const ST_GeoMap *st_Map, FLOAT64 lonDeg)
{
	return (lonDeg - st_Map->lon0) * st_Map->meterPerLon;
}

// CSimResult

// 전 구간 실행 후 표본 수집, 그림 좌표 계산.
VOID CSimResult::f_Run(const ST_SimConfig *st_NewConfig)
{
	ST_SimState	st_Sim = {};
	INT32		nStep;

	f_Tgt_InitSim(&st_Sim, st_NewConfig);

	st_Config	= *st_NewConfig;
	nObjectNum	= st_NewConfig->nTargetNum + 1;
	st_SampleBuf.clear();
	st_SampleBuf.push_back(st_Sim.st_Sample);

	for (nStep = 0; nStep < st_Sim.nStepNum; nStep++)
	{
		f_Tgt_StepSim(&st_Sim);
		st_SampleBuf.push_back(st_Sim.st_Sample);
	}

	f_ComputePoints();
}

// 표본 -> 그림 좌표 (원점 = 플랫폼 시작 위치)
VOID CSimResult::f_ComputePoints(VOID)
{
	INT32 nStep;
	INT32 nObject;

	f_Scn_InitMap(&st_Map, &f_GetState(0, 0)->st_Lla);
	st_PointBuf.clear();

	for (nStep = 0; nStep < f_GetSampleNum(); nStep++)
	{
		for (nObject = 0; nObject < nObjectNum; nObject++)
		{
			st_PointBuf.push_back(f_Scn_LlaToPoint(&st_Map, &f_GetState(nStep, nObject)->st_Lla));
		}
	}
}

// 표본 수.
INT32 CSimResult::f_GetSampleNum(VOID) const
{
	return static_cast<INT32>(st_SampleBuf.size());
}

// 객체 수 (플랫폼 + 표적).
INT32 CSimResult::f_GetObjectNum(VOID) const
{
	return nObjectNum;
}

// 스텝 하나의 표본.
const ST_SimSample *CSimResult::f_GetSample(INT32 nStep) const
{
	return &st_SampleBuf[static_cast<UINT64>(nStep)];
}

// 스텝, 객체 하나의 상태. 객체 0 = 플랫폼.
const ST_TargetState *CSimResult::f_GetState(INT32 nStep, INT32 nObject) const
{
	const ST_SimSample *st_Sample = f_GetSample(nStep);

	return (nObject == 0) ? &st_Sample->st_Platform : &st_Sample->st_Target[nObject - 1];
}

// 스텝, 객체 하나의 그림 좌표.
const ST_PlotPoint *CSimResult::f_GetPoint(INT32 nStep, INT32 nObject) const
{
	return &st_PointBuf[static_cast<UINT64>((nStep * nObjectNum) + nObject)];
}

// 위경도 <-> 그림 좌표 변환 값
const ST_GeoMap *CSimResult::f_GetMap(VOID) const
{
	return &st_Map;
}

// 실행한 설정
const ST_SimConfig *CSimResult::f_GetConfig(VOID) const
{
	return &st_Config;
}
