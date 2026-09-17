#include "pch.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "Scenario_JH.h"
#include "UiNumber_JH.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define SCN_FILE_MAGIC			"TargetSim scenario"
#define SCN_FILE_VERSION		1
#define SCN_FILE_MAX_BYTE		262144
#define SCN_NEW_MANEUVER_SPAN	10.0					// [s] 새 기동의 기본 길이
#define SCN_COPY_LAT_OFFSET		0.01					// [deg] 복제한 표적을 원본과 겹치지 않게 북쪽으로 민다

typedef struct
{
	LPCTSTR				pt_Field[SCN_OBJ_FIELD_NUM];
} ST_PresetObject;

typedef struct
{
	INT32				nTarget;
	EN_TurnType			enTurnType;
	LPCTSTR				pt_Field[SCN_MNV_FIELD_NUM];
} ST_PresetManeuver;

static const LPCTSTR	s_PresetName[SCN_PRESET_NUM] = { _T("명세 시나리오 (과제 3)"), _T("기동 시연 — 선회·상승·Roll"), _T("다표적 접근 — 표적 6") };
static const LPCTSTR	s_FieldName[SCN_OBJ_FIELD_NUM] = { _T("위도"), _T("경도"), _T("고도"), _T("속력"), _T("Roll"), _T("Pitch"), _T("Yaw") };
static const LPCTSTR	s_ManeuverFieldName[SCN_MNV_FIELD_NUM] = { _T("G"), _T("시작"), _T("종료") };
static const LPCTSTR	s_TurnName[SCN_TURN_TYPE_NUM] = { _T("0 없음"), _T("1 Roll"), _T("2 Yaw"), _T("3 Pitch") };

// 과제 명세: 플랫폼 정지, 대함 표적 1, 대공 표적 1
static const ST_PresetObject	s_SpecPlatform = { { _T("32.0"), _T("126.0"), _T("0"), _T("0"), _T("0"), _T("0"), _T("0") } };
static const ST_PresetObject	s_SpecTarget[2] =
{
	{ { _T("32.125"), _T("126.03"), _T("0"), _T("30"), _T("0"), _T("0"), _T("270") } },
	{ { _T("32.12"), _T("126.0"), _T("300"), _T("200"), _T("0"), _T("0"), _T("180") } }
};

// 기동 시연: 명세 표적에 선회, 상승·수평 복귀, Roll 을 차례로 건다.
static const ST_PresetManeuver	s_DemoManeuver[6] =
{
	{ 0, TGT_TURN_YAW,		{ _T("-1.0"), _T("10"), _T("20") } },
	{ 0, TGT_TURN_YAW,		{ _T("1.0"), _T("35"), _T("45") } },
	{ 1, TGT_TURN_YAW,		{ _T("2.0"), _T("5"), _T("25") } },
	{ 1, TGT_TURN_PITCH,	{ _T("0.5"), _T("25"), _T("30") } },
	{ 1, TGT_TURN_PITCH,	{ _T("-0.5"), _T("40"), _T("45") } },
	{ 1, TGT_TURN_ROLL,		{ _T("1.0"), _T("45"), _T("50") } }
};

// 다표적 접근: 플랫폼에서 15 km, 60 도 간격으로 놓고 플랫폼을 향하게 한다.
static const ST_PresetObject	s_MultiTarget[6] =
{
	{ { _T("32.1353"), _T("126.0"), _T("300"), _T("250"), _T("0"), _T("0"), _T("180") } },
	{ { _T("32.0676"), _T("126.1376"), _T("1000"), _T("200"), _T("0"), _T("0"), _T("240") } },
	{ { _T("31.9323"), _T("126.1374"), _T("50"), _T("300"), _T("0"), _T("0"), _T("300") } },
	{ { _T("31.8647"), _T("126.0"), _T("3000"), _T("220"), _T("0"), _T("0"), _T("0") } },
	{ { _T("31.9323"), _T("125.8626"), _T("500"), _T("180"), _T("0"), _T("0"), _T("60") } },
	{ { _T("32.0676"), _T("125.8624"), _T("0"), _T("30"), _T("0"), _T("0"), _T("120") } }
};

static const ST_PresetManeuver	s_MultiManeuver[5] =
{
	{ 1, TGT_TURN_YAW,		{ _T("1.0"), _T("10"), _T("20") } },
	{ 1, TGT_TURN_YAW,		{ _T("-1.0"), _T("20"), _T("40") } },
	{ 1, TGT_TURN_YAW,		{ _T("1.0"), _T("40"), _T("50") } },
	{ 3, TGT_TURN_PITCH,	{ _T("-0.3"), _T("20"), _T("25") } },
	{ 4, TGT_TURN_YAW,		{ _T("-1.5"), _T("15"), _T("25") } }
};

static VOID f_Scn_ClearObject(ST_ObjectText *st_Object)
{
	INT32 nField;
	INT32 nManeuver;

	for (nField = 0; nField < SCN_OBJ_FIELD_NUM; nField++)
	{
		st_Object->st_FieldText[nField].Empty();
	}

	st_Object->nManeuverNum = 0;

	for (nManeuver = 0; nManeuver < TGT_MAX_MANEUVER_NUM; nManeuver++)
	{
		st_Object->st_Maneuver[nManeuver].enTurnType = TGT_TURN_NONE;

		for (nField = 0; nField < SCN_MNV_FIELD_NUM; nField++)
		{
			st_Object->st_Maneuver[nManeuver].st_FieldText[nField].Empty();
		}
	}
}

static VOID f_Scn_SetObject(ST_ObjectText *st_Object, const ST_PresetObject *st_Preset)
{
	INT32 nField;

	f_Scn_ClearObject(st_Object);

	for (nField = 0; nField < SCN_OBJ_FIELD_NUM; nField++)
	{
		st_Object->st_FieldText[nField] = st_Preset->pt_Field[nField];
	}
}

