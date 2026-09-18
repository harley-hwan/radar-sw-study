#include "pch.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Scenario.h"
#include "UiCommon.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define SCN_FILE_MAGIC			"TargetSim scenario"
#define SCN_FILE_VERSION		1
#define SCN_FILE_MAX_BYTE		262144
#define SCN_NEW_MANEUVER_SPAN	10.0					// [s] 새 기동 기본 길이
#define SCN_COPY_LAT_OFFSET		0.01					// [deg] 복제한 표적을 북쪽으로 조금 민다
#define RES_CSV_BUFFER_SIZE		1048576U
#define RES_BYTE_PER_MB			1048576.0

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

static const LPCTSTR	s_PresetName[SCN_PRESET_NUM] = { _T("명세 시나리오 (과제 3)"), _T("기동 시연 (선회, 상승, Roll)"), _T("다표적 접근 (표적 6)") };
static const LPCTSTR	s_FieldName[SCN_OBJ_FIELD_NUM] = { _T("위도"), _T("경도"), _T("고도"), _T("속력"), _T("Roll"), _T("Pitch"), _T("Yaw") };
static const LPCTSTR	s_ManeuverFieldName[SCN_MNV_FIELD_NUM] = { _T("G"), _T("시작"), _T("종료") };
static const LPCTSTR	s_TurnName[SCN_TURN_TYPE_NUM] = { _T("0 없음"), _T("1 Roll"), _T("2 Yaw"), _T("3 Pitch") };

// 과제 명세: 플랫폼 정지, 대함 표적, 대공 표적
static const ST_PresetObject	s_SpecPlatform = { { _T("32.0"), _T("126.0"), _T("0"), _T("0"), _T("0"), _T("0"), _T("0") } };
static const ST_PresetObject	s_SpecTarget[2] =
{
	{ { _T("32.125"), _T("126.03"), _T("0"), _T("30"), _T("0"), _T("0"), _T("270") } },
	{ { _T("32.12"), _T("126.0"), _T("300"), _T("200"), _T("0"), _T("0"), _T("180") } }
};

// 기동 시연: 명세 표적에 선회, 상승과 수평 복귀, Roll
static const ST_PresetManeuver	s_DemoManeuver[6] =
{
	{ 0, TGT_TURN_YAW,		{ _T("-1.0"), _T("10"), _T("20") } },
	{ 0, TGT_TURN_YAW,		{ _T("1.0"), _T("35"), _T("45") } },
	{ 1, TGT_TURN_YAW,		{ _T("2.0"), _T("5"), _T("25") } },
	{ 1, TGT_TURN_PITCH,	{ _T("0.5"), _T("25"), _T("30") } },
	{ 1, TGT_TURN_PITCH,	{ _T("-0.5"), _T("40"), _T("45") } },
	{ 1, TGT_TURN_ROLL,		{ _T("1.0"), _T("45"), _T("50") } }
};

// 다표적 접근: 플랫폼에서 15 km, 60 도 간격, 플랫폼을 향함
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

// 객체 하나의 편집 글자와 기동을 전부 비운다.
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

// 프리셋 값을 객체 편집 글자에 넣는다.
static VOID f_Scn_SetObject(ST_ObjectText *st_Object, const ST_PresetObject *st_Preset)
{
	INT32 nField;

	f_Scn_ClearObject(st_Object);

	for (nField = 0; nField < SCN_OBJ_FIELD_NUM; nField++)
	{
		st_Object->st_FieldText[nField] = st_Preset->pt_Field[nField];
	}
}

// 입력 오류의 위치와 문구를 채운다.
static VOID f_Scn_SetIssue(ST_ScnIssue *st_Issue, EN_ScnPlace enPlace, INT32 nTarget, INT32 nManeuver, INT32 nField, const CString &st_Message)
{
	st_Issue->enPlace		= enPlace;
	st_Issue->nTarget		= nTarget;
	st_Issue->nManeuver		= nManeuver;
	st_Issue->nField		= nField;
	st_Issue->st_Message	= st_Message;
}

