#include "pch.h"

#include <stdio.h>
#include <stdlib.h>

#include "Scenario.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define SCN_NEW_MANEUVER_SPAN	10.0					// [s] 새 기동 기본 길이
#define SCN_COPY_LAT_OFFSET		0.01					// [deg] 새 표적 북쪽 이동량
#define SCN_SPEC_TARGET_NUM		2						// 명세 / 기동 시연 프리셋의 표적 수
#define SCN_DEMO_MANEUVER_NUM	10						// 기동 시연 프리셋의 기동 줄 수

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

static const LPCTSTR	s_PresetName[SCN_PRESET_NUM] = { _T("기본 시나리오"), _T("응용 시나리오") };
static const LPCTSTR	s_TurnName[SCN_TURN_TYPE_NUM] = { _T("0 없음"), _T("1 Roll"), _T("2 Yaw"), _T("3 Pitch") };

// 과제 명세: 플랫폼 정지, 대함 표적, 대공 표적
static const ST_PresetObject	s_SpecPlatform = { { _T("32.0"), _T("126.0"), _T("0"), _T("0"), _T("0"), _T("0"), _T("0") } };
static const ST_PresetObject	s_SpecTarget[SCN_SPEC_TARGET_NUM] =
{
	{ { _T("32.125"), _T("126.03"), _T("0"), _T("30"), _T("0"), _T("0"), _T("270") } },
	{ { _T("32.12"), _T("126.0"), _T("300"), _T("200"), _T("0"), _T("0"), _T("180") } }
};

// 기동 시연: 명세 초기값에 지그재그 기동표 추가. G 부호가 회전 방향 (+ 우선회, - 좌선회).
// 대함(표적 1): Yaw 1 G, 45 도씩 4 회. 대공(표적 2): Yaw 8.205060 G, 90 도씩 6 회 (마지막만 150 도).
static const ST_PresetManeuver	s_DemoManeuver[SCN_DEMO_MANEUVER_NUM] =
{
	{ 0, TGT_TURN_YAW,		{ _T("-1.0"), _T("8.0"), _T("10.4") } },
	{ 0, TGT_TURN_YAW,		{ _T("1.0"), _T("15.8"), _T("18.2") } },
	{ 0, TGT_TURN_YAW,		{ _T("1.0"), _T("41.9"), _T("44.3") } },
	{ 0, TGT_TURN_YAW,		{ _T("-1.0"), _T("49.7"), _T("52.1") } },
	{ 1, TGT_TURN_YAW,		{ _T("-8.205060"), _T("2.8"), _T("6.7") } },
	{ 1, TGT_TURN_YAW,		{ _T("8.205060"), _T("13.3"), _T("17.2") } },
	{ 1, TGT_TURN_YAW,		{ _T("8.205060"), _T("18.6"), _T("22.5") } },
	{ 1, TGT_TURN_YAW,		{ _T("-8.205060"), _T("36.2"), _T("40.1") } },
	{ 1, TGT_TURN_YAW,		{ _T("-8.205060"), _T("41.5"), _T("45.4") } },
	{ 1, TGT_TURN_YAW,		{ _T("8.205060"), _T("53.5"), _T("60.0") } }
};

// 프리셋 값 -> 객체 편집 글자. 기동은 비움.
static VOID f_Scn_SetObject(ST_ObjectText *st_Object, const ST_PresetObject *st_Preset)
{
	INT32 nField;

	for (nField = 0; nField < SCN_OBJ_FIELD_NUM; nField++)
	{
		st_Object->st_FieldText[nField] = st_Preset->pt_Field[nField];
	}

	st_Object->nManeuverNum = 0;
}

// 객체 7 칸 -> 숫자. 각도는 도 -> 라디안.
static VOID f_Scn_ReadObject(const ST_ObjectText *st_Object, STRUCT_Coord_Lla *st_Lla, STRUCT_Coord_Attitude *st_Att, FLOAT64 *pt_Speed)
{
	st_Lla->Lat		= f_Deg_To_Rad(_tstof(st_Object->st_FieldText[SCN_FIELD_LAT]));
	st_Lla->Lon		= f_Deg_To_Rad(_tstof(st_Object->st_FieldText[SCN_FIELD_LON]));
	st_Lla->Alt		= _tstof(st_Object->st_FieldText[SCN_FIELD_ALT]);
	*pt_Speed		= _tstof(st_Object->st_FieldText[SCN_FIELD_SPEED]);
	st_Att->Roll	= f_Deg_To_Rad(_tstof(st_Object->st_FieldText[SCN_FIELD_ROLL]));
	st_Att->Pitch	= f_Deg_To_Rad(_tstof(st_Object->st_FieldText[SCN_FIELD_PITCH]));
	st_Att->Yaw		= f_Deg_To_Rad(_tstof(st_Object->st_FieldText[SCN_FIELD_YAW]));
}

// CScenario

