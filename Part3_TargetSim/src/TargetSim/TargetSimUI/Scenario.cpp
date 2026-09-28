#include "pch.h"

#include <stdio.h>
#include <stdlib.h>

#include "Scenario.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define SCN_NEW_MANEUVER_SPAN	10.0					// [s] 새 기동 기본 길이
#define SCN_COPY_LAT_OFFSET		0.01					// [deg] 새 표적을 북쪽으로 조금 민다
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

static const LPCTSTR	s_PresetName[SCN_PRESET_NUM] = { _T("명세 시나리오 (과제 3)"), _T("기동 시연 (지그재그 기동 60 s)") };
static const LPCTSTR	s_TurnName[SCN_TURN_TYPE_NUM] = { _T("0 없음"), _T("1 Roll"), _T("2 Yaw"), _T("3 Pitch") };

// 과제 명세: 플랫폼 정지, 대함 표적, 대공 표적
static const ST_PresetObject	s_SpecPlatform = { { _T("32.0"), _T("126.0"), _T("0"), _T("0"), _T("0"), _T("0"), _T("0") } };
static const ST_PresetObject	s_SpecTarget[SCN_SPEC_TARGET_NUM] =
{
	{ { _T("32.125"), _T("126.03"), _T("0"), _T("30"), _T("0"), _T("0"), _T("270") } },
	{ { _T("32.12"), _T("126.0"), _T("300"), _T("200"), _T("0"), _T("0"), _T("180") } }
};

// 기동 시연: 명세 표적 두 개에 지그재그 기동을 얹은 60 s 시나리오.
// 초기값과 시뮬레이션 시간, 갱신 간격은 명세 시나리오와 같고 기동표만 다르다.
//
// 기동 지시는 "위도 32.1 에서 좌 90 도" 처럼 좌표로 오는데 기동 구조체에는 시각 칸만
// 있으므로, Core 를 실제로 돌려 그 좌표에 닿는 시각을 찾아 시작 / 종료로 옮겼다.
// 회전하는 동안에도 표적이 계속 나아가므로, 회전이 끝나는 자리가 지시받은 좌표가
// 되도록 선회 반경만큼 미리 꺾는다.
//
// ── 궤적 크기 ──────────────────────────────────────────────────────
// 속력과 시뮬레이션 시간이 명세로 묶여 있어 지시받은 회전 지점을 그대로 쓸 수 없다.
//   대함  30 m/s x 60 s =  1,800 m  <  필요  3,978 m (2.21 배 부족)
//   대공 200 m/s x 60 s = 12,000 m  <  필요 20,735 m (1.73 배 부족)
// 그래서 모양은 그대로 두고 출발점 기준으로 크기만 줄였다 (대함 0.5042, 대공 0.4788 배).
//
// ── G 값 ───────────────────────────────────────────────────────────
// 크기를 줄이면 선회 반경도 같은 비율로 줄어야 모양이 유지되므로 G 가 커진다.
// "정해진 시간에 정확히 목표 각도만큼 돈다" 는 조건에서 역산했다.
//   G = (dYaw[rad] * V) / (g * T),  g = 9.80665
//   대함  44.95 도 / 2.4 s -> |G| 1.000000  (선회 반경  92 m, 스텝당 1.87 도)
//   대공  89.90 도 / 3.9 s -> |G| 8.205060  (선회 반경 497 m, 스텝당 2.31 도)
//   대공 마지막 줄만 149.83 도 / 6.5 s 인데 각속도가 같아 G 는 그대로다.
// 부호가 회전 방향이다 (+ 우선회 / - 좌선회).
// 0.1 s 격자 위에서는 45 / 90 / 150 도를 정확히 맞출 수 없어 각각 44.9504 / 89.9007 /
// 149.8333 도가 되지만, 부호가 번갈아 상쇄되어 누적 침로 오차는 0.1 도 수준이다.
//
// ── 대함 표적 (표적 1) ─────────────────────────────────────────────
// Yaw 270(서) 으로 출발해 남서 -> 서 -> 북서 -> 서 로 한 번 비켜 가는 좌우 대칭 지그재그.
// 가운데가 0.0015 도 파이고, t=60 s 종점 32.125000 / 126.012232 로 출발 위도에서 끝난다.
//
// ── 대공 표적 (표적 2) ─────────────────────────────────────────────
// Yaw 180(남) 으로 출발해 남 -> 동 -> 남 -> 서 -> 남 -> 남서 로 왕복 훑기.
// 마지막 줄만 90 도가 아니라 150 도라 남서향으로 끝나고, 그 회전이 끝나는 t=60.0 s 가
// 그대로 종점이다. U 턴 가운데에 직선 남진 구간이 들어가는 것은 반원만으로는
// 낙차가 2 x 선회 반경밖에 되지 않아서다.
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