// 오류 문구 앞에 붙일 위치 이름 ("표적 2 기동 1" 처럼).
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

// 값이 unit 의 정수배인지 본다.
static INT32 f_Scn_IsGridMultiple(FLOAT64 value, FLOAT64 unit)
{
	const FLOAT64 ratio = value / unit;

	return (fabs(ratio - floor(ratio + 0.5)) <= SCN_GRID_TOL) ? 1 : 0;
}

// CScenario

// 편집 모델. 모든 칸을 비운 채로 시작한다.
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

// 프리셋 이름.
LPCTSTR CScenario::f_PresetName(INT32 nPreset)
{
	return ((nPreset >= 0) && (nPreset < SCN_PRESET_NUM)) ? s_PresetName[nPreset] : _T("");
}

// 기동 회전축 이름.
LPCTSTR CScenario::f_TurnName(INT32 nTurnType)
{
	return ((nTurnType >= 0) && (nTurnType < SCN_TURN_TYPE_NUM)) ? s_TurnName[nTurnType] : _T("");
}

// 프리셋 하나를 편집 글자에 채운다.
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

// 표적을 하나 늘린다. 원본 표적의 값을 물려받고 위도만 조금 옮겨 그림에서 겹쳐 보이지 않게 한다.
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

			// 같은 자리면 그림에서 하나로 보인다.
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

// 표적 하나를 지우고 뒤를 당긴다. 마지막 하나는 지우지 않는다.
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

// 기동을 하나 늘린다. 직전 기동의 종료에서 시작하는 10 s 짜리 Yaw 1 G 를 채워 넣는다.
INT32 CScenario::f_AddManeuver(INT32 nTarget)
{
	INT32	nNewManeuver = -1;
	FLOAT64	duration = 0.0;
	FLOAT64	startTime = 0.0;
	FLOAT64	maxEnd;
	CString	st_Start = _T("0");
	CString	st_End;

	if ((nTarget >= 0) && (nTarget < nTargetNum) && (st_Target[nTarget].nManeuverNum < TGT_MAX_MANEUVER_NUM))
	{
		ST_ObjectText	*st_Object = &st_Target[nTarget];
		ST_ManeuverText	*st_New;

		nNewManeuver	= st_Object->nManeuverNum;
		st_New			= &st_Object->st_Maneuver[nNewManeuver];

		// 직전 기동의 종료에서 시작하는 10 s 짜리 Yaw 1 G. 시뮬레이션 시간이 남아 있으면 거기서 자른다.
		if (nNewManeuver > 0)
		{
			st_Start = st_Object->st_Maneuver[nNewManeuver - 1].st_FieldText[SCN_FIELD_END];
		}

		if (f_Num_Parse(st_DurationText, &duration) != NUM_OK)
		{
			duration = SCN_DURATION_MAX;
		}

		maxEnd = SCN_DURATION_MAX;

		if ((f_Num_Parse(st_Start, &startTime) == NUM_OK) && (startTime < (duration - SCN_TIME_RES)))
		{
			maxEnd = duration;
		}

		if (f_Num_Nudge(st_Start, SCN_NEW_MANEUVER_SPAN, 0, 0.0, maxEnd, &st_End) == 0)
		{
			st_End.Empty();
		}

		st_New->enTurnType						= TGT_TURN_YAW;
		st_New->st_FieldText[SCN_FIELD_GRAVITY]	= _T("1.0");
		st_New->st_FieldText[SCN_FIELD_START]	= st_Start;
		st_New->st_FieldText[SCN_FIELD_END]		= st_End;
		st_Object->nManeuverNum					= nNewManeuver + 1;
	}

	return nNewManeuver;
}

// 기동 하나를 지우고 뒤를 당긴다.
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

