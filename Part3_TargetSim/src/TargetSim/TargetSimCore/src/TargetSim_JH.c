#include <math.h>

#include "TargetSim_JH.h"

// 누적한 시각이나 스텝 번호에 stepTime 을 곱한 시각은 반올림 때문에 10진 기동 시각보다 조금 작을 수 있어 비교에 여유를 둔다.
#define TGT_TIME_EPSILON			1.0e-6
#define TGT_STEP_RATIO_TOLERANCE	1.0e-6

// 기동 회전축이 명세가 정한 0 ~ 3 인지 본다.
static EN_TgtStatus f_Tgt_CheckTurnType(EN_TurnType enTurnType)
{
	EN_TgtStatus enStatus;

	switch (enTurnType)
	{
	case TGT_TURN_NONE:
	case TGT_TURN_ROLL:
	case TGT_TURN_YAW:
	case TGT_TURN_PITCH:
		enStatus = TGT_OK;
		break;

	default:
		enStatus = TGT_ERR_TURN_TYPE;
		break;
	}

	return enStatus;
}

// 표적 하나의 기동 개수와 속력을 본다. 속력은 각속도 계산의 분모라 0 을 허용하지 않는다.
static EN_TgtStatus f_Tgt_CheckTarget(const ST_TargetInit *st_Target)
{
	EN_TgtStatus enStatus;

	if ((st_Target->nManeuverNum < 0) || (st_Target->nManeuverNum > TGT_MAX_MANEUVER_NUM))
	{
		enStatus = TGT_ERR_MANEUVER_NUM;
	}
	else if (st_Target->headingSpeed <= 0.0)
	{
		enStatus = TGT_ERR_SPEED;
	}
	else
	{
		enStatus = TGT_OK;
	}

	return enStatus;
}

// 표적 하나의 기동을 모두 본다. 회전축과 시각 범위를 확인하고 구간이 서로 겹치면 거부한다.
static EN_TgtStatus f_Tgt_CheckManeuvers(const ST_TargetInit *st_Target, FLOAT64 durationTime)
{
	EN_TgtStatus				enStatus = TGT_OK;
	const ST_TargetManeuver		*st_Maneuver;
	const ST_TargetManeuver		*st_Other;
	INT32						nManeuver;
	INT32						nOther;

	for (nManeuver = 0; nManeuver < st_Target->nManeuverNum; nManeuver++)
	{
		st_Maneuver = &st_Target->st_Maneuver[nManeuver];

		if (enStatus == TGT_OK)
		{
			enStatus = f_Tgt_CheckTurnType(st_Maneuver->enTurnType);
		}

		if ((enStatus == TGT_OK) &&
			((st_Maneuver->startTime < 0.0) || (st_Maneuver->endTime <= st_Maneuver->startTime) ||
			 (st_Maneuver->endTime > durationTime)))
		{
			enStatus = TGT_ERR_TIME;
		}
	}

	// 구간을 [startTime, endTime) 로 보므로 한 기동의 끝과 다른 기동의 시작이 같은 것은 겹침이 아니다.
	for (nManeuver = 0; nManeuver < st_Target->nManeuverNum; nManeuver++)
	{
		st_Maneuver = &st_Target->st_Maneuver[nManeuver];

		for (nOther = nManeuver + 1; nOther < st_Target->nManeuverNum; nOther++)
		{
			st_Other = &st_Target->st_Maneuver[nOther];

			if ((enStatus == TGT_OK) &&
				(st_Maneuver->startTime < st_Other->endTime) && (st_Other->startTime < st_Maneuver->endTime))
			{
				enStatus = TGT_ERR_MANEUVER_OVERLAP;
			}
		}
	}

	return enStatus;
}