static VOID f_Scn_SetIssue(ST_ScnIssue *st_Issue, EN_ScnPlace enPlace, INT32 nTarget, INT32 nManeuver, INT32 nField, const CString &st_Message)
{
	st_Issue->enPlace		= enPlace;
	st_Issue->nTarget		= nTarget;
	st_Issue->nManeuver		= nManeuver;
	st_Issue->nField		= nField;
	st_Issue->st_Message	= st_Message;
}

static CString f_Scn_Where(EN_ScnPlace enPlace, INT32 nTarget, INT32 nManeuver)
{
	CString st_Where;

	switch (enPlace)
	{
	case SCN_AT_PLATFORM:
		st_Where = _T("플랫폼");
		break;

	case SCN_AT_TARGET:
		st_Where.Format(_T("표적 %d"), nTarget + 1);
		break;

	case SCN_AT_MANEUVER:
		st_Where.Format(_T("표적 %d 기동 %d"), nTarget + 1, nManeuver + 1);
		break;

	default:
		st_Where = _T("시뮬레이션");
		break;
	}

	return st_Where;
}

static INT32 f_Scn_IsGridMultiple(FLOAT64 value, FLOAT64 unit)
{
	const FLOAT64 ratio = value / unit;

	return (fabs(ratio - floor(ratio + 0.5)) <= SCN_GRID_TOL) ? 1 : 0;
}

CScenario::CScenario()
	: nTargetNum(0)
{
	INT32 nTarget;

	f_Scn_ClearObject(&st_Platform);

	for (nTarget = 0; nTarget < TGT_MAX_TARGET_NUM; nTarget++)
	{
		f_Scn_ClearObject(&st_Target[nTarget]);
	}

	(VOID)memset(&st_ProbeConfig, 0, sizeof(st_ProbeConfig));
}

LPCTSTR CScenario::f_PresetName(INT32 nPreset)
{
	return ((nPreset >= 0) && (nPreset < SCN_PRESET_NUM)) ? s_PresetName[nPreset] : _T("");
}

LPCTSTR CScenario::f_TurnName(INT32 nTurnType)
{
	return ((nTurnType >= 0) && (nTurnType < SCN_TURN_TYPE_NUM)) ? s_TurnName[nTurnType] : _T("");
}

VOID CScenario::f_LoadPreset(INT32 nPreset)
{
	const ST_PresetManeuver	*st_Maneuver = nullptr;
	INT32					nManeuverNum = 0;
	INT32					nIndex;
	INT32					nTarget;

	st_DurationText	= _T("60");
	st_StepText		= _T("0.1");
	f_Scn_SetObject(&st_Platform, &s_SpecPlatform);

	for (nTarget = 0; nTarget < TGT_MAX_TARGET_NUM; nTarget++)
	{
		f_Scn_ClearObject(&st_Target[nTarget]);
	}

	if (nPreset == SCN_PRESET_MULTI)
	{
		nTargetNum		= 6;
		st_Maneuver		= s_MultiManeuver;
		nManeuverNum	= 5;

		for (nTarget = 0; nTarget < nTargetNum; nTarget++)
		{
			f_Scn_SetObject(&st_Target[nTarget], &s_MultiTarget[nTarget]);
		}
	}
	else
	{
		nTargetNum = 2;

		for (nTarget = 0; nTarget < nTargetNum; nTarget++)
		{
			f_Scn_SetObject(&st_Target[nTarget], &s_SpecTarget[nTarget]);
		}

		if (nPreset == SCN_PRESET_MANEUVER)
		{
			st_Maneuver		= s_DemoManeuver;
			nManeuverNum	= 6;
		}
	}

	for (nIndex = 0; nIndex < nManeuverNum; nIndex++)
	{
		ST_ObjectText *st_Object = &st_Target[st_Maneuver[nIndex].nTarget];

		if (st_Object->nManeuverNum < TGT_MAX_MANEUVER_NUM)
		{
			ST_ManeuverText	*st_New = &st_Object->st_Maneuver[st_Object->nManeuverNum];
			INT32			nField;

			st_New->enTurnType = st_Maneuver[nIndex].enTurnType;

			for (nField = 0; nField < SCN_MNV_FIELD_NUM; nField++)
			{
				st_New->st_FieldText[nField] = st_Maneuver[nIndex].pt_Field[nField];
			}

			st_Object->nManeuverNum = st_Object->nManeuverNum + 1;
		}
	}
}

INT32 CScenario::f_AddTarget(INT32 nSourceTarget, INT32 isWithManeuver)
{
	INT32	nNewTarget = -1;
	INT32	nSource = nSourceTarget;
	INT32	nField;
	CString	st_Moved;

	if (nTargetNum < TGT_MAX_TARGET_NUM)
	{
		if ((nSource < 0) || (nSource >= nTargetNum))
		{
			nSource = nTargetNum - 1;
		}

		nNewTarget = nTargetNum;
		f_Scn_ClearObject(&st_Target[nNewTarget]);

		if (nSource >= 0)
		{
			// 초기값을 복사한다. 같은 자리에 겹치면 그림에서 하나로 보이므로 위도를 조금 민다.
			if (isWithManeuver != 0)
			{
				st_Target[nNewTarget] = st_Target[nSource];
			}
			else
			{
				for (nField = 0; nField < SCN_OBJ_FIELD_NUM; nField++)
				{
					st_Target[nNewTarget].st_FieldText[nField] = st_Target[nSource].st_FieldText[nField];
				}
			}

			if (f_Num_Nudge(st_Target[nNewTarget].st_FieldText[SCN_FIELD_LAT], SCN_COPY_LAT_OFFSET, 2, -SCN_LAT_LIMIT, SCN_LAT_LIMIT, &st_Moved) != 0)
			{
				st_Target[nNewTarget].st_FieldText[SCN_FIELD_LAT] = st_Moved;
			}
		}
		else
		{
			f_Scn_SetObject(&st_Target[nNewTarget], &s_SpecTarget[0]);
		}

		nTargetNum = nTargetNum + 1;
	}

	return nNewTarget;
}