// 엔진 오류 코드를 화면에 쓸 한국어 문구로 바꾼다.
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
			st_Text = _T("기동 시각은 0 <= 시작 < 종료 <= 시뮬레이션 시간이어야 합니다");
		}
		else
		{
			st_Text.Format(_T("시뮬레이션 시간은 간격의 정수배이고 스텝 수는 1~%d 이어야 합니다"), TGT_MAX_STEP_NUM);
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

// 칸 하나를 실수로 읽고 허용 범위를 본다. 실패하면 이유를 문구로 남긴다.
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
		st_Rule = _T("숫자로 읽을 수 없습니다 (예: -0.5, 1e3)");
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

// 객체 하나의 일곱 칸을 읽는다. 각도를 도에서 라디안으로 바꾸는 곳은 여기뿐이다.
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

// 편집 글자를 ST_SimConfig 로 만든다. UI 범위를 먼저 보고 Core 검증으로 마무리한다.
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

	// 1 ms 격자여야 시각을 %.3f 로 정확히 쓴다.
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

			// 시작, 종료의 순서와 겹침은 Core 가 판정한다.
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

			// 스텝 격자 밖 시각은 Core 에서 다음 스텝으로 밀린다.
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

			// 스텝당 회전각 30 도 제한
			if ((isOk != 0) && (st_Text->enTurnType != TGT_TURN_NONE))
			{
				turnPerStep = ((fabs(fieldValue[SCN_FIELD_GRAVITY]) * G_FORCE) / st_Init->headingSpeed) * step;

				if (turnPerStep > SCN_TURN_STEP_MAX)
				{
					// 허용 |G| 는 내림해서 보여 준다.
					st_Message.Format(_T("%s G: 스텝당 회전각이 %.0f 도를 넘습니다 (%.3f 도, 이 속력과 간격에서 허용 |G| <= %.3f)"),
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

// Core 판정을 그대로 두고, 기동을 뺀 설정과 기동을 하나씩 늘린 설정을 다시 검증해 위치만 찾는다.
VOID CScenario::f_LocateCoreError(const ST_SimConfig *st_Config, EN_TgtStatus enStatus, ST_ScnIssue *st_Issue)
{
	EN_TgtStatus	enProbe;
	CString			st_Message;
	INT32			isFound;
	INT32			nTarget;
	INT32			nManeuver;

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

				// 종료가 시작보다 늦지 않거나 시뮬레이션 시간을 넘으면 종료 칸
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

// 시나리오 파일

// 시나리오 파일에 쓸 글자에서 구분자와 줄바꿈을 지운다.
static CStringA f_Scn_FileText(const CString &st_Text)
{
	CStringA st_Ascii(st_Text);

	(VOID)st_Ascii.Replace(',', ' ');
	(VOID)st_Ascii.Replace('\r', ' ');
	(VOID)st_Ascii.Replace('\n', ' ');

	return st_Ascii;
}

// 객체 한 줄을 시나리오 파일에 쓴다.
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

// 시나리오를 글자 파일로 저장한다. 쓰다가 실패하면 반쯤 쓴 파일을 지운다.
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
		// 열기 실패 코드 그대로
	}

	return errorCode;
}

// 쉼표로 나눈다. CString::Tokenize 는 빈 칸을 건너뛰어 칸이 밀린다.
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

	if (isDone == 0)
	{
		nPart = nMaxPart + 1;
	}

	return nPart;
}

// 시나리오 파일을 읽는다. 끝까지 성공했을 때만 지금 시나리오를 바꾼다.
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
			// 빈 줄, 주석
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
					// 모르는 키는 건너뛴다.
				}
			}
		}
	}

	if ((isOk != 0) && ((isHeaderSeen == 0) || (isPlatformSeen == 0) || (st_New->nTargetNum < 1)))
	{
		*st_Error = _T("플랫폼과 표적 1 개 이상이 있어야 합니다");
		isOk = 0;
	}

	// 끝까지 읽은 뒤에만 바꾼다.
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

// CSimResult

