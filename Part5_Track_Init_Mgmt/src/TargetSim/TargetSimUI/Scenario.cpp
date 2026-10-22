// 고정 시나리오 값 -> Core 입력, 궤적 계산 실행과 그림 좌표 계산
#include "pch.h"

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

// 기동 1건과 그 표적
typedef struct
{
	INT32				nTarget;				// 0 = 표적 1
	ST_TargetManeuver	st_Maneuver;
} ST_ScnManeuver;

static_assert(SCN_TARGET_NUM <= TGT_MAX_TARGET_NUM, "SCN_TARGET_NUM is too large");
static_assert(SCN_MANEUVER_NUM <= TGT_MAX_MANEUVER_NUM, "SCN_MANEUVER_NUM is too large");

// 응용 시나리오 (Part 3 지그재그, 원본 크기). 표적 2 고도만 300 -> 2,000 m (3D 에서 구분되게)
static const ST_ScnObject	s_Platform = { 32.0, 126.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
static const ST_ScnObject	s_Target[SCN_TARGET_NUM] =
{
	{ 32.125,	126.03,		0.0,		30.0,		0.0,	0.0,	270.0 },
	{ 32.12,	126.0,		2000.0,		200.0,		0.0,	0.0,	180.0 }
};

// G 부호 = 회전 방향 (+ 우선회). 표적 1: Yaw 1 G 45 도 x 4, 표적 2: Yaw 4 G 90 도 x 6 (마지막 150 도)
static const ST_ScnManeuver	s_Maneuver[SCN_MANEUVER_NUM] =
{
	{ 0, { TGT_TURN_YAW, -1.0, 28.4, 30.8 } },
	{ 0, { TGT_TURN_YAW, 1.0, 41.4, 43.8 } },
	{ 0, { TGT_TURN_YAW, 1.0, 72.2, 74.6 } },
	{ 0, { TGT_TURN_YAW, -1.0, 85.2, 87.6 } },
	{ 1, { TGT_TURN_YAW, -4.0, 6.0, 14.0 } },
	{ 1, { TGT_TURN_YAW, 4.0, 24.1, 32.1 } },
	{ 1, { TGT_TURN_YAW, 4.0, 35.2, 43.2 } },
	{ 1, { TGT_TURN_YAW, -4.0, 67.5, 75.5 } },
	{ 1, { TGT_TURN_YAW, -4.0, 78.5, 86.5 } },
	{ 1, { TGT_TURN_YAW, 4.0, 102.6, 116.0 } }
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

// 고정 시나리오 -> ST_SimConfig (기본 계산, 계산 기록 없음)
VOID f_Scn_BuildConfig(ST_SimConfig *st_Config)
{
	ST_TargetInit	*st_Init;
	INT32			nTarget;
	INT32			nIndex;

	*st_Config				= {};
	st_Config->durationTime	= SCN_DURATION_TIME;
	st_Config->stepTime		= SCN_STEP_TIME;
	st_Config->nTargetNum	= SCN_TARGET_NUM;

	f_Scn_SetObject(&s_Platform, &st_Config->st_Platform.st_InitLla, &st_Config->st_Platform.st_InitAtt, &st_Config->st_Platform.headingSpeed);

	for (nTarget = 0; nTarget < SCN_TARGET_NUM; nTarget++)
	{
		st_Init = &st_Config->st_Target[nTarget];
		f_Scn_SetObject(&s_Target[nTarget], &st_Init->st_InitLla, &st_Init->st_InitAtt, &st_Init->headingSpeed);
	}

	for (nIndex = 0; nIndex < SCN_MANEUVER_NUM; nIndex++)
	{
		st_Init										= &st_Config->st_Target[s_Maneuver[nIndex].nTarget];
		st_Init->st_Maneuver[st_Init->nManeuverNum]	= s_Maneuver[nIndex].st_Maneuver;
		st_Init->nManeuverNum						= st_Init->nManeuverNum + 1;
	}
}

// CSimResult

// 전 구간 실행 후 표본 수집, 그림 좌표 계산.
VOID CSimResult::f_Run(const ST_SimConfig *st_Config)
{
	ST_SimState	st_Sim = {};
	INT32		nStep;

	f_Tgt_InitSim(&st_Sim, st_Config);

	nObjectNum = st_Config->nTargetNum + 1;
	st_SampleBuf.clear();
	st_SampleBuf.push_back(st_Sim.st_Sample);

	for (nStep = 0; nStep < st_Sim.nStepNum; nStep++)
	{
		f_Tgt_StepSim(&st_Sim);
		st_SampleBuf.push_back(st_Sim.st_Sample);
	}

	f_ComputePoints();
}

// 표본 -> 플랫폼 초기 위치 기준 동-북-위 좌표.
VOID CSimResult::f_ComputePoints(VOID)
{
	const ST_TargetState	*st_Origin = f_GetState(0, 0);
	STRUCT_Coord_Rect		st_Ned;
	ST_PlotPoint			st_Point;
	INT32					nStep;
	INT32					nObject;

	st_PointBuf.clear();

	for (nStep = 0; nStep < f_GetSampleNum(); nStep++)
	{
		for (nObject = 0; nObject < nObjectNum; nObject++)
		{
			st_Ned			= f_Tgt_EcefToNed(&f_GetState(nStep, nObject)->st_PosEcef, &st_Origin->st_PosEcef, &st_Origin->st_Lla);
			st_Point.east	= st_Ned.y;
			st_Point.north	= st_Ned.x;
			st_Point.up		= -st_Ned.z;
			st_PointBuf.push_back(st_Point);
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