INT32 CScenario::f_DeleteTarget(INT32 nTarget)
{
	INT32 isDeleted = 0;
	INT32 nIndex;

	if ((nTargetNum > 1) && (nTarget >= 0) && (nTarget < nTargetNum))
	{
		for (nIndex = nTarget; nIndex < (nTargetNum - 1); nIndex++)
		{
			st_Target[nIndex] = st_Target[nIndex + 1];
		}

		f_Scn_ClearObject(&st_Target[nTargetNum - 1]);
		nTargetNum	= nTargetNum - 1;
		isDeleted	= 1;
	}

	return isDeleted;
}

INT32 CScenario::f_AddManeuver(INT32 nTarget)
{
	INT32	nNewManeuver = -1;
	FLOAT64	duration = 0.0;
	FLOAT64	startTime = 0.0;
	CString	st_Start = _T("0");
	CString	st_End;

	if ((nTarget >= 0) && (nTarget < nTargetNum) && (st_Target[nTarget].nManeuverNum < TGT_MAX_MANEUVER_NUM))
	{
		ST_ObjectText *st_Object = &st_Target[nTarget];

		// 시작은 직전 기동의 종료로 채워 맞닿은 구간을 쉽게 만든다. 바로 그림에 나타나도록 1 G Yaw 로 시작한다.
		if (st_Object->nManeuverNum > 0)
		{
			st_Start = st_Object->st_Maneuver[st_Object->nManeuverNum - 1].st_FieldText[SCN_FIELD_END];
		}

		if (f_Num_Parse(st_DurationText, &duration) != NUM_OK)
		{
			duration = SCN_DURATION_MAX;
		}

		if ((f_Num_Parse(st_Start, &startTime) == NUM_OK) && (startTime >= (duration - SCN_GRID_TOL)))
		{
			// 직전 기동이 시뮬레이션 끝까지 차 있다. 넣으면 시작 = 종료인 빈 구간이 되어 바로 오류가 나므로 넣지 않는다.
			nNewManeuver = SCN_ADD_NO_ROOM;
		}
		else
		{
			ST_ManeuverText *st_New = &st_Object->st_Maneuver[st_Object->nManeuverNum];

			if (f_Num_Nudge(st_Start, SCN_NEW_MANEUVER_SPAN, 0, 0.0, duration, &st_End) == 0)
			{
				st_End.Empty();
			}

			st_New->enTurnType						= TGT_TURN_YAW;
			st_New->st_FieldText[SCN_FIELD_GRAVITY]	= _T("1.0");
			st_New->st_FieldText[SCN_FIELD_START]	= st_Start;
			st_New->st_FieldText[SCN_FIELD_END]		= st_End;
			nNewManeuver							= st_Object->nManeuverNum;
			st_Object->nManeuverNum					= nNewManeuver + 1;
		}
	}

	return nNewManeuver;
}

INT32 CScenario::f_DeleteManeuver(INT32 nTarget, INT32 nManeuver)
{
	INT32 isDeleted = 0;
	INT32 nIndex;
	INT32 nField;

	if ((nTarget >= 0) && (nTarget < nTargetNum) && (nManeuver >= 0) && (nManeuver < st_Target[nTarget].nManeuverNum))
	{
		ST_ObjectText *st_Object = &st_Target[nTarget];

		for (nIndex = nManeuver; nIndex < (st_Object->nManeuverNum - 1); nIndex++)
		{
			st_Object->st_Maneuver[nIndex] = st_Object->st_Maneuver[nIndex + 1];
		}

		st_Object->st_Maneuver[st_Object->nManeuverNum - 1].enTurnType = TGT_TURN_NONE;

		for (nField = 0; nField < SCN_MNV_FIELD_NUM; nField++)
		{
			st_Object->st_Maneuver[st_Object->nManeuverNum - 1].st_FieldText[nField].Empty();
		}

		st_Object->nManeuverNum	= st_Object->nManeuverNum - 1;
		isDeleted				= 1;
	}

	return isDeleted;
}

CString CScenario::f_StatusText(EN_TgtStatus enStatus, INT32 isManeuver)
{
	CString st_Text;

	switch (enStatus)
	{
	case TGT_OK:
		st_Text = _T("정상");
		break;

	case TGT_ERR_NULL:
		st_Text = _T("내부 오류: 설정 또는 상태가 비어 있습니다");
		break;

	case TGT_ERR_TARGET_NUM:
		st_Text.Format(_T("표적 수는 1~%d 개여야 합니다"), TGT_MAX_TARGET_NUM);
		break;

	case TGT_ERR_MANEUVER_NUM:
		st_Text.Format(_T("표적별 기동 수는 0~%d 개여야 합니다"), TGT_MAX_MANEUVER_NUM);
		break;

	case TGT_ERR_TIME:
		if (isManeuver != 0)
		{
			st_Text = _T("기동 시각은 0 ≤ 시작 < 종료 ≤ 시뮬레이션 시간이어야 합니다");
		}
		else
		{
			st_Text.Format(_T("시뮬레이션 시간은 시간 간격의 정수배이고 스텝 수는 1~%d 이어야 합니다"), TGT_MAX_STEP_NUM);
		}
		break;

	case TGT_ERR_SPEED:
		st_Text = _T("속력이 허용 범위를 벗어났습니다");
		break;

	case TGT_ERR_TURN_TYPE:
		st_Text = _T("기동 축은 0~3 이어야 합니다");
		break;

	case TGT_ERR_MANEUVER_OVERLAP:
		st_Text = _T("같은 표적의 앞선 기동과 구간이 겹칩니다 (종료 = 다음 시작은 허용)");
		break;

	case TGT_ERR_COORD:
		st_Text = _T("좌표 변환에 실패했습니다");
		break;

	case TGT_ERR_SIM_STATE:
		st_Text = _T("내부 오류: 시뮬레이션 상태가 설정과 맞지 않습니다");
		break;

	case TGT_ERR_SIM_END:
		st_Text = _T("내부 오류: 마지막 표본 뒤로 진행을 요청했습니다");
		break;

	default:
		st_Text.Format(_T("알 수 없는 상태 코드 %d"), static_cast<INT32>(enStatus));
		break;
	}

	return st_Text + _T(" [") + CString(f_Tgt_StatusStr(enStatus)) + _T("]");
}

