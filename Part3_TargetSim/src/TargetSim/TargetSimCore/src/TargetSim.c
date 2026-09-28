#include <math.h>

#include "TargetSim.h"

// 스텝 번호에 stepTime 을 곱한 시각은 반올림 때문에 10진 기동 시각보다 조금 작을 수 있어 비교에 여유를 둔다.
#define TGT_TIME_EPSILON			1.0e-6

// 주어진 시각에 걸린 기동을 찾아 그 회전축의 각속도를 돌려준다. 걸린 기동이 없으면 세 축 모두 0 이다.
static STRUCT_Coord_Attitude f_Tgt_GetAttRate(const ST_TargetInit *st_Target, FLOAT64 simTime)
{
	STRUCT_Coord_Attitude	st_Rate = { 0.0, 0.0, 0.0 };
	const ST_TargetManeuver	*st_Active = NULL;
	const ST_TargetManeuver	*st_Maneuver;
	FLOAT64					evalTime;
	FLOAT64					omega;
	INT32					nManeuver;

	evalTime = simTime + TGT_TIME_EPSILON;

	for (nManeuver = 0; nManeuver < st_Target->nManeuverNum; nManeuver++)
	{
		st_Maneuver = &st_Target->st_Maneuver[nManeuver];

		if ((st_Active == NULL) && (st_Maneuver->startTime <= evalTime) && (evalTime < st_Maneuver->endTime))
		{
			st_Active = st_Maneuver;
		}
	}

	if (st_Active != NULL)
	{
		// 구심가속도 V * omega = n * g 에서 나온 각속도다. Roll 은 구심가속도와 무관하지만 Gravity Value 하나로 세 축을 다루려고 같은 식을 쓴다.
		omega = (st_Active->gravityValue * G_FORCE) / st_Target->headingSpeed;

		switch (st_Active->enTurnType)
		{
		case TGT_TURN_ROLL:
			st_Rate.Roll = omega;
			break;

		case TGT_TURN_YAW:
			st_Rate.Yaw = omega;
			break;

		case TGT_TURN_PITCH:
			st_Rate.Pitch = omega;
			break;

		case TGT_TURN_NONE:
		default:
			// 회전 없이 직진한다.
			break;
		}
	}

	return st_Rate;
}

// 동체 속도 (V, 0, 0) 을 자세로 한 번, 위경도로 한 번 돌려 ECEF 속도로 만든다.
// 회전만 거치므로 속력은 그대로 보존된다.
static STRUCT_Coord_Rect f_Tgt_GetVelEcef(const STRUCT_Coord_Attitude *st_Att, const STRUCT_Coord_Lla *st_Lla, FLOAT64 headingSpeed)
{
	STRUCT_Coord_Rect	st_VelNed;

	// 참고 소스의 f_Trans_Body_To_Ned 는 인자를 Roll, Yaw, Pitch 순서로 받는다.
	st_VelNed = f_Trans_Body_To_Ned(headingSpeed, 0.0, 0.0, st_Att->Roll, st_Att->Yaw, st_Att->Pitch);

	// f_Trans_Ned_To_Ecef 는 기준점 이동까지 더한다. 속도처럼 방향만 옮길 때는 기준점을 원점으로 준다.
	return f_Trans_Ned_To_Ecef(st_VelNed.x, st_VelNed.y, st_VelNed.z, 0.0, 0.0, 0.0, st_Lla->Lat, st_Lla->Lon);
}

// 객체 하나의 t = 0 상태를 만든다.
static VOID f_Tgt_InitState(ST_TargetState *st_State, const STRUCT_Coord_Lla *st_InitLla, const STRUCT_Coord_Attitude *st_InitAtt, FLOAT64 headingSpeed)
{
	st_State->simTime	= 0.0;
	st_State->st_Lla	= *st_InitLla;
	st_State->st_Att	= *st_InitAtt;

	// 참고 소스의 f_Trans_Lla_To_Ecef 는 경도를 먼저 받는다.
	st_State->st_PosEcef	= f_Trans_Lla_To_Ecef(st_InitLla->Lon, st_InitLla->Lat, st_InitLla->Alt);
	st_State->st_VelEcef	= f_Tgt_GetVelEcef(st_InitAtt, st_InitLla, headingSpeed);
}