// 주어진 시각에 걸린 기동을 찾아 그 회전축의 각속도를 돌려준다. 걸린 기동이 없으면 세 축 모두 0 이다.
static EN_TgtStatus f_Tgt_GetAttRate(ST_CoordAtt *st_Rate, const ST_TargetInit *st_Target, FLOAT64 simTime)
{
	EN_TgtStatus				enStatus;
	const ST_TargetManeuver		*st_Active = NULL;
	const ST_TargetManeuver		*st_Maneuver;
	FLOAT64						evalTime;
	FLOAT64						omega;
	INT32						nManeuver;

	st_Rate->roll	= 0.0;
	st_Rate->pitch	= 0.0;
	st_Rate->yaw	= 0.0;

	// 설정 검증 없이 불릴 수도 있으므로 기동 배열 범위와 각속도 분모를 여기서 확인한다.
	enStatus = f_Tgt_CheckTarget(st_Target);

	if (enStatus == TGT_OK)
	{
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
			case TGT_TURN_NONE:
				break;

			case TGT_TURN_ROLL:
				st_Rate->roll = omega;
				break;

			case TGT_TURN_YAW:
				st_Rate->yaw = omega;
				break;

			case TGT_TURN_PITCH:
				st_Rate->pitch = omega;
				break;

			default:
				enStatus = TGT_ERR_TURN_TYPE;
				break;
			}
		}
	}

	return enStatus;
}

// 동체 속도 (V, 0, 0) 을 자세로 한 번, 위경도로 한 번 돌려 ECEF 속도로 만든다.
// 회전만 거치므로 속력은 그대로 보존된다.
static EN_CoordStatus f_Tgt_GetVelEcef(ST_CoordRect *st_VelEcef, const ST_CoordAtt *st_Att, const ST_CoordLla *st_Lla, FLOAT64 headingSpeed)
{
	EN_CoordStatus	enStatus;
	ST_CoordRect	st_VelBody;
	ST_CoordRect	st_VelNed;

	st_VelBody.x = headingSpeed;
	st_VelBody.y = 0.0;
	st_VelBody.z = 0.0;

	enStatus = f_Trans_Body_To_Ned(&st_VelNed, &st_VelBody, st_Att);

	if (enStatus == COORD_OK)
	{
		enStatus = f_Trans_NedVec_To_Ecef(st_VelEcef, &st_VelNed, st_Lla->lat, st_Lla->lon);
	}

	return enStatus;
}

// 시뮬레이션 시간을 간격으로 나눈 스텝 수. 정수배가 아니거나 범위를 벗어나면 0 이다.
static INT32 f_Tgt_GetStepNum(const ST_SimConfig *st_Config)
{
	INT32	nStepNum = 0;
	FLOAT64	ratio;
	FLOAT64	nearest;

	// 긍정형 비교라 NaN 은 여기서 걸리고, 나눗셈 분모는 하한 이상임이 보장된다.
	if ((st_Config->stepTime >= TGT_MIN_STEP_TIME) && (st_Config->durationTime >= st_Config->stepTime))
	{
		ratio	= st_Config->durationTime / st_Config->stepTime;
		nearest	= floor(ratio + 0.5);

		// 범위 밖이나 NaN 인 실수를 정수로 바꾸면 미정의 동작이라 범위를 확인한 뒤에만 변환한다.
		if ((nearest <= (FLOAT64)TGT_MAX_STEP_NUM) && (fabs(ratio - nearest) <= TGT_STEP_RATIO_TOLERANCE))
		{
			nStepNum = (INT32)nearest;
		}
	}

	return nStepNum;
}

// 설정 전체를 검사한다. 먼저 걸린 항목의 오류 코드를 돌려준다.
EN_TgtStatus f_Tgt_ValidateConfig(const ST_SimConfig *st_Config)
{
	EN_TgtStatus			enStatus;
	const ST_TargetInit		*st_Target;
	INT32					nTarget;

	if (st_Config == NULL)
	{
		enStatus = TGT_ERR_NULL;
	}
	else if ((st_Config->nTargetNum < 1) || (st_Config->nTargetNum > TGT_MAX_TARGET_NUM))
	{
		enStatus = TGT_ERR_TARGET_NUM;
	}
	else if (f_Tgt_GetStepNum(st_Config) < 1)
	{
		enStatus = TGT_ERR_TIME;
	}
	else if (!((st_Config->st_Platform.headingSpeed >= 0.0) && (st_Config->st_Platform.headingSpeed < HUGE_VAL)))
	{
		enStatus = TGT_ERR_SPEED;
	}
	else
	{
		enStatus = TGT_OK;

		for (nTarget = 0; nTarget < st_Config->nTargetNum; nTarget++)
		{
			st_Target = &st_Config->st_Target[nTarget];

			if (enStatus == TGT_OK)
			{
				enStatus = f_Tgt_CheckTarget(st_Target);
			}

			if (enStatus == TGT_OK)
			{
				enStatus = f_Tgt_CheckManeuvers(st_Target, st_Config->durationTime);
			}
		}
	}

	return enStatus;
}