INT32 CScenario::f_ReadNumber(const CString &st_Text, FLOAT64 minValue, FLOAT64 maxValue, LPCTSTR pt_Where, LPCTSTR pt_Name,
	FLOAT64 *pt_Value, ST_ScnIssue *st_Issue) const
{
	INT32		isOk = 0;
	FLOAT64		value = 0.0;
	CString		st_Rule;

	switch (f_Num_Parse(st_Text, &value))
	{
	case NUM_OK:
		if ((value >= minValue) && (value <= maxValue))
		{
			*pt_Value	= value;
			isOk		= 1;
		}
		else
		{
			st_Rule.Format(_T("%g ~ %g 범위여야 합니다"), minValue, maxValue);
		}
		break;

	case NUM_EMPTY:
		st_Rule = _T("값을 입력하십시오");
		break;

	case NUM_SYNTAX:
		st_Rule = _T("숫자로 읽을 수 없습니다 (소수점은 '.', 예: -0.5, 1e3)");
		break;

	case NUM_NOT_FINITE:
		st_Rule = _T("유한한 값이어야 합니다");
		break;

	default:
		st_Rule = _T("알 수 없는 입력 오류입니다");
		break;
	}

	if (isOk == 0)
	{
		st_Issue->st_Message = CString(pt_Where) + _T(" ") + pt_Name + _T(": ") + st_Rule + _T(" (입력: '") + st_Text + _T("')");
	}

	return isOk;
}

INT32 CScenario::f_ReadObject(const ST_ObjectText *st_Object, EN_ScnPlace enPlace, INT32 nTarget, FLOAT64 minSpeed,
	ST_CoordLla *st_Lla, ST_CoordAtt *st_Att, FLOAT64 *pt_Speed, ST_ScnIssue *st_Issue) const
{
	const FLOAT64	minValue[SCN_OBJ_FIELD_NUM] = { -SCN_LAT_LIMIT, -SCN_LON_LIMIT, SCN_ALT_MIN, minSpeed, -SCN_ROLL_LIMIT, -SCN_PITCH_LIMIT, -SCN_YAW_LIMIT };
	const FLOAT64	maxValue[SCN_OBJ_FIELD_NUM] = { SCN_LAT_LIMIT, SCN_LON_LIMIT, SCN_ALT_MAX, SCN_SPEED_MAX, SCN_ROLL_LIMIT, SCN_PITCH_LIMIT, SCN_YAW_LIMIT };
	const CString	st_Where = f_Scn_Where(enPlace, nTarget, -1);
	FLOAT64			fieldValue[SCN_OBJ_FIELD_NUM] = { 0.0 };
	INT32			isOk = 1;
	INT32			nField;

	for (nField = 0; nField < SCN_OBJ_FIELD_NUM; nField++)
	{
		if (isOk != 0)
		{
			isOk = f_ReadNumber(st_Object->st_FieldText[nField], minValue[nField], maxValue[nField], st_Where, s_FieldName[nField],
				&fieldValue[nField], st_Issue);

			if (isOk == 0)
			{
				st_Issue->enPlace	= enPlace;
				st_Issue->nTarget	= nTarget;
				st_Issue->nManeuver	= -1;
				st_Issue->nField	= nField;
			}
		}
	}

	if (isOk != 0)
	{
		st_Lla->lat		= f_Deg_To_Rad(fieldValue[SCN_FIELD_LAT]);
		st_Lla->lon		= f_Deg_To_Rad(fieldValue[SCN_FIELD_LON]);
		st_Lla->alt		= fieldValue[SCN_FIELD_ALT];
		*pt_Speed		= fieldValue[SCN_FIELD_SPEED];
		st_Att->roll	= f_Deg_To_Rad(fieldValue[SCN_FIELD_ROLL]);
		st_Att->pitch	= f_Deg_To_Rad(fieldValue[SCN_FIELD_PITCH]);
		st_Att->yaw		= f_Deg_To_Rad(fieldValue[SCN_FIELD_YAW]);
	}

	return isOk;
}