// 프리셋 이름.
LPCTSTR CScenario::f_PresetName(INT32 nPreset)
{
	return s_PresetName[nPreset];
}

// 기동 회전축 이름.
LPCTSTR CScenario::f_TurnName(INT32 nTurnType)
{
	return s_TurnName[nTurnType];
}

// 프리셋 -> 편집 글자. 기동 시연은 명세 시나리오 + 기동표.
VOID CScenario::f_LoadPreset(INT32 nPreset)
{
	INT32 nTarget;
	INT32 nIndex;
	INT32 nField;

	f_Scn_SetObject(&st_Platform, &s_SpecPlatform);
	nTargetNum = SCN_SPEC_TARGET_NUM;

	for (nTarget = 0; nTarget < nTargetNum; nTarget++)
	{
		f_Scn_SetObject(&st_Target[nTarget], &s_SpecTarget[nTarget]);
	}

	if (nPreset == SCN_PRESET_MANEUVER)
	{
		for (nIndex = 0; nIndex < SCN_DEMO_MANEUVER_NUM; nIndex++)
		{
			ST_ObjectText	*st_Object = &st_Target[s_DemoManeuver[nIndex].nTarget];
			ST_ManeuverText	*st_New = &st_Object->st_Maneuver[st_Object->nManeuverNum];

			st_New->enTurnType = s_DemoManeuver[nIndex].enTurnType;

			for (nField = 0; nField < SCN_MNV_FIELD_NUM; nField++)
			{
				st_New->st_FieldText[nField] = s_DemoManeuver[nIndex].pt_Field[nField];
			}

			st_Object->nManeuverNum = st_Object->nManeuverNum + 1;
		}
	}
}

// 표적 추가. 원본 초기값 복사 후 위도만 조금 이동. 원본 없으면 마지막 표적 복사.
// 반환: 새 표적 번호, 가득 차면 -1.
INT32 CScenario::f_AddTarget(INT32 nSourceTarget)
{
	const INT32	nSource = (nSourceTarget >= 0) ? nSourceTarget : (nTargetNum - 1);
	INT32		nNewTarget = -1;
	INT32		nField;

	if (nTargetNum < TGT_MAX_TARGET_NUM)
	{
		nNewTarget = nTargetNum;

		for (nField = 0; nField < SCN_OBJ_FIELD_NUM; nField++)
		{
			st_Target[nNewTarget].st_FieldText[nField] = st_Target[nSource].st_FieldText[nField];
		}

		st_Target[nNewTarget].st_FieldText[SCN_FIELD_LAT].Format(_T("%.10g"), _tstof(st_Target[nSource].st_FieldText[SCN_FIELD_LAT]) + SCN_COPY_LAT_OFFSET);
		st_Target[nNewTarget].nManeuverNum = 0;
		nTargetNum = nTargetNum + 1;
	}

	return nNewTarget;
}

// 표적 삭제 후 뒤쪽 당김.
VOID CScenario::f_DeleteTarget(INT32 nTarget)
{
	INT32 nIndex;

	for (nIndex = nTarget; nIndex < (nTargetNum - 1); nIndex++)
	{
		st_Target[nIndex] = st_Target[nIndex + 1];
	}

	nTargetNum = nTargetNum - 1;
}

// 기동 추가. 직전 기동 종료 시각부터 10 s, Yaw 1 G 기본값. 가득 차면 -1.
INT32 CScenario::f_AddManeuver(INT32 nTarget)
{
	ST_ObjectText	*st_Object = &st_Target[nTarget];
	ST_ManeuverText	*st_New;
	INT32			nNewManeuver = -1;

	if (st_Object->nManeuverNum < TGT_MAX_MANEUVER_NUM)
	{
		nNewManeuver	= st_Object->nManeuverNum;
		st_New			= &st_Object->st_Maneuver[nNewManeuver];

		st_New->enTurnType						= TGT_TURN_YAW;
		st_New->st_FieldText[SCN_FIELD_GRAVITY]	= _T("1.0");
		st_New->st_FieldText[SCN_FIELD_START]	= (nNewManeuver > 0) ? st_Object->st_Maneuver[nNewManeuver - 1].st_FieldText[SCN_FIELD_END] : CString(_T("0"));
		st_New->st_FieldText[SCN_FIELD_END].Format(_T("%.10g"), _tstof(st_New->st_FieldText[SCN_FIELD_START]) + SCN_NEW_MANEUVER_SPAN);
		st_Object->nManeuverNum					= nNewManeuver + 1;
	}

	return nNewManeuver;
}

// 기동 삭제 후 뒤쪽 당김.
VOID CScenario::f_DeleteManeuver(INT32 nTarget, INT32 nManeuver)
{
	ST_ObjectText	*st_Object = &st_Target[nTarget];
	INT32			nIndex;

	for (nIndex = nManeuver; nIndex < (st_Object->nManeuverNum - 1); nIndex++)
	{
		st_Object->st_Maneuver[nIndex] = st_Object->st_Maneuver[nIndex + 1];
	}

	st_Object->nManeuverNum = st_Object->nManeuverNum - 1;
}