// 중점법으로 한 스텝 전진한다. 반 스텝 뒤의 자세와 위치로 만든 속도를 그 스텝의 대표 속도로 쓴다.
static VOID f_Tgt_Propagate(ST_TargetState *st_State, const STRUCT_Coord_Attitude *st_Rate, FLOAT64 headingSpeed, FLOAT64 stepTime, FLOAT64 nextTime)
{
	ST_TargetState			st_Next;
	STRUCT_Coord_Attitude	st_AttMid;
	STRUCT_Coord_Lla		st_LlaMid;
	STRUCT_Coord_Rect		st_PosMid;
	STRUCT_Coord_Rect		st_VelStart;
	STRUCT_Coord_Rect		st_VelMid;
	FLOAT64					halfStep;

	// 중점법: 반 스텝 뒤의 자세와 위치에서 구한 속도로 한 스텝을 옮긴다.
	halfStep = 0.5 * stepTime;

	st_AttMid.Roll	= st_State->st_Att.Roll + (st_Rate->Roll * halfStep);
	st_AttMid.Pitch	= st_State->st_Att.Pitch + (st_Rate->Pitch * halfStep);
	st_AttMid.Yaw	= st_State->st_Att.Yaw + (st_Rate->Yaw * halfStep);

	// 오일러각을 직접 적분하므로 pitch 가 ±90° 근처면 yaw 축이 속도 방향과 겹쳐(짐벌락) yaw 기동이 궤적을 바꾸지 못한다.
	st_Next.st_Att.Roll		= st_State->st_Att.Roll + (st_Rate->Roll * stepTime);
	st_Next.st_Att.Pitch	= st_State->st_Att.Pitch + (st_Rate->Pitch * stepTime);
	st_Next.st_Att.Yaw		= st_State->st_Att.Yaw + (st_Rate->Yaw * stepTime);

	// 반 스텝 뒤 위치는 스텝 시작 속도로 예측한다. 중앙 속도까지 스텝 시작 위경도를 NED 기준으로 만들면 지구 곡률만큼 방향이 뒤처져
	// 수평 비행 고도가 스텝마다 d^2/2R 씩 오르고 위치 오차가 stepTime 에 비례해서만 줄어든다.
	st_VelStart = f_Tgt_GetVelEcef(&st_State->st_Att, &st_State->st_Lla, headingSpeed);

	st_PosMid.x = st_State->st_PosEcef.x + (st_VelStart.x * halfStep);
	st_PosMid.y = st_State->st_PosEcef.y + (st_VelStart.y * halfStep);
	st_PosMid.z = st_State->st_PosEcef.z + (st_VelStart.z * halfStep);

	st_LlaMid = f_Trans_Ecef_To_Lla(st_PosMid.x, st_PosMid.y, st_PosMid.z);
	st_VelMid = f_Tgt_GetVelEcef(&st_AttMid, &st_LlaMid, headingSpeed);

	st_Next.st_PosEcef.x = st_State->st_PosEcef.x + (st_VelMid.x * stepTime);
	st_Next.st_PosEcef.y = st_State->st_PosEcef.y + (st_VelMid.y * stepTime);
	st_Next.st_PosEcef.z = st_State->st_PosEcef.z + (st_VelMid.z * stepTime);

	st_Next.st_Lla		= f_Trans_Ecef_To_Lla(st_Next.st_PosEcef.x, st_Next.st_PosEcef.y, st_Next.st_PosEcef.z);
	st_Next.st_VelEcef	= f_Tgt_GetVelEcef(&st_Next.st_Att, &st_Next.st_Lla, headingSpeed);
	st_Next.simTime		= nextTime;

	*st_State = st_Next;
}

// 설정을 사본으로 보관하고 표본 0 을 만든다.
VOID f_Tgt_InitSim(ST_SimState *st_Sim, const ST_SimConfig *st_Config)
{
	ST_SimSample	*st_Sample = &st_Sim->st_Sample;
	FLOAT64			stepNum;
	INT32			nTarget;

	st_Sim->st_Config = *st_Config;

	// 10진 값끼리 나눈 결과는 정수보다 조금 작을 수 있어 반올림한다.
	stepNum				= floor((st_Config->durationTime / st_Config->stepTime) + 0.5);
	st_Sim->nStepNum	= (INT32)stepNum;

	st_Sample->simTime		= 0.0;
	st_Sample->nStepIndex	= 0;
	st_Sample->nTargetNum	= st_Config->nTargetNum;

	f_Tgt_InitState(&st_Sample->st_Platform, &st_Config->st_Platform.st_InitLla, &st_Config->st_Platform.st_InitAtt, st_Config->st_Platform.headingSpeed);

	for (nTarget = 0; nTarget < st_Config->nTargetNum; nTarget++)
	{
		f_Tgt_InitState(&st_Sample->st_Target[nTarget], &st_Config->st_Target[nTarget].st_InitLla, &st_Config->st_Target[nTarget].st_InitAtt,
			st_Config->st_Target[nTarget].headingSpeed);
	}
}