INT32 CScenario::f_BuildConfig(ST_SimConfig *st_Config, ST_ScnIssue *st_Issue)
{
	const FLOAT64	fieldMin[SCN_MNV_FIELD_NUM] = { -SCN_G_LIMIT, -HUGE_VAL, -HUGE_VAL };
	const FLOAT64	fieldMax[SCN_MNV_FIELD_NUM] = { SCN_G_LIMIT, HUGE_VAL, HUGE_VAL };
	CString			st_Message;
	CString			st_Where;
	FLOAT64			duration = 0.0;
	FLOAT64			step = 0.0;
	FLOAT64			fieldValue[SCN_MNV_FIELD_NUM] = { 0.0 };
	FLOAT64			turnPerStep;
	EN_TgtStatus	enStatus;
	INT32			isOk;
	INT32			nField;
	INT32			nTarget;
	INT32			nManeuver;

	(VOID)memset(st_Config, 0, sizeof(ST_SimConfig));
	f_Scn_SetIssue(st_Issue, SCN_AT_NONE, -1, -1, -1, CString());

	isOk = f_ReadNumber(st_DurationText, SCN_TIME_RES, SCN_DURATION_MAX, _T("시뮬레이션"), _T("시간"), &duration, st_Issue);

	if (isOk == 0)
	{
		st_Issue->enPlace = SCN_AT_DURATION;
	}
	else
	{
		isOk = f_ReadNumber(st_StepText, SCN_TIME_RES, SCN_STEP_MAX, _T("시뮬레이션"), _T("간격"), &step, st_Issue);

		if (isOk == 0)
		{
			st_Issue->enPlace = SCN_AT_STEP;
		}
	}

	// 1 ms 격자면 시각을 %.3f 로 정확히 쓸 수 있다.
	if ((isOk != 0) && (f_Scn_IsGridMultiple(step, SCN_TIME_RES) == 0))
	{
		f_Scn_SetIssue(st_Issue, SCN_AT_STEP, -1, -1, -1,
			CString(_T("시뮬레이션 간격: 1 ms (0.001 s) 단위여야 합니다 (입력: '")) + st_StepText + _T("')"));
		isOk = 0;
	}

	if (isOk != 0)
	{
		st_Config->durationTime	= duration;
		st_Config->stepTime		= step;

		isOk = f_ReadObject(&st_Platform, SCN_AT_PLATFORM, -1, 0.0,
			&st_Config->st_Platform.st_InitLla, &st_Config->st_Platform.st_InitAtt, &st_Config->st_Platform.headingSpeed, st_Issue);
	}

	if (isOk != 0)
	{
		st_Config->nTargetNum = nTargetNum;
	}

	for (nTarget = 0; (nTarget < nTargetNum) && (isOk != 0); nTarget++)
	{
		const ST_ObjectText	*st_Source = &st_Target[nTarget];
		ST_TargetInit		*st_Init = &st_Config->st_Target[nTarget];

		isOk = f_ReadObject(st_Source, SCN_AT_TARGET, nTarget, SCN_TGT_SPEED_MIN,
			&st_Init->st_InitLla, &st_Init->st_InitAtt, &st_Init->headingSpeed, st_Issue);

		if (isOk != 0)
		{
			st_Init->nManeuverNum = st_Source->nManeuverNum;
		}

		for (nManeuver = 0; (nManeuver < st_Source->nManeuverNum) && (isOk != 0); nManeuver++)
		{
			const ST_ManeuverText	*st_Text = &st_Source->st_Maneuver[nManeuver];
			ST_TargetManeuver		*st_Maneuver = &st_Init->st_Maneuver[nManeuver];

			st_Where = f_Scn_Where(SCN_AT_MANEUVER, nTarget, nManeuver);

			// 시작·종료의 순서와 겹침은 Core 가 판정하므로 여기서는 유한성만 본다.
			for (nField = 0; (nField < SCN_MNV_FIELD_NUM) && (isOk != 0); nField++)
			{
				isOk = f_ReadNumber(st_Text->st_FieldText[nField], fieldMin[nField], fieldMax[nField], st_Where, s_ManeuverFieldName[nField],
					&fieldValue[nField], st_Issue);

				if (isOk == 0)
				{
					st_Issue->enPlace	= SCN_AT_MANEUVER;
					st_Issue->nTarget	= nTarget;
					st_Issue->nManeuver	= nManeuver;
					st_Issue->nField	= nField;
				}
			}

			// 스텝 격자 밖 시각은 Core 에서 다음 스텝으로 밀리거나 기동이 통째로 무시될 수 있다.
			for (nField = SCN_FIELD_START; (nField <= SCN_FIELD_END) && (isOk != 0); nField++)
			{
				if (f_Scn_IsGridMultiple(fieldValue[nField], step) == 0)
				{
					st_Message.Format(_T("%s %s: 시뮬레이션 간격 %g s 의 정수배여야 합니다 (입력: '%s')"), st_Where.GetString(),
						s_ManeuverFieldName[nField], step, st_Text->st_FieldText[nField].GetString());
					f_Scn_SetIssue(st_Issue, SCN_AT_MANEUVER, nTarget, nManeuver, nField, st_Message);
					isOk = 0;
				}
			}

			// 스텝당 회전각을 30 도 이하로 묶어 중점법 오차와 비물리 선회를 막는다.
			if ((isOk != 0) && (st_Text->enTurnType != TGT_TURN_NONE))
			{
				turnPerStep = ((fabs(fieldValue[SCN_FIELD_GRAVITY]) * G_FORCE) / st_Init->headingSpeed) * step;

				if (turnPerStep > SCN_TURN_STEP_MAX)
				{
					// 허용 |G| 는 내림해서 보여 줘야 그 값을 그대로 넣었을 때 다시 걸리지 않는다.
					st_Message.Format(_T("%s G: 스텝당 회전각이 %.0f° 를 넘습니다 (%.3f°, 이 표적 속력·간격에서 허용 |G| ≤ %.3f)"),
						st_Where.GetString(), f_Rad_To_Deg(SCN_TURN_STEP_MAX), f_Rad_To_Deg(turnPerStep),
						floor(((SCN_TURN_STEP_MAX * st_Init->headingSpeed) / (G_FORCE * step)) * 1000.0) / 1000.0);
					f_Scn_SetIssue(st_Issue, SCN_AT_MANEUVER, nTarget, nManeuver, SCN_FIELD_GRAVITY, st_Message);
					isOk = 0;
				}
			}

			if (isOk != 0)
			{
				st_Maneuver->enTurnType		= st_Text->enTurnType;
				st_Maneuver->gravityValue	= fieldValue[SCN_FIELD_GRAVITY];
				st_Maneuver->startTime		= fieldValue[SCN_FIELD_START];
				st_Maneuver->endTime		= fieldValue[SCN_FIELD_END];
			}
		}
	}

	if (isOk != 0)
	{
		enStatus = f_Tgt_ValidateConfig(st_Config);

		if (enStatus != TGT_OK)
		{
			f_LocateCoreError(st_Config, enStatus, st_Issue);
			isOk = 0;
		}
	}

	return isOk;
}

