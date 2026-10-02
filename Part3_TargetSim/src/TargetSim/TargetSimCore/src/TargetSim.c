#include "TargetSim.h"

// simTime 에 걸린 기동의 각속도 반환. 기동 없으면 0.
static ST_AttRate f_Tgt_GetAttRate(const ST_TargetInit *st_Target, FLOAT64 simTime)
{
	ST_AttRate				st_AttRate = { 0.0, 0.0, 0.0 };
	const ST_TargetManeuver	*st_ActiveManeuver = NULL;
	const ST_TargetManeuver	*st_Maneuver;
	FLOAT64					omega;
	INT32					nManeuver;

	for (nManeuver = 0; nManeuver < st_Target->nManeuverNum; nManeuver++)
	{
		st_Maneuver = &st_Target->st_Maneuver[nManeuver];

		if ((st_ActiveManeuver == NULL) && (st_Maneuver->startTime <= simTime) && (simTime < st_Maneuver->endTime))
		{
			st_ActiveManeuver = st_Maneuver;
		}
	}

	if (st_ActiveManeuver != NULL)
	{
		// 각속도 omega = n * g / V.
		omega = (st_ActiveManeuver->gravityValue * G_FORCE) / st_Target->headingSpeed;

		switch (st_ActiveManeuver->enTurnType)
		{
		case TGT_TURN_ROLL:
			st_AttRate.Roll = omega;
			break;

		case TGT_TURN_YAW:
			st_AttRate.Yaw = omega;
			break;

		case TGT_TURN_PITCH:
			st_AttRate.Pitch = omega;
			break;

		case TGT_TURN_NONE:
		default:
			// 회전 없음.
			break;
		}
	}

	return st_AttRate;
}

// 동체 속도 (V, 0, 0) -> NED -> ECEF 속도. st_Platform 이 있으면 플랫폼 기준 (비교용).
static STRUCT_Coord_Rect f_Tgt_GetVelEcef(const STRUCT_Coord_Attitude *st_Att, const STRUCT_Coord_Lla *st_Lla, const ST_TargetState *st_Platform,
	FLOAT64 headingSpeed)
{
	STRUCT_Coord_Rect	st_VelNed;
	STRUCT_Coord_Rect	st_VelEcef;

	// 동체 -> NED. 인자 순서 Roll, Yaw, Pitch.
	st_VelNed = f_Trans_Body_To_Ned(headingSpeed, 0.0, 0.0, st_Att->Roll, st_Att->Yaw, st_Att->Pitch);

	if (st_Platform == NULL)
	{
		// NED -> ECEF. 기준점 0 으로 회전만 적용. 북 · 동 · 아래는 표적 자리 기준.
		st_VelEcef = f_Trans_Ned_To_Ecef(st_VelNed.x, st_VelNed.y, st_VelNed.z, 0.0, 0.0, 0.0, st_Lla->Lat, st_Lla->Lon);
	}
	else
	{
		// NED -> ECEF. 기준점에 플랫폼 ECEF 위치, 위경도를 넣고 더해진 위치를 다시 뺌. 플랫폼 수평면 기준.
		st_VelEcef = f_Trans_Ned_To_Ecef(st_VelNed.x, st_VelNed.y, st_VelNed.z, st_Platform->st_PosEcef.x, st_Platform->st_PosEcef.y,
			st_Platform->st_PosEcef.z, st_Platform->st_Lla.Lat, st_Platform->st_Lla.Lon);
		st_VelEcef.x = st_VelEcef.x - st_Platform->st_PosEcef.x;
		st_VelEcef.y = st_VelEcef.y - st_Platform->st_PosEcef.y;
		st_VelEcef.z = st_VelEcef.z - st_Platform->st_PosEcef.z;
	}

	return st_VelEcef;
}

// 플랫폼 또는 표적 하나의 시작 상태 생성.
static VOID f_Tgt_InitState(ST_TargetState *st_State, const STRUCT_Coord_Lla *st_InitLla, const STRUCT_Coord_Attitude *st_InitAtt,
	const ST_TargetState *st_Platform, FLOAT64 headingSpeed)
{
	st_State->simTime	= 0.0;
	st_State->st_Lla	= *st_InitLla;
	st_State->st_Att	= *st_InitAtt;

	// LLA -> ECEF 위치. 인자 순서 경도, 위도, 고도.
	st_State->st_PosEcef	= f_Trans_Lla_To_Ecef(st_InitLla->Lon, st_InitLla->Lat, st_InitLla->Alt);
	st_State->st_VelEcef	= f_Tgt_GetVelEcef(st_InitAtt, st_InitLla, st_Platform, headingSpeed);
}

