#include "TargetSim.h"

// 자세 각속도. Roll, Pitch, Yaw 가 1 초에 바뀌는 양.
typedef struct
{
	FLOAT64					Roll;							// [rad/s]
	FLOAT64					Pitch;							// [rad/s]
	FLOAT64					Yaw;							// [rad/s]
} ST_AttRate;

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

// 동체 속도 (V, 0, 0) -> NED -> ECEF 속도.
static STRUCT_Coord_Rect f_Tgt_GetVelEcef(const STRUCT_Coord_Attitude *st_Att, const STRUCT_Coord_Lla *st_Lla, FLOAT64 headingSpeed)
{
	STRUCT_Coord_Rect	st_VelNed;

	// 동체 -> NED. 인자 순서 Roll, Yaw, Pitch.
	st_VelNed = f_Trans_Body_To_Ned(headingSpeed, 0.0, 0.0, st_Att->Roll, st_Att->Yaw, st_Att->Pitch);

	// NED -> ECEF. 기준점 0 으로 회전만 적용.
	return f_Trans_Ned_To_Ecef(st_VelNed.x, st_VelNed.y, st_VelNed.z, 0.0, 0.0, 0.0, st_Lla->Lat, st_Lla->Lon);
}

// 플랫폼 또는 표적 하나의 시작 상태 생성.
static VOID f_Tgt_InitState(ST_TargetState *st_State, const STRUCT_Coord_Lla *st_InitLla, const STRUCT_Coord_Attitude *st_InitAtt, FLOAT64 headingSpeed)
{
	st_State->simTime	= 0.0;
	st_State->st_Lla	= *st_InitLla;
	st_State->st_Att	= *st_InitAtt;

	// LLA -> ECEF 위치. 인자 순서 경도, 위도, 고도.
	st_State->st_PosEcef	= f_Trans_Lla_To_Ecef(st_InitLla->Lon, st_InitLla->Lat, st_InitLla->Alt);
	st_State->st_VelEcef	= f_Tgt_GetVelEcef(st_InitAtt, st_InitLla, headingSpeed);
}

// 중점법으로 한 스텝 전진. 반 스텝 뒤(중간 지점) 속도로 이동.
static VOID f_Tgt_Propagate(ST_TargetState *st_State, const ST_AttRate *st_AttRate, FLOAT64 headingSpeed, FLOAT64 stepTime, FLOAT64 nextTime)
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
	st_VelStart = f_Tgt_GetVelEcef(&st_State->st_Att, &st_State->st_Lla, headingSpeed);

	st_PosMid.x = st_State->st_PosEcef.x + (st_VelStart.x * halfStep);
	st_PosMid.y = st_State->st_PosEcef.y + (st_VelStart.y * halfStep);
	st_PosMid.z = st_State->st_PosEcef.z + (st_VelStart.z * halfStep);

	// 반 스텝 뒤 위경도, 자세로 중간 지점 속도 계산.
	st_LlaMid = f_Trans_Ecef_To_Lla(st_PosMid.x, st_PosMid.y, st_PosMid.z);
	st_VelMid = f_Tgt_GetVelEcef(&st_AttMid, &st_LlaMid, headingSpeed);

	// 중간 지점 속도로 한 스텝 이동.
	st_NextState.st_PosEcef.x = st_State->st_PosEcef.x + (st_VelMid.x * stepTime);
	st_NextState.st_PosEcef.y = st_State->st_PosEcef.y + (st_VelMid.y * stepTime);
	st_NextState.st_PosEcef.z = st_State->st_PosEcef.z + (st_VelMid.z * stepTime);

	// 새 위치의 LLA, 속도 계산 후 덮어씀.
	st_NextState.st_Lla		= f_Trans_Ecef_To_Lla(st_NextState.st_PosEcef.x, st_NextState.st_PosEcef.y, st_NextState.st_PosEcef.z);
	st_NextState.st_VelEcef	= f_Tgt_GetVelEcef(&st_NextState.st_Att, &st_NextState.st_Lla, headingSpeed);
	st_NextState.simTime	= nextTime;

	*st_State = st_NextState;
}

// 설정 복사, 0 초 결과 생성.
VOID f_Tgt_InitSim(ST_SimState *st_Sim, const ST_SimConfig *st_Config)
{
	ST_SimSample	*st_Sample = &st_Sim->st_Sample;
	INT32			nTarget;

	st_Sim->st_Config = *st_Config;

	// 스텝 수.
	st_Sim->nStepNum = (INT32)(st_Config->durationTime / st_Config->stepTime);

	st_Sample->simTime		= 0.0;
	st_Sample->nStepIndex	= 0;
	st_Sample->nTargetNum	= st_Config->nTargetNum;

	// 플랫폼, 표적의 시작 상태.
	f_Tgt_InitState(&st_Sample->st_Platform, &st_Config->st_Platform.st_InitLla, &st_Config->st_Platform.st_InitAtt, st_Config->st_Platform.headingSpeed);

	for (nTarget = 0; nTarget < st_Config->nTargetNum; nTarget++)
	{
		f_Tgt_InitState(&st_Sample->st_Target[nTarget], &st_Config->st_Target[nTarget].st_InitLla, &st_Config->st_Target[nTarget].st_InitAtt,
			st_Config->st_Target[nTarget].headingSpeed);
	}
}

// 플랫폼, 전 표적 한 스텝 진행.
VOID f_Tgt_StepSim(ST_SimState *st_Sim)
{
	const ST_SimConfig	*st_Config = &st_Sim->st_Config;
	const ST_AttRate	st_NoManeuverRate = { 0.0, 0.0, 0.0 };
	ST_SimSample		*st_Sample = &st_Sim->st_Sample;
	ST_AttRate			st_ManeuverRate;
	FLOAT64				curTime;
	FLOAT64				nextTime;
	INT32				nTarget;

	// 시각 = 스텝 번호 * stepTime.
	curTime		= (FLOAT64)st_Sample->nStepIndex * st_Config->stepTime;
	nextTime	= (FLOAT64)(st_Sample->nStepIndex + 1) * st_Config->stepTime;

	// 플랫폼: 기동 없음, 각속도 0.
	f_Tgt_Propagate(&st_Sample->st_Platform, &st_NoManeuverRate, st_Config->st_Platform.headingSpeed, st_Config->stepTime, nextTime);

	// 표적: 현재 시각의 기동 각속도로 전진.
	for (nTarget = 0; nTarget < st_Config->nTargetNum; nTarget++)
	{
		st_ManeuverRate = f_Tgt_GetAttRate(&st_Config->st_Target[nTarget], curTime);
		f_Tgt_Propagate(&st_Sample->st_Target[nTarget], &st_ManeuverRate, st_Config->st_Target[nTarget].headingSpeed, st_Config->stepTime, nextTime);
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