VOID CScenario::f_LocateCoreError(const ST_SimConfig *st_Config, EN_TgtStatus enStatus, ST_ScnIssue *st_Issue)
{
	EN_TgtStatus	enProbe;
	CString			st_Message;
	INT32			isFound;
	INT32			nTarget;
	INT32			nManeuver;

	// 판정은 Core 가 하고, 기동을 뺀 설정과 기동을 하나씩 늘린 설정을 다시 검증해 위치만 찾는다.
	st_ProbeConfig = *st_Config;

	for (nTarget = 0; nTarget < TGT_MAX_TARGET_NUM; nTarget++)
	{
		st_ProbeConfig.st_Target[nTarget].nManeuverNum = 0;
	}

	enProbe = f_Tgt_ValidateConfig(&st_ProbeConfig);

	if (enProbe == TGT_ERR_TIME)
	{
		st_Message.Format(_T("시뮬레이션 시간: %s (시간 / 간격 = %.6f)"),
			f_StatusText(enProbe, 0).GetString(), st_Config->durationTime / st_Config->stepTime);
		f_Scn_SetIssue(st_Issue, SCN_AT_DURATION, -1, -1, -1, st_Message);
		isFound = 1;
	}
	else if (enProbe == TGT_ERR_SPEED)
	{
		f_Scn_SetIssue(st_Issue, SCN_AT_PLATFORM, -1, -1, SCN_FIELD_SPEED, CString(_T("플랫폼 속력: ")) + f_StatusText(enProbe, 0));
		isFound = 1;
	}
	else if (enProbe != TGT_OK)
	{
		f_Scn_SetIssue(st_Issue, SCN_AT_NONE, -1, -1, -1, f_StatusText(enProbe, 0));
		isFound = 1;
	}
	else
	{
		isFound = 0;
	}

	for (nTarget = 0; (nTarget < st_Config->nTargetNum) && (nTarget < TGT_MAX_TARGET_NUM) && (isFound == 0); nTarget++)
	{
		st_ProbeConfig.nTargetNum	= 1;
		st_ProbeConfig.st_Target[0]	= st_Config->st_Target[nTarget];

		for (nManeuver = 0; (nManeuver < st_Config->st_Target[nTarget].nManeuverNum) && (nManeuver < TGT_MAX_MANEUVER_NUM) && (isFound == 0); nManeuver++)
		{
			st_ProbeConfig.st_Target[0].nManeuverNum = nManeuver + 1;
			enProbe = f_Tgt_ValidateConfig(&st_ProbeConfig);

			if (enProbe != TGT_OK)
			{
				const ST_TargetManeuver	*st_Bad = &st_Config->st_Target[nTarget].st_Maneuver[nManeuver];
				INT32					nField = SCN_FIELD_START;

				// 종료가 시작보다 늦지 않거나 시뮬레이션 시간을 넘으면 종료 칸을, 그 밖에는(음수 시작, 겹침) 시작 칸을 가리킨다.
				if ((enProbe == TGT_ERR_TIME) && (st_Bad->startTime >= 0.0) &&
					((st_Bad->endTime <= st_Bad->startTime) || (st_Bad->endTime > st_Config->durationTime)))
				{
					nField = SCN_FIELD_END;
				}

				f_Scn_SetIssue(st_Issue, SCN_AT_MANEUVER, nTarget, nManeuver, nField,
					f_Scn_Where(SCN_AT_MANEUVER, nTarget, nManeuver) + _T(": ") + f_StatusText(enProbe, 1));
				isFound = 1;
			}
		}
	}

	if (isFound == 0)
	{
		f_Scn_SetIssue(st_Issue, SCN_AT_NONE, -1, -1, -1, f_StatusText(enStatus, 0));
	}
}

// ---------------------------------------------------------------------------
// 파일

static CStringA f_Scn_FileText(const CString &st_Text)
{
	CStringA st_Ascii(st_Text);

	// 한 줄에 쉼표로 나눠 쓰므로 구분 문자가 값에 섞이지 않게 한다.
	(VOID)st_Ascii.Replace(',', ' ');
	(VOID)st_Ascii.Replace('\r', ' ');
	(VOID)st_Ascii.Replace('\n', ' ');

	return st_Ascii;
}

static VOID f_Scn_WriteObject(FILE *st_File, const CHAR *pt_Key, const ST_ObjectText *st_Object)
{
	INT32 nField;

	(VOID)fprintf(st_File, "%s=", pt_Key);

	for (nField = 0; nField < SCN_OBJ_FIELD_NUM; nField++)
	{
		(VOID)fprintf(st_File, "%s%s", (nField > 0) ? "," : "", f_Scn_FileText(st_Object->st_FieldText[nField]).GetString());
	}

	(VOID)fputs("\r\n", st_File);
}