// 계산 과정 기록에 이번 스텝의 값을 그대로 남김 (계산 로그용). st_Trace 가 NULL 이면 아무것도 안 함.
static VOID f_Tgt_KeepTrace(ST_StepTrace *st_Trace, const ST_AttRate *st_AttRate, FLOAT64 headingSpeed, const ST_TargetState *st_Start,
	const STRUCT_Coord_Rect *st_VelStart, const STRUCT_Coord_Attitude *st_AttMid, const STRUCT_Coord_Rect *st_PosMid,
	const STRUCT_Coord_Lla *st_LlaMid, const STRUCT_Coord_Rect *st_VelMid, const ST_TargetState *st_Next)
{
	if (st_Trace != NULL)
	{
		st_Trace->headingSpeed	= headingSpeed;
		st_Trace->st_AttRate	= *st_AttRate;
		st_Trace->st_Start		= *st_Start;
		st_Trace->st_VelStart	= *st_VelStart;
		st_Trace->st_AttMid		= *st_AttMid;
		st_Trace->st_PosMid		= *st_PosMid;
		st_Trace->st_LlaMid		= *st_LlaMid;
		st_Trace->st_VelMid		= *st_VelMid;
		st_Trace->st_Next		= *st_Next;
	}
}

// 남긴 기록에 객체, 스텝 번호를 붙여 pf_Trace 로 넘김. st_Trace 가 NULL 이면 아무것도 안 함.
// 속도의 북, 동, 아래 성분은 f_Tgt_GetVelEcef 의 첫 단계와 같은 식으로 구함.
static VOID f_Tgt_SendTrace(const ST_SimConfig *st_Config, ST_StepTrace *st_Trace, INT32 nObject, INT32 nStepIndex)
{
	const STRUCT_Coord_Attitude	*st_Att;

	if ((st_Trace != NULL) && (st_Config->pf_Trace != NULL))
	{
		st_Trace->nObject		= nObject;
		st_Trace->nStepIndex	= nStepIndex;
		st_Trace->stepTime		= st_Config->stepTime;

		st_Att						= &st_Trace->st_Start.st_Att;
		st_Trace->st_VelNedStart	= f_Trans_Body_To_Ned(st_Trace->headingSpeed, 0.0, 0.0, st_Att->Roll, st_Att->Yaw, st_Att->Pitch);
		st_Att						= &st_Trace->st_AttMid;
		st_Trace->st_VelNedMid		= f_Trans_Body_To_Ned(st_Trace->headingSpeed, 0.0, 0.0, st_Att->Roll, st_Att->Yaw, st_Att->Pitch);

		st_Config->pf_Trace(st_Trace, st_Config->pt_TraceUser);
	}
}

// 0 초 상태의 계산 과정 기록 (pf_Trace 가 있을 때만). 반 스텝 뒤 값 칸에도 0 초 상태를 넣음.
static VOID f_Tgt_SendInitTrace(const ST_SimConfig *st_Config, INT32 nObject, const ST_TargetState *st_State)
{
	const ST_AttRate	st_NoManeuverRate = { 0.0, 0.0, 0.0 };
	const FLOAT64		headingSpeed = (nObject == 0) ? st_Config->st_Platform.headingSpeed : st_Config->st_Target[nObject - 1].headingSpeed;
	ST_StepTrace		st_Trace;

	if (st_Config->pf_Trace != NULL)
	{
		f_Tgt_KeepTrace(&st_Trace, &st_NoManeuverRate, headingSpeed, st_State, &st_State->st_VelEcef, &st_State->st_Att,
			&st_State->st_PosEcef, &st_State->st_Lla, &st_State->st_VelEcef, st_State);
		f_Tgt_SendTrace(st_Config, &st_Trace, nObject, -1);
	}
}