// 객체 하나의 t = 0 상태를 만든다. 모든 항목이 성공했을 때만 한 번에 반영한다.
EN_TgtStatus f_Tgt_InitState(ST_TargetState *st_State, const ST_CoordLla *st_InitLla, const ST_CoordAtt *st_InitAtt, FLOAT64 headingSpeed)
{
	EN_TgtStatus	enStatus;
	EN_CoordStatus	enCoord;
	ST_TargetState	st_New;

	if ((st_State == NULL) || (st_InitLla == NULL) || (st_InitAtt == NULL))
	{
		enStatus = TGT_ERR_NULL;
	}
	else
	{
		// 입력이 st_State 의 멤버를 가리켜도 되도록 새 상태를 따로 만든 뒤 한 번에 옮긴다.
		st_New.simTime		= 0.0;
		st_New.st_Lla		= *st_InitLla;
		st_New.st_Att.roll	= f_Coord_WrapAngle(st_InitAtt->roll);
		st_New.st_Att.pitch	= f_Coord_WrapAngle(st_InitAtt->pitch);
		st_New.st_Att.yaw	= f_Coord_WrapAngle(st_InitAtt->yaw);

		enCoord = f_Trans_Lla_To_Ecef(&st_New.st_PosEcef, &st_New.st_Lla);

		if (enCoord == COORD_OK)
		{
			enCoord = f_Tgt_GetVelEcef(&st_New.st_VelEcef, &st_New.st_Att, &st_New.st_Lla, headingSpeed);
		}

		if (enCoord == COORD_OK)
		{
			*st_State	= st_New;
			enStatus	= TGT_OK;
		}
		else
		{
			enStatus = TGT_ERR_COORD;
		}
	}

	return enStatus;
}