// 프리셋 값을 객체 편집 글자에 넣는다. 기동은 비운다.
static VOID f_Scn_SetObject(ST_ObjectText *st_Object, const ST_PresetObject *st_Preset)
{
	INT32 nField;

	for (nField = 0; nField < SCN_OBJ_FIELD_NUM; nField++)
	{
		st_Object->st_FieldText[nField] = st_Preset->pt_Field[nField];
	}

	st_Object->nManeuverNum = 0;
}

// 객체 하나의 일곱 칸을 숫자로 읽는다. 각도를 도에서 라디안으로 바꾸는 곳은 여기뿐이다.
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

// 프리셋 하나를 편집 글자에 채운다. 기동 시연은 명세 시나리오에 기동표만 더한 것이다.
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

// 표적을 하나 늘린다. 원본 표적의 초기값만 물려받고 위도를 조금 옮겨 그림에서 겹쳐 보이지 않게 한다.
// 원본이 없으면(플랫폼을 고른 채 추가) 마지막 표적을 물려받는다. 가득 차 있으면 -1
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

// 표적 하나를 지우고 뒤를 당긴다.
VOID CScenario::f_DeleteTarget(INT32 nTarget)
{
	INT32 nIndex;

	for (nIndex = nTarget; nIndex < (nTargetNum - 1); nIndex++)
	{
		st_Target[nIndex] = st_Target[nIndex + 1];
	}

	nTargetNum = nTargetNum - 1;
}

// 기동을 하나 늘린다. 직전 기동이 끝나는 시각부터 10 s 동안 Yaw 1 G 로 채워 둔다. 가득 차 있으면 -1
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

// 기동 하나를 지우고 뒤를 당긴다.
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

// 편집 글자를 ST_SimConfig 로 만든다. 시뮬레이션 시간과 간격은 과제 명세 값이다.
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

// 설정으로 전 구간을 돌려 표본을 모으고 그림 좌표를 만든다.
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

// 표본을 플랫폼 초기 위치 기준 동-북 평면 좌표로 바꾼다.
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

// 스텝과 객체 하나의 상태. 객체 0 은 플랫폼이다.
const ST_TargetState *CSimResult::f_GetState(INT32 nStep, INT32 nObject) const
{
	const ST_SimSample *st_Sample = f_GetSample(nStep);

	return (nObject == 0) ? &st_Sample->st_Platform : &st_Sample->st_Target[nObject - 1];
}

// 스텝과 객체 하나의 그림 좌표.
const ST_PlotPoint *CSimResult::f_GetPoint(INT32 nStep, INT32 nObject) const
{
	return &st_PointBuf[static_cast<UINT64>((nStep * nObjectNum) + nObject)];
}

// 결과 표 칸 하나의 글자. 표와 CSV 가 같은 서식을 쓰도록 여기로 모았다.
// 시각은 소수 3 자리, 위경도는 9 자리(약 0.1 mm), 고도는 4 자리로 적는다.
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

// 결과 전체를 CSV 로 쓴다. 한 줄이 한 시각이다. 파일을 열지 못하면(다른 프로그램이 잡고 있는 등) 0 을 돌려준다.
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