// 상태의 시각, 위치, 속도가 모두 유한한지 본다.
static INT32 f_Res_IsStateFinite(const ST_TargetState *st_State)
{
	INT32 isFinite = 0;

	if (isfinite(st_State->simTime) && isfinite(st_State->st_Lla.lat) && isfinite(st_State->st_Lla.lon) &&
		isfinite(st_State->st_Lla.alt) && isfinite(st_State->st_PosEcef.x) && isfinite(st_State->st_PosEcef.y) &&
		isfinite(st_State->st_PosEcef.z) && isfinite(st_State->st_VelEcef.x) && isfinite(st_State->st_VelEcef.y) &&
		isfinite(st_State->st_VelEcef.z))
	{
		isFinite = 1;
	}

	return isFinite;
}

// 실행 결과. 버퍼는 실행할 때 잡는다.
CSimResult::CSimResult() noexcept
	: st_SampleBuf(nullptr)
	, st_PointBuf(nullptr)
	, nSampleNum(0)
	, nObjectNum(0)
	, stepTime(0.0)
	, runMs(0.0)
	, st_PendingSample(nullptr)
	, st_PendingPoint(nullptr)
	, nPendingSampleNum(0)
	, nPendingObjectNum(0)
	, pendingStepTime(0.0)
	, pendingRunMs(0.0)
{
	(VOID)memset(&st_Sim, 0, sizeof(st_Sim));
}

// 현재 버퍼와 대기 버퍼를 모두 푼다.
CSimResult::~CSimResult()
{
	free(st_SampleBuf);
	free(st_PointBuf);
	free(st_PendingSample);
	free(st_PendingPoint);
}

// 대기 버퍼를 현재 결과로 바꾼다. 표와 그림이 옛 버퍼를 놓은 뒤에 불러야 한다.
VOID CSimResult::f_Commit(VOID)
{
	if (st_PendingSample != nullptr)
	{
		free(st_SampleBuf);
		free(st_PointBuf);

		st_SampleBuf		= st_PendingSample;
		st_PointBuf			= st_PendingPoint;
		nSampleNum			= nPendingSampleNum;
		nObjectNum			= nPendingObjectNum;
		stepTime			= pendingStepTime;
		runMs				= pendingRunMs;

		st_PendingSample	= nullptr;
		st_PendingPoint		= nullptr;
		nPendingSampleNum	= 0;
		nPendingObjectNum	= 0;
	}
}