// 중점법으로 한 스텝 전진한다. 반 스텝 뒤의 자세와 위치로 만든 속도를 그 스텝의 대표 속도로 쓴다.
static EN_TgtStatus f_Tgt_Propagate(ST_TargetState *st_State, const ST_CoordAtt *st_Rate, FLOAT64 headingSpeed, FLOAT64 stepTime, FLOAT64 nextTime)
{
	EN_TgtStatus	enStatus;
	EN_CoordStatus	enCoord;
	ST_TargetState	st_Next;
	ST_CoordAtt		st_AttMid;
	ST_CoordLla		st_LlaMid;
	ST_CoordRect	st_PosMid;
	ST_CoordRect	st_VelStart;
	ST_CoordRect	st_VelMid;
	ST_CoordRect	st_Delta;
	FLOAT64			halfStep;

	// 중점법: 반 스텝 뒤의 자세와 위치에서 구한 속도로 한 스텝을 옮긴다.
	halfStep = 0.5 * stepTime;

	st_AttMid.roll	= st_State->st_Att.roll + (st_Rate->roll * halfStep);
	st_AttMid.pitch	= st_State->st_Att.pitch + (st_Rate->pitch * halfStep);
	st_AttMid.yaw	= st_State->st_Att.yaw + (st_Rate->yaw * halfStep);

	// 오일러각을 직접 적분하므로 pitch 가 ±90° 근처면 yaw 축이 속도 방향과 겹쳐(짐벌락) yaw 기동이 궤적을 바꾸지 못한다.
	st_Next.st_Att.roll		= f_Coord_WrapAngle(st_State->st_Att.roll + (st_Rate->roll * stepTime));
	st_Next.st_Att.pitch	= f_Coord_WrapAngle(st_State->st_Att.pitch + (st_Rate->pitch * stepTime));
	st_Next.st_Att.yaw		= f_Coord_WrapAngle(st_State->st_Att.yaw + (st_Rate->yaw * stepTime));

	// 반 스텝 뒤 위치는 스텝 시작 속도로 예측한다. 중앙 속도까지 스텝 시작 위경도를 NED 기준으로 만들면 지구 곡률만큼 방향이 뒤처져
	// 수평 비행 고도가 스텝마다 d^2/2R 씩 오르고 위치 오차가 stepTime 에 비례해서만 줄어든다.
	enCoord = f_Tgt_GetVelEcef(&st_VelStart, &st_State->st_Att, &st_State->st_Lla, headingSpeed);

	if (enCoord == COORD_OK)
	{
		enCoord = f_Coord_VecScale(&st_Delta, &st_VelStart, halfStep);
	}

	if (enCoord == COORD_OK)
	{
		enCoord = f_Coord_VecAdd(&st_PosMid, &st_State->st_PosEcef, &st_Delta);
	}

	if (enCoord == COORD_OK)
	{
		enCoord = f_Trans_Ecef_To_Lla(&st_LlaMid, &st_PosMid);
	}

	if (enCoord == COORD_OK)
	{
		enCoord = f_Tgt_GetVelEcef(&st_VelMid, &st_AttMid, &st_LlaMid, headingSpeed);
	}

	if (enCoord == COORD_OK)
	{
		enCoord = f_Coord_VecScale(&st_Delta, &st_VelMid, stepTime);
	}

	if (enCoord == COORD_OK)
	{
		enCoord = f_Coord_VecAdd(&st_Next.st_PosEcef, &st_State->st_PosEcef, &st_Delta);
	}

	if (enCoord == COORD_OK)
	{
		enCoord = f_Trans_Ecef_To_Lla(&st_Next.st_Lla, &st_Next.st_PosEcef);
	}

	if (enCoord == COORD_OK)
	{
		enCoord = f_Tgt_GetVelEcef(&st_Next.st_VelEcef, &st_Next.st_Att, &st_Next.st_Lla, headingSpeed);
	}

	if (enCoord == COORD_OK)
	{
		st_Next.simTime	= nextTime;
		*st_State		= st_Next;
		enStatus		= TGT_OK;
	}
	else
	{
		enStatus = TGT_ERR_COORD;
	}

	return enStatus;
}

// 표적 하나를 한 스텝 진행한다. 기동을 조회해 각속도를 얻고 적분에 넘긴다.
static EN_TgtStatus f_Tgt_StepTargetAt(ST_TargetState *st_State, const ST_TargetInit *st_Target, FLOAT64 stepTime, FLOAT64 curTime, FLOAT64 nextTime)
{
	EN_TgtStatus	enStatus;
	ST_CoordAtt		st_Rate;

	enStatus = f_Tgt_GetAttRate(&st_Rate, st_Target, curTime);

	if (enStatus == TGT_OK)
	{
		enStatus = f_Tgt_Propagate(st_State, &st_Rate, st_Target->headingSpeed, stepTime, nextTime);
	}

	return enStatus;
}

// 플랫폼은 기동이 없어 각속도 0 으로 같은 적분을 거친다. 이 경로에는 속도로 나누는 연산이 없어 정지 플랫폼도 안전하다.
static EN_TgtStatus f_Tgt_StepPlatformAt(ST_TargetState *st_State, const ST_PlatformInit *st_Platform, FLOAT64 stepTime, FLOAT64 nextTime)
{
	EN_TgtStatus	enStatus;
	ST_CoordAtt		st_Rate;

	st_Rate.roll	= 0.0;
	st_Rate.pitch	= 0.0;
	st_Rate.yaw		= 0.0;

	enStatus = f_Tgt_Propagate(st_State, &st_Rate, st_Platform->headingSpeed, stepTime, nextTime);

	return enStatus;
}