// 플랫폼과 표적 전원을 한 스텝 진행한다.
VOID f_Tgt_StepSim(ST_SimState *st_Sim)
{
	const ST_SimConfig			*st_Config = &st_Sim->st_Config;
	const STRUCT_Coord_Attitude	st_NoRate = { 0.0, 0.0, 0.0 };
	ST_SimSample				*st_Sample = &st_Sim->st_Sample;
	STRUCT_Coord_Attitude		st_Rate;
	FLOAT64						curTime;
	FLOAT64						nextTime;
	INT32						nTarget;

	// 누적 대신 스텝 번호로 시각을 매겨 반올림 오차가 쌓이지 않게 하고, 기동 판정도 이 시각으로 한다.
	curTime		= (FLOAT64)st_Sample->nStepIndex * st_Config->stepTime;
	nextTime	= (FLOAT64)(st_Sample->nStepIndex + 1) * st_Config->stepTime;

	// 플랫폼은 기동이 없어 각속도 0 으로 같은 적분을 거친다. 이 경로에는 속도로 나누는 연산이 없어 정지 플랫폼도 된다.
	f_Tgt_Propagate(&st_Sample->st_Platform, &st_NoRate, st_Config->st_Platform.headingSpeed, st_Config->stepTime, nextTime);

	for (nTarget = 0; nTarget < st_Config->nTargetNum; nTarget++)
	{
		st_Rate = f_Tgt_GetAttRate(&st_Config->st_Target[nTarget], curTime);
		f_Tgt_Propagate(&st_Sample->st_Target[nTarget], &st_Rate, st_Config->st_Target[nTarget].headingSpeed, st_Config->stepTime, nextTime);
	}

	st_Sample->nStepIndex	= st_Sample->nStepIndex + 1;
	st_Sample->simTime		= nextTime;
}

// ECEF 위치 → NED. 원본 변환이 기준점 빼기와 회전을 함께 한다.
STRUCT_Coord_Rect f_Tgt_EcefToNed(const STRUCT_Coord_Rect *st_Ecef, const STRUCT_Coord_Rect *st_RefEcef, const STRUCT_Coord_Lla *st_RefLla)
{
	return f_Trans_Ecef_To_Ned(st_Ecef->x, st_Ecef->y, st_Ecef->z, st_RefEcef->x, st_RefEcef->y, st_RefEcef->z, st_RefLla->Lat, st_RefLla->Lon);
}

// NED(기준점 기준 수평면) → 위경고도
STRUCT_Coord_Lla f_Tgt_NedToLla(const STRUCT_Coord_Rect *st_Ned, const STRUCT_Coord_Rect *st_RefEcef, const STRUCT_Coord_Lla *st_RefLla)
{
	STRUCT_Coord_Rect st_Ecef;

	st_Ecef = f_Trans_Ned_To_Ecef(st_Ned->x, st_Ned->y, st_Ned->z, st_RefEcef->x, st_RefEcef->y, st_RefEcef->z, st_RefLla->Lat, st_RefLla->Lon);

	return f_Trans_Ecef_To_Lla(st_Ecef.x, st_Ecef.y, st_Ecef.z);
}

// 위경고도 → NED. f_Trans_Lla_To_Ecef 는 경도를 먼저 받는다.
STRUCT_Coord_Rect f_Tgt_LlaToNed(const STRUCT_Coord_Lla *st_Lla, const STRUCT_Coord_Rect *st_RefEcef, const STRUCT_Coord_Lla *st_RefLla)
{
	STRUCT_Coord_Rect st_Ecef;

	st_Ecef = f_Trans_Lla_To_Ecef(st_Lla->Lon, st_Lla->Lat, st_Lla->Alt);

	return f_Tgt_EcefToNed(&st_Ecef, st_RefEcef, st_RefLla);
}