// 설정으로 전 구간을 돌려 대기 버퍼에 담는다. 실패하면 지금 결과는 그대로 둔다.
INT32 CSimResult::f_Run(const ST_SimConfig *st_Config, CString *st_Error)
{
	ST_SimSample	*st_NewSample = nullptr;
	ST_PlotPoint	*st_NewPoint = nullptr;
	EN_TgtStatus	enStatus;
	LARGE_INTEGER	st_Frequency;
	LARGE_INTEGER	st_Begin;
	LARGE_INTEGER	st_End;
	UINT64			sampleBytes;
	UINT64			pointBytes;
	INT32			nNewSampleNum = 0;
	INT32			nNewObjectNum = 0;
	INT32			nStep;
	INT32			isOk = 1;

	st_Error->Empty();
	(VOID)::QueryPerformanceFrequency(&st_Frequency);
	(VOID)::QueryPerformanceCounter(&st_Begin);

	free(st_PendingSample);
	free(st_PendingPoint);
	st_PendingSample	= nullptr;
	st_PendingPoint		= nullptr;

	(VOID)memset(&st_Sim, 0, sizeof(st_Sim));
	enStatus = f_Tgt_InitSim(&st_Sim, st_Config);

	if (enStatus != TGT_OK)
	{
		*st_Error = CString(_T("초기화 실패: ")) + CScenario::f_StatusText(enStatus, 0);
		isOk = 0;
	}
	else if ((st_Sim.nStepNum < 1) || (st_Sim.nStepNum > TGT_MAX_STEP_NUM))
	{
		*st_Error = CString(_T("초기화 실패: ")) + CScenario::f_StatusText(TGT_ERR_SIM_STATE, 0);
		isOk = 0;
	}
	else
	{
		nNewSampleNum	= st_Sim.nStepNum + 1;
		nNewObjectNum	= st_Config->nTargetNum + 1;
		sampleBytes		= static_cast<UINT64>(nNewSampleNum) * static_cast<UINT64>(sizeof(ST_SimSample));
		pointBytes		= static_cast<UINT64>(nNewSampleNum) * static_cast<UINT64>(nNewObjectNum) * static_cast<UINT64>(sizeof(ST_PlotPoint));

		// 최악 116 MB. new 의 예외 대신 NULL 로 받는다.
		st_NewSample	= static_cast<ST_SimSample *>(calloc(static_cast<UINT64>(nNewSampleNum), sizeof(ST_SimSample)));
		st_NewPoint		= static_cast<ST_PlotPoint *>(calloc(static_cast<UINT64>(nNewSampleNum) * static_cast<UINT64>(nNewObjectNum), sizeof(ST_PlotPoint)));

		if ((st_NewSample == nullptr) || (st_NewPoint == nullptr))
		{
			st_Error->Format(_T("메모리 부족: 결과 버퍼에 약 %.0f MB 가 필요합니다"), static_cast<FLOAT64>(sampleBytes + pointBytes) / RES_BYTE_PER_MB);
			isOk = 0;
		}
	}

	if (isOk != 0)
	{
		st_NewSample[0] = st_Sim.st_Sample;

		for (nStep = 1; (nStep < nNewSampleNum) && (isOk != 0); nStep++)
		{
			enStatus = f_Tgt_StepSim(&st_Sim);

			if (enStatus == TGT_OK)
			{
				st_NewSample[nStep] = st_Sim.st_Sample;
			}
			else
			{
				st_Error->Format(_T("스텝 %d (t = %s s) 진행 실패: "), nStep,
					f_Num_ToText(static_cast<FLOAT64>(nStep) * st_Config->stepTime, 3).GetString());
				*st_Error += CScenario::f_StatusText(enStatus, 0);
				isOk = 0;
			}
		}
	}

	if (isOk != 0)
	{
		isOk = f_ComputePoints(st_NewSample, st_NewPoint, nNewSampleNum, nNewObjectNum, st_Error);
	}

	if (isOk != 0)
	{
		(VOID)::QueryPerformanceCounter(&st_End);

		st_PendingSample	= st_NewSample;
		st_PendingPoint		= st_NewPoint;
		nPendingSampleNum	= nNewSampleNum;
		nPendingObjectNum	= nNewObjectNum;
		pendingStepTime		= st_Config->stepTime;
		pendingRunMs		= (st_Frequency.QuadPart > 0) ?
			((static_cast<FLOAT64>(st_End.QuadPart - st_Begin.QuadPart) * 1000.0) / static_cast<FLOAT64>(st_Frequency.QuadPart)) : 0.0;
	}
	else
	{
		free(st_NewSample);
		free(st_NewPoint);
	}

	return isOk;
}