// 쓰지 않는 표적 칸을 0 으로 채운다. 표, CSV, 그림이 초기화되지 않은 값을 읽지 않게 한다.
static VOID f_Tgt_ClearState(ST_TargetState *st_State)
{
	st_State->simTime		= 0.0;
	st_State->st_PosEcef.x	= 0.0;
	st_State->st_PosEcef.y	= 0.0;
	st_State->st_PosEcef.z	= 0.0;
	st_State->st_VelEcef.x	= 0.0;
	st_State->st_VelEcef.y	= 0.0;
	st_State->st_VelEcef.z	= 0.0;
	st_State->st_Lla.lat	= 0.0;
	st_State->st_Lla.lon	= 0.0;
	st_State->st_Lla.alt	= 0.0;
	st_State->st_Att.roll	= 0.0;
	st_State->st_Att.pitch	= 0.0;
	st_State->st_Att.yaw	= 0.0;
}

// 표적 하나만 단독으로 한 스텝 진행한다. 시각은 상태에 누적한다.
EN_TgtStatus f_Tgt_StepTarget(ST_TargetState *st_State, const ST_TargetInit *st_Target, FLOAT64 stepTime)
{
	EN_TgtStatus enStatus;

	if ((st_State == NULL) || (st_Target == NULL))
	{
		enStatus = TGT_ERR_NULL;
	}
	else if (stepTime <= 0.0)
	{
		enStatus = TGT_ERR_TIME;
	}
	else
	{
		enStatus = f_Tgt_StepTargetAt(st_State, st_Target, stepTime, st_State->simTime, st_State->simTime + stepTime);
	}

	return enStatus;
}

// 설정을 검증하고 표본 0 을 만든다. 설정은 사본으로 보관해 이후 원본이 바뀌어도 영향이 없다.
EN_TgtStatus f_Tgt_InitSim(ST_SimState *st_Sim, const ST_SimConfig *st_Config)
{
	EN_TgtStatus			enStatus;
	const ST_TargetInit		*st_Target;
	ST_SimSample			st_New;
	INT32					nStepNum;
	INT32					nTargetNum;
	INT32					nTarget;

	if ((st_Sim == NULL) || (st_Config == NULL))
	{
		enStatus = TGT_ERR_NULL;
	}
	else
	{
		enStatus = f_Tgt_ValidateConfig(st_Config);

		if (enStatus == TGT_OK)
		{
			nStepNum	= f_Tgt_GetStepNum(st_Config);
			nTargetNum	= st_Config->nTargetNum;

			st_New.simTime		= 0.0;
			st_New.nStepIndex	= 0;
			st_New.nTargetNum	= nTargetNum;

			enStatus = f_Tgt_InitState(&st_New.st_Platform, &st_Config->st_Platform.st_InitLla, &st_Config->st_Platform.st_InitAtt, st_Config->st_Platform.headingSpeed);

			for (nTarget = 0; nTarget < TGT_MAX_TARGET_NUM; nTarget++)
			{
				if (nTarget < nTargetNum)
				{
					if (enStatus == TGT_OK)
					{
						st_Target	= &st_Config->st_Target[nTarget];
						enStatus	= f_Tgt_InitState(&st_New.st_Target[nTarget], &st_Target->st_InitLla, &st_Target->st_InitAtt, st_Target->headingSpeed);
					}
				}
				else
				{
					f_Tgt_ClearState(&st_New.st_Target[nTarget]);
				}
			}

			if (enStatus == TGT_OK)
			{
				// 설정에서 읽을 값은 위에서 모두 읽었으므로 st_Config 가 st_Sim 안의 설정을 가리켜도 안전하다.
				st_Sim->st_Config	= *st_Config;
				st_Sim->nStepNum	= nStepNum;
				st_Sim->st_Sample	= st_New;
			}
		}
	}

	return enStatus;
}