// 편집 글자 -> ST_SimConfig. 시간, 간격은 과제 명세 값.
VOID CScenario::f_BuildConfig(ST_SimConfig *st_Config) const
{
	INT32 nTarget;
	INT32 nManeuver;

	st_Config->durationTime	= SCN_DURATION_TIME;
	st_Config->stepTime		= SCN_STEP_TIME;
	st_Config->nTargetNum	= nTargetNum;

	f_Scn_ReadObject(&st_Platform, &st_Config->st_Platform.st_InitLla, &st_Config->st_Platform.st_InitAtt, &st_Config->st_Platform.headingSpeed);

	for (nTarget = 0; nTarget < nTargetNum; nTarget++)
	{
		const ST_ObjectText	*st_Source = &st_Target[nTarget];
		ST_TargetInit		*st_Init = &st_Config->st_Target[nTarget];

		f_Scn_ReadObject(st_Source, &st_Init->st_InitLla, &st_Init->st_InitAtt, &st_Init->headingSpeed);
		st_Init->nManeuverNum = st_Source->nManeuverNum;

		for (nManeuver = 0; nManeuver < st_Source->nManeuverNum; nManeuver++)
		{
			const ST_ManeuverText	*st_Text = &st_Source->st_Maneuver[nManeuver];
			ST_TargetManeuver		*st_Maneuver = &st_Init->st_Maneuver[nManeuver];

			st_Maneuver->enTurnType		= st_Text->enTurnType;
			st_Maneuver->gravityValue	= _tstof(st_Text->st_FieldText[SCN_FIELD_GRAVITY]);
			st_Maneuver->startTime		= _tstof(st_Text->st_FieldText[SCN_FIELD_START]);
			st_Maneuver->endTime		= _tstof(st_Text->st_FieldText[SCN_FIELD_END]);
		}
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

// 표본 -> 플랫폼 초기 위치 기준 동-북 평면 좌표.
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

// 결과 표 칸 하나의 글자. 표, CSV 공용.
// 시각 소수 3 자리, 위경도 9 자리, 고도 4 자리.
VOID CSimResult::f_FormatCell(INT32 nRow, INT32 nColumn, CHAR *pt_Buf, INT32 bufSize) const
{
	const ST_SimSample		*st_Sample = f_GetSample(nRow);
	const ST_TargetState	*st_State;

	if (nColumn == 0)
	{
		(VOID)_snprintf_s(pt_Buf, static_cast<UINT64>(bufSize), _TRUNCATE, "%d", st_Sample->nStepIndex);
	}
	else if (nColumn == 1)
	{
		(VOID)_snprintf_s(pt_Buf, static_cast<UINT64>(bufSize), _TRUNCATE, "%.3f", st_Sample->simTime);
	}
	else
	{
		// 객체마다 위도, 경도, 고도 세 칸
		st_State = f_GetState(nRow, (nColumn - 2) / 3);

		if (((nColumn - 2) % 3) == 0)
		{
			(VOID)_snprintf_s(pt_Buf, static_cast<UINT64>(bufSize), _TRUNCATE, "%.9f", f_Rad_To_Deg(st_State->st_Lla.Lat));
		}
		else if (((nColumn - 2) % 3) == 1)
		{
			(VOID)_snprintf_s(pt_Buf, static_cast<UINT64>(bufSize), _TRUNCATE, "%.9f", f_Rad_To_Deg(st_State->st_Lla.Lon));
		}
		else
		{
			(VOID)_snprintf_s(pt_Buf, static_cast<UINT64>(bufSize), _TRUNCATE, "%.4f", st_State->st_Lla.Alt);
		}
	}
}

// 결과 전체 CSV 저장 (한 줄 = 한 시각). 파일 열기 실패 시 0 반환.
INT32 CSimResult::f_WriteCsv(const CString &st_Path) const
{
	const INT32	nColumnNum = 2 + (3 * nObjectNum);
	FILE		*st_File = nullptr;
	CHAR		pt_Cell[RES_CELL_SIZE];
	INT32		isWritten = 0;
	INT32		nObject;
	INT32		nRow;
	INT32		nColumn;

	(VOID)_wfopen_s(&st_File, st_Path.GetString(), L"wb");

	if (st_File != nullptr)
	{
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

		for (nRow = 0; nRow < f_GetSampleNum(); nRow++)
		{
			for (nColumn = 0; nColumn < nColumnNum; nColumn++)
			{
				if (nColumn > 0)
				{
					(VOID)fputc(',', st_File);
				}

				f_FormatCell(nRow, nColumn, pt_Cell, RES_CELL_SIZE);
				(VOID)fputs(pt_Cell, st_File);
			}

			(VOID)fputs("\r\n", st_File);
		}

		(VOID)fclose(st_File);
		isWritten = 1;
	}

	return isWritten;
}