// 표본을 플랫폼 초기 위치 기준 동-북 평면 좌표로 바꾼다. 기준 행렬은 한 번만 만든다.
INT32 CSimResult::f_ComputePoints(const ST_SimSample *st_Sample, ST_PlotPoint *st_Point, INT32 nNewSampleNum, INT32 nNewObjectNum, CString *st_Error) const
{
	const ST_TargetState	*st_Origin = &st_Sample[0].st_Platform;
	const ST_TargetState	*st_State;
	ST_Matrix				st_Dcm;
	ST_CoordRect			st_Diff;
	ST_CoordRect			st_Ned;
	INT32					isOk = 1;
	INT32					nStep;
	INT32					nObject;

	// 기준 DCM 은 한 번만 만든다.
	if ((f_Res_IsStateFinite(st_Origin) == 0) || (f_Coord_Dcm_Ned_To_Ecef(&st_Dcm, st_Origin->st_Lla.lat, st_Origin->st_Lla.lon) != COORD_OK))
	{
		*st_Error = _T("스텝 0 플랫폼: 결과에 유한하지 않은 값이 있거나 기준 좌표를 만들 수 없습니다");
		isOk = 0;
	}

	for (nStep = 0; (nStep < nNewSampleNum) && (isOk != 0); nStep++)
	{
		for (nObject = 0; (nObject < nNewObjectNum) && (isOk != 0); nObject++)
		{
			st_State = (nObject == 0) ? &st_Sample[nStep].st_Platform : &st_Sample[nStep].st_Target[nObject - 1];

			if (f_Res_IsStateFinite(st_State) == 0)
			{
				if (nObject == 0)
				{
					st_Error->Format(_T("스텝 %d 플랫폼: 결과에 유한하지 않은 값이 있어 버렸습니다"), nStep);
				}
				else
				{
					st_Error->Format(_T("스텝 %d 표적 %d: 결과에 유한하지 않은 값이 있어 버렸습니다"), nStep, nObject);
				}

				isOk = 0;
			}
			else if ((f_Coord_VecSub(&st_Diff, &st_State->st_PosEcef, &st_Origin->st_PosEcef) != COORD_OK) ||
					 (f_Coord_RotateVecInv(&st_Ned, &st_Dcm, &st_Diff) != COORD_OK))
			{
				st_Error->Format(_T("스텝 %d: 그림 좌표 변환에 실패했습니다"), nStep);
				isOk = 0;
			}
			else
			{
				st_Point[(nStep * nNewObjectNum) + nObject].east	= st_Ned.y;
				st_Point[(nStep * nNewObjectNum) + nObject].north	= st_Ned.x;
			}
		}
	}

	return isOk;
}

// 표본 수.
INT32 CSimResult::f_GetSampleNum(VOID) const
{
	return nSampleNum;
}

// 객체 수 (플랫폼 + 표적).
INT32 CSimResult::f_GetObjectNum(VOID) const
{
	return nObjectNum;
}

// 결과 표의 열 수. 스텝, 시각, 객체마다 위도 경도 고도.
INT32 CSimResult::f_GetColumnNum(VOID) const
{
	return (nSampleNum > 0) ? (2 + (3 * nObjectNum)) : 0;
}

// 시간 갱신 간격.
FLOAT64 CSimResult::f_GetStepTime(VOID) const
{
	return stepTime;
}

// 직전 실행에 걸린 시간.
FLOAT64 CSimResult::f_GetRunMs(VOID) const
{
	return runMs;
}

// 스텝 하나의 표본. 범위 밖이면 NULL.
const ST_SimSample *CSimResult::f_GetSample(INT32 nStep) const
{
	const ST_SimSample *st_Sample = nullptr;

	if ((st_SampleBuf != nullptr) && (nStep >= 0) && (nStep < nSampleNum))
	{
		st_Sample = &st_SampleBuf[nStep];
	}

	return st_Sample;
}

// 스텝과 객체 하나의 상태. 객체 0 은 플랫폼이다.
const ST_TargetState *CSimResult::f_GetState(INT32 nStep, INT32 nObject) const
{
	const ST_SimSample		*st_Sample = f_GetSample(nStep);
	const ST_TargetState	*st_State = nullptr;

	if ((st_Sample != nullptr) && (nObject >= 0) && (nObject < nObjectNum))
	{
		st_State = (nObject == 0) ? &st_Sample->st_Platform : &st_Sample->st_Target[nObject - 1];
	}

	return st_State;
}

// 스텝과 객체 하나의 그림 좌표.
const ST_PlotPoint *CSimResult::f_GetPoint(INT32 nStep, INT32 nObject) const
{
	const ST_PlotPoint *st_Point = nullptr;

	if ((st_PointBuf != nullptr) && (nStep >= 0) && (nStep < nSampleNum) && (nObject >= 0) && (nObject < nObjectNum))
	{
		st_Point = &st_PointBuf[(nStep * nObjectNum) + nObject];
	}

	return st_Point;
}