// 플랫폼과 표적 전원을 한 스텝 진행한다. 매 호출 재검증하고, 사본에서 모두 성공했을 때만 반영한다.
EN_TgtStatus f_Tgt_StepSim(ST_SimState *st_Sim)
{
	EN_TgtStatus			enStatus;
	const ST_SimConfig		*st_Config;
	ST_SimSample			st_Next;
	INT32					nStepIndex;
	INT32					nNextIndex;
	INT32					nTargetNum;
	INT32					nTarget;
	FLOAT64					stepTime;
	FLOAT64					curTime;
	FLOAT64					nextTime;

	if (st_Sim == NULL)
	{
		enStatus = TGT_ERR_NULL;
	}
	else
	{
		st_Config = &st_Sim->st_Config;

		// 공개 구조체라 호출 사이에 바뀔 수 있으므로 배열 인덱스와 나눗셈의 근거를 매 호출 다시 세운다.
		if (f_Tgt_ValidateConfig(st_Config) != TGT_OK)
		{
			enStatus = TGT_ERR_SIM_STATE;
		}
		else if ((f_Tgt_GetStepNum(st_Config) != st_Sim->nStepNum) ||
				 (st_Sim->st_Sample.nStepIndex < 0) || (st_Sim->st_Sample.nStepIndex > st_Sim->nStepNum) ||
				 (st_Sim->st_Sample.nTargetNum != st_Config->nTargetNum))
		{
			enStatus = TGT_ERR_SIM_STATE;
		}
		else if (st_Sim->st_Sample.nStepIndex == st_Sim->nStepNum)
		{
			enStatus = TGT_ERR_SIM_END;
		}
		else
		{
			nStepIndex	= st_Sim->st_Sample.nStepIndex;
			nNextIndex	= nStepIndex + 1;
			nTargetNum	= st_Config->nTargetNum;
			stepTime	= st_Config->stepTime;

			// 누적 대신 스텝 번호로 시각을 매겨 반올림 오차가 쌓이지 않게 하고, 기동 판정도 이 시각으로 한다.
			curTime		= (FLOAT64)nStepIndex * stepTime;
			nextTime	= (FLOAT64)nNextIndex * stepTime;

			// 한 표본 안에서 시각이 섞이지 않도록 사본에서 모두 진행한 뒤 한 번에 반영한다.
			st_Next = st_Sim->st_Sample;

			enStatus = f_Tgt_StepPlatformAt(&st_Next.st_Platform, &st_Config->st_Platform, stepTime, nextTime);

			for (nTarget = 0; nTarget < nTargetNum; nTarget++)
			{
				if (enStatus == TGT_OK)
				{
					enStatus = f_Tgt_StepTargetAt(&st_Next.st_Target[nTarget], &st_Config->st_Target[nTarget], stepTime, curTime, nextTime);
				}
			}

			if (enStatus == TGT_OK)
			{
				st_Next.nStepIndex	= nNextIndex;
				st_Next.simTime		= nextTime;
				st_Sim->st_Sample	= st_Next;
			}
		}
	}

	return enStatus;
}

// 엔진 상태 코드의 이름 문자열.
const CHAR *f_Tgt_StatusStr(EN_TgtStatus status)
{
	const CHAR *text;

	switch (status)
	{
	case TGT_OK:
		text = "TGT_OK";
		break;

	case TGT_ERR_NULL:
		text = "TGT_ERR_NULL";
		break;

	case TGT_ERR_TARGET_NUM:
		text = "TGT_ERR_TARGET_NUM";
		break;

	case TGT_ERR_MANEUVER_NUM:
		text = "TGT_ERR_MANEUVER_NUM";
		break;

	case TGT_ERR_TIME:
		text = "TGT_ERR_TIME";
		break;

	case TGT_ERR_SPEED:
		text = "TGT_ERR_SPEED";
		break;

	case TGT_ERR_TURN_TYPE:
		text = "TGT_ERR_TURN_TYPE";
		break;

	case TGT_ERR_MANEUVER_OVERLAP:
		text = "TGT_ERR_MANEUVER_OVERLAP";
		break;

	case TGT_ERR_COORD:
		text = "TGT_ERR_COORD";
		break;

	case TGT_ERR_SIM_STATE:
		text = "TGT_ERR_SIM_STATE";
		break;

	case TGT_ERR_SIM_END:
		text = "TGT_ERR_SIM_END";
		break;

	default:
		text = "TGT_ERR_UNKNOWN";
		break;
	}

	return text;
}