// 중점법으로 한 스텝 전진. 반 스텝 뒤(중간 지점) 속도로 이동.
static VOID f_Tgt_Propagate(ST_TargetState *st_State, const ST_AttRate *st_AttRate, const ST_TargetState *st_Platform, ST_StepTrace *st_Trace,
	FLOAT64 headingSpeed, FLOAT64 stepTime, FLOAT64 nextTime, INT32 noMidPoint)
{
	ST_TargetState			st_NextState;
	STRUCT_Coord_Attitude	st_AttMid;
	STRUCT_Coord_Lla		st_LlaMid;
	STRUCT_Coord_Rect		st_PosMid;
	STRUCT_Coord_Rect		st_VelStart;
	STRUCT_Coord_Rect		st_VelMid;
	FLOAT64					halfStep;

	// 반 스텝 뒤 자세.
	halfStep = 0.5 * stepTime;

	st_AttMid.Roll	= st_State->st_Att.Roll + (st_AttRate->Roll * halfStep);
	st_AttMid.Pitch	= st_State->st_Att.Pitch + (st_AttRate->Pitch * halfStep);
	st_AttMid.Yaw	= st_State->st_Att.Yaw + (st_AttRate->Yaw * halfStep);

	// 스텝 끝 자세.
	st_NextState.st_Att.Roll	= st_State->st_Att.Roll + (st_AttRate->Roll * stepTime);
	st_NextState.st_Att.Pitch	= st_State->st_Att.Pitch + (st_AttRate->Pitch * stepTime);
	st_NextState.st_Att.Yaw		= st_State->st_Att.Yaw + (st_AttRate->Yaw * stepTime);

	// 반 스텝 뒤 위치. 스텝 시작 속도 사용.
	st_VelStart = f_Tgt_GetVelEcef(&st_State->st_Att, &st_State->st_Lla, st_Platform, headingSpeed);

	st_PosMid.x = st_State->st_PosEcef.x + (st_VelStart.x * halfStep);
	st_PosMid.y = st_State->st_PosEcef.y + (st_VelStart.y * halfStep);
	st_PosMid.z = st_State->st_PosEcef.z + (st_VelStart.z * halfStep);

	// 반 스텝 뒤 위경도, 자세로 중간 지점 속도 계산.
	st_LlaMid = f_Trans_Ecef_To_Lla(st_PosMid.x, st_PosMid.y, st_PosMid.z);
	st_VelMid = f_Tgt_GetVelEcef(&st_AttMid, &st_LlaMid, st_Platform, headingSpeed);

	// 중간 지점 속도로 한 스텝 이동.
	st_NextState.st_PosEcef.x = st_State->st_PosEcef.x + (st_VelMid.x * stepTime);
	st_NextState.st_PosEcef.y = st_State->st_PosEcef.y + (st_VelMid.y * stepTime);
	st_NextState.st_PosEcef.z = st_State->st_PosEcef.z + (st_VelMid.z * stepTime);

	// 비중점법 (비교용): 스텝 시작 속도로 한 스텝 이동. 위 이동을 덮어씀.
	if (noMidPoint != 0)
	{
		st_NextState.st_PosEcef.x = st_State->st_PosEcef.x + (st_VelStart.x * stepTime);
		st_NextState.st_PosEcef.y = st_State->st_PosEcef.y + (st_VelStart.y * stepTime);
		st_NextState.st_PosEcef.z = st_State->st_PosEcef.z + (st_VelStart.z * stepTime);
	}

	// 새 위치의 LLA, 속도 계산 후 덮어씀.
	st_NextState.st_Lla		= f_Trans_Ecef_To_Lla(st_NextState.st_PosEcef.x, st_NextState.st_PosEcef.y, st_NextState.st_PosEcef.z);
	st_NextState.st_VelEcef	= f_Tgt_GetVelEcef(&st_NextState.st_Att, &st_NextState.st_Lla, st_Platform, headingSpeed);
	st_NextState.simTime	= nextTime;

	// 계산 로그를 켰으면 이번 스텝의 값을 그대로 남김.
	f_Tgt_KeepTrace(st_Trace, st_AttRate, headingSpeed, st_State, &st_VelStart,
		&st_AttMid, &st_PosMid, &st_LlaMid, &st_VelMid, &st_NextState);

	*st_State = st_NextState;
}

// 설정 복사, 0 초 결과 생성.
VOID f_Tgt_InitSim(ST_SimState *st_Sim, const ST_SimConfig *st_Config)
{
	ST_SimSample			*st_Sample = &st_Sim->st_Sample;
	const ST_TargetState	*st_Ref = (st_Config->usePlatform != 0) ? &st_Sample->st_Platform : NULL;
	INT32					nTarget;

	st_Sim->st_Config = *st_Config;

	// 스텝 수. 103.6 / 0.1 = 1035.999... 처럼 나눗셈 오차가 있어 반올림.
	st_Sim->nStepNum = (INT32)((st_Config->durationTime / st_Config->stepTime) + 0.5);

	st_Sample->simTime		= 0.0;
	st_Sample->nStepIndex	= 0;
	st_Sample->nTargetNum	= st_Config->nTargetNum;

	// 플랫폼, 표적의 시작 상태.
	f_Tgt_InitState(&st_Sample->st_Platform, &st_Config->st_Platform.st_InitLla, &st_Config->st_Platform.st_InitAtt, st_Ref,
		st_Config->st_Platform.headingSpeed);
	f_Tgt_SendInitTrace(st_Config, 0, &st_Sample->st_Platform);

	for (nTarget = 0; nTarget < st_Config->nTargetNum; nTarget++)
	{
		f_Tgt_InitState(&st_Sample->st_Target[nTarget], &st_Config->st_Target[nTarget].st_InitLla, &st_Config->st_Target[nTarget].st_InitAtt,
			st_Ref, st_Config->st_Target[nTarget].headingSpeed);
		f_Tgt_SendInitTrace(st_Config, nTarget + 1, &st_Sample->st_Target[nTarget]);
	}
}