// 결과 표 칸 하나의 글자. 표와 CSV 가 같은 서식을 쓰도록 여기로 모았다.
INT32 CSimResult::f_FormatCell(INT32 nRow, INT32 nColumn, CHAR *pt_Buf, INT32 bufSize) const
{
	const ST_SimSample		*st_Sample;
	const ST_TargetState	*st_State;
	INT32					nLength = -1;
	INT32					nObject;
	INT32					nPart;

	if ((pt_Buf != nullptr) && (bufSize > 0))
	{
		pt_Buf[0] = '\0';
	}

	if ((pt_Buf != nullptr) && (bufSize > 1) && (st_SampleBuf != nullptr) && (nRow >= 0) && (nRow < nSampleNum) &&
		(nColumn >= 0) && (nColumn < (2 + (3 * nObjectNum))))
	{
		st_Sample = &st_SampleBuf[nRow];

		if (nColumn == 0)
		{
			nLength = _snprintf_s(pt_Buf, static_cast<UINT64>(bufSize), _TRUNCATE, "%d", st_Sample->nStepIndex);
		}
		else if (nColumn == 1)
		{
			nLength = f_Num_FormatFixed(pt_Buf, bufSize, st_Sample->simTime, 3);
		}
		else
		{
			nObject		= (nColumn - 2) / 3;
			nPart		= (nColumn - 2) % 3;
			st_State	= (nObject == 0) ? &st_Sample->st_Platform : &st_Sample->st_Target[nObject - 1];

			if (nPart == 0)
			{
				nLength = f_Num_FormatFixed(pt_Buf, bufSize, f_Rad_To_Deg(st_State->st_Lla.lat), 9);
			}
			else if (nPart == 1)
			{
				nLength = f_Num_FormatFixed(pt_Buf, bufSize, f_Rad_To_Deg(st_State->st_Lla.lon), 9);
			}
			else
			{
				nLength = f_Num_FormatFixed(pt_Buf, bufSize, st_State->st_Lla.alt, 4);
			}
		}
	}

	return nLength;
}

// 결과 전체를 CSV 로 쓴다. 한 줄이 한 시각이다.
INT32 CSimResult::f_WriteCsv(const CString &st_Path, INT32 *pt_LineNum) const
{
	const INT32		nColumnNum = 2 + (3 * nObjectNum);
	FILE			*st_File = nullptr;
	CHAR			pt_Cell[RES_CELL_SIZE];
	INT32			errorCode;
	INT32			nLineNum = 0;
	INT32			nObject;
	INT32			nRow;
	INT32			nColumn;

	errorCode = _wfopen_s(&st_File, st_Path.GetString(), _T("wb"));

	if ((errorCode == 0) && (st_File != nullptr))
	{
		(VOID)setvbuf(st_File, nullptr, _IOFBF, RES_CSV_BUFFER_SIZE);

		// 가로형: 한 줄이 한 시각
		(VOID)fputs("step,time_s", st_File);

		for (nObject = 0; nObject < nObjectNum; nObject++)
		{
			if (nObject == 0)
			{
				(VOID)fputs(",platform_lat_deg,platform_lon_deg,platform_alt_m", st_File);
			}
			else
			{
				(VOID)fprintf(st_File, ",target%d_lat_deg,target%d_lon_deg,target%d_alt_m", nObject, nObject, nObject);
			}
		}

		(VOID)fputs("\r\n", st_File);
		nLineNum = 1;

		for (nRow = 0; nRow < nSampleNum; nRow++)
		{
			for (nColumn = 0; nColumn < nColumnNum; nColumn++)
			{
				if (nColumn > 0)
				{
					(VOID)fputc(',', st_File);
				}

				if (f_FormatCell(nRow, nColumn, pt_Cell, RES_CELL_SIZE) > 0)
				{
					(VOID)fputs(pt_Cell, st_File);
				}
			}

			(VOID)fputs("\r\n", st_File);
			nLineNum = nLineNum + 1;
		}

		if (ferror(st_File) != 0)
		{
			errorCode = (errno != 0) ? errno : EIO;
		}

		if ((fclose(st_File) != 0) && (errorCode == 0))
		{
			errorCode = (errno != 0) ? errno : EIO;
		}

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
		nLineNum = 0;
	}

	*pt_LineNum = nLineNum;

	return errorCode;
}