INT32 CScenario::f_Save(const CString &st_Path) const
{
	FILE	*st_File = nullptr;
	INT32	errorCode;
	INT32	nTarget;
	INT32	nManeuver;

	errorCode = _wfopen_s(&st_File, st_Path.GetString(), _T("wb"));

	if ((errorCode == 0) && (st_File != nullptr))
	{
		(VOID)fprintf(st_File, "%s %d\r\n", SCN_FILE_MAGIC, SCN_FILE_VERSION);
		(VOID)fprintf(st_File, "duration=%s\r\n", f_Scn_FileText(st_DurationText).GetString());
		(VOID)fprintf(st_File, "step=%s\r\n", f_Scn_FileText(st_StepText).GetString());
		f_Scn_WriteObject(st_File, "platform", &st_Platform);

		for (nTarget = 0; nTarget < nTargetNum; nTarget++)
		{
			f_Scn_WriteObject(st_File, "target", &st_Target[nTarget]);

			for (nManeuver = 0; nManeuver < st_Target[nTarget].nManeuverNum; nManeuver++)
			{
				const ST_ManeuverText *st_Maneuver = &st_Target[nTarget].st_Maneuver[nManeuver];

				(VOID)fprintf(st_File, "maneuver=%d,%s,%s,%s\r\n", static_cast<INT32>(st_Maneuver->enTurnType),
					f_Scn_FileText(st_Maneuver->st_FieldText[SCN_FIELD_GRAVITY]).GetString(),
					f_Scn_FileText(st_Maneuver->st_FieldText[SCN_FIELD_START]).GetString(),
					f_Scn_FileText(st_Maneuver->st_FieldText[SCN_FIELD_END]).GetString());
			}
		}

		if (ferror(st_File) != 0)
		{
			errorCode = (errno != 0) ? errno : EIO;
		}

		if ((fclose(st_File) != 0) && (errorCode == 0))
		{
			errorCode = (errno != 0) ? errno : EIO;
		}

		// 반쯤 쓴 파일을 남기지 않는다.
		if (errorCode != 0)
		{
			(VOID)_wremove(st_Path.GetString());
		}
	}
	else if (errorCode == 0)
	{
		errorCode = EIO;
	}
	else
	{
		// 열기 실패 코드를 그대로 돌려준다.
	}

	return errorCode;
}

// 쉼표로 나눈다. CString::Tokenize 는 빈 칸을 건너뛰어 칸이 밀리므로 쓰지 않는다.
static INT32 f_Scn_Split(const CString &st_Line, CString *st_Part, INT32 nMaxPart)
{
	INT32 nPart = 0;
	INT32 nStart = 0;
	INT32 nComma;
	INT32 isDone = 0;

	while ((isDone == 0) && (nPart < nMaxPart))
	{
		nComma = st_Line.Find(_T(','), nStart);

		if (nComma < 0)
		{
			st_Part[nPart] = st_Line.Mid(nStart);
			isDone = 1;
		}
		else
		{
			st_Part[nPart] = st_Line.Mid(nStart, nComma - nStart);
			nStart = nComma + 1;
		}

		(VOID)st_Part[nPart].Trim();
		st_Part[nPart] = st_Part[nPart].Left(SCN_TEXT_LIMIT);
		nPart = nPart + 1;
	}

	// 칸이 남았는데 자리가 모자라면 칸 수가 맞지 않는 것이다.
	if (isDone == 0)
	{
		nPart = nMaxPart + 1;
	}

	return nPart;
}