// 플랫폼, 전 표적 한 스텝 진행.
VOID f_Tgt_StepSim(ST_SimState *st_Sim)
{
	const ST_SimConfig		*st_Config = &st_Sim->st_Config;
	const ST_AttRate		st_NoManeuverRate = { 0.0, 0.0, 0.0 };
	ST_SimSample			*st_Sample = &st_Sim->st_Sample;
	const ST_TargetState	*st_Ref = (st_Config->usePlatform != 0) ? &st_Sample->st_Platform : NULL;
	ST_StepTrace			st_TraceBuf;
	ST_StepTrace			*st_Trace = (st_Config->pf_Trace != NULL) ? &st_TraceBuf : NULL;
	ST_AttRate				st_ManeuverRate;
	FLOAT64					curTime;
	FLOAT64					nextTime;
	INT32					nTarget;

	// 시각 = 스텝 번호 * stepTime.
	curTime		= (FLOAT64)st_Sample->nStepIndex * st_Config->stepTime;
	nextTime	= (FLOAT64)(st_Sample->nStepIndex + 1) * st_Config->stepTime;

	// 플랫폼: 기동 없음, 각속도 0.
	f_Tgt_Propagate(&st_Sample->st_Platform, &st_NoManeuverRate, st_Ref, st_Trace,
		st_Config->st_Platform.headingSpeed, st_Config->stepTime, nextTime, st_Config->noMidPoint);
	f_Tgt_SendTrace(st_Config, st_Trace, 0, st_Sample->nStepIndex);

	// 표적: 현재 시각의 기동 각속도로 전진.
	for (nTarget = 0; nTarget < st_Config->nTargetNum; nTarget++)
	{
		st_ManeuverRate = f_Tgt_GetAttRate(&st_Config->st_Target[nTarget], curTime);
		f_Tgt_Propagate(&st_Sample->st_Target[nTarget], &st_ManeuverRate, st_Ref, st_Trace,
			st_Config->st_Target[nTarget].headingSpeed, st_Config->stepTime, nextTime, st_Config->noMidPoint);
		f_Tgt_SendTrace(st_Config, st_Trace, nTarget + 1, st_Sample->nStepIndex);
	}

	st_Sample->nStepIndex	= st_Sample->nStepIndex + 1;
	st_Sample->simTime		= nextTime;
}

// ECEF 위치 -> NED.
STRUCT_Coord_Rect f_Tgt_EcefToNed(const STRUCT_Coord_Rect *st_Ecef, const STRUCT_Coord_Rect *st_RefEcef, const STRUCT_Coord_Lla *st_RefLla)
{
	return f_Trans_Ecef_To_Ned(st_Ecef->x, st_Ecef->y, st_Ecef->z, st_RefEcef->x, st_RefEcef->y, st_RefEcef->z, st_RefLla->Lat, st_RefLla->Lon);
}

// NED -> LLA.
STRUCT_Coord_Lla f_Tgt_NedToLla(const STRUCT_Coord_Rect *st_Ned, const STRUCT_Coord_Rect *st_RefEcef, const STRUCT_Coord_Lla *st_RefLla)
{
	STRUCT_Coord_Rect st_Ecef;

	st_Ecef = f_Trans_Ned_To_Ecef(st_Ned->x, st_Ned->y, st_Ned->z, st_RefEcef->x, st_RefEcef->y, st_RefEcef->z, st_RefLla->Lat, st_RefLla->Lon);

	return f_Trans_Ecef_To_Lla(st_Ecef.x, st_Ecef.y, st_Ecef.z);
}

// LLA -> NED.
STRUCT_Coord_Rect f_Tgt_LlaToNed(const STRUCT_Coord_Lla *st_Lla, const STRUCT_Coord_Rect *st_RefEcef, const STRUCT_Coord_Lla *st_RefLla)
{
	STRUCT_Coord_Rect st_Ecef;

	st_Ecef = f_Trans_Lla_To_Ecef(st_Lla->Lon, st_Lla->Lat, st_Lla->Alt);

	return f_Tgt_EcefToNed(&st_Ecef, st_RefEcef, st_RefLla);
}