INT32 CScenario::f_Load(const CString &st_Path, CString *st_Error)
{
	CScenario	*st_New = new CScenario();
	FILE		*st_File = nullptr;
	CHAR		*pt_Data = nullptr;
	CString		st_Part[SCN_OBJ_FIELD_NUM + 1];
	CString		st_Text;
	CString		st_Line;
	CString		st_Key;
	CString		st_Value;
	UINT64		nRead = 0U;
	INT32		isOk = 0;
	INT32		isHeaderSeen = 0;
	INT32		isPlatformSeen = 0;
	INT32		nLine = 0;
	INT32		nPos = 0;
	INT32		nBreak;
	INT32		nEqual;
	INT32		nField;
	INT32		nType;
	FLOAT64		typeValue = 0.0;

	st_Error->Empty();
	st_New->st_DurationText	= _T("60");
	st_New->st_StepText		= _T("0.1");

	if ((_wfopen_s(&st_File, st_Path.GetString(), _T("rb")) != 0) || (st_File == nullptr))
	{
		*st_Error = _T("파일을 열 수 없습니다");
	}
	else
	{
		pt_Data = static_cast<CHAR *>(calloc(static_cast<UINT64>(SCN_FILE_MAX_BYTE) + 1U, sizeof(CHAR)));

		if (pt_Data == nullptr)
		{
			*st_Error = _T("메모리가 부족합니다");
		}
		else
		{
			// 한 바이트 더 읽어 보아 한도를 넘는 파일을 가려낸다.
			nRead = fread(pt_Data, sizeof(CHAR), static_cast<UINT64>(SCN_FILE_MAX_BYTE) + 1U, st_File);

			if (nRead > static_cast<UINT64>(SCN_FILE_MAX_BYTE))
			{
				*st_Error = _T("시나리오 파일이 너무 큽니다");
			}
			else
			{
				st_Text	= CString(CStringA(pt_Data, static_cast<INT32>(nRead)));
				isOk	= 1;
			}
		}

		(VOID)fclose(st_File);
	}

	while ((isOk != 0) && (nPos >= 0) && (nPos < st_Text.GetLength()))
	{
		nBreak	= st_Text.Find(_T('\n'), nPos);
		st_Line	= (nBreak < 0) ? st_Text.Mid(nPos) : st_Text.Mid(nPos, nBreak - nPos);
		nPos	= (nBreak < 0) ? -1 : (nBreak + 1);
		nLine	= nLine + 1;
		(VOID)st_Line.Trim();

		if (st_Line.IsEmpty() || (st_Line[0] == _T('#')))
		{
			// 빈 줄과 주석은 건너뛴다.
		}
		else if (isHeaderSeen == 0)
		{
			if (st_Line.Left(static_cast<INT32>(strlen(SCN_FILE_MAGIC))) == CString(SCN_FILE_MAGIC))
			{
				isHeaderSeen = 1;
			}
			else
			{
				*st_Error = _T("TargetSim 시나리오 파일이 아닙니다");
				isOk = 0;
			}
		}
		else
		{
			nEqual = st_Line.Find(_T('='));

			if (nEqual <= 0)
			{
				st_Error->Format(_T("%d 번째 줄을 읽을 수 없습니다"), nLine);
				isOk = 0;
			}
			else
			{
				st_Key		= st_Line.Left(nEqual);
				st_Value	= st_Line.Mid(nEqual + 1);
				(VOID)st_Key.Trim();
				(VOID)st_Value.Trim();

				if (st_Key == _T("duration"))
				{
					st_New->st_DurationText = st_Value.Left(SCN_TEXT_LIMIT);
				}
				else if (st_Key == _T("step"))
				{
					st_New->st_StepText = st_Value.Left(SCN_TEXT_LIMIT);
				}
				else if ((st_Key == _T("platform")) || (st_Key == _T("target")))
				{
					ST_ObjectText *st_Object = nullptr;

					if (f_Scn_Split(st_Value, st_Part, SCN_OBJ_FIELD_NUM) != SCN_OBJ_FIELD_NUM)
					{
						st_Error->Format(_T("%d 번째 줄: 값이 %d 개여야 합니다"), nLine, SCN_OBJ_FIELD_NUM);
						isOk = 0;
					}
					else if (st_Key == _T("platform"))
					{
						st_Object		= &st_New->st_Platform;
						isPlatformSeen	= 1;
					}
					else if (st_New->nTargetNum >= TGT_MAX_TARGET_NUM)
					{
						st_Error->Format(_T("%d 번째 줄: 표적은 %d 개까지입니다"), nLine, TGT_MAX_TARGET_NUM);
						isOk = 0;
					}
					else
					{
						st_Object = &st_New->st_Target[st_New->nTargetNum];
						st_New->nTargetNum = st_New->nTargetNum + 1;
					}

					if (st_Object != nullptr)
					{
						for (nField = 0; nField < SCN_OBJ_FIELD_NUM; nField++)
						{
							st_Object->st_FieldText[nField] = st_Part[nField];
						}
					}
				}
				else if (st_Key == _T("maneuver"))
				{
					if (st_New->nTargetNum < 1)
					{
						st_Error->Format(_T("%d 번째 줄: 기동은 표적 줄 뒤에 와야 합니다"), nLine);
						isOk = 0;
					}
					else if (f_Scn_Split(st_Value, st_Part, SCN_MNV_FIELD_NUM + 1) != (SCN_MNV_FIELD_NUM + 1))
					{
						st_Error->Format(_T("%d 번째 줄: 값이 %d 개여야 합니다"), nLine, SCN_MNV_FIELD_NUM + 1);
						isOk = 0;
					}
					else if ((f_Num_Parse(st_Part[0], &typeValue) != NUM_OK) || (typeValue < 0.0) || (typeValue > 3.0) ||
							 (f_Scn_IsGridMultiple(typeValue, 1.0) == 0))
					{
						st_Error->Format(_T("%d 번째 줄: 기동 축은 0~3 이어야 합니다"), nLine);
						isOk = 0;
					}
					else if (st_New->st_Target[st_New->nTargetNum - 1].nManeuverNum >= TGT_MAX_MANEUVER_NUM)
					{
						st_Error->Format(_T("%d 번째 줄: 기동은 표적마다 %d 개까지입니다"), nLine, TGT_MAX_MANEUVER_NUM);
						isOk = 0;
					}
					else
					{
						ST_ObjectText	*st_Object = &st_New->st_Target[st_New->nTargetNum - 1];
						ST_ManeuverText	*st_Maneuver = &st_Object->st_Maneuver[st_Object->nManeuverNum];

						nType = static_cast<INT32>(floor(typeValue + 0.5));
						st_Maneuver->enTurnType = static_cast<EN_TurnType>(nType);

						for (nField = 0; nField < SCN_MNV_FIELD_NUM; nField++)
						{
							st_Maneuver->st_FieldText[nField] = st_Part[nField + 1];
						}

						st_Object->nManeuverNum = st_Object->nManeuverNum + 1;
					}
				}
				else
				{
					// 모르는 키는 나중 판과의 호환을 위해 건너뛴다.
				}
			}
		}
	}

	if ((isOk != 0) && ((isHeaderSeen == 0) || (isPlatformSeen == 0) || (st_New->nTargetNum < 1)))
	{
		*st_Error = _T("플랫폼과 표적 1 개 이상이 있어야 합니다");
		isOk = 0;
	}

	// 끝까지 읽은 뒤에만 바꿔서, 읽다 실패하면 편집 중이던 시나리오가 그대로 남는다.
	if (isOk != 0)
	{
		st_DurationText	= st_New->st_DurationText;
		st_StepText		= st_New->st_StepText;
		st_Platform		= st_New->st_Platform;
		nTargetNum		= st_New->nTargetNum;

		for (nField = 0; nField < TGT_MAX_TARGET_NUM; nField++)
		{
			st_Target[nField] = st_New->st_Target[nField];
		}
	}

	free(pt_Data);
	delete st_New;

	return isOk;
}
